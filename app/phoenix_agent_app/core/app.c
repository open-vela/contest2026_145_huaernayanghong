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
#include "../cartridges/cartridge_familiar.h"
#include "../cartridges/cartridge_clock.h"
#include "../cartridges/cartridge_zen.h"
#include "../cartridges/cartridge_memo.h"
#include "../cartridges/cartridge_agent.h"
#include "web_portal.h"
#include "../utils/time_utils.h"
#include "../tools/tools.h"
#include "../harness/llm_provider.h"
#include "../hal/hal_manager.h"
#include "../perception/perception.h"
#include "../voice/voice_pipeline.h"
#include "../hal/network_mgr.h"
#include <stdio.h>
#include <string.h>

static bool g_app_initialized = false;

int phoenix_app_init(const phoenix_app_config_t *config)
{
    if (g_app_initialized) {
        printf("[PhoenixApp] Notice: Already initialized, reusing existing context.\n");
        return 0;
    }

    const char *store_dir = (config && config->storage_dir) ? config->storage_dir : "/data/phoenix";
    const char *sounds_dir = (config && config->sounds_dir) ? config->sounds_dir : "/data/sounds";
    const char *api_key = (config && config->api_key) ? config->api_key : NULL;
    bool reg_tools = config ? config->register_tools : true;

    printf("[PhoenixApp] 🚀 Initializing Phoenix HoloDesk-S1 Subsystems...\n");

    /* 0. Hardware Abstraction Layer (HAL) */
    hal_config_t hal_cfg;
    memset(&hal_cfg, 0, sizeof(hal_cfg));
    hal_cfg.sound_data_root = sounds_dir;
    int ret = hal_init(&hal_cfg);
    if (ret != 0) {
        printf("[PhoenixApp] ⚠️ Warning: HAL init returned %d, continuing with defaults.\n", ret);
    }

    /* 1. Configuration Subsystem */
    ret = phoenix_config_init(store_dir);
    if (ret != 0) {
        printf("[PhoenixApp] ⚠️ Warning: Config init returned %d.\n", ret);
    }

    /* 2. Persistent Storage Engine */
    ret = phoenix_store_init(store_dir);
    if (ret != 0) {
        printf("[PhoenixApp] ⚠️ Warning: Store init returned %d, continuing in volatile mode.\n", ret);
    }

    /* 3. Core Event Bus */
    ret = phoenix_event_bus_init();
    if (ret != 0) {
        printf("[PhoenixApp] ❌ Error: Event bus init failed (%d)!\n", ret);
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
        printf("[PhoenixApp] ⚠️ Warning: Perception engine init returned %d.\n", ret);
    }

    /* 6. Voice Pipeline Engine */
    phoenix_voice_pipeline_init(NULL, NULL);
    phoenix_voice_pipeline_start();

    /* 7. Tool Registry */
    ret = phoenix_tool_registry_init();
    if (ret != 0) {
        printf("[PhoenixApp] ❌ Error: Tool registry init failed (%d)!\n", ret);
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

    /* 9. LLM Channel Provider */
    if (api_key && strlen(api_key) > 0) {
        phoenix_llm_config_t llm_cfg;
        memset(&llm_cfg, 0, sizeof(llm_cfg));
        strncpy(llm_cfg.api_key, api_key, sizeof(llm_cfg.api_key) - 1);
        phoenix_llm_provider_init(&llm_cfg);
    } else {
        phoenix_llm_provider_init(NULL);
    }

    /* 10. Embedded Web Portal (Optional) */
    if (config && config->enable_web_portal) {
        phoenix_web_portal_start(config->web_port, NULL);
    }

    /* 11. Cartridge Plugin & Orchestrator Engine (全量注册 6 大卡带) */
    cartridge_mgr_init(NULL);
    cartridge_home_register();
    cartridge_familiar_register();
    cartridge_clock_register();
    cartridge_zen_register();
    cartridge_memo_register();
    cartridge_agent_register();
    cartridge_mgr_switch_to("home");

    g_app_initialized = true;
    printf("[PhoenixApp] ✅ All Phoenix subsystems initialized successfully (6 Cartridges active).\n");
    return 0;
}

void phoenix_app_tick(void)
{
    if (!g_app_initialized) return;
    phoenix_event_bus_drain();
    phoenix_voice_pipeline_tick();
    phoenix_store_flush();
    phoenix_web_portal_drain_commands();

    /* 20Hz (50ms) 具身环境与微敲击感知主循环驱动 */
    static uint64_t s_last_percept_ms = 0;
    uint64_t now_ms = time_utils_get_ms();
    if (now_ms - s_last_percept_ms >= 50) {
        s_last_percept_ms = now_ms;
        phoenix_perception_step();
    }

    /* 1s heart beat tick dispatch for active cartridge */
    static uint32_t s_last_tick_sec = 0;
    uint32_t now_sec = (uint32_t)(now_ms / 1000);
    if (now_sec != s_last_tick_sec) {
        s_last_tick_sec = now_sec;
        cartridge_mgr_dispatch_tick_1s();
    }
}

void phoenix_app_deinit(void)
{
    if (!g_app_initialized) return;

    printf("[PhoenixApp] 🔄 De-initializing Phoenix Subsystems...\n");

    cartridge_mgr_deinit();
    net_mgr_deinit();
    phoenix_web_portal_stop();
    phoenix_voice_pipeline_deinit();
    phoenix_llm_provider_deinit();
    phoenix_tool_registry_deinit();
    phoenix_perception_deinit();
    phoenix_expression_deinit();
    phoenix_event_bus_deinit();
    phoenix_store_deinit();
    phoenix_config_deinit();
    hal_deinit();

    g_app_initialized = false;
    printf("[PhoenixApp] 🏁 Subsystems teardown complete.\n");
}

bool phoenix_app_is_initialized(void)
{
    return g_app_initialized;
}
