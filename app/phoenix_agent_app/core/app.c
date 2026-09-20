/**
 * @file app.c
 * @brief Phoenix HoloDesk-S1 Master Application Facade Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "app.h"
#include "event_bus.h"
#include "config.h"
#include "store.h"
#include "expression.h"
#include "tool_registry.h"
#include "cartridge_mgr.h"
#include "../cartridges/cartridge_home.h"
#include "../cartridges/cartridge_clock.h"
#include "../cartridges/cartridge_zen.h"
#include "../cartridges/cartridge_memo.h"
#include "../cartridges/cartridge_agent.h"
#include "web_portal.h"
#include "agent_core.h"
#include "../utils/time_utils.h"
#include "../tools/tools.h"
#include "../harness/llm_provider.h"
#include "../harness/asr_provider.h"
#include "../hal/hal_manager.h"
#include "../perception/perception.h"
#include "../voice/voice_pipeline.h"
#include "audio_test_service.h"
#include "../hal/network_mgr.h"
#include "../utils/log_utils.h"
#include <stdio.h>
#include <string.h>

#define TAG "PhoenixApp"

static bool g_app_initialized = false;

static void on_voice_recognized_text(const char *recognized_text, void *user_data)
{
    (void)user_data;
    if (!recognized_text || recognized_text[0] == '\0') {
        return;
    }
    LOG_I(TAG, "🎙️ 语音唤醒并识别到输入: \"%s\"", recognized_text);
    phoenix_agent_ctx_t *agent = phoenix_agent_get_instance();
    if (agent) {
        phoenix_agent_chat_async(agent, recognized_text);
    }
}


int phoenix_app_init(const phoenix_app_config_t *config)
{
    /* 优先初始化统一日志子系统 */
    phoenix_log_init();

    if (g_app_initialized) {
        LOG_I(TAG, "Notice: Already initialized, reusing existing context.");
        return 0;
    }

    const char *store_dir = (config && config->storage_dir) ? config->storage_dir : "/data/phoenix";
    const char *sounds_dir = (config && config->sounds_dir) ? config->sounds_dir : "/data/sounds";
    const char *api_key = (config && config->api_key) ? config->api_key : NULL;
    bool reg_tools = config ? config->register_tools : true;

    /* 挂载 Flash 滚动黑匣子持久化日志 (64KB 自动轮转覆盖，供 adb pull 提取分析) */
    char log_file_path[256];
    snprintf(log_file_path, sizeof(log_file_path), "%s/logs/phoenix.log", store_dir);
    phoenix_log_enable_file(log_file_path, 64 * 1024);

    LOG_I(TAG, "🚀 Initializing Phoenix HoloDesk-S1 Subsystems (Unified Logging & Flash Blackbox Enabled)...");

    /* 0. Hardware Abstraction Layer (HAL) */
    hal_config_t hal_cfg;
    memset(&hal_cfg, 0, sizeof(hal_cfg));
    hal_cfg.sound_data_root = sounds_dir;
    int ret = hal_init(&hal_cfg);
    if (ret != 0) {
        LOG_W(TAG, "⚠️ Warning: HAL init returned %d, continuing with defaults.", ret);
    }

    /* 1. Configuration Subsystem */
    ret = phoenix_config_init(store_dir);
    if (ret != 0) {
        LOG_W(TAG, "⚠️ Warning: Config init returned %d.", ret);
    }

    /* 2. Persistent Storage Engine */
    ret = phoenix_store_init(store_dir);
    if (ret != 0) {
        LOG_W(TAG, "⚠️ Warning: Store init returned %d, continuing in volatile mode.", ret);
    }

    /* 3. Core Event Bus */
    ret = phoenix_event_bus_init();
    if (ret != 0) {
        LOG_E(TAG, "❌ Error: Event bus init failed (%d)!", ret);
        phoenix_store_deinit();
        phoenix_config_deinit();
        hal_deinit();
        return ret;
    }

    /* 4. Embodied Expression Engine */
    phoenix_expression_init();

    /* 5. Embodied Perception Engine */
    ret = phoenix_perception_init(NULL);
    if (ret != 0) {
        LOG_W(TAG, "⚠️ Warning: Perception engine init returned %d.", ret);
    }

    /* 6. Voice Pipeline Engine */
    phoenix_voice_pipeline_init(on_voice_recognized_text, NULL);
    phoenix_voice_pipeline_start();

    /* 7. Tool Registry */
    ret = phoenix_tool_registry_init();
    if (ret != 0) {
        LOG_E(TAG, "❌ Error: Tool registry init failed (%d)!", ret);
        phoenix_voice_pipeline_deinit();
        phoenix_perception_deinit();
        phoenix_expression_deinit();
        phoenix_event_bus_deinit();
        phoenix_store_deinit();
        phoenix_config_deinit();
        hal_deinit();
        return ret;
    }

    /* 8. Built-in Embodied Tools */
    if (reg_tools) {
        phoenix_register_builtin_tools();
    }

    /* 9. LLM Channel Provider (优先尝试从持久化配置文件回读 API Key 与 Model) */
    char saved_key[128] = {0};
    if (api_key && strlen(api_key) > 0) {
        strncpy(saved_key, api_key, sizeof(saved_key) - 1);
    } else {
        phoenix_config_get_str(PHOENIX_CFG_API_KEY, "", saved_key, sizeof(saved_key));
    }

    if (saved_key[0] != '\0') {
        phoenix_llm_config_t llm_cfg;
        memset(&llm_cfg, 0, sizeof(llm_cfg));
        strncpy(llm_cfg.api_key, saved_key, sizeof(llm_cfg.api_key) - 1);
        char model_buf[64] = {0};
        phoenix_config_get_str(PHOENIX_CFG_MODEL, "deepseek-chat", model_buf, sizeof(model_buf));
        strncpy(llm_cfg.model_name, model_buf, sizeof(llm_cfg.model_name) - 1);
        char url_buf[PHOENIX_MAX_URL_LEN] = {0};
        phoenix_config_get_str(PHOENIX_CFG_BASE_URL, "https://api.deepseek.com/v1/chat/completions", url_buf, sizeof(url_buf));
        strncpy(llm_cfg.base_url, url_buf, sizeof(llm_cfg.base_url) - 1);
        llm_cfg.temperature = 70;
        phoenix_llm_provider_init(&llm_cfg);
        LOG_I(TAG, "🔑 已从配置载入 API Key (%zu 字节), 模型: %s, Endpoint: %s",
              strlen(saved_key), model_buf, url_buf);
    } else {
        phoenix_llm_provider_init(NULL);
        LOG_I(TAG, "💡 本地未配置 API Key, 初始化离线具身仿真驱动模式");
    }

    /* 9.1 ASR (Speech-to-Text) Provider 初始化 */
    char asr_saved_key[128] = {0};
    phoenix_config_get_str(PHOENIX_CFG_ASR_API_KEY, "", asr_saved_key, sizeof(asr_saved_key));
    if (asr_saved_key[0] == '\0' && saved_key[0] != '\0') {
        /* 未独立配置 ASR Key 时复用 LLM Key */
        strncpy(asr_saved_key, saved_key, sizeof(asr_saved_key) - 1);
    }
    phoenix_asr_config_t asr_cfg;
    memset(&asr_cfg, 0, sizeof(asr_cfg));
    strncpy(asr_cfg.api_key, asr_saved_key, sizeof(asr_cfg.api_key) - 1);
    phoenix_config_get_str(PHOENIX_CFG_ASR_BACKEND, "cloud", asr_cfg.backend, sizeof(asr_cfg.backend));
    phoenix_config_get_str(PHOENIX_CFG_ASR_BASE_URL, "https://api.groq.com/openai/v1/audio/transcriptions", asr_cfg.base_url, sizeof(asr_cfg.base_url));
    phoenix_config_get_str(PHOENIX_CFG_ASR_MODEL, "whisper-large-v3", asr_cfg.model_name, sizeof(asr_cfg.model_name));
    phoenix_asr_init(&asr_cfg);

    /* 10. Embedded Web Portal (Optional) */
    if (config && config->enable_web_portal) {
        phoenix_web_portal_start(config->web_port, NULL);
    }

    /* 10.1 声学实验室与音频测试服务初始化 */
    audio_test_service_init();

    /* 11. Cartridge Plugin & Orchestrator Engine (注册 5 大卡带，以灵眸具身智能体为首发默认) */
    cartridge_mgr_init(NULL);
    cartridge_agent_register();
    cartridge_home_register();
    cartridge_clock_register();
    cartridge_zen_register();
    cartridge_memo_register();
    cartridge_mgr_switch_to("agent");

    g_app_initialized = true;
    LOG_I(TAG, "✅ All Phoenix subsystems initialized successfully (5 Cartridges active, Default: agent).");
    return 0;
}

void phoenix_app_tick(void)
{
    if (!g_app_initialized) return;
    phoenix_event_bus_drain();
    phoenix_voice_pipeline_tick();
    audio_test_service_tick();
    phoenix_store_flush();
    phoenix_web_portal_drain_commands();

    uint64_t now_ms = time_utils_get_ms();

    /* 1Hz (1000ms) 周期性刷新日志与标准输出缓冲，避免每 5ms 主循环高频调用 fflush(stdout) */
    static uint64_t s_last_log_flush_ms = 0;
    if (now_ms - s_last_log_flush_ms >= 1000) {
        s_last_log_flush_ms = now_ms;
        phoenix_log_flush();
    }

    /* 20Hz (50ms) 具身环境与微敲击感知主循环驱动 */
    static uint64_t s_last_percept_ms = 0;
    if (now_ms - s_last_percept_ms >= 50) {
        s_last_percept_ms = now_ms;
        phoenix_perception_step();
    }

    /* 1s heart beat tick dispatch for active cartridge & background services */
    static uint32_t s_last_tick_sec = 0;
    uint32_t now_sec = (uint32_t)(now_ms / 1000);
    if (now_sec != s_last_tick_sec) {
        s_last_tick_sec = now_sec;
        pomodoro_service_tick_1s();
        cartridge_mgr_dispatch_tick_1s();
    }
}

void phoenix_app_deinit(void)
{
    if (!g_app_initialized) return;

    LOG_I(TAG, "🔄 De-initializing Phoenix Subsystems...");

    cartridge_mgr_deinit();
    audio_test_service_deinit();
    net_mgr_deinit();
    phoenix_web_portal_stop();
    phoenix_voice_pipeline_deinit();
    phoenix_asr_deinit();
    phoenix_llm_provider_deinit();
    phoenix_tool_registry_deinit();
    phoenix_perception_deinit();
    phoenix_expression_deinit();
    phoenix_event_bus_deinit();
    phoenix_store_deinit();
    phoenix_config_deinit();
    hal_deinit();

    g_app_initialized = false;
    LOG_I(TAG, "🏁 Subsystems teardown complete.");
    phoenix_log_deinit();
}

bool phoenix_app_is_initialized(void)
{
    return g_app_initialized;
}
