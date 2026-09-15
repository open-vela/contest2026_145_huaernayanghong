/**
 * @file hal_system.c
 * @brief Phoenix HoloDesk-S1 System Capability HAL Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "hal_system.h"
#include "hal_driver.h"
#include <stdio.h>
#include <string.h>

int hal_system_init(void)
{
    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->system_ops.init) {
        return drv->system_ops.init();
    }
    return 0;
}

int hal_system_deinit(void)
{
    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->system_ops.deinit) {
        return drv->system_ops.deinit();
    }
    return 0;
}

int hal_system_get_telemetry(hal_system_telemetry_t *out_telem)
{
    if (!out_telem) return -1;
    memset(out_telem, 0, sizeof(*out_telem));

    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->system_ops.get_telemetry) {
        return drv->system_ops.get_telemetry(out_telem);
    }

    /* Fallback default telemetry */
    strncpy(out_telem->board_model, "OpenVela Gemini-S1 (Generic HAL)", sizeof(out_telem->board_model) - 1);
    strncpy(out_telem->os_version, "OpenVela OS v1.0", sizeof(out_telem->os_version) - 1);
    out_telem->cpu_temperature_c = 42.0f;
    out_telem->cpu_freq_mhz = 1200;
    out_telem->mem_total_kb = 128 * 1024;
    out_telem->mem_free_kb = 64 * 1024;
    out_telem->mem_used_pct = 50;
    out_telem->uptime_seconds = 3600;
    return 0;
}

int hal_system_launch_app(const char *app_package_or_alias)
{
    if (!app_package_or_alias || strlen(app_package_or_alias) == 0) return -1;

    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->system_ops.launch_app) {
        return drv->system_ops.launch_app(app_package_or_alias);
    }

    printf("[HAL:System] 🚀 Launch native app: %s\n", app_package_or_alias);
    return 0;
}

const char* hal_system_get_storage_base_path(void)
{
    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->system_ops.get_storage_base_path) {
        return drv->system_ops.get_storage_base_path();
    }
    return "/data/phoenix";
}
