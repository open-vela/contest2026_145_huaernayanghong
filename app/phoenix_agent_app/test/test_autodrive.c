/**
 * @file test_autodrive.c
 * @brief 模拟器端到端自动化测试与自驱动场景引擎实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "test_autodrive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __NuttX__
#include <sys/ioctl.h>
#include <nuttx/video/fb.h>
#endif

#include "core/cartridge_mgr.h"
#include "cartridges/cartridge_agent.h"
#include "ui/ui_settings.h"
#include "core/event_bus.h"
#include "core/store.h"
#include "core/tool_registry.h"
#include "core/expression.h"
#include "utils/log_utils.h"

#define TAG "AutoDrive"

static const char s_b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_encode_chunk(const uint8_t *data, size_t len, char *out, size_t *out_len)
{
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t a = (i < len) ? data[i] : 0;
        uint32_t b = (i + 1 < len) ? data[i + 1] : 0;
        uint32_t c = (i + 2 < len) ? data[i + 2] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;

        out[o++] = s_b64_table[(triple >> 18) & 0x3F];
        out[o++] = s_b64_table[(triple >> 12) & 0x3F];
        out[o++] = (i + 1 < len) ? s_b64_table[(triple >> 6) & 0x3F] : '=';
        out[o++] = (i + 2 < len) ? s_b64_table[triple & 0x3F] : '=';
    }
    *out_len = o;
}

static void dump_screen_framebuffer(const char *scenario_id)
{
#ifdef __NuttX__
    int fd = open("/dev/fb0", O_RDONLY);
    if (fd < 0) {
        printf("[AUTOTEST:DUMP_WARN] /dev/fb0 open failed\n");
        return;
    }

    struct fb_videoinfo_s vinfo;
    struct fb_planeinfo_s pinfo;
    memset(&vinfo, 0, sizeof(vinfo));
    memset(&pinfo, 0, sizeof(pinfo));
    pinfo.display = 0;

    if (ioctl(fd, FBIOGET_VIDEOINFO, &vinfo) < 0 || ioctl(fd, FBIOGET_PLANEINFO, &pinfo) < 0) {
        close(fd);
        printf("[AUTOTEST:DUMP_WARN] ioctl fb info failed\n");
        return;
    }

    uint32_t w = vinfo.xres;
    uint32_t h = vinfo.yres;
    uint32_t stride = pinfo.stride;
    uint32_t bpp = pinfo.bpp;
    uint8_t *fb = (uint8_t *)pinfo.fbmem;

    if (!fb || w == 0 || h == 0 || w > 1920 || h > 1080) {
        printf("[AUTOTEST:DUMP_WARN] screen buffer unavailable (fb=%p, w=%u, h=%u)\n", fb, (unsigned)w, (unsigned)h);
        close(fd);
        return;
    }

    if (stride == 0) {
        stride = w * (bpp >> 3);
    }

    uint32_t row_stride = (w * 3 + 3) & ~3;
    uint32_t img_size = row_stride * h;
    uint32_t file_size = 54 + img_size;

    printf("\n[SCREENSHOT_RAW:%s:START]\n", scenario_id);
    fflush(stdout);

    /* 54 字节标准 Windows 24-bit Top-down BMP Header (负高度) */
    uint8_t hdr[54];
    memset(hdr, 0, 54);
    hdr[0] = 'B'; hdr[1] = 'M';
    memcpy(&hdr[2], &file_size, 4);
    uint32_t offset = 54;
    memcpy(&hdr[10], &offset, 4);
    uint32_t biSize = 40;
    memcpy(&hdr[14], &biSize, 4);
    memcpy(&hdr[18], &w, 4);
    int32_t neg_h = -(int32_t)h;
    memcpy(&hdr[22], &neg_h, 4);
    uint16_t planes = 1;
    memcpy(&hdr[26], &planes, 2);
    uint16_t out_bpp = 24;
    memcpy(&hdr[28], &out_bpp, 2);
    memcpy(&hdr[34], &img_size, 4);

    char b64_buf[4096];
    size_t b64_len = 0;

    b64_encode_chunk(hdr, 54, b64_buf, &b64_len);
    b64_buf[b64_len] = '\n';
    fwrite(b64_buf, 1, b64_len + 1, stdout);

    uint8_t row_buf[1920 * 3];

    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *src_row = fb + y * stride;
        for (uint32_t x = 0; x < w; x++) {
            uint8_t r = 0, g = 0, b = 0;
            if (bpp == 16) {
                uint16_t pix = *((const uint16_t *)(src_row + x * 2));
                r = ((pix >> 11) & 0x1F) * 255 / 31;
                g = ((pix >> 5) & 0x3F) * 255 / 63;
                b = (pix & 0x1F) * 255 / 31;
            } else if (bpp == 32) {
                b = src_row[x * 4 + 0];
                g = src_row[x * 4 + 1];
                r = src_row[x * 4 + 2];
            } else if (bpp == 24) {
                b = src_row[x * 3 + 0];
                g = src_row[x * 3 + 1];
                r = src_row[x * 3 + 2];
            }
            row_buf[x * 3 + 0] = b;
            row_buf[x * 3 + 1] = g;
            row_buf[x * 3 + 2] = r;
        }
        for (uint32_t pad = w * 3; pad < row_stride; pad++) {
            row_buf[pad] = 0;
        }
        b64_encode_chunk(row_buf, row_stride, b64_buf, &b64_len);
        b64_buf[b64_len] = '\n';
        fwrite(b64_buf, 1, b64_len + 1, stdout);
    }

    printf("[SCREENSHOT_RAW:%s:END]\n", scenario_id);
    fflush(stdout);

    close(fd);
#else
    printf("[AUTOTEST:DUMP_SIM] %s\n", scenario_id);
#endif
}

typedef enum {
    STAGE_INIT = 0,
    STAGE_HOME_PREPARE,
    STAGE_HOME_CAPTURE,
    STAGE_CLOCK_PREPARE,
    STAGE_CLOCK_CAPTURE,
    STAGE_AGENT_PREPARE,
    STAGE_AGENT_CAPTURE,
    STAGE_SETTINGS_PREPARE,
    STAGE_SETTINGS_CAPTURE,
    STAGE_FINISH
} autodrive_stage_t;

typedef struct {
    uv_loop_t *loop;
    phoenix_ui_t *ui;
    phoenix_agent_ctx_t *agent;
    lv_timer_t *timer;
    autodrive_stage_t stage;
    uint32_t step_count;
} autodrive_ctx_t;

static autodrive_ctx_t s_ctx;

static void autodrive_step_cb(lv_timer_t *timer)
{
    autodrive_ctx_t *ctx = (autodrive_ctx_t *)lv_timer_get_user_data(timer);
    if (!ctx) return;

    ctx->step_count++;

    switch (ctx->stage) {
        case STAGE_INIT:
            printf("\n====================================================\n");
            printf(" 🚀 [AUTOTEST] Starting Phoenix HoloDesk-S1 Visual UI Auto-Drive\n");
            printf("====================================================\n");
            ctx->stage = STAGE_HOME_PREPARE;
            break;

        case STAGE_HOME_PREPARE:
            printf("[AUTOTEST:SCENARIO:START] home - 桌面主仪表盘 (时间·待办·内嵌番茄钟)\n");
            cartridge_mgr_switch_to("home");
            ctx->stage = STAGE_HOME_CAPTURE;
            break;

        case STAGE_HOME_CAPTURE:
            dump_screen_framebuffer("home");
            printf("[AUTOTEST:SCENARIO:READY] home\n");
            ctx->stage = STAGE_CLOCK_PREPARE;
            break;

        case STAGE_CLOCK_PREPARE:
            printf("[AUTOTEST:SCENARIO:START] clock - 拟物机械翻页钟\n");
            cartridge_mgr_switch_to("clock");
            ctx->stage = STAGE_CLOCK_CAPTURE;
            break;

        case STAGE_CLOCK_CAPTURE:
            dump_screen_framebuffer("clock");
            printf("[AUTOTEST:SCENARIO:READY] clock\n");
            ctx->stage = STAGE_AGENT_PREPARE;
            break;

        case STAGE_AGENT_PREPARE:
            printf("[AUTOTEST:SCENARIO:START] agent - 主动式灵眸AI语音交互\n");
            cartridge_mgr_switch_to("agent");
            cartridge_agent_trigger_voice_chat("帮我开启25分钟专注流");
            ctx->stage = STAGE_AGENT_CAPTURE;
            break;

        case STAGE_AGENT_CAPTURE:
            dump_screen_framebuffer("agent");
            printf("[AUTOTEST:SCENARIO:READY] agent\n");
            ctx->stage = STAGE_SETTINGS_PREPARE;
            break;

        case STAGE_SETTINGS_PREPARE:
            printf("[AUTOTEST:SCENARIO:START] settings_drawer - 下拉设置与控制中心全功能抽屉\n");
            if (ctx->ui) {
                phoenix_ui_hide_bubble(ctx->ui);
                if (ctx->ui->settings) {
                    ui_settings_open(ctx->ui->settings);
                }
            }
            ctx->stage = STAGE_SETTINGS_CAPTURE;
            break;

        case STAGE_SETTINGS_CAPTURE:
            dump_screen_framebuffer("settings_drawer");
            printf("[AUTOTEST:SCENARIO:READY] settings_drawer\n");
            ctx->stage = STAGE_FINISH;
            break;

        case STAGE_FINISH:
            printf("\n====================================================\n");
            printf(" 🎉 [AUTOTEST:ALL_SCENARIOS_FINISHED] Result: SUCCESS\n");
            printf("====================================================\n");
            if (ctx->ui && ctx->ui->settings) {
                ui_settings_close(ctx->ui->settings);
            }
            if (ctx->timer) {
                lv_timer_delete(ctx->timer);
                ctx->timer = NULL;
            }
            if (ctx->loop) {
                printf("[AUTOTEST] Stopping Libuv Loop cleanly...\n");
                uv_stop(ctx->loop);
            }
            break;

        default:
            break;
    }
}

int phoenix_autodrive_start_gui(uv_loop_t *loop, phoenix_ui_t *ui, phoenix_agent_ctx_t *agent)
{
    if (!loop || !ui) return -1;

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.loop = loop;
    s_ctx.ui = ui;
    s_ctx.agent = agent;
    s_ctx.stage = STAGE_INIT;

    /* 创建 1200ms 周期定时器，保障每一屏渲染及动效平稳完成后再步进截屏 */
    s_ctx.timer = lv_timer_create(autodrive_step_cb, 1200, &s_ctx);
    if (!s_ctx.timer) {
        LOG_E(TAG, "Failed to create autodrive timer!");
        return -1;
    }

    LOG_I(TAG, "GUI Auto-Drive Scenario Engine initialized.");
    return 0;
}

int phoenix_autodrive_run_cli(void)
{
    printf("====================================================\n");
    printf(" 🧪 [AUTOTEST-CLI] Phoenix Subsystems Smoke Test\n");
    printf("====================================================\n");

    int passed = 0;
    int total = 0;

    /* 1. Test Store */
    total++;
    phoenix_stats_t stats_before;
    phoenix_store_get_stats(&stats_before);
    uint32_t merit_after = phoenix_store_add_merit(1);
    if (merit_after == stats_before.total_merit + 1) {
        printf("  ✅ [PASS] Store Merit Add & Retrieve\n");
        passed++;
    } else {
        printf("  ❌ [FAIL] Store Merit mismatch\n");
    }

    /* 2. Test Tools */
    total++;
    size_t tool_count = phoenix_tool_get_count();
    if (tool_count >= 5) {
        printf("  ✅ [PASS] Tool Registry (Count: %zu >= 5)\n", tool_count);
        passed++;
    } else {
        printf("  ❌ [FAIL] Tool Registry count low: %zu\n", tool_count);
    }

    /* 3. Test Wooden Fish Tool Execution */
    total++;
    char tool_res[256] = {0};
    int r = phoenix_tool_execute("knock_wooden_fish", "{\"count\":1}", tool_res, sizeof(tool_res));
    if (r == 0 && strstr(tool_res, "merit")) {
        printf("  ✅ [PASS] Tool Execution: knock_wooden_fish -> %s\n", tool_res);
        passed++;
    } else {
        printf("  ❌ [FAIL] Tool Execution failed: %d\n", r);
    }

    /* 4. Test Cartridge Manager */
    total++;
    if (cartridge_mgr_switch_to("home") == 0 &&
        cartridge_mgr_switch_to("clock") == 0 &&
        cartridge_mgr_switch_to("agent") == 0) {
        printf("  ✅ [PASS] Cartridge Manager Multi-Switch Cycle\n");
        passed++;
    } else {
        printf("  ❌ [FAIL] Cartridge Manager switch failed\n");
    }

    printf("====================================================\n");
    printf(" 📊 CLI Test Summary: %d / %d PASSED\n", passed, total);
    printf("====================================================\n");

    return (passed == total) ? 0 : -1;
}
