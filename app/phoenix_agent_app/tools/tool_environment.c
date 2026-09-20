/**
 * @file tool_environment.c
 * @brief Ambient Environment & Sensor Telemetry Tool Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#if defined(__has_include) && __has_include("core/tool_registry.h")
#  include "core/tool_registry.h"
#  include "core/event_bus.h"
#  include "hal/hal_sensor.h"
#else
#  include "../core/tool_registry.h"
#  include "../core/event_bus.h"
#  include "../hal/hal_sensor.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tool_environment_exec(const char *args_json, char *result_out, size_t max_len)
{
    (void)args_json;

    /* 1. 读取环境温湿度 (SHTC3 / 模拟硬件) */
    hal_env_data_t env;
    memset(&env, 0, sizeof(env));
    hal_sensor_read_env(&env);

    /* 2. 读取环境光照度 (Lux) */
    hal_light_data_t light;
    memset(&light, 0, sizeof(light));
    hal_sensor_read_light(&light);

    /* 3. 读取电池与电源状态 */
    hal_battery_data_t bat;
    memset(&bat, 0, sizeof(bat));
    hal_sensor_read_battery(&bat);

    if (result_out && max_len > 0) {
        snprintf(result_out, max_len,
                 "{\"temperature_c\":%.1f,\"humidity_pct\":%.1f,\"env_valid\":%s,"
                 "\"light_lux\":%u,\"is_dark\":%s,\"is_direct_sunlight\":%s,"
                 "\"battery_pct\":%u,\"is_charging\":%s,\"is_low_power\":%s,\"voltage_mv\":%u}",
                 env.temperature_c, env.humidity_pct, env.is_valid ? "true" : "false",
                 light.lux, light.is_dark_environment ? "true" : "false", light.is_direct_sunlight ? "true" : "false",
                 bat.percentage, bat.is_charging ? "true" : "false", bat.is_low_power ? "true" : "false", bat.voltage_mv);
    }

    /* 触发全局轻量飘字动效 */
    phoenix_event_data_t fly_evt;
    memset(&fly_evt, 0, sizeof(fly_evt));
    fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
    fly_evt.data.flying_text.text = "🌡️ 环境传感器采样就绪";
    fly_evt.data.flying_text.color_rgb = 0x00e5ff;
    phoenix_event_publish(&fly_evt);

    return 0;
}

const phoenix_tool_desc_t g_tool_environment = {
    .name = "query_environment",
    .description = "查询当前桌面环境传感器实时读数（环境温度、相对湿度、环境光照度Lux、暗室/强光判定及电源电池状态）",
    .parameters_schema = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
    .execute = tool_environment_exec
};
