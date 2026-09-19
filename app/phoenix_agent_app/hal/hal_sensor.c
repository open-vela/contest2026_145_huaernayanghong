/**
 * @file hal_sensor.c
 * @brief Phoenix HoloDesk-S1 Sensor HAL Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "hal_sensor.h"
#include "hal_driver.h"
#include <stdio.h>
#include <string.h>

int hal_sensor_init(void)
{
    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->sensor_ops.init) {
        return drv->sensor_ops.init();
    }
    return 0;
}

int hal_sensor_deinit(void)
{
    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->sensor_ops.deinit) {
        return drv->sensor_ops.deinit();
    }
    return 0;
}

bool hal_sensor_poll_tap(hal_tap_event_t *out_tap)
{
    if (!out_tap) return false;
    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->sensor_ops.poll_tap) {
        return drv->sensor_ops.poll_tap(out_tap);
    }
    return false;
}

int hal_sensor_read_light(hal_light_data_t *out_light)
{
    if (!out_light) return -1;
    memset(out_light, 0, sizeof(*out_light));
    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->sensor_ops.read_light) {
        return drv->sensor_ops.read_light(out_light);
    }
    /* Default fallback: 300 Lux indoor normal */
    out_light->lux = 300;
    out_light->is_dark_environment = false;
    out_light->is_direct_sunlight = false;
    return 0;
}

int hal_sensor_read_battery(hal_battery_data_t *out_battery)
{
    if (!out_battery) return -1;
    memset(out_battery, 0, sizeof(*out_battery));
    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->sensor_ops.read_battery) {
        return drv->sensor_ops.read_battery(out_battery);
    }
    /* Default fallback: 85% full, charging */
    out_battery->percentage = 85;
    out_battery->is_charging = true;
    out_battery->is_low_power = false;
    out_battery->voltage_mv = 4050;
    return 0;
}

int hal_sensor_read_env(hal_env_data_t *out_env)
{
    if (!out_env) return -1;
    memset(out_env, 0, sizeof(*out_env));
    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->sensor_ops.read_env) {
        return drv->sensor_ops.read_env(out_env);
    }
    /* Default fallback: 26.0℃, 60%RH indoor comfortable baseline */
    out_env->temperature_c = 26.0f;
    out_env->humidity_pct = 60.0f;
    out_env->is_valid = false;
    return 0;
}
