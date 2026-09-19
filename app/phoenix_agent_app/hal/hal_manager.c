/**
 * @file hal_manager.c
 * @brief Phoenix HoloDesk-S1 HAL Manager Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "hal_manager.h"
#include "ble_prov_service.h"
#if defined(__has_include) && __has_include("utils/log_utils.h")
#  include "utils/log_utils.h"
#else
#  include "../utils/log_utils.h"
#endif
#include <stdio.h>
#include <string.h>

#define TAG "HAL:Manager"

/* Forward declare platform default drivers */
extern const hal_driver_t g_hal_driver_mock;
extern const hal_driver_t g_hal_driver_openvela;

static const hal_driver_t *g_active_driver = NULL;
static bool g_hal_initialized = false;

int hal_register_driver(const hal_driver_t *driver)
{
    if (!driver) return -1;
    g_active_driver = driver;
    LOG_I(TAG, "Registered active driver: [%s]", driver->driver_name ? driver->driver_name : "Unknown");
    return 0;
}

const hal_driver_t* hal_get_active_driver(void)
{
    return g_active_driver;
}

int hal_init(const hal_config_t *config)
{
    if (g_hal_initialized) {
        LOG_I(TAG, "Notice: HAL already initialized.");
        return 0;
    }

    LOG_I(TAG, "⚡ Initializing Hardware Abstraction Layer (HAL)...");

    /* 1. Determine driver */
    if (config && config->custom_driver) {
        hal_register_driver(config->custom_driver);
    } else if (!g_active_driver) {
#if defined(__NUTTX__) || defined(CONFIG_LVX_USE_DEMO_CONTEST2026_145_PHOENIX_AGENT_APP)
        hal_register_driver(&g_hal_driver_openvela);
#else
        hal_register_driver(&g_hal_driver_mock);
#endif
    }

    const char *sound_root = (config && config->sound_data_root) ? config->sound_data_root : "/data/sounds";

    /* 2. Initialize subsystems */
    hal_system_init();
    hal_sdcard_init();
    hal_actuator_init(sound_root);
    hal_sensor_init();
    /* 蓝牙配网广播默认保持关闭，需用户在设置中手动开启 */
    /* ble_prov_service_init(NULL); */

    g_hal_initialized = true;
    LOG_I(TAG, "✅ HAL Subsystems Initialized Successfully.");
    return 0;
}

void hal_deinit(void)
{
    if (!g_hal_initialized) return;

    LOG_I(TAG, "🔄 De-initializing HAL Subsystems...");
    ble_prov_service_deinit();
    hal_sensor_deinit();
    hal_actuator_deinit();
    hal_sdcard_deinit();
    hal_system_deinit();

    g_hal_initialized = false;
    LOG_I(TAG, "🏁 HAL Subsystems Teardown Complete.");
}

bool hal_is_initialized(void)
{
    return g_hal_initialized;
}
