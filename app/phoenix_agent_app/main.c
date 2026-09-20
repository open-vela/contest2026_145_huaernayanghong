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
#include "core/config.h"
#include "core/event_bus.h"
#include "core/tool_registry.h"
#include "core/agent_core.h"
#include "harness/llm_provider.h"
#include "core/cartridge_mgr.h"
#include "core/web_portal.h"
#include "hal/network_mgr.h"
#include "hal/hal_sensor.h"
#include "hal/hal_system.h"
#include "utils/log_mgr.h"
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

static void run_agent_selftest(phoenix_agent_ctx_t *agent)
{
    printf("\n====================================================\n");
    printf(" 🧪 Phoenix Agent 端侧具身闭环自动化巡检套件 (Self-Test)\n");
    printf("====================================================\n");

    int total = 0, passed = 0;

    /* Step 1: Config Subsystem Check */
    total++;
    char key[128] = {0};
    char url[256] = {0};
    phoenix_config_get_str(PHOENIX_CFG_API_KEY, "", key, sizeof(key));
    phoenix_config_get_str(PHOENIX_CFG_BASE_URL, "", url, sizeof(url));
    bool cfg_ok = (url[0] != '\0');
    printf("[Test 1/4] 系统与网络配置检查: %s (Endpoint: %s, Key已配: %s)\n",
           cfg_ok ? "✅ PASS" : "❌ FAIL", url[0] ? url : "None", key[0] ? "是" : "否 (离线模式)");
    if (cfg_ok) passed++;

    /* Step 2: LLM Ping Check */
    total++;
    uint32_t lat_ms = 0;
    int http_status = 0;
    char err_diag[256] = {0};
    printf("[Test 2/4] 云端模型心跳握手探测 (llm ping)...\n");
    int ping_rc = phoenix_llm_ping(&lat_ms, &http_status, err_diag, sizeof(err_diag));
    bool ping_ok = (ping_rc == 0 && http_status == 200);
    printf("           状态: HTTP %d, 往返延迟: %u ms, 诊断: %s -> %s\n",
           http_status, lat_ms, err_diag, ping_ok ? "✅ PASS" : "⚠️ FAIL (若无网请配Key/BaseURL)");
    if (ping_ok) passed++;

    /* Step 3: Tool Execution Unit Tests */
    total++;
    printf("[Test 3/4] 内置具身工具独立单元测试:\n");
    const char *test_tools[][2] = {
        {"knock_wooden_fish", "{\"count\":2}"},
        {"manage_pomodoro", "{\"action\":\"status\"}"},
        {"set_eye_emotion", "{\"emotion\":\"happy\"}"},
        {"system_health", "{}"},
        {"launch_app", "{\"app_id\":\"zen\"}"},
        {"blink_led", "{\"count\":2,\"interval_ms\":100}"}
    };
    bool tools_all_ok = true;
    size_t num_test_tools = sizeof(test_tools) / sizeof(test_tools[0]);
    for (size_t i = 0; i < num_test_tools; i++) {
        char out_buf[256] = {0};
        int trc = phoenix_tool_execute(test_tools[i][0], test_tools[i][1], out_buf, sizeof(out_buf));
        bool tok = (trc == 0 && strlen(out_buf) > 0);
        printf("           - [%s] -> %s (输出: %.60s)\n",
               test_tools[i][0], tok ? "OK" : "FAIL", out_buf);
        if (!tok) tools_all_ok = false;
    }
    printf("           工具集执行结果: %s\n", tools_all_ok ? "✅ PASS" : "❌ FAIL");
    if (tools_all_ok) passed++;

    /* Step 4: End-to-End Agent ReAct Tool Calling Test */
    total++;
    printf("[Test 4/4] 端到端 ReAct 意图决策与工具调用闭环测试:\n");
    printf("           发送意图: \"帮我敲一下木鱼积累功德\"\n");
    int chat_rc = phoenix_agent_chat(agent, "帮我敲一下木鱼积累功德");
    phoenix_app_tick();
    bool chat_ok = (chat_rc == 0);
    printf("           智能体 ReAct 回环结果: %s\n", chat_ok ? "✅ PASS" : "❌ FAIL");
    if (chat_ok) passed++;

    printf("====================================================\n");
    printf(" 🏁 自动化巡检结果汇总: %d / %d 项测试通过 (通过率 %d%%)\n",
           passed, total, (passed * 100) / total);
    printf("====================================================\n");
}

static int run_cli_mode(int argc, char *argv[])
{
    printf("====================================================\n");
    printf(" 🤖 Phoenix Agent ADB / CLI Diagnostics Console\n");
    printf("====================================================\n");

    const char *cmd = argv[1];

    /* 帮助文档，不初始化庞大子系统直接极速返回 */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        printf("Usage: phoenix_agent_app <command> [args...]\n\n");
        printf("Available Commands (ADB Debug & Test Suites):\n");
        printf("  config [get [key]] / [set <k> <v>] / [reset] : 查看/修改系统与大模型配置并持久化\n");
        printf("  llm ping                                     : 快速测试云端大模型网络握手与 API Key\n");
        printf("  llm chat <prompt>                            : 直接向大模型发起单轮问答 (跳过工具)\n");
        printf("  agent ask <prompt>                           : 启动 ReAct 智能体问答 (端侧决策与工具调用)\n");
        printf("  agent selftest                               : 一键运行大模型与工具链全流程自检\n");
        printf("  tools                                        : 列出已注册具身外设工具及 JSON Schema\n");
        printf("  tool <name> [json_args]                      : 独立单步执行指定的具身技能插件\n");
        printf("  cartridge [list] / [switch <name>]           : 巡检全部业务卡带或动态切换\n");
        printf("  sensor [status] / [knock]                    : 查询传感器数据或模拟敲击事件\n");
        printf("  wifi [status] / [scan]                       : 查询当前 Wi-Fi 工作模式或扫描热点\n");
        printf("  log [level <val>] / [dump] / [clear]         : 动态查询/设置日志级别或转储黑匣子\n");
        printf("  --autotest-cli                               : 运行全量无头回归测试套件\n");
        printf("====================================================\n");
        return 0;
    }

    /* 护城河 4: 平坦内存多实例防踩隔离 (Flat Memory Multi-instance Guard) */
    bool app_was_already_init = phoenix_app_is_initialized();
    if (!app_was_already_init) {
        /* 1. Initialize all core subsystems via Unified Application Facade */
        if (phoenix_app_init(NULL) != 0) {
            printf("[CLI] Failed to initialize Phoenix Application subsystems.\n");
            return -1;
        }
    } else {
        printf("[CLI] ⚡ 检测到 Phoenix 后台主服务已常驻运行，启用平坦内存安全共享模式\n");
    }

    /* 2. Subscribe to console logger events */
    phoenix_event_subscribe(PHOENIX_EVT_STATE_CHANGED, on_cli_event_logger, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_FLYING_TEXT, on_cli_event_logger, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_PLAY_SOUND, on_cli_event_logger, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_TOOL_TRIGGERED, on_cli_event_logger, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_LLM_FINISHED, on_cli_event_logger, NULL);

    phoenix_agent_ctx_t *agent = phoenix_agent_get_instance();
    bool created_own_agent = false;
    if (!agent) {
        agent = phoenix_agent_core_init();
        created_own_agent = true;
    }

    if (strcmp(cmd, "config") == 0) {
        if (argc >= 4 && strcmp(argv[2], "set") == 0) {
            const char *k = argv[3];
            const char *v = (argc >= 5) ? argv[4] : "";
            phoenix_config_set_str(k, v);
            phoenix_config_save();

            /* 动态同步至大模型 Harness */
            if (strcmp(k, PHOENIX_CFG_API_KEY) == 0 || strcmp(k, "api_key") == 0) {
                phoenix_llm_set_api_key(v);
            } else if (strcmp(k, PHOENIX_CFG_BASE_URL) == 0 || strcmp(k, "base_url") == 0) {
                phoenix_llm_set_base_url(v);
            } else if (strcmp(k, PHOENIX_CFG_MODEL) == 0 || strcmp(k, "model") == 0) {
                phoenix_llm_set_model(v);
            }
            printf("[CLI:Config] ✅ 配置 [%s] 已更新并持久化至 Flash: %s\n", k, v);
        } else if (argc >= 3 && strcmp(argv[2], "reset") == 0) {
            phoenix_config_reset_defaults();
            phoenix_config_save();
            printf("[CLI:Config] ✅ 配置已恢复出厂默认值。\n");
        } else if (argc >= 4 && strcmp(argv[2], "get") == 0) {
            char val_buf[256] = {0};
            phoenix_config_get_str(argv[3], "(not found)", val_buf, sizeof(val_buf));
            printf("[CLI:Config] %s = %s\n", argv[3], val_buf);
        } else {
            /* 默认 dump 全部配置并脱敏显示敏感密钥 */
            phoenix_config_dump();
        }
    } else if (strcmp(cmd, "llm") == 0) {
        if (argc >= 3 && strcmp(argv[2], "ping") == 0) {
            char url[256] = {0};
            phoenix_config_get_str(PHOENIX_CFG_BASE_URL, "", url, sizeof(url));
            printf("[CLI:LLM] 📡 正在探测云端大模型服务连通性...\n");
            printf("[CLI:LLM] 目标 Endpoint: %s\n", url[0] ? url : "Default");

            uint32_t lat_ms = 0;
            int status = 0;
            char diag[256] = {0};
            int rc = phoenix_llm_ping(&lat_ms, &status, diag, sizeof(diag));
            if (rc == 0 && status == 200) {
                printf("[CLI:LLM] ✅ 探测成功! HTTP %d (OK), 往返耗时: %u ms\n", status, lat_ms);
            } else {
                printf("[CLI:LLM] ❌ 探测失败! HTTP %d, 往返耗时: %u ms\n", status, lat_ms);
                printf("[CLI:LLM] 诊断原因: %s\n", diag[0] ? diag : "连接异常");
                printf("[CLI:LLM] 提示: 请确认 Wi-Fi 是否连网，或使用以下命令配置正确的 Key:\n"
                       "  phoenix_agent_app config set api_key <你的DeepSeek-API-Key>\n");
            }
        } else if (argc >= 3 && strcmp(argv[2], "chat") == 0) {
            const char *prompt = (argc >= 4) ? argv[3] : "你好";
            printf("[CLI:LLM] 💬 直接向大模型发起单轮问答: \"%s\"\n", prompt);

            phoenix_chat_msg_t send_msg;
            memset(&send_msg, 0, sizeof(send_msg));
            send_msg.role = PHOENIX_ROLE_USER;
            send_msg.content = (char *)prompt;

            phoenix_chat_resp_t resp;
            memset(&resp, 0, sizeof(resp));

            int rc = phoenix_llm_provider_chat(&send_msg, 1, NULL, &resp);
            if (rc == 0 && resp.content) {
                if (resp.reasoning_content && strlen(resp.reasoning_content) > 0) {
                    printf("[CLI:LLM:Thinking] 🧠 %s\n", resp.reasoning_content);
                }
                printf("[CLI:LLM:Answer] 🤖 %s\n", resp.content);
                printf("[CLI:LLM:Metrics] 耗时: %u ms, Tokens: [Prompt %u, Completion %u, Total %u]\n",
                       resp.latency_ms, resp.prompt_tokens, resp.completion_tokens, resp.total_tokens);
            } else {
                printf("[CLI:LLM] ❌ 请求失败 (HTTP %d): %s\n",
                       resp.http_status, resp.content ? resp.content : "网络握手或鉴权错误");
            }
            phoenix_llm_resp_free(&resp);
        } else {
            printf("Usage: phoenix_agent_app llm [ping | chat <prompt>]\n");
        }
    } else if (strcmp(cmd, "agent") == 0) {
        if (argc >= 3 && strcmp(argv[2], "selftest") == 0) {
            run_agent_selftest(agent);
        } else if (argc >= 4 && strcmp(argv[2], "ask") == 0) {
            printf("[CLI:Agent] 🚀 发送智能体意图: \"%s\"\n", argv[3]);
            phoenix_agent_chat(agent, argv[3]);
            phoenix_app_tick();
        } else if (argc >= 3) {
            printf("[CLI:Agent] 🚀 发送智能体意图: \"%s\"\n", argv[2]);
            phoenix_agent_chat(agent, argv[2]);
            phoenix_app_tick();
        } else {
            printf("Usage: phoenix_agent_app agent [ask <prompt> | selftest]\n");
        }
    } else if (strcmp(cmd, "log") == 0) {
        if (argc >= 4 && strcmp(argv[2], "level") == 0) {
            int lvl = phoenix_log_level_from_str(argv[3]);
            phoenix_log_set_level(lvl);
            printf("[CLI:Log] ✅ 日志过滤级别已切换为: %s (%d)\n", phoenix_log_level_to_str(lvl), lvl);
        } else if (argc >= 3 && strcmp(argv[2], "dump") == 0) {
            printf("[CLI:Log] 持久化日志文件: %s\n", phoenix_log_get_file_path() ? phoenix_log_get_file_path() : "未启用文件");
            printf("--- 环形内存近期日志截取 ---\n");
            char recent[2048] = {0};
            phoenix_log_get_recent(recent, sizeof(recent));
            printf("%s\n", recent);
        } else if (argc >= 3 && strcmp(argv[2], "clear") == 0) {
            phoenix_log_clear_recent();
            printf("[CLI:Log] ✅ 环形内存日志已清空。\n");
        } else {
            printf("[CLI:Log] 当前级别: %s, 磁盘文件: %s\n",
                   phoenix_log_level_to_str(phoenix_log_get_level()),
                   phoenix_log_get_file_path() ? phoenix_log_get_file_path() : "None");
        }
    } else if (strcmp(cmd, "wifi") == 0) {
        if (argc >= 3 && strcmp(argv[2], "scan") == 0) {
            printf("[CLI:WiFi] 正在扫描周边 Wi-Fi 热点...\n");
            net_wifi_ap_info_t aps[16];
            int n = net_mgr_scan_wifi(aps, 16);
            printf("[CLI:WiFi] 扫描完成，发现 %d 个可用热点:\n", n);
            for (int i = 0; i < n; i++) {
                printf("  [%2d] %-24s (RSSI: %3d dBm, Auth: %s)\n", i + 1, aps[i].ssid, aps[i].rssi, aps[i].auth);
            }
        } else {
            char ip[32] = {0};
            char ssid[32] = {0};
            net_mode_t mode = net_mgr_get_mode();
            net_mgr_get_ip(ip, sizeof(ip));
            net_mgr_get_ssid(ssid, sizeof(ssid));
            const char *mode_str = "未连接 (Disconnected)";
            if (mode == NET_MODE_STA_CONNECTED) mode_str = "STA 已连网";
            else if (mode == NET_MODE_STA_CONNECTING) mode_str = "STA 正在握手连接...";
            else if (mode == NET_MODE_SOFTAP_CONFIG) mode_str = "SoftAP 独立配网热点";
            printf("[CLI:WiFi] 工作模式: %s, SSID: %s, 本机 IP: %s\n", mode_str, ssid, ip[0] ? ip : "0.0.0.0");
        }
    } else if (strcmp(cmd, "sensor") == 0) {
        if (argc >= 3 && strcmp(argv[2], "knock") == 0) {
            printf("[CLI:Sensor] 模拟桌面敲击震动 (强度: 50, 单敲)...\n");
            cartridge_mgr_dispatch_knock(50, 1);
            phoenix_app_tick();
            printf("[CLI:Sensor] ✅ 敲击事件已派发，活跃卡带与木鱼功德已更新。\n");
        } else {
            hal_light_data_t ld;
            hal_battery_data_t bd;
            hal_sensor_read_light(&ld);
            hal_sensor_read_battery(&bd);
            printf("[CLI:Sensor] 环境光 ALS: %u Lux (暗光: %s), 电池电量: %u%% (电压: %umV, 充电: %s)\n",
                   (unsigned int)ld.lux, ld.is_dark_environment ? "是" : "否",
                   bd.percentage, bd.voltage_mv, bd.is_charging ? "是" : "否");
        }
    } else if (strcmp(cmd, "cartridge") == 0) {
        if (argc >= 4 && strcmp(argv[2], "switch") == 0) {
            const char *target = argv[3];
            int r = cartridge_mgr_switch_to(target);
            printf("[CLI:Cartridge] 切换卡带至 [%s] -> %s (返回码 %d)\n", target, (r == 0) ? "成功" : "失败", r);
        } else {
            cartridge_t *cur = cartridge_mgr_get_current();
            printf("[CLI:Cartridge] 当前活跃卡带: [%s] (%s)\n",
                   cur ? cur->ops.id : "None", cur ? cur->ops.name : "");
            size_t count = cartridge_mgr_get_count();
            printf("--- 已挂载业务卡带清单 (%zu 个) ---\n", count);
            for (size_t i = 0; i < count; i++) {
                cartridge_t *c = cartridge_mgr_get_by_index(i);
                if (c) {
                    printf("  [%zu] %-10s : %s %s %s\n", i, c->ops.id, c->ops.icon, c->ops.name,
                           (c == cur) ? "(当前活跃)" : "");
                }
            }
        }
    } else if (strcmp(cmd, "tools") == 0) {
        printf("--- 已注册的具身工具列表 (%zu个) ---\n", phoenix_tool_get_count());
        char *schema = phoenix_tool_build_schema_json();
        if (schema) {
            printf("%s\n", schema);
            free(schema);
        }
    } else if (strcmp(cmd, "tool") == 0 && argc >= 3) {
        const char *tool_name = argv[2];
        const char *tool_args = (argc >= 4) ? argv[3] : "{}";
        char result_buf[512] = {0};
        printf("[CLI] 执行工具: %s, 参数: %s\n", tool_name, tool_args);
        phoenix_tool_execute(tool_name, tool_args, result_buf, sizeof(result_buf));
        phoenix_app_tick();
        printf("[CLI] 输出结果: %s\n", result_buf);
    } else if (strcmp(cmd, "ask") == 0 && argc >= 3) {
        /* 向后兼容命令: 等同于 agent ask */
        printf("[CLI] 发送智能体提问: \"%s\"\n", argv[2]);
        phoenix_agent_chat(agent, argv[2]);
        phoenix_app_tick();
    } else {
        /* Treat full string as prompt */
        printf("[CLI] 发送智能体提问: \"%s\"\n", argv[1]);
        phoenix_agent_chat(agent, argv[1]);
        phoenix_app_tick();
    }

    /* 护城河 4: 若系统处于常驻运行模式，严禁释放全局 Agent 单例及抽空后台底层驱动 */
    if (created_own_agent && agent) {
        phoenix_agent_core_destroy(agent);
    }

    if (!app_was_already_init) {
        /* 3. Clean teardown via Unified Facade */
        phoenix_app_deinit();
    } else {
        printf("[CLI] ✅ CLI 任务结束，已安全保留后台常驻 GUI 与外设驱动底座\n");
    }
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
    phoenix_lcd_dev_t *lcd = lv_display_get_driver_data(disp);
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

    lv_display_t *disp = NULL;
#if HAS_NUTTX_LCD_DEV
    /* 2.1 优先创建严格 64 字节硬件对齐的 LCD 显示屏 (/dev/lcd0) */
    printf("[PhoenixApp] Initializing 64-byte aligned LVGL display (/dev/lcd0)...\n");
    disp = phoenix_create_aligned_lcd_display("/dev/lcd0");
#endif

    if (disp == NULL) {
        printf("[PhoenixApp] ❌ ERROR: LVGL display initialization failure!\n");
        lv_deinit();
        return 1;
    }

    /* 关键修复：将创建的 display 设为默认显示屏，确保后续在创建输入设备时能够正确识别宿主屏幕 */
    lv_display_set_default(disp);

    /* 2.2 屏幕与默认显示器就绪后，初始化触控输入驱动 (/dev/input0) */
    lv_nuttx_dsc_init(&info);

    /*
     * 将 info.fb_path 显式设为 NULL，避免底层 lv_nuttx_init 再次重复调用未对齐的系统 LCD 驱动；
     * 触控输入 /dev/input0 保持由 lv_nuttx_init 负责注册绑定。
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

    /* 
     * 关键修复：lv_nuttx_init 内部执行了 lv_memzero(result)，导致之前赋的值被清空；
     * 此处必须将 disp 重新回填到 result.disp 中，供后续事件循环及触控绑定使用。
     */
    result.disp = disp;

    /* 关键修复：显式将触控设备绑定到该显示屏，确保坐标映射与触控/滑动事件正常派发 */
    if (result.indev && result.disp) {
        lv_indev_set_display(result.indev, result.disp);
    }
    printf("[PhoenixApp] ✅ LVGL display & touch input (/dev/input0) linked and initialized successfully.\n");

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

    /* 3.1 显式启动网络管理器，完成开机网络自检与自动拉起（SoftAP 或 STA 回连） */
    net_mgr_init();

    /* 4. 创建智能体核心协调器与多层 UI 交互体系 */
    phoenix_agent_ctx_t *agent_core = phoenix_agent_core_init();
    phoenix_ui_t *ui = phoenix_ui_create(lv_screen_active(), agent_core);
    if (!ui) {
        printf("[PhoenixApp] ❌ ERROR: Failed to create Phoenix Agent UI!\n");
        phoenix_agent_core_destroy(agent_core);
        net_mgr_deinit();
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
        hal_system_record_frame();
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
    net_mgr_deinit();
    phoenix_app_deinit();

    printf("Phoenix HoloDesk-S1 Agent Exited Cleanly.\n");
    return 0;
}
