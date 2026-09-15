/**
 * @file tool_pomodoro.c
 * @brief Pomodoro Deep Focus Timer Tool Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "core/tool_registry.h"
#include "core/event_bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_include)
#  if __has_include(<netutils/cJSON.h>)
#    include <netutils/cJSON.h>
#  elif __has_include(<cJSON/cJSON.h>)
#    include <cJSON/cJSON.h>
#  elif __has_include(<cjson/cJSON.h>)
#    include <cjson/cJSON.h>
#  else
#    include <cJSON.h>
#  endif
#else
#  include <netutils/cJSON.h>
#endif

static bool g_pomodoro_active = false;
static uint16_t g_remaining_s = 0;

static int tool_pomodoro_exec(const char *args_json, char *result_out, size_t max_len)
{
    char action[32] = "start";
    int minutes = 25;

    if (args_json && strlen(args_json) > 0) {
        cJSON *root = cJSON_Parse(args_json);
        if (root) {
            cJSON *a = cJSON_GetObjectItem(root, "action");
            if (a && a->valuestring) {
                strncpy(action, a->valuestring, sizeof(action) - 1);
                action[sizeof(action) - 1] = '\0';
            }
            cJSON *m = cJSON_GetObjectItem(root, "duration_minutes");
            if (m && m->type == cJSON_Number && m->valueint > 0) {
                minutes = m->valueint;
            }
            cJSON_Delete(root);
        }
    }

    if (strcmp(action, "stop") == 0) {
        g_pomodoro_active = false;
        g_remaining_s = 0;

        if (result_out && max_len > 0) {
            snprintf(result_out, max_len, "{\"status\":\"stopped\"}");
        }
    } else {
        g_pomodoro_active = true;
        g_remaining_s = (uint16_t)(minutes * 60);

        if (result_out && max_len > 0) {
            snprintf(result_out, max_len, "{\"status\":\"started\",\"remaining_seconds\":%u}", g_remaining_s);
        }
    }

    /* Publish tick event for UI update */
    phoenix_event_data_t tick_evt;
    memset(&tick_evt, 0, sizeof(tick_evt));
    tick_evt.type = PHOENIX_EVT_POMODORO_TICK;
    tick_evt.data.stats.is_active = g_pomodoro_active;
    tick_evt.data.stats.remaining_s = g_remaining_s;
    phoenix_event_publish(&tick_evt);

    return 0;
}

const phoenix_tool_desc_t g_tool_pomodoro = {
    .name = "manage_pomodoro",
    .description = "管理桌面开发者沉浸专注流番茄钟（支持开启、暂停或指定专注时长）",
    .parameters_schema = "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"start\",\"stop\"],\"description\":\"动作: start 或 stop\"},\"duration_minutes\":{\"type\":\"integer\",\"description\":\"专注时长(分钟)，默认为25\"}},\"required\":[]}",
    .execute = tool_pomodoro_exec
};
