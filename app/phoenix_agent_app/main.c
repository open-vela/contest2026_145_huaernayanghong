/**
 * @file main.c
 * @brief Phoenix HoloDesk-S1 Main Application Entry (Dual Mode: GUI & CLI Test)
 * @author OpenVela Contest 2026 Team 145
 */

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <lvgl/lvgl.h>

#if defined(CONFIG_BOARDCTL)
#include <sys/boardctl.h>
#endif

#include "core/app.h"
#include "core/event_bus.h"
#include "core/tool_registry.h"
#include "core/agent_core.h"
#include "core/web_portal.h"
#include "ui/ui.h"
#include "test/test_autodrive.h"

#if !defined(HOST_TEST_RUNNER) && (defined(__NuttX__) || defined(__openvela__) || defined(CONFIG_LV_USE_NUTTX_LCD))
#define HAS_NUTTX_LCD_DEV 1
#include <fcntl.h>
#include <sys/ioctl.h>
#include <nuttx/lcd/lcd_dev.h>
#else
#define HAS_NUTTX_LCD_DEV 0
#endif

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
static void lv_nuttx_uv_loop(uv_loop_t *loop, lv_nuttx_result_t *result)
{
    lv_nuttx_uv_t uv_info;
    void *data;

    uv_loop_init(loop);

    lv_memset(&uv_info, 0, sizeof(uv_info));
    uv_info.loop = loop;
    uv_info.disp = result->disp;
    uv_info.indev = result->indev;
#ifdef CONFIG_UINPUT_TOUCH
    uv_info.uindev = result->utouch_indev;
#endif

    data = lv_nuttx_uv_init(&uv_info);
    uv_run(loop, UV_RUN_DEFAULT);
    lv_nuttx_uv_deinit(&data);
}
#endif

static void on_cli_event_logger(const phoenix_event_data_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;

    switch (event->type) {
        case PHOENIX_EVT_STATE_CHANGED:
            printf("[CLI:State] %s\n", event->data.state.message ? event->data.state.message : "");
            break;
        case PHOENIX_EVT_FLYING_TEXT:
            printf("[CLI:Anim] 漂浮动效: %s (颜色: 0x%06X)\n",
                   event->data.flying_text.text ? event->data.flying_text.text : "",
                   (unsigned int)event->data.flying_text.color_rgb);
            break;
        case PHOENIX_EVT_PLAY_SOUND:
            printf("[CLI:Audio] 触发音效 ID: %d\n", event->data.sound.sound_id);
            break;
        case PHOENIX_EVT_TOOL_TRIGGERED:
            printf("[CLI:Tool] 工具 [%s] 执行%s, 结果: %s\n",
                   event->data.tool.tool_name,
                   event->data.tool.success ? "成功" : "失败",
                   event->data.tool.result_summary ? event->data.tool.result_summary : "{}");
            break;
        case PHOENIX_EVT_LLM_FINISHED:
            printf("[CLI:Answer] 🤖 灵眸回复: %s\n", event->data.llm_text.text ? event->data.llm_text.text : "");
            break;
        default:
            break;
    }
}

static int run_cli_mode(int argc, char *argv[])
{
    printf("====================================================\n");
    printf(" 🤖 Phoenix Agent CLI Test Console\n");
    printf("====================================================\n");

    /* 1. Initialize all core subsystems via Unified Application Facade */
    if (phoenix_app_init(NULL) != 0) {
        printf("[CLI] Failed to initialize Phoenix Application subsystems.\n");
        return -1;
    }

    /* 2. Subscribe to console logger events */
    phoenix_event_subscribe(PHOENIX_EVT_STATE_CHANGED, on_cli_event_logger, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_FLYING_TEXT, on_cli_event_logger, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_PLAY_SOUND, on_cli_event_logger, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_TOOL_TRIGGERED, on_cli_event_logger, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_LLM_FINISHED, on_cli_event_logger, NULL);

    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();

    if (strcmp(argv[1], "tools") == 0) {
        printf("--- 已注册的具身工具列表 (%zu个) ---\n", phoenix_tool_get_count());
        char *schema = phoenix_tool_build_schema_json();
        if (schema) {
            printf("%s\n", schema);
            free(schema);
        }
    } else if (strcmp(argv[1], "tool") == 0 && argc >= 3) {
        const char *tool_name = argv[2];
        const char *tool_args = (argc >= 4) ? argv[3] : "{}";
        char result_buf[512] = {0};
        printf("[CLI] 执行工具: %s, 参数: %s\n", tool_name, tool_args);
        phoenix_tool_execute(tool_name, tool_args, result_buf, sizeof(result_buf));
        printf("[CLI] 输出结果: %s\n", result_buf);
    } else if (strcmp(argv[1], "ask") == 0 && argc >= 3) {
        printf("[CLI] 发送提问: \"%s\"\n", argv[2]);
        phoenix_agent_chat(agent, argv[2]);
    } else {
        /* Treat full string as prompt */
        printf("[CLI] 发送提问: \"%s\"\n", argv[1]);
        phoenix_agent_chat(agent, argv[1]);
    }

    if (agent) {
        phoenix_agent_core_destroy(agent);
    }

    /* 3. Clean teardown via Unified Facade */
    phoenix_app_deinit();
    return 0;
}

#if HAS_NUTTX_LCD_DEV
typedef struct {
    int fd;
    lv_display_t *disp;
    void *draw_buf;
    struct lcddev_area_s area;
    struct lcddev_area_align_s align_info;
} phoenix_lcd_dev_t;

static int32_t align_round_up(int32_t v, uint16_t align)
{
    return (v + align - 1) & ~(align - 1);
}

static void lcd_rounder_cb(lv_event_t *e)
{
    phoenix_lcd_dev_t *lcd = lv_event_get_user_data(e);
    lv_area_t *area = lv_event_get_param(e);
    struct lcddev_area_align_s *align_info = &lcd->align_info;
    int32_t w;
    int32_t h;

    area->x1 &= ~(align_info->col_start_align - 1);
    area->y1 &= ~(align_info->row_start_align - 1);

    w = align_round_up(lv_area_get_width(area), align_info->width_align);
    h = align_round_up(lv_area_get_height(area), align_info->height_align);

    area->x2 = area->x1 + w - 1;
    area->y2 = area->y1 + h - 1;
}

static void lcd_flush_cb(lv_display_t *disp, const lv_area_t *area_p, uint8_t *color_p)
{
    phoenix_lcd_dev_t *lcd = disp->driver_data;
    if (!lcd || lcd->fd < 0) {
        lv_display_flush_ready(disp);
        return;
    }

    lcd->area.row_start = area_p->y1;
    lcd->area.row_end = area_p->y2;
    lcd->area.col_start = area_p->x1;
    lcd->area.col_end = area_p->x2;
    lcd->area.data = (uint8_t *)color_p;
    ioctl(lcd->fd, LCDDEVIO_PUTAREA, (unsigned long)&(lcd->area));
    lv_display_flush_ready(disp);
}

static void lcd_release_cb(lv_event_t *e)
{
    lv_display_t *disp = (lv_display_t *)lv_event_get_user_data(e);
    phoenix_lcd_dev_t *lcd = lv_display_get_driver_data(disp);
    if (lcd) {
        lv_display_set_driver_data(disp, NULL);
        lv_display_set_flush_cb(disp, NULL);

        if (lcd->draw_buf) {
            free(lcd->draw_buf);
            lcd->draw_buf = NULL;
        }
        if (lcd->fd >= 0) {
            close(lcd->fd);
            lcd->fd = -1;
        }
        free(lcd);
    }
}

/**
 * @brief 在业务层创建严格 64 字节硬件对齐的 LCD 显示屏 (/dev/lcd0)
 * 
 * 针对全志 R528-S3 (Gemini-S1) 启用 G2D 硬件加速器时 CONFIG_LV_DRAW_BUF_ALIGN=64 的硬性要求，
 * 使用 memalign 显式按 64 字节边界分配屏幕渲染显存，彻底解决底层 lv_nuttx_lcd.c 使用普通
 * malloc 导致 lv_display_set_buffers 断言崩溃的问题。
 */
static lv_display_t *phoenix_create_aligned_lcd_display(const char *dev_path)
{
    struct fb_videoinfo_s vinfo;
    struct lcd_planeinfo_s pinfo;
    int fd = open(dev_path, O_CLOEXEC);
    if (fd < 0) {
        printf("[PhoenixApp] ❌ Error: cannot open LCD device: %s\n", dev_path);
        return NULL;
    }

    if (ioctl(fd, LCDDEVIO_GETVIDEOINFO, (unsigned long)((uintptr_t)&vinfo)) < 0) {
        printf("[PhoenixApp] ❌ Error: ioctl(LCDDEVIO_GETVIDEOINFO) failed\n");
        close(fd);
        return NULL;
    }

    if (ioctl(fd, LCDDEVIO_GETPLANEINFO, (unsigned long)((uintptr_t)&pinfo)) < 0) {
        printf("[PhoenixApp] ❌ Error: ioctl(LCDDEVIO_GETPLANEINFO) failed\n");
        close(fd);
        return NULL;
    }

    phoenix_lcd_dev_t *lcd = (phoenix_lcd_dev_t *)calloc(1, sizeof(phoenix_lcd_dev_t));
    if (!lcd) {
        close(fd);
        return NULL;
    }

    lv_display_t *disp = lv_display_create(vinfo.xres, vinfo.yres);
    if (!disp) {
        free(lcd);
        close(fd);
        return NULL;
    }

    uint32_t px_size = lv_color_format_get_size(lv_display_get_color_format(disp));
    uint32_t buf_size = vinfo.xres * vinfo.yres * px_size;

    /*
     * 核心对齐保障：强制按 64 字节对齐分配显存，彻底规避未定义堆偏移引发的 Assertion Failed。
     */
    void *draw_buf = memalign(64, buf_size);
    if (!draw_buf) {
        printf("[PhoenixApp] ❌ Error: memalign 64-byte draw_buf failed!\n");
        lv_display_delete(disp);
        free(lcd);
        close(fd);
        return NULL;
    }

    lcd->fd = fd;
    lcd->disp = disp;
    lcd->draw_buf = draw_buf;
    if (ioctl(fd, LCDDEVIO_GETAREAALIGN, &lcd->align_info) < 0) {
        lcd->align_info.row_start_align = 1;
        lcd->align_info.height_align = 1;
        lcd->align_info.col_start_align = 1;
        lcd->align_info.width_align = 1;
    }

    lv_display_set_buffers(disp, draw_buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, lcd_flush_cb);
    lv_display_add_event_cb(disp, lcd_rounder_cb, LV_EVENT_INVALIDATE_AREA, lcd);
    lv_display_add_event_cb(disp, lcd_release_cb, LV_EVENT_DELETE, disp);
    lv_display_set_driver_data(disp, lcd);

    return disp;
}
#endif

int main(int argc, FAR char *argv[])
{
    bool autotest_gui = false;

    /* If arguments passed, inspect for CLI or AutoTest modes */
    if (argc > 1) {
        if (strcmp(argv[1], "--autotest-cli") == 0) {
            if (phoenix_app_init(NULL) != 0) {
                printf("[CLI] Failed to initialize Phoenix subsystems.\n");
                return -1;
            }
            int r = phoenix_autodrive_run_cli();
            phoenix_app_deinit();
            return r;
        } else if (strcmp(argv[1], "--autotest") == 0 || strcmp(argv[1], "--autotest-gui") == 0) {
            autotest_gui = true;
        } else {
            return run_cli_mode(argc, argv);
        }
    }

    /* Otherwise, launch full GUI Living Cyber-Eye */
    lv_nuttx_dsc_t info;
    lv_nuttx_result_t result;
    uv_loop_t ui_loop;
    lv_memset(&ui_loop, 0, sizeof(uv_loop_t));

    printf("====================================================\n");
    printf(" 🚀 Phoenix HoloDesk-S1 Living Cyber-Eye Agent #145 \n");
    printf("====================================================\n");

    /* 1. 防多实例冲突检查：避免破坏后台已常驻运行的进程 */
    if (lv_is_initialized() || phoenix_app_is_initialized()) {
        printf("[PhoenixApp] ⚠️ Notice: Phoenix Agent GUI is already running in background.\n");
        return 0;
    }

#if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
    boardctl(BOARDIOC_INIT, 0);
#endif

    /*
     * =========================================================================
     * 2. 【方案B - 时序前置与硬件显存对齐】
     * 优先完成 LVGL 核心与屏幕显示驱动初始化，杜绝后续业务模块堆分配造成的地址偏移
     * =========================================================================
     */
    lv_init();
    lv_nuttx_dsc_init(&info);

    /*
     * 将 info.fb_path 显式设为 NULL，避免底层 lv_nuttx_init 自动调用存在 malloc
     * 未对齐缺陷的系统 LCD 驱动；触控输入 /dev/input0 保持由 lv_nuttx_init 初始化。
     */
    info.fb_path = NULL;
#if defined(CONFIG_LV_USE_NUTTX_TOUCHSCREEN) || defined(CONFIG_INPUT_TOUCHSCREEN) || defined(CONFIG_INPUT)
#  ifdef CONFIG_EXAMPLES_LVGLDEMO_INPUT_DEVPATH
    info.input_path = CONFIG_EXAMPLES_LVGLDEMO_INPUT_DEVPATH;
#  else
    info.input_path = "/dev/input0";
#  endif
#elif !defined(HOST_TEST_RUNNER)
    info.input_path = "/dev/input0";
#endif

    lv_nuttx_init(&info, &result);
    usleep(50000);

#if HAS_NUTTX_LCD_DEV
    printf("[PhoenixApp] Initializing 64-byte aligned LVGL display (/dev/lcd0)...\n");
    result.disp = phoenix_create_aligned_lcd_display("/dev/lcd0");
#endif

    if (result.disp == NULL) {
        printf("[PhoenixApp] ❌ ERROR: LVGL display initialization failure (result.disp is NULL)!\n");
        lv_nuttx_deinit(&result);
        lv_deinit();
        return 1;
    }
    printf("[PhoenixApp] ✅ LVGL display & input initialized successfully.\n");

    /*
     * =========================================================================
     * 3. 屏幕与显存就绪后，初始化 Phoenix 业务子系统 (HAL/感知/卡带/Web Portal)
     * =========================================================================
     */
    phoenix_app_config_t app_cfg;
    memset(&app_cfg, 0, sizeof(app_cfg));
    app_cfg.enable_web_portal = true;
    app_cfg.web_port = PHOENIX_STANDARD_HTTP_PORT;
    if (phoenix_app_init(&app_cfg) != 0) {
        printf("[PhoenixApp] ❌ Error: Phoenix Application Facade init failed!\n");
        lv_nuttx_deinit(&result);
        lv_deinit();
        return -1;
    }

    /* 4. 创建智能体核心协调器与多层 UI 交互体系 */
    phoenix_agent_ctx_t *agent_core = phoenix_agent_core_init();
    phoenix_ui_t *ui = phoenix_ui_create(lv_screen_active(), agent_core);
    if (!ui) {
        printf("[PhoenixApp] ❌ ERROR: Failed to create Phoenix Agent UI!\n");
        phoenix_agent_core_destroy(agent_core);
        phoenix_app_deinit();
        lv_nuttx_deinit(&result);
        lv_deinit();
        return 1;
    }

    /* 4.1 若开启自动化测试则启动测试挂载脚本 */
    if (autotest_gui) {
        phoenix_autodrive_start_gui(&ui_loop, ui, agent_core);
    }

    /* 5. 交互事件主循环 */
#ifdef CONFIG_LV_USE_NUTTX_LIBUV
    lv_nuttx_uv_loop(&ui_loop, &result);
#else
    while (1) {
        uint32_t idle = lv_timer_handler();
        phoenix_app_tick();
        usleep(idle ? idle * 1000 : 5000);
    }
#endif

    /* 6. 退出与资源释放 */
    phoenix_ui_destroy(ui);
    if (agent_core) {
        phoenix_agent_core_destroy(agent_core);
    }
    lv_nuttx_deinit(&result);
    lv_deinit();

    /* 7. 统一业务子系统下电 */
    phoenix_app_deinit();

    printf("Phoenix HoloDesk-S1 Agent Exited Cleanly.\n");
    return 0;
}
