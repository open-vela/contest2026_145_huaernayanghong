/**
 * @file tool_system_health.c
 * @brief System Diagnostic & Hardware Metric Tool Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "core/tool_registry.h"
#include "core/event_bus.h"
#include "hal/hal_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tool_system_health_exec(const char *args_json, char *result_out, size_t max_len)
{
    (void)args_json;

    /* Gather dynamic system health status via HAL */
    hal_system_telemetry_t telem;
    memset(&telem, 0, sizeof(telem));
    hal_system_get_telemetry(&telem);

    if (result_out && max_len > 0) {
        snprintf(result_out, max_len,
                 "{\"board\":\"%s\",\"os\":\"%s\",\"status\":\"healthy\","
                 "\"temperature_c\":%.1f,\"cpu_freq_mhz\":%u,\"mem_total_kb\":%u,"
                 "\"mem_free_kb\":%u,\"mem_used_pct\":%u,\"uptime_s\":%llu}",
                 telem.board_model, telem.os_version, telem.cpu_temperature_c,
                 telem.cpu_freq_mhz, telem.mem_total_kb, telem.mem_free_kb,
                 telem.mem_used_pct, (unsigned long long)telem.uptime_seconds);
    }

    phoenix_event_data_t fly_evt;
    memset(&fly_evt, 0, sizeof(fly_evt));
    fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
    fly_evt.data.flying_text.text = "系统自检健康";
    fly_evt.data.flying_text.color_rgb = 0x00e676;
    phoenix_event_publish(&fly_evt);

    return 0;
}

const phoenix_tool_desc_t g_tool_system_health = {
    .name = "query_system_health",
    .description = "查询 OpenVela 嵌入式硬件平台运行状态、核心温度、内存健康度及架构信息",
    .parameters_schema = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
    .execute = tool_system_health_exec
};
