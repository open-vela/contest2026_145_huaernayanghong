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
#include "ui/ui.h"
#include "test/test_autodrive.h"

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

    /* 1. Check if already running to prevent destroying active background instance */
    if (lv_is_initialized() || phoenix_app_is_initialized()) {
        printf("[PhoenixApp] ⚠️ Notice: Phoenix Agent GUI is already running in background.\n");
        return 0;
    }

    /* 2. Initialize Subsystems via Unified Application Facade */
    phoenix_app_config_t app_cfg;
    memset(&app_cfg, 0, sizeof(app_cfg));
    app_cfg.enable_web_portal = true;
    app_cfg.web_port = 8080;
    if (phoenix_app_init(&app_cfg) != 0) {
        printf("[PhoenixApp] ❌ Error: Phoenix Application Facade init failed!\n");
        return -1;
    }

#if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
    boardctl(BOARDIOC_INIT, 0);
#endif

    lv_init();
    lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
    info.fb_path = "/dev/lcd0";
#endif
#ifdef CONFIG_INPUT_TOUCHSCREEN
#  ifdef CONFIG_EXAMPLES_LVGLDEMO_INPUT_DEVPATH
    info.input_path = CONFIG_EXAMPLES_LVGLDEMO_INPUT_DEVPATH;
#  else
    info.input_path = "/dev/input0";
#  endif
#endif

    printf("[PhoenixApp] Initializing LVGL display (fb_path: %s)...\n",
           info.fb_path ? info.fb_path : "default");
    lv_nuttx_init(&info, &result);
    usleep(100000);

    if (result.disp == NULL) {
        printf("[PhoenixApp] ❌ ERROR: LVGL display initialization failure (result.disp is NULL)!\n");
        lv_deinit();
        phoenix_app_deinit();
        return 1;
    }
    printf("[PhoenixApp] ✅ LVGL display initialized successfully.\n");

    /* 3. Create Agent Core & UI Components */
    phoenix_agent_ctx_t *agent_core = phoenix_agent_core_init();
    phoenix_ui_t *ui = phoenix_ui_create(lv_screen_active(), agent_core);
    if (!ui) {
        printf("[PhoenixApp] ❌ ERROR: Failed to create Phoenix Agent UI!\n");
        phoenix_agent_core_destroy(agent_core);
        lv_deinit();
        phoenix_app_deinit();
        return 1;
    }

    /* 3.1 Start GUI Auto-Drive if in autotest mode */
    if (autotest_gui) {
        phoenix_autodrive_start_gui(&ui_loop, ui, agent_core);
    }

    /* 4. Event Loop */
#ifdef CONFIG_LV_USE_NUTTX_LIBUV
    lv_nuttx_uv_loop(&ui_loop, &result);
#else
    while (1) {
        uint32_t idle = lv_timer_handler();
        phoenix_app_tick();
        usleep(idle ? idle * 1000 : 5000);
    }
#endif

    /* 5. Cleanup Resources */
    phoenix_ui_destroy(ui);
    if (agent_core) {
        phoenix_agent_core_destroy(agent_core);
    }
    lv_nuttx_deinit(&result);
    lv_deinit();

    /* 6. Unified Subsystem Teardown */
    phoenix_app_deinit();

    printf("Phoenix HoloDesk-S1 Agent Exited Cleanly.\n");
    return 0;
}
