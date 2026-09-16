#include "tools.h"
#if defined(__has_include) && __has_include("core/tool_registry.h")
#  include "core/tool_registry.h"
#  include "core/event_bus.h"
#  include "core/store.h"
#  include "hal/hal_actuator.h"
#else
#  include "../core/tool_registry.h"
#  include "../core/event_bus.h"
#  include "../core/store.h"
#  include "../hal/hal_actuator.h"
#endif
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

static bool     g_pomodoro_active = false;
static uint16_t g_remaining_s = 0;
static uint16_t g_total_duration_s = 25 * 60;

void pomodoro_service_tick_1s(void)
{
    if (!g_pomodoro_active) return;

    if (g_remaining_s > 0) {
        g_remaining_s--;

        /* 广播 tick 事件通知 UI 与微胶囊 */
        phoenix_event_data_t tick_evt;
        memset(&tick_evt, 0, sizeof(tick_evt));
        tick_evt.type = PHOENIX_EVT_POMODORO_TICK;
        tick_evt.data.stats.is_active = true;
        tick_evt.data.stats.remaining_s = g_remaining_s;
        phoenix_event_publish(&tick_evt);

        if (g_remaining_s == 0) {
            g_pomodoro_active = false;

            /* 累加持久化数据 */
            phoenix_store_add_pomodoro(g_total_duration_s);

            /* 触觉微马达脉冲与庆祝提示音 */
            hal_actuator_trigger_haptic(HAL_HAPTIC_PULSE);
            hal_actuator_play_sound(HAL_SOUND_CELEBRATE);

            /* 广播全局飞字动效 */
            phoenix_event_data_t fly_evt;
            memset(&fly_evt, 0, sizeof(fly_evt));
            fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
            fly_evt.data.flying_text.text = "🎉 专注达成！灵眸提醒休息片刻";
            fly_evt.data.flying_text.color_rgb = 0xffd700;
            phoenix_event_publish(&fly_evt);

            /* 广播最终完成事件 */
            memset(&tick_evt, 0, sizeof(tick_evt));
            tick_evt.type = PHOENIX_EVT_POMODORO_TICK;
            tick_evt.data.stats.is_active = false;
            tick_evt.data.stats.remaining_s = 0;
            phoenix_event_publish(&tick_evt);
        }
    }
}

bool pomodoro_service_is_active(void)
{
    return g_pomodoro_active;
}

uint16_t pomodoro_service_get_remaining(void)
{
    return g_remaining_s;
}

int pomodoro_service_start(uint16_t duration_minutes)
{
    if (duration_minutes == 0) duration_minutes = 25;
    g_pomodoro_active = true;
    g_total_duration_s = duration_minutes * 60;
    g_remaining_s = g_total_duration_s;

    /* 启动时触觉微震动与提示音 */
    hal_actuator_trigger_haptic(HAL_HAPTIC_CLICK);
    hal_actuator_play_sound(HAL_SOUND_WOODEN_FISH);

    phoenix_event_data_t tick_evt;
    memset(&tick_evt, 0, sizeof(tick_evt));
    tick_evt.type = PHOENIX_EVT_POMODORO_TICK;
    tick_evt.data.stats.is_active = true;
    tick_evt.data.stats.remaining_s = g_remaining_s;
    phoenix_event_publish(&tick_evt);
    return 0;
}

int pomodoro_service_stop(void)
{
    g_pomodoro_active = false;
    g_remaining_s = 0;

    phoenix_event_data_t tick_evt;
    memset(&tick_evt, 0, sizeof(tick_evt));
    tick_evt.type = PHOENIX_EVT_POMODORO_TICK;
    tick_evt.data.stats.is_active = false;
    tick_evt.data.stats.remaining_s = 0;
    phoenix_event_publish(&tick_evt);
    return 0;
}

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
        pomodoro_service_stop();
        if (result_out && max_len > 0) {
            snprintf(result_out, max_len, "{\"status\":\"stopped\"}");
        }
    } else {
        pomodoro_service_start((uint16_t)minutes);
        if (result_out && max_len > 0) {
            snprintf(result_out, max_len, "{\"status\":\"started\",\"remaining_seconds\":%u}", g_remaining_s);
        }
    }

    return 0;
}

const phoenix_tool_desc_t g_tool_pomodoro = {
    .name = "manage_pomodoro",
    .description = "管理桌面开发者沉浸专注流番茄钟（支持开启、暂停或指定专注时长）",
    .parameters_schema = "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"start\",\"stop\"],\"description\":\"动作: start 或 stop\"},\"duration_minutes\":{\"type\":\"integer\",\"description\":\"专注时长(分钟)，默认为25\"}},\"required\":[]}",
    .execute = tool_pomodoro_exec
};
