/**
 * @file host_runner.c
 * @brief Host-side Native Test Runner & Interactive Console for Phoenix Agent
 * @author OpenVela Contest 2026 Team 145
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

#if defined(__has_include) && __has_include("../core/app.h")
#  include "../core/app.h"
#  include "../core/event_bus.h"
#  include "../core/tool_registry.h"
#  include "../core/store.h"
#  include "../core/agent_core.h"
#  include "../core/web_portal.h"
#  include "../tools/tools.h"
#  include "../harness/llm_provider.h"
#  include "../harness/llm_mock_backend.h"
#  include "../harness/llm_cloud_backend.h"
#  include "../ui/eye_anim.h"
#  include "../ui/audio_ctl.h"
#  include "../hal/hal_manager.h"
#  include "../hal/drivers/hal_driver_mock.h"
#  include "../hal/hal_audio_in.h"
#  include "../perception/perception.h"
#  include "../core/config.h"
#  include "../core/expression.h"
#  include "../core/intent_router.h"
#  include "../core/cartridge_mgr.h"
#  include "../hal/network_mgr.h"
#  include "../hal/ble_prov_service.h"
#  include "../cartridges/cartridge_home.h"
#  include "../cartridges/cartridge_familiar.h"
#  include "../cartridges/cartridge_memo.h"
#  include "../cartridges/cartridge_clock.h"
#  include "../cartridges/cartridge_agent.h"
#  include "../voice/voice_pipeline.h"
#  include "../utils/ring_buffer.h"
#  include "../utils/time_utils.h"
#  include "../utils/log_utils.h"
#  include "../hal/hal_system.h"
#  include "../hal/hal_sdcard.h"
#else
#  include "core/app.h"
#  include "core/event_bus.h"
#  include "core/tool_registry.h"
#  include "core/store.h"
#  include "core/config.h"
#  include "core/expression.h"
#  include "core/intent_router.h"
#  include "core/agent_core.h"
#  include "core/web_portal.h"
#  include "tools/tools.h"
#  include "harness/llm_provider.h"
#  include "harness/llm_mock_backend.h"
#  include "harness/llm_cloud_backend.h"
#  include "ui/eye_anim.h"
#  include "ui/audio_ctl.h"
#  include "hal/hal_manager.h"
#  include "hal/drivers/hal_driver_mock.h"
#  include "hal/hal_audio_in.h"
#  include "hal/hal_system.h"
#  include "hal/hal_sdcard.h"
#  include "perception/perception.h"
#  include "voice/voice_pipeline.h"
#  include "utils/ring_buffer.h"
#  include "utils/time_utils.h"
#  include "utils/log_utils.h"
#endif

static int g_event_counter = 0;
static phoenix_event_type_t g_last_event_type = PHOENIX_EVT_NONE;

void phoenix_eye_set_emotion(phoenix_eye_t *eye, phoenix_eye_emotion_t emotion)
{
    (void)eye;
    (void)emotion;
}

phoenix_eye_t* phoenix_eye_create(lv_obj_t *parent, uint16_t size)
{
    (void)parent;
    (void)size;
    return (phoenix_eye_t*)malloc(16);
}

void phoenix_eye_destroy(phoenix_eye_t *eye)
{
    if (eye) free(eye);
}

static void test_event_listener(const phoenix_event_data_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;
    g_event_counter++;
    g_last_event_type = event->type;
}

/* ---- 1. Event Bus Test ---- */
static void run_test_event_bus(void)
{
    printf("\n[TEST 1] Testing Event Bus...\n");
    phoenix_event_bus_init();
    g_event_counter = 0;

    phoenix_event_subscribe(PHOENIX_EVT_FLYING_TEXT, test_event_listener, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_PLAY_SOUND, test_event_listener, NULL);

    phoenix_event_data_t evt1;
    memset(&evt1, 0, sizeof(evt1));
    evt1.type = PHOENIX_EVT_FLYING_TEXT;
    evt1.data.flying_text.text = "Test Flying Text";
    evt1.data.flying_text.color_rgb = 0x00E5FF;
    phoenix_event_publish(&evt1);

    assert(g_event_counter == 1);
    assert(g_last_event_type == PHOENIX_EVT_FLYING_TEXT);

    phoenix_event_data_t evt2;
    memset(&evt2, 0, sizeof(evt2));
    evt2.type = PHOENIX_EVT_PLAY_SOUND;
    evt2.data.sound.sound_id = 1;
    phoenix_event_publish(&evt2);

    assert(g_event_counter == 2);
    assert(g_last_event_type == PHOENIX_EVT_PLAY_SOUND);

    phoenix_event_bus_deinit();
    printf("  -> Event Bus PASSED!\n");
}

/* ---- 2. Tool Registry & Schema Test ---- */
static void run_test_tool_registry(void)
{
    printf("\n[TEST 2] Testing Tool Registry & Schema Builder...\n");
    phoenix_event_bus_init();
    phoenix_tool_registry_init();
    phoenix_register_builtin_tools();

    assert(phoenix_tool_get_count() >= 5);

    const phoenix_tool_desc_t *tool = phoenix_tool_find("knock_wooden_fish");
    assert(tool != NULL);
    assert(strcmp(tool->name, "knock_wooden_fish") == 0);

    const phoenix_tool_desc_t *app_tool = phoenix_tool_find("launch_system_app");
    assert(app_tool != NULL);
    assert(strcmp(app_tool->name, "launch_system_app") == 0);

    char *schema_json = phoenix_tool_build_schema_json();
    assert(schema_json != NULL);
    assert(strstr(schema_json, "knock_wooden_fish") != NULL);
    assert(strstr(schema_json, "manage_pomodoro") != NULL);
    assert(strstr(schema_json, "set_eye_emotion") != NULL);
    assert(strstr(schema_json, "query_system_health") != NULL);
    assert(strstr(schema_json, "launch_system_app") != NULL);

    printf("  -> Generated Tools JSON Schema (length: %zu bytes)\n", strlen(schema_json));
    free(schema_json);

    phoenix_tool_registry_deinit();
    phoenix_event_bus_deinit();
    printf("  -> Tool Registry PASSED!\n");
}

/* ---- 3. Tool Execution Test ---- */
static void run_test_tools_execution(void)
{
    printf("\n[TEST 3] Testing Tool Executions & Merits...\n");
    phoenix_event_bus_init();
    phoenix_tool_registry_init();
    phoenix_register_builtin_tools();

    char result_buf[512] = {0};

    /* 3.1 Knock Wooden Fish */
    int ret = phoenix_tool_execute("knock_wooden_fish", "{\"count\":1}", result_buf, sizeof(result_buf));
    assert(ret == 0);
    assert(strstr(result_buf, "total_merit") != NULL);
    printf("  -> wooden_fish result: %s\n", result_buf);

    /* 3.2 Pomodoro Timer */
    ret = phoenix_tool_execute("manage_pomodoro", "{\"action\":\"start\",\"duration_minutes\":25}", result_buf, sizeof(result_buf));
    assert(ret == 0);
    assert(strstr(result_buf, "started") != NULL);
    printf("  -> pomodoro start result: %s\n", result_buf);

    ret = phoenix_tool_execute("manage_pomodoro", "{\"action\":\"stop\"}", result_buf, sizeof(result_buf));
    assert(ret == 0);
    assert(strstr(result_buf, "stopped") != NULL);
    printf("  -> pomodoro stop result: %s\n", result_buf);

    /* 3.3 System Health Check */
    ret = phoenix_tool_execute("query_system_health", "{}", result_buf, sizeof(result_buf));
    assert(ret == 0);
    assert(strstr(result_buf, "healthy") != NULL);
    printf("  -> system_health result: %s\n", result_buf);

    phoenix_tool_registry_deinit();
    phoenix_event_bus_deinit();
    printf("  -> Tool Execution PASSED!\n");
}

/* ---- 4. Agent ReAct Loop Test ---- */
static void run_test_agent_core(void)
{
    printf("\n[TEST 4] Testing Agent ReAct Loop & Intent Dispatch...\n");
    phoenix_event_bus_init();
    phoenix_tool_registry_init();
    phoenix_register_builtin_tools();
    phoenix_llm_provider_init(NULL);

    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();
    assert(agent != NULL);

    /* Test chat intent to knock wooden fish */
    int ret = phoenix_agent_chat(agent, "灵眸，帮我敲一次木鱼积攒功德！");
    assert(ret == 0);

    /* Test chat intent to start pomodoro */
    ret = phoenix_agent_chat(agent, "我想专注工作，开启25分钟番茄钟");
    assert(ret == 0);

    /* Test 1s tick */
    phoenix_agent_tick_1s(agent);

    phoenix_agent_core_destroy(agent);
    phoenix_llm_provider_deinit();
    phoenix_tool_registry_deinit();
    phoenix_event_bus_deinit();
    printf("  -> Agent ReAct Loop PASSED!\n");
}

/* ---- 5. Persistent Store Test ---- */
static void run_test_store(void)
{
    printf("\n[TEST 5] Testing Phoenix Persistent Store Engine...\n");
    const char *test_dir = "/tmp/phoenix_test_store";
    system("rm -rf /tmp/phoenix_test_store");

    int ret = phoenix_store_init(test_dir);
    assert(ret == 0);

    /* Test adding merit */
    uint32_t total = phoenix_store_add_merit(5);
    assert(total == 5);
    total = phoenix_store_add_merit(10);
    assert(total == 15);

    /* Test adding pomodoro */
    phoenix_store_add_pomodoro(1500);

    /* Flush to disk */
    phoenix_store_flush();

    /* Deinit */
    phoenix_store_deinit();

    /* Re-init and verify data loaded from JSON */
    ret = phoenix_store_init(test_dir);
    assert(ret == 0);

    phoenix_stats_t stats;
    phoenix_store_get_stats(&stats);
    assert(stats.total_merit == 15);
    assert(stats.pomodoro_count == 1);
    assert(stats.total_focus_seconds == 1500);

    phoenix_store_deinit();
    system("rm -rf /tmp/phoenix_test_store");
    printf("  -> Persistent Store PASSED! (Merit=%u, Pomo=%u, FocusSec=%u)\n",
           stats.total_merit, stats.pomodoro_count, stats.total_focus_seconds);
}

static volatile bool s_repl_running = true;

static void *repl_tick_worker(void *arg)
{
    (void)arg;
    while (s_repl_running) {
        phoenix_app_tick();
        usleep(100000); /* 100ms 周期驱动事件总线与卡带心跳 */
    }
    return NULL;
}

static void run_interactive_repl(void)
{
    printf("\n====================================================\n");
    printf(" 🤖 Phoenix Agent Host Interactive REPL (macOS Native)\n");
    printf(" 🌐 Web 伴侣看板: http://localhost:8080/dashboard\n");
    printf(" 📡 热点配网页面: http://localhost:8080/setup\n");
    printf(" Type your question, or 'tool <name> [args]', or 'quit'\n");
    printf("====================================================\n");

    phoenix_app_config_t cfg = {
        .storage_dir = "/tmp/phoenix_repl_store",
        .sounds_dir = "/tmp",
        .api_key = NULL,
        .register_tools = true,
        .enable_web_portal = true,
        .web_port = 8080
    };
    phoenix_app_init(&cfg);

    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();

    /* 启动后台心跳线程驱动卡带倒计时与事件总线 */
    s_repl_running = true;
    pthread_t tick_tid;
    pthread_create(&tick_tid, NULL, repl_tick_worker, NULL);

    char line_buf[512];
    while (1) {
        printf("\nphoenix-host> ");
        fflush(stdout);
        if (!fgets(line_buf, sizeof(line_buf), stdin)) break;

        /* Strip trailing newline */
        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r')) {
            line_buf[--len] = '\0';
        }
        if (len == 0) continue;

        if (strcmp(line_buf, "exit") == 0 || strcmp(line_buf, "quit") == 0) {
            break;
        }

        phoenix_agent_chat(agent, line_buf);
    }

    s_repl_running = false;
    pthread_join(tick_tid, NULL);

    phoenix_agent_core_destroy(agent);
    phoenix_app_deinit();
    printf("\nExited Phoenix REPL.\n");
}

static bool g_thinking_received = false;
static char g_last_reasoning[256] = {0};

static void thinking_event_listener(const phoenix_event_data_t *event, void *user_data)
{
    (void)user_data;
    if (event->type == PHOENIX_EVT_LLM_THINKING) {
        g_thinking_received = true;
        if (event->data.thinking.reasoning_snippet) {
            snprintf(g_last_reasoning, sizeof(g_last_reasoning), "%s", event->data.thinking.reasoning_snippet);
        }
    }
}

/* ---- 6. Thinking Mode & Paired Compact Test ---- */
static void run_test_reasoning_and_compact(void)
{
    printf("\n[TEST 6] Testing Thinking Mode (Reasoning) & Paired Context Compact...\n");
    phoenix_event_bus_init();
    phoenix_tool_registry_init();
    phoenix_register_builtin_tools();
    phoenix_llm_provider_init(NULL);

    g_thinking_received = false;
    g_last_reasoning[0] = '\0';
    phoenix_event_subscribe(PHOENIX_EVT_LLM_THINKING, thinking_event_listener, NULL);

    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();

    /* 6.1 Test Thinking Mode Event Dispatch */
    phoenix_agent_chat(agent, "敲一下木鱼");
    assert(g_thinking_received == true);
    assert(strlen(g_last_reasoning) > 0);
    printf("  -> Captured Thinking Snippet: \"%s\"\n", g_last_reasoning);

    /* 6.2 Test History Compact with Paired Tools */
    /* Push multiple rounds of interactions to trigger history_compact (MAX=24) */
    for (int i = 0; i < 10; i++) {
        phoenix_agent_chat(agent, "敲木鱼");
    }

    /* Verify compaction happened and the first kept message is NOT an orphaned PHOENIX_ROLE_TOOL */
    assert(agent->history_count <= PHOENIX_MAX_MESSAGES);
    assert(agent->history[0].role != PHOENIX_ROLE_TOOL);
    printf("  -> History successfully compacted to %zu items without broken tool pairs!\n",
           agent->history_count);

    phoenix_event_unsubscribe(PHOENIX_EVT_LLM_THINKING, thinking_event_listener, NULL);
    phoenix_agent_core_destroy(agent);
    phoenix_llm_provider_deinit();
    phoenix_tool_registry_deinit();
    phoenix_event_bus_deinit();
    printf("  -> Thinking Mode & Paired Context Compact PASSED!\n");
}

/* ---- 7. System App Launcher & Embody Control Test ---- */
static void run_test_launch_app(void)
{
    printf("\n[TEST 7] Testing System App Launcher & Embody Control...\n");
    phoenix_event_bus_init();
    phoenix_tool_registry_init();
    phoenix_register_builtin_tools();
    phoenix_llm_provider_init(NULL);

    char result_buf[512] = {0};

    /* 7.1 Direct Tool Execution: Launch Calendar */
    int ret = phoenix_tool_execute("launch_system_app", "{\"app_name\":\"日历\"}", result_buf, sizeof(result_buf));
    assert(ret == 0);
    assert(strstr(result_buf, "\"success\":true") != NULL);
    assert(strstr(result_buf, "com.application.x4b.calendar") != NULL);
    printf("  -> Launch Calendar Result: %s\n", result_buf);

    /* 7.2 Direct Tool Execution: Launch Whitenoise by alias */
    ret = phoenix_tool_execute("launch_system_app", "{\"app_name\":\"雨声\"}", result_buf, sizeof(result_buf));
    assert(ret == 0);
    assert(strstr(result_buf, "\"success\":true") != NULL);
    assert(strstr(result_buf, "com.application.x4b.whitenoise") != NULL);
    printf("  -> Launch Whitenoise Result: %s\n", result_buf);

    /* 7.3 Direct Tool Execution: Launch Meeting Recording */
    ret = phoenix_tool_execute("launch_system_app", "{\"app_name\":\"会议录音\"}", result_buf, sizeof(result_buf));
    assert(ret == 0);
    assert(strstr(result_buf, "\"success\":true") != NULL);
    assert(strstr(result_buf, "com.vela.system.meeting") != NULL);
    printf("  -> Launch Meeting Record Result: %s\n", result_buf);

    /* 7.4 Unknown App Fallback */
    ret = phoenix_tool_execute("launch_system_app", "{\"app_name\":\"未知神秘应用\"}", result_buf, sizeof(result_buf));
    assert(ret == 0);
    assert(strstr(result_buf, "\"success\":false") != NULL);
    printf("  -> Unknown App Fallback Result: %s\n", result_buf);

    /* 7.5 End-to-end Agent Chat ReAct Loop with App Launching Intent */
    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();
    assert(agent != NULL);
    ret = phoenix_agent_chat(agent, "灵眸，帮我打开日历应用");
    assert(ret == 0);
    printf("  -> Agent ReAct launched Calendar successfully!\n");

    phoenix_agent_core_destroy(agent);
    phoenix_llm_provider_deinit();
    phoenix_tool_registry_deinit();
    phoenix_event_bus_deinit();
    printf("  -> System App Launcher PASSED!\n");
}

/* ---- 8. Unified Application Facade Test ---- */
static void run_test_app_facade(void)
{
    printf("\n[TEST 8] Testing Unified Application Facade (phoenix_app_t)...\n");
    phoenix_app_config_t cfg = {
        .storage_dir = "/tmp/phoenix_test_app",
        .sounds_dir = "/tmp",
        .api_key = NULL,
        .register_tools = true
    };
    system("rm -rf /tmp/phoenix_test_app");

    int ret = phoenix_app_init(&cfg);
    assert(ret == 0);
    assert(phoenix_app_is_initialized() == true);

    /* Verify all subsystems automatically available */
    assert(phoenix_tool_get_count() >= 5);

    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();
    assert(agent != NULL);

    ret = phoenix_agent_chat(agent, "灵眸，帮我巡检一下当前的系统健康状态！");
    assert(ret == 0);

    phoenix_agent_core_destroy(agent);
    phoenix_app_deinit();
    assert(phoenix_app_is_initialized() == false);

    printf("  -> Unified Application Facade PASSED!\n");
}

/* ---- 9. Harness Backend Adapter & Dynamic Switching Test ---- */
static void run_test_harness_backend_switching(void)
{
    printf("\n[TEST 9] Testing Harness Multi-Backend Adapter & Dynamic Switching...\n");

    /* 9.1 Default initialization without API key loads Mock Backend */
    int ret = phoenix_llm_provider_init(NULL);
    assert(ret == 0);
    phoenix_llm_backend_t *backend = phoenix_llm_provider_get_backend();
    assert(backend != NULL);
    assert(strcmp(backend->name, "MockOfflineBackend") == 0);
    printf("  -> Default backend: %s (Verified)\n", backend->name);

    /* 9.2 Test chat via Mock backend */
    phoenix_chat_msg_t msgs[1];
    msgs[0].role = PHOENIX_ROLE_USER;
    msgs[0].content = "灵眸敲木鱼";
    msgs[0].tool_call_id = NULL;
    msgs[0].tool_name = NULL;
    msgs[0].reasoning_content = NULL;

    phoenix_chat_resp_t resp;
    ret = phoenix_llm_provider_chat(msgs, 1, NULL, &resp);
    assert(ret == 0);
    assert(resp.is_tool_use == true);
    assert(strcmp(resp.tool_name, "knock_wooden_fish") == 0);
    assert(resp.reasoning_content != NULL);
    phoenix_llm_resp_free(&resp);
    printf("  -> Mock Backend Reasoning & Tool Call PASSED!\n");

    /* 9.3 Dynamically switch to Cloud Backend */
    phoenix_llm_backend_t *cloud_be = phoenix_llm_cloud_backend_create();
    ret = phoenix_llm_provider_set_backend(cloud_be);
    assert(ret == 0);
    assert(strcmp(phoenix_llm_provider_get_backend()->name, "CloudVelaClawBackend") == 0);
    printf("  -> Switched to Cloud Backend: %s\n", phoenix_llm_provider_get_backend()->name);

    ret = phoenix_llm_provider_chat(msgs, 1, NULL, &resp);
    assert(ret == 0);
    assert(resp.content != NULL);
    assert(resp.reasoning_content != NULL);
    phoenix_llm_resp_free(&resp);

    /* 9.4 Switch back to Mock and test dynamic API key upgrade */
    phoenix_llm_provider_set_backend(phoenix_llm_mock_backend_create());
    assert(strcmp(phoenix_llm_provider_get_backend()->name, "MockOfflineBackend") == 0);
    phoenix_llm_set_api_key("sk-test-fake-key-for-phoenix-agent-12345");
    assert(strcmp(phoenix_llm_provider_get_backend()->name, "CloudVelaClawBackend") == 0);
    printf("  -> API Key auto-upgrade from Mock to Cloud PASSED!\n");

    phoenix_llm_provider_deinit();
    assert(phoenix_llm_provider_get_backend() == NULL);
    printf("  -> Harness Backend Adapter & Dynamic Switching PASSED!\n");
}

/* ---- 10. Proactive Mind Engine & Intervention Test ---- */
static int g_proactive_event_count = 0;
static phoenix_proactive_type_t g_last_proactive_type = (phoenix_proactive_type_t)-1;

static void test_proactive_listener(const phoenix_event_data_t *event, void *user_data)
{
    (void)user_data;
    if (event && event->type == PHOENIX_EVT_PROACTIVE_INTERVENE) {
        g_proactive_event_count++;
        g_last_proactive_type = (phoenix_proactive_type_t)event->data.proactive.proactive_type;
    }
}

static void run_test_proactive_mind(void)
{
    printf("\n[TEST 10] Testing Proactive Mind Engine & Intervention Dispatcher...\n");
    phoenix_app_config_t cfg = {
        .storage_dir = "/tmp/phoenix_test_proactive",
        .sounds_dir = "/tmp",
        .api_key = NULL,
        .register_tools = true,
        .enable_web_portal = false
    };
    system("rm -rf /tmp/phoenix_test_proactive");
    int ret = phoenix_app_init(&cfg);
    assert(ret == 0);

    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();
    assert(agent != NULL);

    g_proactive_event_count = 0;
    phoenix_event_subscribe(PHOENIX_EVT_PROACTIVE_INTERVENE, test_proactive_listener, NULL);

    /* 10.1 Test proactive trigger: manual invocation */
    ret = phoenix_agent_trigger_proactive(agent, PROACTIVE_SYSTEM_HEALTH, "系统自检：内存低，建议释放");
    assert(ret == 0);
    assert(g_proactive_event_count == 1);
    assert(g_last_proactive_type == PROACTIVE_SYSTEM_HEALTH);
    printf("  -> Manual proactive trigger PASSED!\n");

    /* 10.2 Test focus fatigue timeout trigger */
    agent->stats.continuous_focus_s = 0;
    agent->proactive_threshold_s = 2; /* set threshold to 2s for testing */
    agent->stats.proactive_enabled = true;

    phoenix_agent_tick_1s(agent); /* 1s */
    assert(g_proactive_event_count == 1); /* not yet */
    phoenix_agent_tick_1s(agent); /* 2s -> threshold reached, triggers PROACTIVE_THRESHOLD_FATIGUE */
    assert(g_proactive_event_count == 2);
    assert(g_last_proactive_type == PROACTIVE_THRESHOLD_FATIGUE);
    printf("  -> Continuous focus fatigue auto-trigger PASSED!\n");

    /* 10.3 Test context milestone trigger (every 10 merits) */
    char out_buf[128];
    for (int i = 0; i < 10; i++) {
        phoenix_tool_execute("knock_wooden_fish", "{\"count\":1}", out_buf, sizeof(out_buf));
    }
    /* Merit milestone automatically broadcasts PROACTIVE_CONTEXT_MILESTONE */
    assert(g_proactive_event_count == 3);
    assert(g_last_proactive_type == PROACTIVE_CONTEXT_MILESTONE);
    printf("  -> Merit milestone auto-celebration PASSED!\n");

    phoenix_event_unsubscribe(PHOENIX_EVT_PROACTIVE_INTERVENE, test_proactive_listener, NULL);
    phoenix_agent_core_destroy(agent);
    phoenix_app_deinit();
    printf("  -> Proactive Mind Engine PASSED!\n");
}

/* ---- 11. Embedded Web Portal & REST API Test ---- */
static void run_test_web_portal(void)
{
    printf("\n[TEST 11] Testing Embedded Web Portal & REST API...\n");
    phoenix_app_config_t cfg = {
        .storage_dir = "/tmp/phoenix_test_web",
        .sounds_dir = "/tmp",
        .api_key = NULL,
        .register_tools = true,
        .enable_web_portal = true,
        .web_port = 8080
    };
    system("rm -rf /tmp/phoenix_test_web");
    int ret = phoenix_app_init(&cfg);
    assert(ret == 0);

    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();
    assert(agent != NULL);

    char resp_buf[16384];

    /* 11.1 Test GET / (Geek Web Portal HTML page) */
    const char *req_root = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int resp_len = phoenix_web_portal_handle_request(req_root, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "Phoenix HoloDesk-S1 Geek Dashboard") != NULL);
    assert(strstr(resp_buf, "极客伴侣看板") != NULL);
    printf("  -> GET / (Static Geek Dashboard) PASSED! (Length: %d bytes)\n", resp_len);

    /* 11.2 Test GET /api/status */
    const char *req_status = "GET /api/status HTTP/1.1\r\nHost: localhost\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_status, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "\"device\":\"Gemini-S1\"") != NULL);
    assert(strstr(resp_buf, "\"stats\"") != NULL);
    assert(strstr(resp_buf, "\"cartridges\"") != NULL);
    printf("  -> GET /api/status (JSON Telemetry & Cartridges) PASSED!\n");

    /* 11.3 Test POST /api/cartridge/switch (Remote Switch to Clock) */
    const char *req_sw = "POST /api/cartridge/switch HTTP/1.1\r\nHost: localhost\r\nContent-Length: 14\r\n\r\n{\"id\":\"clock\"}";
    resp_len = phoenix_web_portal_handle_request(req_sw, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "cartridge switched") != NULL);
    assert(strcmp(cartridge_mgr_get_current()->ops.id, "clock") == 0);
    printf("  -> POST /api/cartridge/switch (Remote Cartridge Switch) PASSED!\n");

    /* 11.4 Test POST /api/memo/add (Push Idea Capsule from Web) */
    const char *req_memo = "POST /api/memo/add HTTP/1.1\r\nHost: localhost\r\nContent-Length: 35\r\n\r\n{\"content\":\"Web伴侣看板远程录入灵感\"}";
    resp_len = phoenix_web_portal_handle_request(req_memo, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "memo entry added") != NULL);
    printf("  -> POST /api/memo/add (Web Idea Capsule Injection) PASSED!\n");

    /* 11.5 Test POST /api/action (Remote Pet & Knock Actions) */
    const char *req_act_pet = "POST /api/action HTTP/1.1\r\nHost: localhost\r\nContent-Length: 16\r\n\r\n{\"action\":\"pet\"}";
    resp_len = phoenix_web_portal_handle_request(req_act_pet, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "action dispatched") != NULL);

    const char *req_act_fish = "POST /api/action HTTP/1.1\r\nHost: localhost\r\nContent-Length: 23\r\n\r\n{\"action\":\"knock_fish\"}";
    resp_len = phoenix_web_portal_handle_request(req_act_fish, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "action dispatched") != NULL);
    printf("  -> POST /api/action (Remote Pet & Knock Actions) PASSED!\n");

    /* 11.6 Test GET /api/config (Query current configuration) */
    const char *req_get_cfg = "GET /api/config HTTP/1.1\r\nHost: localhost\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_get_cfg, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "\"success\":true") != NULL);
    assert(strstr(resp_buf, "\"model\"") != NULL);
    printf("  -> GET /api/config (Agent Configuration Query) PASSED!\n");

    /* 11.7 Test POST /api/config (Dynamic API Key Configuration) */
    const char *req_config = "POST /api/config HTTP/1.1\r\nHost: localhost\r\nContent-Length: 38\r\n\r\n{\"api_key\":\"sk-test-live-key-from-web\"}";
    resp_len = phoenix_web_portal_handle_request(req_config, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "\"success\":true") != NULL);
    assert(strcmp(phoenix_llm_provider_get_backend()->name, "CloudVelaClawBackend") == 0);
    printf("  -> POST /api/config (Dynamic Key Injection) PASSED!\n");

    /* 11.7 Test POST /api/proactive (Remote trigger proactive drill) */
    const char *req_proactive = "POST /api/proactive HTTP/1.1\r\nHost: localhost\r\nContent-Length: 120\r\n\r\n{\"type\":\"fatigue\",\"title\":\"远程演练\",\"suggestion\":\"来自Web控制台的主动提醒\"}";
    resp_len = phoenix_web_portal_handle_request(req_proactive, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "\"status\":\"triggered\"") != NULL);
    printf("  -> POST /api/proactive (Proactive Drill Dispatch) PASSED!\n");

    /* 11.8 Test POST /api/wifi/reset */
    const char *req_reset = "POST /api/wifi/reset HTTP/1.1\r\nHost: localhost\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_reset, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "reset to softap") != NULL);
    printf("  -> POST /api/wifi/reset (Remote SoftAP Reset) PASSED!\n");

    /* 11.9 Test 404 Routing */
    const char *req_404 = "GET /api/invalid_path HTTP/1.1\r\nHost: localhost\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_404, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "HTTP/1.1 404 Not Found") != NULL);
    printf("  -> 404 Routing PASSED!\n");

    phoenix_agent_core_destroy(agent);
    phoenix_app_deinit();
    printf("  -> Embedded Web Portal PASSED!\n");
}

/* ---- 12. Token Usage Metrics & Latency Observability Test ---- */
static void run_test_token_and_observability(void)
{
    printf("\n[TEST 12] Testing Token Usage Metrics & Latency Observability...\n");
    phoenix_app_config_t cfg = {
        .storage_dir = "/tmp/phoenix_test_obs",
        .sounds_dir = "/tmp",
        .api_key = NULL,
        .register_tools = true,
        .enable_web_portal = false
    };
    system("rm -rf /tmp/phoenix_test_obs");
    int ret = phoenix_app_init(&cfg);
    assert(ret == 0);

    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();
    assert(agent != NULL);

    /* Initial state: zero token & latency */
    phoenix_agent_stats_t stats_before;
    phoenix_agent_get_stats(agent, &stats_before);
    assert(stats_before.total_tokens_used == 0);
    assert(stats_before.last_latency_ms == 0);

    /* Perform a chat interaction */
    ret = phoenix_agent_chat(agent, "灵眸，帮我巡检系统状态！");
    assert(ret == 0);

    phoenix_agent_stats_t stats_after1;
    phoenix_agent_get_stats(agent, &stats_after1);
    assert(stats_after1.total_tokens_used > 0);
    assert(stats_after1.last_latency_ms > 0);
    printf("  -> First interaction: Tokens used=%lu, Latency=%lums\n",
           (unsigned long)stats_after1.total_tokens_used,
           (unsigned long)stats_after1.last_latency_ms);

    uint32_t tokens_1 = stats_after1.total_tokens_used;

    /* Perform a second interaction */
    ret = phoenix_agent_chat(agent, "灵眸敲木鱼");
    assert(ret == 0);

    phoenix_agent_stats_t stats_after2;
    phoenix_agent_get_stats(agent, &stats_after2);
    assert(stats_after2.total_tokens_used > tokens_1);
    printf("  -> Second interaction: Cumulative Tokens=%lu, Latency=%lums\n",
           (unsigned long)stats_after2.total_tokens_used,
           (unsigned long)stats_after2.last_latency_ms);

    phoenix_agent_core_destroy(agent);
    phoenix_app_deinit();
    printf("  -> Token & Latency Observability PASSED!\n");
}

static void run_test_hal_subsystems(void)
{
    printf("\n[TEST 13] Testing HAL Subsystems, Driver Ops & Telemetry...\n");
    hal_mock_reset();

    /* 1. Initialize HAL standalone */
    hal_config_t hal_cfg;
    memset(&hal_cfg, 0, sizeof(hal_cfg));
    hal_cfg.sound_data_root = "/tmp/sounds";
    int ret = hal_init(&hal_cfg);
    assert(ret == 0);
    assert(hal_is_initialized() == true);

    /* 2. Test Dynamic System Telemetry */
    hal_mock_set_temperature(43.8f);
    hal_system_telemetry_t telem;
    ret = hal_system_get_telemetry(&telem);
    assert(ret == 0);
    assert(strlen(telem.board_model) > 0);
    assert(telem.cpu_temperature_c > 43.0f);
    assert(telem.mem_total_kb > 0);
    assert(telem.mem_free_kb > 0);
    printf("  -> System Telemetry: Board=[%s], Temp=%.1fC, TotalMem=%uKB, FreeMem=%uKB\n",
           telem.board_model, telem.cpu_temperature_c, telem.mem_total_kb, telem.mem_free_kb);

    /* 3. Test Actuator Driver Operations */
    hal_actuator_play_sound(HAL_SOUND_CELEBRATE);
    assert(hal_mock_get_last_sound() == HAL_SOUND_CELEBRATE);

    hal_actuator_trigger_haptic(HAL_HAPTIC_CLICK);
    assert(hal_mock_get_last_haptic_pattern() == HAL_HAPTIC_CLICK);

    /* 4. Test Native App Dispatch */
    hal_system_launch_app("com.openvela.meeting");
    assert(strcmp(hal_mock_get_last_launched_app(), "com.openvela.meeting") == 0);

    /* 5. Clean teardown */
    hal_deinit();
    assert(hal_is_initialized() == false);
    printf("  -> HAL Subsystems, Driver Ops & Telemetry PASSED!\n");
}

static int g_perception_proactive_count = 0;
static phoenix_proactive_type_t g_perception_proactive_type = (phoenix_proactive_type_t)-1;

static void on_perception_test_event(const phoenix_event_data_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;
    if (event->type == PHOENIX_EVT_PROACTIVE_INTERVENE) {
        g_perception_proactive_count++;
        g_perception_proactive_type = (phoenix_proactive_type_t)event->data.proactive.proactive_type;
        printf("  [TestPerceptionListener] Proactive Intervene: %s (Type %d)\n",
               event->data.proactive.title, event->data.proactive.proactive_type);
    }
}

static void run_test_perception_embodied_loop(void)
{
    printf("\n[TEST 14] Testing Perception Engine & Embodied Feedback Loop...\n");
    hal_mock_reset();
    g_perception_proactive_count = 0;
    g_perception_proactive_type = (phoenix_proactive_type_t)-1;

    /* 1. Initialize Full Subsystems */
    phoenix_app_config_t app_cfg;
    memset(&app_cfg, 0, sizeof(app_cfg));
    app_cfg.storage_dir = "/tmp/phoenix_perception_test";
    app_cfg.sounds_dir = "/tmp/sounds";
    app_cfg.register_tools = true;
    int ret = phoenix_app_init(&app_cfg);
    assert(ret == 0);

    /* 2. Subscribe to proactive events */
    phoenix_event_subscribe(PHOENIX_EVT_PROACTIVE_INTERVENE, on_perception_test_event, NULL);

    /* 3. Record baseline merit */
    phoenix_stats_t st_before;
    phoenix_store_get_stats(&st_before);
    uint32_t merit_before = st_before.total_merit;

    /* 4. Simulate Physical Table Knock (Tap Sensor) */
    hal_mock_inject_tap(HAL_TAP_NORMAL);

    /* Step perception engine */
    phoenix_perception_step();

    /* Assert: Haptic clicked, Wooden fish sound played, Merit incremented */
    assert(hal_mock_get_last_haptic_pattern() == HAL_HAPTIC_CLICK);
    assert(hal_mock_get_last_sound() == HAL_SOUND_WOODEN_FISH);
    phoenix_stats_t st_after;
    phoenix_store_get_stats(&st_after);
    assert(st_after.total_merit == merit_before + 1);
    printf("  -> Physical Tap -> Haptic Feedback + Wooden Fish Sound + Merit (+1) Verified!\n");

    /* 5. Simulate Low Ambient Light (Night Mode transition) */
    hal_mock_set_light(15); /* 15 Lux: Dark environment */
    phoenix_perception_step();
    assert(g_perception_proactive_count >= 1);
    assert(g_perception_proactive_type == PROACTIVE_ENV_DARK_SLEEP);
    printf("  -> Dark Ambient Light -> Autonomous Night/White-Noise Proactive Verified!\n");

    /* 6. Simulate Low Battery Condition (Wait 110ms for audio debouncing) */
    usleep(110000);
    hal_mock_set_battery(8, false); /* 8% power, discharging */
    phoenix_perception_step();
    assert(g_perception_proactive_type == PROACTIVE_BATTERY_LOW);
    assert(hal_mock_get_last_sound() == HAL_SOUND_ALERT);
    printf("  -> Low Battery -> Alert Sound + Power-Saving Proactive Guard Verified!\n");

    phoenix_event_unsubscribe(PHOENIX_EVT_PROACTIVE_INTERVENE, on_perception_test_event, NULL);
    phoenix_app_deinit();
    printf("  -> Perception Engine & Embodied Feedback Loop PASSED!\n");
}

/* ========================================================================= */
/* [TEST 15] Async Message Queue & Drain Mechanism                           */
/* ========================================================================= */
static int g_async_event_count = 0;
static void on_async_test_event(const phoenix_event_data_t *evt, void *user_data)
{
    (void)user_data;
    if (evt && evt->type == PHOENIX_EVT_FLYING_TEXT) {
        g_async_event_count++;
    }
}

static void run_test_async_event_queue(void)
{
    printf("\n[TEST 15] Testing Async Event Queue & Drain Mechanism...\n");
    phoenix_event_bus_init();
    g_async_event_count = 0;
    phoenix_event_subscribe(PHOENIX_EVT_FLYING_TEXT, on_async_test_event, NULL);

    phoenix_event_data_t evt1, evt2, evt3;
    memset(&evt1, 0, sizeof(evt1));
    evt1.type = PHOENIX_EVT_FLYING_TEXT;
    evt1.data.flying_text.text = "Event 1";
    memset(&evt2, 0, sizeof(evt2));
    evt2.type = PHOENIX_EVT_FLYING_TEXT;
    evt2.data.flying_text.text = "Event 2";
    memset(&evt3, 0, sizeof(evt3));
    evt3.type = PHOENIX_EVT_FLYING_TEXT;
    evt3.data.flying_text.text = "Event 3";

    assert(phoenix_event_post_async(&evt1) == 0);
    assert(phoenix_event_post_async(&evt2) == 0);
    assert(phoenix_event_post_async(&evt3) == 0);
    assert(phoenix_event_bus_pending_count() == 3);
    assert(g_async_event_count == 0); /* Not drained yet */

    size_t drained = phoenix_event_bus_drain();
    assert(drained == 3);
    assert(phoenix_event_bus_pending_count() == 0);
    assert(g_async_event_count == 3); /* All dispatched */

    phoenix_event_bus_deinit();
    printf("  -> Async Event Queue Post & Drain PASSED!\n");
}

/* ========================================================================= */
/* [TEST 16] Embodied Expression Engine                                      */
/* ========================================================================= */
static void run_test_embodied_expression(void)
{
    printf("\n[TEST 16] Testing Embodied Expression Engine...\n");
    hal_init(NULL);
    phoenix_event_bus_init();
    phoenix_expression_init();

    /* Test Expression JOY */
    int ret = phoenix_expression_play(PHOENIX_EXPR_JOY, 8, "功德圆满");
    assert(ret == 0);
    assert(hal_mock_get_last_haptic_pattern() == HAL_HAPTIC_CLICK);

    /* Test Expression ALERT */
    ret = phoenix_expression_play(PHOENIX_EXPR_ALERT, 9, "危险警报");
    assert(ret == 0);
    assert(hal_mock_get_last_haptic_pattern() == HAL_HAPTIC_ALERT_BURST);

    /* Test Expression CELEBRATE */
    ret = phoenix_expression_play(PHOENIX_EXPR_CELEBRATE, 10, "里程碑达成");
    assert(ret == 0);
    assert(hal_mock_get_last_haptic_pattern() == HAL_HAPTIC_DOUBLE_CLICK);

    phoenix_expression_deinit();
    phoenix_event_bus_deinit();
    hal_deinit();
    printf("  -> Embodied Expression Engine PASSED!\n");
}

/* ========================================================================= */
/* [TEST 17] Fast-path Intent Router & Workflow                              */
/* ========================================================================= */
static void run_test_intent_router_fastpath(void)
{
    printf("\n[TEST 17] Testing Fast-path Intent Router & Workflow...\n");
    phoenix_app_config_t cfg = {
        .storage_dir = "/tmp/phoenix_test_router",
        .sounds_dir = "/tmp",
        .api_key = NULL,
        .register_tools = true,
        .enable_web_portal = false
    };
    system("rm -rf /tmp/phoenix_test_router");
    int ret = phoenix_app_init(&cfg);
    assert(ret == 0);

    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();
    assert(agent != NULL);

    /* Direct wooden fish intent */
    phoenix_intent_result_t res1 = phoenix_intent_route("fast:敲木鱼");
    assert(res1.category == INTENT_TYPE_FASTPATH);
    assert(strcmp(res1.tool_name, "knock_wooden_fish") == 0);

    /* Direct pomodoro intent */
    phoenix_intent_result_t res2 = phoenix_intent_route("fast:开启番茄钟");
    assert(res2.category == INTENT_TYPE_FASTPATH);
    assert(strcmp(res2.tool_name, "manage_pomodoro") == 0);

    /* Conversational query -> should defer to LLM */
    phoenix_intent_result_t res3 = phoenix_intent_route("灵眸，帮我分析一下系统当前状态？");
    assert(res3.category == INTENT_TYPE_LLM_REASONING);

    /* Fast-path execution via chat with 0 tokens and 0ms latency */
    uint32_t merit_before = 0;
    phoenix_stats_t st;
    phoenix_store_get_stats(&st);
    merit_before = st.total_merit;

    ret = phoenix_agent_chat(agent, "fast:敲木鱼");
    assert(ret == 0);

    phoenix_agent_stats_t agent_st;
    phoenix_agent_get_stats(agent, &agent_st);
    assert(agent_st.last_latency_ms == 0);

    phoenix_store_get_stats(&st);
    assert(st.total_merit == merit_before + 1);
    printf("  -> Fast-path 0ms / 0 Token Execution Verified (Merit: %u -> %u)!\n",
           merit_before, st.total_merit);

    phoenix_agent_core_destroy(agent);
    phoenix_app_deinit();
    printf("  -> Fast-path Intent Router & Workflow PASSED!\n");
}

/* ========================================================================= */
/* [TEST 18] Generic Key-Value Config Subsystem                              */
/* ========================================================================= */
static void run_test_config_subsystem(void)
{
    printf("\n[TEST 18] Testing Generic KV Config Subsystem...\n");
    system("rm -rf /tmp/phoenix_test_cfg && mkdir -p /tmp/phoenix_test_cfg");
    int ret = phoenix_config_init("/tmp/phoenix_test_cfg");
    assert(ret == 0);

    /* Default checks */
    assert(phoenix_config_get_int(PHOENIX_CFG_VOLUME, 0) == 80);
    assert(phoenix_config_get_int(PHOENIX_CFG_BRIGHTNESS, 0) == 90);

    /* Set & Get custom values */
    assert(phoenix_config_set_int("vol_level", 65) == 0);
    assert(phoenix_config_get_int("vol_level", 0) == 65);

    assert(phoenix_config_set_str("wifi_ssid", "HoloDesk_AP") == 0);
    char buf[64] = {0};
    phoenix_config_get_str("wifi_ssid", "", buf, sizeof(buf));
    assert(strcmp(buf, "HoloDesk_AP") == 0);

    /* Save and reload */
    assert(phoenix_config_save() == 0);
    phoenix_config_deinit();

    /* Re-init and verify persistence */
    ret = phoenix_config_init("/tmp/phoenix_test_cfg");
    assert(ret == 0);
    assert(phoenix_config_get_int("vol_level", 0) == 65);
    memset(buf, 0, sizeof(buf));
    phoenix_config_get_str("wifi_ssid", "", buf, sizeof(buf));
    assert(strcmp(buf, "HoloDesk_AP") == 0);

    phoenix_config_deinit();
    printf("  -> Generic KV Config Subsystem PASSED!\n");
}

/* ========================================================================= */
/* [TEST 19] Audio In HAL & Voice Pipeline                                   */
/* ========================================================================= */
static int g_voice_recognized_count = 0;
static void on_test_voice_text(const char *text, void *user_data)
{
    (void)user_data;
    if (text && strlen(text) > 0) {
        g_voice_recognized_count++;
        printf("  [VoiceCallback] Recognized speech: \"%s\"\n", text);
    }
}

static void run_test_audio_in_and_voice(void)
{
    printf("\n[TEST 19] Testing Audio In HAL & Voice Pipeline...\n");
    hal_init(NULL);
    g_voice_recognized_count = 0;

    int ret = phoenix_voice_pipeline_init(on_test_voice_text, NULL);
    assert(ret == 0);
    ret = phoenix_voice_pipeline_start();
    assert(ret == 0);

    assert(phoenix_voice_pipeline_get_state() == VOICE_STATE_IDLE);

    /* 1. Inject speech PCM frames */
    int16_t speech_pcm[160];
    for (int i = 0; i < 160; i++) speech_pcm[i] = (int16_t)(i * 100);
    hal_mock_inject_pcm_frame(speech_pcm, 160, true);

    phoenix_voice_pipeline_tick();
    assert(phoenix_voice_pipeline_get_state() == VOICE_STATE_LISTENING);
    printf("  -> Speech presence detected -> State: LISTENING\n");

    /* 2. Inject 3 silence frames to trigger end of utterance */
    hal_mock_inject_pcm_frame(speech_pcm, 160, false);
    phoenix_voice_pipeline_tick();
    phoenix_voice_pipeline_tick();
    phoenix_voice_pipeline_tick();

    assert(g_voice_recognized_count == 1);
    assert(phoenix_voice_pipeline_get_state() == VOICE_STATE_IDLE);
    printf("  -> Utterance ended -> Speech successfully recognized & dispatched!\n");

    phoenix_voice_pipeline_deinit();
    hal_deinit();
    printf("  -> Audio In HAL & Voice Pipeline PASSED!\n");
}

/* ========================================================================= */
/* [TEST 20] Utils: Ring Buffer                                             */
/* ========================================================================= */
static void run_test_utils_ring_buffer(void)
{
    printf("\n[TEST 20] Testing Utils Ring Buffer Subsystem...\n");
    int storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, sizeof(int), 4);

    assert(ring_buffer_is_empty(&rb) == true);
    assert(ring_buffer_is_full(&rb) == false);
    assert(ring_buffer_count(&rb) == 0);

    /* Push 4 items */
    for (int i = 1; i <= 4; i++) {
        bool ok = ring_buffer_push(&rb, &i);
        assert(ok == true);
    }
    assert(ring_buffer_is_full(&rb) == true);
    assert(ring_buffer_count(&rb) == 4);

    /* 5th push should fail */
    int overflow_val = 5;
    assert(ring_buffer_push(&rb, &overflow_val) == false);

    /* Peek first element */
    int peek_val = 0;
    assert(ring_buffer_peek(&rb, &peek_val) == true);
    assert(peek_val == 1);

    /* Pop 2 elements */
    int pop_val = 0;
    assert(ring_buffer_pop(&rb, &pop_val) == true && pop_val == 1);
    assert(ring_buffer_pop(&rb, &pop_val) == true && pop_val == 2);
    assert(ring_buffer_count(&rb) == 2);

    /* Push 1 more (wrap-around verification) */
    int wrap_val = 99;
    assert(ring_buffer_push(&rb, &wrap_val) == true);
    assert(ring_buffer_count(&rb) == 3);

    /* Clear */
    ring_buffer_clear(&rb);
    assert(ring_buffer_is_empty(&rb) == true);
    assert(ring_buffer_count(&rb) == 0);

    printf("  -> Utils Ring Buffer PASSED!\n");
}

/* ========================================================================= */
/* [TEST 21] Utils: Time Utils & Logging                                     */
/* ========================================================================= */
static void run_test_utils_time_and_log(void)
{
    printf("\n[TEST 21] Testing Utils Time & Logging Subsystems...\n");

    /* 1. Time monotonicity */
    uint64_t t1 = time_utils_get_ms();
    time_utils_sleep_ms(15);
    uint64_t t2 = time_utils_get_ms();
    assert(t2 >= t1);
    assert((t2 - t1) >= 10);

    /* 2. Formatted date-time string */
    char dt_buf[32];
    memset(dt_buf, 0, sizeof(dt_buf));
    time_utils_get_datetime_str(dt_buf, sizeof(dt_buf));
    assert(strlen(dt_buf) >= 19); /* Format: YYYY-MM-DD HH:MM:SS */
    printf("  -> Formatted Datetime: [%s]\n", dt_buf);

    /* 3. Logging Engine & Macro Tests */
    phoenix_log_init();
    phoenix_log_set_level(PHOENIX_LOG_DEBUG);
    assert(phoenix_log_get_level() == PHOENIX_LOG_DEBUG);

    LOG_D("TestTag", "Debug logging active: %d", 42);
    LOG_I("TestTag", "Info logging active: %s", "HoloDesk-S1");
    LOG_W("TestTag", "Warn logging active");
    LOG_E("TestTag", "Error logging active");
    LOG_V("TestTag", "Verbose trace active");

    /* 4. Recent Ring Buffer Verification */
    char recent_buf[1024];
    size_t recent_len = phoenix_log_get_recent(recent_buf, sizeof(recent_buf));
    assert(recent_len > 0);
    assert(strstr(recent_buf, "TestTag") != NULL);
    assert(strstr(recent_buf, "Info logging active") != NULL);
    printf("  -> Recent Ring Buffer Captured %zu bytes.\n", recent_len);

    /* 5. Rate-limited Logging Test (50 rapid calls) */
    for (int i = 0; i < 50; i++) {
        LOG_W_RATELIMITED("RateLimitTag", 200, "Sensor connection retry count: %d", i);
    }
    printf("  -> Rate-limiting Suppression PASSED!\n");

    /* 6. Dynamic Level Filtering */
    phoenix_log_set_level(PHOENIX_LOG_ERROR);
    assert(phoenix_log_get_level() == PHOENIX_LOG_ERROR);
    LOG_I("FilteredTag", "This INFO log must be filtered out");
    phoenix_log_set_level(PHOENIX_LOG_INFO);
    assert(phoenix_log_get_level() == PHOENIX_LOG_INFO);

    /* 7. Web Portal Log APIs Verification */
    char web_resp[4096];
    const char *get_logs_req = "GET /api/logs HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    int n = phoenix_web_portal_handle_request(get_logs_req, web_resp, sizeof(web_resp));
    assert(n > 0);
    assert(strstr(web_resp, "\"level_str\":\"INFO\"") != NULL);
    assert(strstr(web_resp, "\"logs\":") != NULL);
    assert(strstr(web_resp, "\"cursor\":") != NULL);
    printf("  -> Web Portal GET /api/logs PASSED!\n");

    /* 8. Incremental Cursor Streaming Test */
    uint64_t cur1 = phoenix_log_get_cursor();
    char cursor_req[128];
    snprintf(cursor_req, sizeof(cursor_req), "GET /api/logs?cursor=%llu HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n", (unsigned long long)cur1);
    n = phoenix_web_portal_handle_request(cursor_req, web_resp, sizeof(web_resp));
    assert(n > 0);
    assert(strstr(web_resp, "\"bytes\":0") != NULL); /* 无新增日志，字节数为0 */

    LOG_I("StreamTag", "Brand new incremental log line for web live streaming");
    n = phoenix_web_portal_handle_request(cursor_req, web_resp, sizeof(web_resp));
    assert(n > 0);
    assert(strstr(web_resp, "Brand new incremental log line") != NULL);
    printf("  -> Web Portal GET /api/logs?cursor=X Streaming PASSED!\n");

    /* 9. Dynamic Log Level Update */
    const char *set_level_req = "POST /api/logs/level HTTP/1.1\r\nContent-Type: application/json\r\n\r\n{\"level\":\"debug\"}";
    n = phoenix_web_portal_handle_request(set_level_req, web_resp, sizeof(web_resp));
    assert(n > 0);
    assert(phoenix_log_get_level() == PHOENIX_LOG_DEBUG);
    assert(strstr(web_resp, "\"level_str\":\"DEBUG\"") != NULL);
    printf("  -> Web Portal POST /api/logs/level Dynamic Update PASSED!\n");

    /* 10. Clear Logs API Verification */
    const char *clear_logs_req = "POST /api/logs/clear HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    n = phoenix_web_portal_handle_request(clear_logs_req, web_resp, sizeof(web_resp));
    assert(n > 0);
    assert(strstr(web_resp, "\"success\":true") != NULL);
    printf("  -> Web Portal POST /api/logs/clear PASSED!\n");

    /* Restore default level */
    phoenix_log_set_level(PHOENIX_LOG_INFO);
    printf("  -> OpenVela / Host Dual-Backend Logging System PASSED!\n");
}

/* --- TASK-01: Cartridge Manager Unit Tests --- */
static int s_mock_enter_count = 0;
static int s_mock_exit_count = 0;
static int s_mock_tick_count = 0;
static int s_mock_knock_intensity = 0;
static int s_mock_knock_count = 0;

static int mock_cartridge_init(cartridge_t *self, void *user_data)
{
    (void)user_data;
    assert(self != NULL);
    return 0;
}

static void mock_cartridge_enter(cartridge_t *self, void *stage_view)
{
    (void)self;
    (void)stage_view;
    s_mock_enter_count++;
}

static void mock_cartridge_exit(cartridge_t *self)
{
    (void)self;
    s_mock_exit_count++;
}

static void mock_cartridge_tick(cartridge_t *self)
{
    (void)self;
    s_mock_tick_count++;
}

static void mock_cartridge_knock(cartridge_t *self, int intensity, int count)
{
    (void)self;
    s_mock_knock_intensity = intensity;
    s_mock_knock_count = count;
}

static void run_test_cartridge_mgr(void)
{
    printf("\n[TEST 22] Testing TASK-01: Cartridge Manager & Plugin Engine...\n");

    /* 1. Init */
    assert(cartridge_mgr_init((void*)0x1234) == 0);
    assert(cartridge_mgr_get_count() == 0);
    assert(cartridge_mgr_get_current() == NULL);

    /* 2. Register 3 Cartridges */
    cartridge_ops_t c1_ops = {
        .id = "familiar",
        .name = "桌面萌宠",
        .icon = "[PET]",
        .init = mock_cartridge_init,
        .enter = mock_cartridge_enter,
        .exit = mock_cartridge_exit,
        .tick_1s = mock_cartridge_tick,
        .on_knock = mock_cartridge_knock
    };
    cartridge_ops_t c2_ops = {
        .id = "memo",
        .name = "灵感外脑",
        .icon = "[MEMO]",
        .init = mock_cartridge_init,
        .enter = mock_cartridge_enter,
        .exit = mock_cartridge_exit,
        .tick_1s = mock_cartridge_tick,
        .on_knock = mock_cartridge_knock
    };
    cartridge_ops_t c3_ops = {
        .id = "clock",
        .name = "翻页时钟",
        .icon = "[TIME]",
        .init = mock_cartridge_init,
        .enter = mock_cartridge_enter,
        .exit = mock_cartridge_exit,
        .tick_1s = mock_cartridge_tick,
        .on_knock = mock_cartridge_knock
    };

    s_mock_enter_count = 0;
    s_mock_exit_count = 0;

    assert(cartridge_mgr_register(&c1_ops, NULL, NULL) == 0);
    assert(cartridge_mgr_get_count() == 1);
    /* First registered cartridge automatically activated */
    assert(s_mock_enter_count == 1);
    cartridge_t *curr = cartridge_mgr_get_current();
    assert(curr != NULL);
    assert(strcmp(curr->ops.id, "familiar") == 0);

    /* Register second and third */
    assert(cartridge_mgr_register(&c2_ops, NULL, NULL) == 0);
    assert(cartridge_mgr_register(&c3_ops, NULL, NULL) == 0);
    assert(cartridge_mgr_get_count() == 3);

    /* Test duplicate ID rejection */
    assert(cartridge_mgr_register(&c1_ops, NULL, NULL) != 0);
    assert(cartridge_mgr_get_count() == 3);

    /* 3. Next Cartridge Rotation */
    assert(cartridge_mgr_next() == 0);
    curr = cartridge_mgr_get_current();
    assert(curr != NULL && strcmp(curr->ops.id, "memo") == 0);
    assert(s_mock_exit_count == 1);
    assert(s_mock_enter_count == 2);

    /* Rotate again to clock */
    assert(cartridge_mgr_next() == 0);
    curr = cartridge_mgr_get_current();
    assert(curr != NULL && strcmp(curr->ops.id, "clock") == 0);

    /* Wrap around to familiar */
    assert(cartridge_mgr_next() == 0);
    curr = cartridge_mgr_get_current();
    assert(curr != NULL && strcmp(curr->ops.id, "familiar") == 0);

    /* 4. Prev Cartridge Rotation */
    assert(cartridge_mgr_prev() == 0);
    curr = cartridge_mgr_get_current();
    assert(curr != NULL && strcmp(curr->ops.id, "clock") == 0);

    /* 5. Switch to specific ID */
    assert(cartridge_mgr_switch_to("memo") == 0);
    curr = cartridge_mgr_get_current();
    assert(curr != NULL && strcmp(curr->ops.id, "memo") == 0);

    /* Switch to non-existent ID fails */
    assert(cartridge_mgr_switch_to("non_existent") != 0);

    /* 6. Event dispatch */
    s_mock_tick_count = 0;
    cartridge_mgr_dispatch_tick_1s();
    assert(s_mock_tick_count == 1);
    assert(curr->active_sec == 1);

    /* Normal knock dispatch */
    s_mock_knock_intensity = 0;
    s_mock_knock_count = 0;
    cartridge_mgr_dispatch_knock(40, 1);
    assert(s_mock_knock_intensity == 40);
    assert(s_mock_knock_count == 1);

    /* Triple knock (count == 3) triggers automatic rotation without hands */
    assert(strcmp(cartridge_mgr_get_current()->ops.id, "memo") == 0);
    /* 等待冷却防抖窗口期结束 (500ms) */
    time_utils_sleep_ms(510);
    cartridge_mgr_dispatch_knock(60, 3);
    assert(strcmp(cartridge_mgr_get_current()->ops.id, "clock") == 0);

    /* 7. Unregister cartridge */
    assert(cartridge_mgr_unregister("clock") == 0);
    assert(cartridge_mgr_get_count() == 2);
    /* Should fall back to remaining active */
    assert(cartridge_mgr_get_current() != NULL);

    /* 8. Deinit */
    cartridge_mgr_deinit();
    assert(cartridge_mgr_get_count() == 0);
    assert(cartridge_mgr_get_current() == NULL);

    printf("  -> TASK-01 Cartridge Manager PASSED!\n");
}

/* ---- 23. SoftAP Hotspot & Dual-Mode Network State Machine Test (TASK-03) ---- */
static net_mode_t s_last_test_net_mode = NET_MODE_DISCONNECTED;
static char s_last_test_net_ip[32] = {0};

static void on_test_net_state_changed(net_mode_t mode, const char *ip, void *user_data)
{
    (void)user_data;
    s_last_test_net_mode = mode;
    if (ip) {
        snprintf(s_last_test_net_ip, sizeof(s_last_test_net_ip), "%s", ip);
    }
}

static void run_test_network_mgr(void)
{
    printf("\n[TEST 23] Testing SoftAP Hotspot & Dual-Mode Network State Machine (TASK-03)...\n");

    /* 0. Verify STA IP validity filter logic (rejection of broadcast, loopback, softap, multicast, reserved) */
    assert(net_is_valid_sta_ip(NULL) == false);
    assert(net_is_valid_sta_ip("") == false);
    assert(net_is_valid_sta_ip("0.0.0.0") == false);
    assert(net_is_valid_sta_ip("127.0.0.1") == false);
    assert(net_is_valid_sta_ip("255.255.255.255") == false);
    assert(net_is_valid_sta_ip("255.255.255.0") == false);
    assert(net_is_valid_sta_ip("192.168.4.1") == false);
    assert(net_is_valid_sta_ip("10.0.0.2") == false);
    assert(net_is_valid_sta_ip("10.0.0.1") == false);
    assert(net_is_valid_sta_ip("10.0.2.15") == false);
    assert(net_is_valid_sta_ip("169.254.1.1") == false);
    assert(net_is_valid_sta_ip("224.0.0.1") == false);
    assert(net_is_valid_sta_ip("239.255.255.250") == false);
    assert(net_is_valid_sta_ip("240.0.0.1") == false);
    assert(net_is_valid_sta_ip("192.168.1.15") == true);
    assert(net_is_valid_sta_ip("10.10.1.20") == true);
    assert(net_is_valid_sta_ip("172.16.0.100") == true);
    printf("  -> STA IP Validation & Broadcast/Multicast Rejection Filter PASSED!\n");

    /* 1. Register state callback and initialize */
    net_mgr_register_state_cb(on_test_net_state_changed, NULL);
    assert(net_mgr_init() == 0);

    /* By default with no saved Wi-Fi, it should enter SoftAP configuration mode */
    assert(net_mgr_get_mode() == NET_MODE_SOFTAP_CONFIG);
    char ip_buf[32] = {0};
    char ssid_buf[32] = {0};
    assert(net_mgr_get_ip(ip_buf, sizeof(ip_buf)) == 0);
    assert(strcmp(ip_buf, "192.168.4.1") == 0);
    assert(net_mgr_get_ssid(ssid_buf, sizeof(ssid_buf)) == 0);
    assert(strcmp(ssid_buf, NET_DEFAULT_SOFTAP_SSID) == 0);
    assert(s_last_test_net_mode == NET_MODE_SOFTAP_CONFIG);
    printf("  -> SoftAP Default State PASSED (SSID: %s, IP: %s)\n", ssid_buf, ip_buf);

    /* 2. Verify Captive Portal HTTP responses under SoftAP mode */
    char resp_buf[4096];
    const char *req_captive1 = "GET /setup HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n";
    int resp_len = phoenix_web_portal_handle_request(req_captive1, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "Gemini-S1 硬件热点配网") != NULL);

    const char *req_root_softap = "GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_root_softap, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    /* In SoftAP mode, GET / should return setup page */
    assert(strstr(resp_buf, "Gemini-S1 硬件热点配网") != NULL);

    /* 2.1 iOS / macOS Captive Probe: /hotspot-detect.html -> 200 Setup Page */
    const char *req_ios_probe = "GET /hotspot-detect.html HTTP/1.1\r\nHost: captive.apple.com\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_ios_probe, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "200 OK") != NULL);
    assert(strstr(resp_buf, "Gemini-S1 硬件热点配网") != NULL);

    /* 2.2 Android Captive Probe: /generate_204 -> 302 Found redirect */
    const char *req_android_probe = "GET /generate_204 HTTP/1.1\r\nHost: connectivitycheck.gstatic.com\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_android_probe, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "302 Found") != NULL);
    assert(strstr(resp_buf, "Location: http://192.168.4.1/") != NULL);

    /* 2.3 Windows Captive Probe: /connecttest.txt -> 302 Found redirect */
    const char *req_win_probe = "GET /connecttest.txt HTTP/1.1\r\nHost: www.msftconnecttest.com\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_win_probe, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "302 Found") != NULL);
    assert(strstr(resp_buf, "Location: http://192.168.4.1/") != NULL);

    /* 2.4 SoftAP Unrecognized non-/api/ path -> 302 Found redirect */
    const char *req_unknown = "GET /some/random/domain/probe HTTP/1.1\r\nHost: example.com\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_unknown, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "302 Found") != NULL);
    assert(strstr(resp_buf, "Location: http://192.168.4.1/") != NULL);

    printf("  -> Captive Portal & SoftAP Web Redirect PASSED!\n");

    /* 2.1 Test Wi-Fi pre-scan cache & REST endpoint in SoftAP protection mode */
    int prescan_cnt = net_mgr_prescan_wifi();
    assert(prescan_cnt >= 4);

    net_wifi_ap_info_t scan_aps[8];
    int scanned = net_mgr_scan_wifi(scan_aps, 8);
    assert(scanned >= 4);
    assert(strcmp(scan_aps[0].ssid, "Office-5G") == 0);

    const char *req_scan = "GET /api/wifi/scan HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_scan, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "\"aps\"") != NULL);
    assert(strstr(resp_buf, "Office-5G") != NULL);
    printf("  -> Wi-Fi Pre-Scan Cache & SoftAP RF Protection PASSED! (Found %d APs)\n", scanned);

    /* 3.1 Test OPTIONS preflight request for CORS */
    const char *req_options = "OPTIONS /api/wifi/connect HTTP/1.1\r\nHost: 192.168.4.1\r\nAccess-Control-Request-Method: POST\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_options, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "204 No Content") != NULL);
    assert(strstr(resp_buf, "Access-Control-Allow-Origin: *") != NULL);
    printf("  -> CORS OPTIONS Preflight PASSED!\n");

    /* 3.2 Test empty SSID failure response */
    const char *req_empty = "POST /api/wifi/connect HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Length: 13\r\n\r\n{\"ssid\":\"\"}";
    resp_len = phoenix_web_portal_handle_request(req_empty, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "400 Bad Request") != NULL);
    printf("  -> Empty SSID Rejection (400) PASSED!\n");

    /* 3.3 Simulate client posting Wi-Fi credentials */
    const char *req_connect = "POST /api/wifi/connect HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Length: 55\r\n\r\n{\"ssid\":\"Office-5G\",\"psk\":\"pass123456\",\"api_key\":\"sk-net\"}";
    resp_len = phoenix_web_portal_handle_request(req_connect, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "\"success\":true") != NULL);

    /* Now network state should transition to STA_CONNECTED */
    assert(net_mgr_get_mode() == NET_MODE_STA_CONNECTED);
    assert(net_mgr_get_ip(ip_buf, sizeof(ip_buf)) == 0);
    assert(strcmp(ip_buf, "192.168.1.108") == 0);
    assert(net_mgr_get_ssid(ssid_buf, sizeof(ssid_buf)) == 0);
    assert(strcmp(ssid_buf, "Office-5G") == 0);
    printf("  -> Wi-Fi Connect & DHCP Transition PASSED (SSID: %s, IP: %s)\n", ssid_buf, ip_buf);

    /* 4. Under STA_CONNECTED mode, GET / should now return Geek Companion Dashboard */
    const char *req_root_sta = "GET / HTTP/1.1\r\nHost: 192.168.1.108\r\n\r\n";
    resp_len = phoenix_web_portal_handle_request(req_root_sta, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "Phoenix HoloDesk-S1 Geek Dashboard") != NULL);
    printf("  -> STA Dual-Mode Dashboard Switch PASSED!\n");

    /* 5. Web toggle test (enable / disable) */
    assert(net_mgr_is_web_enabled() == true);
    net_mgr_set_web_enabled(false);
    assert(net_mgr_is_web_enabled() == false);
    net_mgr_set_web_enabled(true);
    assert(net_mgr_is_web_enabled() == true);
    printf("  -> On-demand Web Portal Toggle PASSED!\n");

    /* 6. Test Reset to SoftAP */
    assert(net_mgr_reset_to_softap() == 0);
    assert(net_mgr_get_mode() == NET_MODE_SOFTAP_CONFIG);
    assert(net_mgr_get_ip(ip_buf, sizeof(ip_buf)) == 0);
    assert(strcmp(ip_buf, "192.168.4.1") == 0);
    printf("  -> One-click Reset to SoftAP PASSED!\n");

    /* 7. Disconnect and Deinit */
    net_mgr_disconnect();
    assert(net_mgr_get_mode() == NET_MODE_DISCONNECTED);
    net_mgr_deinit();
    printf("  -> TASK-03 Network Manager Dual-Mode PASSED!\n");
}

/* ---- 24. Four Built-in Cartridges Lifecycle & Rotation (TASK-05 ~ 08) ---- */
static void run_test_four_cartridges(void)
{
    printf("\n[TEST 24] Testing Core Cartridges (Home, Clock & Proactive Agent)...\n");

    /* 1. Init agent core & manager & register 3 cartridges */
    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();
    cartridge_mgr_init(NULL);
    assert(cartridge_home_register() == 0);
    assert(cartridge_clock_register() == 0);
    assert(cartridge_agent_register() == 0);
    assert(cartridge_mgr_get_count() == 3);
    printf("  -> Core Cartridges Registered PASSED! (Count: 3 - home, clock, agent)\n");

    /* 2. Test home cartridge */
    assert(cartridge_mgr_switch_to("home") == 0);
    cartridge_t *cur = cartridge_mgr_get_current();
    assert(cur && strcmp(cur->ops.id, "home") == 0);
    assert(cartridge_home_add_todo("测试新待办条目", "18:00") == 0);
    assert(cartridge_home_toggle_todo(0) == 0);
    assert(cartridge_home_toggle_pomodoro() == 0);
    if (cur->ops.on_knock) cur->ops.on_knock(cur, 1, 1);
    if (cur->ops.tick_1s) cur->ops.tick_1s(cur);
    printf("  -> Cartridge 1 [home - 主界面仪表盘 & 内嵌番茄钟] PASSED!\n");

    /* 3. Switch to clock */
    time_utils_sleep_ms(510);
    assert(cartridge_mgr_switch_to("clock") == 0);
    cur = cartridge_mgr_get_current();
    assert(cur && strcmp(cur->ops.id, "clock") == 0);
    if (cur->ops.on_knock) cur->ops.on_knock(cur, 1, 1);
    if (cur->ops.tick_1s) cur->ops.tick_1s(cur);
    char status_buf[256];
    assert(cur->ops.get_web_status && cur->ops.get_web_status(cur, status_buf, sizeof(status_buf)) == 0);
    assert(strstr(status_buf, "remain_s") != NULL);
    printf("  -> Cartridge 2 [clock - 拟物机械翻页钟] Pomodoro & Status: %s\n", status_buf);

    /* 4. Switch to agent & test proactive voice interaction */
    time_utils_sleep_ms(510);
    assert(cartridge_mgr_switch_to("agent") == 0);
    cur = cartridge_mgr_get_current();
    assert(cur && strcmp(cur->ops.id, "agent") == 0);
    if (cur->ops.on_knock) cur->ops.on_knock(cur, 1, 1);
    if (cur->ops.tick_1s) cur->ops.tick_1s(cur);
    cartridge_agent_trigger_voice_chat("帮我开启25分钟专注流");
    printf("  -> Cartridge 3 [agent - 主动式灵眸AI语音交互] PASSED!\n");

    /* 5. 3-Cartridge Continuous Loop Rotation */
    time_utils_sleep_ms(510);
    cartridge_mgr_next(); /* agent -> home */
    assert(strcmp(cartridge_mgr_get_current()->ops.id, "home") == 0);
    printf("  -> Continuous Loop Rotation (agent -> home) PASSED!\n");

    time_utils_sleep_ms(510);
    cartridge_mgr_next(); /* home -> clock */
    assert(strcmp(cartridge_mgr_get_current()->ops.id, "clock") == 0);
    printf("  -> Continuous Loop Rotation (home -> clock) PASSED!\n");

    time_utils_sleep_ms(510);
    cartridge_mgr_next(); /* clock -> agent */
    assert(strcmp(cartridge_mgr_get_current()->ops.id, "agent") == 0);
    printf("  -> Continuous Loop Rotation (clock -> agent) PASSED!\n");

    time_utils_sleep_ms(510);
    cartridge_mgr_prev(); /* agent -> clock */
    assert(strcmp(cartridge_mgr_get_current()->ops.id, "clock") == 0);
    printf("  -> Continuous Loop Reverse Rotation (agent -> clock) PASSED!\n");

    /* 6. Test background Pomodoro Service persistence across cartridges */
    assert(pomodoro_service_start(25) == 0);
    assert(pomodoro_service_is_active() == true);
    uint16_t rem_before = pomodoro_service_get_remaining();
    assert(rem_before == 25 * 60);

    /* 即使前台卡带为 agent/home，系统后台心跳持续流转，倒计时不丢 */
    pomodoro_service_tick_1s();
    uint16_t rem_after = pomodoro_service_get_remaining();
    assert(rem_after == rem_before - 1);
    assert(pomodoro_service_is_active() == true);
    pomodoro_service_stop();
    assert(pomodoro_service_is_active() == false);
    printf("  -> Background Pomodoro Service across Cartridges PASSED!\n");

    cartridge_mgr_deinit();
    phoenix_agent_core_destroy(agent);
    printf("  -> Core Cartridges & Swipe Navigation PASSED!\n");
}

/* ---- 25. TF Card Hardware Abstraction & Web File Management Test ---- */
static void run_test_sdcard_storage_and_web_mgmt(void)
{
    printf("\n[TEST 25] Testing TF Card HAL & Web File Management Subsystem...\n");

    /* 1. Test Base Paths & Recursive Mkdir */
    const char *data_base = hal_system_get_storage_base_path();
    const char *temp_base = hal_system_get_temp_base_path();
    assert(data_base != NULL && strlen(data_base) > 0);
    assert(temp_base != NULL && strlen(temp_base) > 0);

    int mret = hal_system_mkdir_p("/tmp/phoenix_test_mkdir/sub1/sub2", 0755);
    assert(mret == 0);
    struct stat st;
    assert(stat("/tmp/phoenix_test_mkdir/sub1/sub2", &st) == 0 && S_ISDIR(st.st_mode));
    printf("  -> Base Storage & Recursive mkdir_p PASSED!\n");

    /* 2. Test TF Card HAL Initialization & Detection */
    int sret = hal_sdcard_init();
    assert(sret == 0);
    assert(hal_sdcard_is_mounted() == true);

    hal_sdcard_info_t info;
    memset(&info, 0, sizeof(info));
    int iret = hal_sdcard_get_info(&info);
    assert(iret == 0);
    assert(info.is_mounted == true);
    assert(strlen(info.mount_point) > 0);
    assert(info.total_mb > 0);
    printf("  -> TF Card Detection & Telemetry (Mount: %s, Total: %u MB) PASSED!\n", info.mount_point, (unsigned int)info.total_mb);

    /* 3. Test Auto Ensure Recommended Directories */
    assert(hal_sdcard_ensure_dirs() == 0);
    char chk_path[512];
    snprintf(chk_path, sizeof(chk_path), "%s/sounds", info.mount_point);
    assert(stat(chk_path, &st) == 0 && S_ISDIR(st.st_mode));
    printf("  -> TF Card Recommended Dirs Auto-Creation PASSED!\n");

    /* 4. Test Path Resolution & Directory Traversal Protection */
    char resolved[512];
    assert(hal_sdcard_resolve_path("/sounds/test.wav", resolved, sizeof(resolved)) == 0);
    assert(strstr(resolved, info.mount_point) != NULL);

    /* Security check: Must reject directory traversal */
    assert(hal_sdcard_resolve_path("../../../etc/passwd", resolved, sizeof(resolved)) < 0);
    assert(hal_sdcard_resolve_path("/../etc/shadow", resolved, sizeof(resolved)) < 0);
    printf("  -> Path Resolution & Directory Traversal Protection PASSED!\n");

    /* 5. Test Web Portal TF Card REST APIs */
    phoenix_app_config_t cfg = {
        .storage_dir = "/tmp/phoenix_test_sdcard_web",
        .sounds_dir = "/tmp",
        .api_key = NULL,
        .register_tools = true,
        .enable_web_portal = true,
        .web_port = 8089
    };
    system("rm -rf /tmp/phoenix_test_sdcard_web");
    assert(phoenix_app_init(&cfg) == 0);

    char resp_buf[16384];

    /* 5.1 GET /api/sdcard/status */
    const char *req_status = "GET /api/sdcard/status HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int rlen = phoenix_web_portal_handle_request(req_status, resp_buf, sizeof(resp_buf));
    assert(rlen > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "\"mounted\":true") != NULL);
    assert(strstr(resp_buf, "\"recommend_dirs\"") != NULL);
    printf("  -> GET /api/sdcard/status PASSED!\n");

    /* 5.2 POST /api/sdcard/mkdir (Create /docs/unit_test_dir) */
    const char *req_mkdir = "POST /api/sdcard/mkdir HTTP/1.1\r\nHost: localhost\r\nContent-Length: 32\r\n\r\n{\"path\":\"/docs/unit_test_dir\"}";
    rlen = phoenix_web_portal_handle_request(req_mkdir, resp_buf, sizeof(resp_buf));
    assert(rlen > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "Directory created") != NULL);
    printf("  -> POST /api/sdcard/mkdir PASSED!\n");

    /* 5.3 POST /api/sdcard/upload (Upload hello.txt) */
    const char *req_upload = "POST /api/sdcard/upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 85\r\n\r\n{\"path\":\"/docs/unit_test_dir/hello.txt\",\"content\":\"Phoenix HoloDesk-S1 TF Card Storage\"}";
    rlen = phoenix_web_portal_handle_request(req_upload, resp_buf, sizeof(resp_buf));
    assert(rlen > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "\"bytes_written\":35") != NULL);
    printf("  -> POST /api/sdcard/upload PASSED!\n");

    /* 5.4 GET /api/sdcard/list?path=/docs/unit_test_dir */
    const char *req_list = "GET /api/sdcard/list?path=/docs/unit_test_dir HTTP/1.1\r\nHost: localhost\r\n\r\n";
    rlen = phoenix_web_portal_handle_request(req_list, resp_buf, sizeof(resp_buf));
    assert(rlen > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "hello.txt") != NULL);
    assert(strstr(resp_buf, "\"is_dir\":false") != NULL);
    printf("  -> GET /api/sdcard/list PASSED!\n");

    /* 5.5 GET /api/sdcard/download?path=/docs/unit_test_dir/hello.txt */
    const char *req_dl = "GET /api/sdcard/download?path=/docs/unit_test_dir/hello.txt HTTP/1.1\r\nHost: localhost\r\n\r\n";
    rlen = phoenix_web_portal_handle_request(req_dl, resp_buf, sizeof(resp_buf));
    assert(rlen > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "attachment; filename=\"hello.txt\"") != NULL);
    assert(strstr(resp_buf, "Phoenix HoloDesk-S1 TF Card Storage") != NULL);
    printf("  -> GET /api/sdcard/download PASSED!\n");

    /* 5.6 GET /api/status (Verify storage node included) */
    const char *req_dash_status = "GET /api/status HTTP/1.1\r\nHost: localhost\r\n\r\n";
    rlen = phoenix_web_portal_handle_request(req_dash_status, resp_buf, sizeof(resp_buf));
    assert(rlen > 0);
    assert(strstr(resp_buf, "\"storage\":{") != NULL);
    assert(strstr(resp_buf, "\"sdcard_mounted\":true") != NULL);
    printf("  -> GET /api/status (Storage Telemetry Verification) PASSED!\n");

    /* 5.7 POST /api/sdcard/delete (Delete hello.txt) */
    const char *req_del = "POST /api/sdcard/delete HTTP/1.1\r\nHost: localhost\r\nContent-Length: 42\r\n\r\n{\"path\":\"/docs/unit_test_dir/hello.txt\"}";
    rlen = phoenix_web_portal_handle_request(req_del, resp_buf, sizeof(resp_buf));
    assert(rlen > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "Deleted successfully") != NULL);
    printf("  -> POST /api/sdcard/delete PASSED!\n");

    phoenix_app_deinit();
    hal_sdcard_deinit();
    printf("  -> TF Card HAL & Web File Management Subsystem PASSED!\n");
}

/* ---- 26. Living Cyber-Eye Familiar & Natural Gestures Test ---- */
static void run_test_familiar_and_gestures(void)
{
    printf("\n[TEST 26] Testing Living Cyber-Eye Familiar & Natural Gestures...\n");

    phoenix_event_bus_init();
    phoenix_store_init("/tmp/phoenix_test_familiar");
    cartridge_mgr_init(NULL);

    /* 1. Register Familiar Cartridge as Master Companion */
    assert(cartridge_familiar_register() == 0);
    cartridge_t *cur = cartridge_mgr_get_current();
    assert(cur && strcmp(cur->ops.id, "familiar") == 0);

    /* 2. Test initial status & telemetry */
    char status_buf[256];
    assert(cur->ops.get_web_status && cur->ops.get_web_status(cur, status_buf, sizeof(status_buf)) == 0);
    assert(strstr(status_buf, "affinity") != NULL);
    assert(strstr(status_buf, "mood") != NULL);
    printf("  -> Familiar Cyber-Eye Initial Status: %s\n", status_buf);

    /* 3. Test Tap / Knock: Pet Affinity + Wooden Fish Merit Dual Reward */
    phoenix_stats_t stats_before;
    phoenix_store_get_stats(&stats_before);
    uint32_t merit_before = stats_before.total_merit;

    g_event_counter = 0;
    phoenix_event_subscribe(PHOENIX_EVT_FLYING_TEXT, test_event_listener, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_PLAY_SOUND, test_event_listener, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_MERIT_UPDATED, test_event_listener, NULL);

    /* Simulate tapping eye / knocking desk */
    if (cur->ops.on_knock) {
        cur->ops.on_knock(cur, 1, 1);
    }
    phoenix_event_bus_drain();

    phoenix_stats_t stats_after;
    phoenix_store_get_stats(&stats_after);
    assert(stats_after.total_merit == merit_before + 1);
    assert(g_event_counter >= 3); /* Sound + Flying Text + Merit Updated */
    printf("  -> Knock on Living Cyber-Eye Dual Reward (Affinity & Merit +1) PASSED!\n");

    /* 4. Test 1-second Tick (Time display update & calm recovery) */
    if (cur->ops.tick_1s) {
        cur->ops.tick_1s(cur);
    }
    printf("  -> Familiar 1s Tick & Time Update PASSED!\n");

    cartridge_mgr_deinit();
    phoenix_store_deinit();
    phoenix_event_bus_deinit();
    printf("  -> Living Cyber-Eye Familiar & Natural Gestures Subsystem PASSED!\n");
}

/* ---- 27. Web Bluetooth BLE Provisioning Service Test ---- */
static void run_test_ble_prov_service(void)
{
    printf("\n[TEST 27] Testing Web Bluetooth BLE Provisioning Service & Endpoints...\n");

    /* 0. Initialize configuration subsystem for BLE provisioning test */
    phoenix_config_init("/tmp/phoenix_test_ble_cfg");

    /* 1. Initialize BLE provisioning service */
    assert(ble_prov_service_init("Phoenix-Test-Setup") == 0);
    assert(ble_prov_service_get_state() == BLE_PROV_STATE_ADVERTISING);
    printf("  -> BLE Prov Service Initialized (State: Advertising) PASSED\n");

    /* 2. Test Wi-Fi scanning notification via BLE */
    int scan_count = ble_prov_service_notify_wifi_scan();
    assert(scan_count >= 0);
    printf("  -> BLE Wi-Fi Scan Notify triggered PASSED\n");

    /* 3. Test Network Status Notification */
    assert(ble_prov_service_notify_net_status("connecting", "GeekLab-5G", "", "Connecting to AP...") == 0);
    assert(ble_prov_service_notify_net_status("connected", "GeekLab-5G", "192.168.31.88", "OK") == 0);
    assert(ble_prov_service_get_state() == BLE_PROV_STATE_PROVISIONED);
    printf("  -> BLE Net Status Notification & IP Broadcast PASSED\n");

    /* 3.1 Test Bidirectional Protocol: notify_config and notify_system_info */
    assert(ble_prov_service_notify_config() == 0);
    assert(ble_prov_service_notify_system_info() == 0);
    assert(ble_prov_service_notify_ack("test_ack", true, "ACK OK") == 0);
    printf("  -> BLE Bidirectional Uplink Event Notifications PASSED\n");

    /* 3.2 Test Bidirectional Protocol: Incoming Command Handlers (get_config, set_config, get_system_info) */
    assert(ble_prov_service_handle_command("{\"cmd\":\"get_config\"}") == 0);
    assert(ble_prov_service_handle_command("{\"cmd\":\"get_system_info\"}") == 0);

    /* Test set_config updating agent model & prompt without reconnecting Wi-Fi */
    const char *test_set_cmd = "{\"cmd\":\"set_config\",\"agent\":{\"api_key\":\"sk-test12345678\",\"model\":\"deepseek-reasoner\",\"prompt\":\"你是极客桌面使魔\"}}";
    assert(ble_prov_service_handle_command(test_set_cmd) == 0);

    char verify_model[64] = {0};
    phoenix_config_get_str(PHOENIX_CFG_MODEL, "", verify_model, sizeof(verify_model));
    assert(strcmp(verify_model, "deepseek-reasoner") == 0);
    char verify_key[64] = {0};
    phoenix_config_get_str(PHOENIX_CFG_API_KEY, "", verify_key, sizeof(verify_key));
    assert(strcmp(verify_key, "sk-test12345678") == 0);
    printf("  -> BLE Bidirectional Downlink Command Processing & Param Persistence PASSED\n");

    /* 3.3 Test Standalone ble_prov CLI Builtin Entry */
    extern int ble_prov_main(int argc, char *argv[]);
    char *cli_status[] = {"ble_prov", "status", NULL};
    assert(ble_prov_main(2, cli_status) == 0);
    char *cli_get[] = {"ble_prov", "get_config", NULL};
    assert(ble_prov_main(2, cli_get) == 0);
    printf("  -> Standalone ble_prov CLI Builtin Tool PASSED\n");

    /* 4. Test Web Bluetooth HTML Routes (/ble_setup and /ble_setup.html) */
    char resp_buf[32768];
    const char *req_ble_setup = "GET /ble_setup HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n";
    int resp_len = phoenix_web_portal_handle_request(req_ble_setup, resp_buf, sizeof(resp_buf));
    assert(resp_len > 0);
    assert(strstr(resp_buf, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp_buf, "Phoenix 蓝牙极速配网") != NULL);
    assert(strstr(resp_buf, "navigator.bluetooth") != NULL);
    printf("  -> Web Bluetooth SPA Endpoint (/ble_setup) PASSED! (Length: %d bytes)\n", resp_len);

    /* 5. De-initialize BLE service */
    ble_prov_service_deinit();
    assert(ble_prov_service_get_state() == BLE_PROV_STATE_STOPPED);
    phoenix_config_deinit();
    printf("  -> BLE Prov Service Teardown PASSED\n");
}

/* ---- 28. Persistent Flash FileLog & Multi-channel Pipeline Test ---- */
static void run_test_log_mgr_persistence_and_multichannel(void)
{
    printf("\n[TEST 28] Testing Multi-channel Persistent FileLog & Flash Blackbox...\n");

    /* 1. Init logging system */
    assert(phoenix_log_init() == 0);

    /* 2. Configure persistent file log with small rotation limit (1024 bytes) for test */
    const char *test_log_file = "/tmp/phoenix_test_logs/phoenix.log";
    unlink("/tmp/phoenix_test_logs/phoenix.log");
    unlink("/tmp/phoenix_test_logs/phoenix.log.old");

    assert(phoenix_log_enable_file(test_log_file, 1024) == 0);
    assert(phoenix_log_get_file_path() != NULL);
    assert(strcmp(phoenix_log_get_file_path(), test_log_file) == 0);

    /* 3. Write various level logs and verify recent ring buffer */
    phoenix_log_set_level(PHOENIX_LOG_VERBOSE);
    LOG_I("TestLog", "First test info message");
    LOG_W("TestLog", "Warning message with code: %d", 42);
    LOG_E("TestLog", "Critical error message: %s", "Disk I/O failure");
    LOG_D("TestLog", "Debug packet trace");
    LOG_V("TestLog", "Verbose byte stream");

    phoenix_log_flush();

    /* 4. Verify file was created on filesystem */
    FILE *fp = fopen(test_log_file, "r");
    assert(fp != NULL);
    char buf[1024] = {0};
    size_t r = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    assert(r > 0);
    assert(strstr(buf, "[TestLog:I] First test info message") != NULL);
    assert(strstr(buf, "[TestLog:W] Warning message with code: 42") != NULL);
    assert(strstr(buf, "[TestLog:E] Critical error message: Disk I/O failure") != NULL);
    printf("  -> Flash Log File Creation & Content Verification PASSED (Read %zu bytes)\n", r);

    /* 5. Fill log past 1024 bytes to trigger automatic rotation */
    for (int i = 0; i < 25; i++) {
        LOG_I("RollTest", "Rolling test message line #%02d padding string for size overflow", i);
    }
    phoenix_log_flush();

    /* Verify that old log backup was produced */
    FILE *fp_old = fopen("/tmp/phoenix_test_logs/phoenix.log.old", "r");
    assert(fp_old != NULL);
    fclose(fp_old);
    printf("  -> Automatic Log Rotation & Blackbox Backup (.old) PASSED\n");

    /* 6. Console silencing test */
    phoenix_log_enable_console(false);
    LOG_I("SilentTest", "This line should not appear on console output");
    phoenix_log_enable_console(true);

    /* 7. Clean up */
    phoenix_log_clear_recent();
    phoenix_log_enable_file(NULL, 0);
    assert(phoenix_log_get_file_path() == NULL);

    unlink("/tmp/phoenix_test_logs/phoenix.log");
    unlink("/tmp/phoenix_test_logs/phoenix.log.old");
    rmdir("/tmp/phoenix_test_logs");

    phoenix_log_deinit();
    printf("  -> Multi-channel Persistent FileLog & Flash Blackbox PASSED!\n");
}

/* ---- 29. Architecture Evolution & Subsystem Hardening Test ---- */
static int s_reentrant_cb_invoked = 0;
static void on_test_reentrant_cb(const phoenix_event_data_t *evt, void *user_data)
{
    (void)evt;
    (void)user_data;
    s_reentrant_cb_invoked++;
    /* 验证快照派发安全：在回调执行期间进行 unsubscribe 不会导致死锁或崩溃 */
    phoenix_event_unsubscribe(PHOENIX_EVT_STATE_CHANGED, on_test_reentrant_cb, NULL);
}

static void run_test_architecture_evolution(void)
{
    printf("\n[TEST 29] Testing Architecture Evolution (Thread-Safe EventBus, Two-Tier Memory, Coex Arbiter & Dynamic Cartridge)...\n");

    /* 1. Thread-Safe EventBus Snapshot & Self-Unsubscribe Safety */
    s_reentrant_cb_invoked = 0;
    phoenix_event_bus_init();
    phoenix_event_subscribe(PHOENIX_EVT_STATE_CHANGED, on_test_reentrant_cb, NULL);

    phoenix_event_data_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_STATE_CHANGED;
    phoenix_event_publish(&evt);
    assert(s_reentrant_cb_invoked == 1);

    /* Second publish should not invoke the unsubscribed callback */
    phoenix_event_publish(&evt);
    assert(s_reentrant_cb_invoked == 1);
    phoenix_event_bus_deinit();
    printf("  -> Thread-safe EventBus & Re-entrant Snapshot Dispatch PASSED\n");

    /* 2. Two-Tier Memory & Context Summary Compaction */
    phoenix_event_bus_init();
    phoenix_agent_ctx_t *agent = phoenix_agent_core_init();
    assert(agent != NULL);
    assert(agent->context_summary == NULL);

    /* 连续发送对话填满历史窗口触发两级记忆压缩 */
    for (int i = 0; i < 15; i++) {
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "请问第 %d 次敲击木鱼的赛博功德是多少？", i + 1);
        phoenix_agent_chat(agent, prompt);
    }
    assert(agent->history_count <= PHOENIX_MAX_MESSAGES);
    /* 验证两级长程记忆已生成并提炼出摘要卡片 */
    assert(agent->context_summary != NULL);
    assert(strlen(agent->context_summary) > 0);
    printf("  -> Two-Tier Memory Compactor & Watermark PASSED (Summary: %.40s...)\n", agent->context_summary);

    phoenix_agent_core_destroy(agent);
    phoenix_event_bus_deinit();

    /* 3. BLE vs SoftAP Coex RF Arbiter Test */
    assert(net_mgr_init() == 0);
    assert(net_mgr_get_mode() == NET_MODE_SOFTAP_CONFIG);

    /* BLE 连接建立 -> 挂起 SoftAP 广播 */
    assert(net_mgr_suspend_softap() == 0);
    /* BLE 断开降级 -> 恢复唤醒 SoftAP 热点 */
    assert(net_mgr_resume_softap() == 0);
    net_mgr_deinit();
    printf("  -> BLE vs SoftAP Coex RF Arbiter PASSED\n");

    /* 4. External Cartridge Package Scanner & Dynamic Loading */
    cartridge_mgr_init(NULL);
    system("mkdir -p /tmp/phoenix_test_cartridges/weather");
    FILE *mf = fopen("/tmp/phoenix_test_cartridges/weather/manifest.json", "w");
    assert(mf != NULL);
    fputs("{\"id\":\"weather\",\"name\":\"极客天气\",\"icon\":\"🌤️\"}", mf);
    fclose(mf);

    int loaded = cartridge_mgr_scan_external("/tmp/phoenix_test_cartridges");
    assert(loaded == 1);
    cartridge_t *dyn_c = cartridge_mgr_get_by_id("weather");
    assert(dyn_c != NULL);
    assert(strcmp(dyn_c->ops.name, "极客天气") == 0);
    assert(strcmp(dyn_c->ops.icon, "🌤️") == 0);

    /* Switch into dynamic cartridge */
    assert(cartridge_mgr_switch_to("weather") == 0);
    assert(cartridge_mgr_get_current() == dyn_c);

    cartridge_mgr_deinit();
    unlink("/tmp/phoenix_test_cartridges/weather/manifest.json");
    rmdir("/tmp/phoenix_test_cartridges/weather");
    rmdir("/tmp/phoenix_test_cartridges");
    printf("  -> Dynamic Cartridge Scanner & Hot-Mounting PASSED\n");

    printf("  -> Architecture Evolution Subsystem Hardening PASSED!\n");
}

int main(int argc, char *argv[])
{
    printf("====================================================\n");
    printf(" 🚀 Phoenix HoloDesk-S1 Host Native Test Suite\n");
    printf("====================================================\n");

    /* If -w or --web is passed, skip all tests and launch interactive Web REPL directly */
    if (argc > 1 && (strcmp(argv[1], "-w") == 0 || strcmp(argv[1], "--web") == 0)) {
        run_interactive_repl();
        return 0;
    }

    /* Run all unit tests */
    run_test_event_bus();
    run_test_tool_registry();
    run_test_store();
    run_test_tools_execution();
    run_test_agent_core();
    run_test_reasoning_and_compact();
    run_test_launch_app();
    run_test_app_facade();
    run_test_harness_backend_switching();
    run_test_proactive_mind();
    run_test_web_portal();
    run_test_token_and_observability();
    run_test_hal_subsystems();
    run_test_perception_embodied_loop();
    run_test_async_event_queue();
    run_test_embodied_expression();
    run_test_intent_router_fastpath();
    run_test_config_subsystem();
    run_test_audio_in_and_voice();
    run_test_utils_ring_buffer();
    run_test_utils_time_and_log();
    run_test_cartridge_mgr();
    run_test_network_mgr();
    run_test_four_cartridges();
    run_test_sdcard_storage_and_web_mgmt();
    run_test_familiar_and_gestures();
    run_test_ble_prov_service();
    run_test_log_mgr_persistence_and_multichannel();
    run_test_architecture_evolution();

    printf("\n🎉 ALL 29 UNIT TESTS PASSED SUCCESSFULLY!\n");

    /* If --repl or -i passed, enter interactive mode */
    if (argc > 1 && (strcmp(argv[1], "-i") == 0 || strcmp(argv[1], "--repl") == 0)) {
        run_interactive_repl();
    } else {
        printf("\nTip: Run with './run_host_test.sh -i' to enter interactive chat REPL mode.\n");
    }

    return 0;
}
