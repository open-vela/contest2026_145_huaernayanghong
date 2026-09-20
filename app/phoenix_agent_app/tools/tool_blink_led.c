/**
 * @file tool_blink_led.c
 * @brief Board Physical LED Blink & Control Tool Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "tools.h"
#if defined(__has_include) && __has_include("core/tool_registry.h")
#  include "core/tool_registry.h"
#  include "core/event_bus.h"
#  include "hal/hal_actuator.h"
#else
#  include "../core/tool_registry.h"
#  include "../core/event_bus.h"
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
#  elif __has_include(<cJSON.h>)
#    include <cJSON.h>
#  elif __has_include("../../../../apps/netutils/cjson/cJSON/cJSON.h")
#    include "../../../../apps/netutils/cjson/cJSON/cJSON.h"
#  elif __has_include("../../../../../apps/netutils/cjson/cJSON/cJSON.h")
#    include "../../../../../apps/netutils/cjson/cJSON/cJSON.h"
#  else
#    include <cJSON.h>
#  endif
#else
#  include <netutils/cJSON.h>
#endif

static uint32_t parse_color_rgb(const char *color_str)
{
    if (!color_str) return 0xffd700; /* 默认金色 */
    if (strstr(color_str, "red") || strstr(color_str, "红")) return 0xff5252;
    if (strstr(color_str, "green") || strstr(color_str, "绿")) return 0x00e676;
    if (strstr(color_str, "blue") || strstr(color_str, "蓝")) return 0x448aff;
    if (strstr(color_str, "cyan") || strstr(color_str, "青")) return 0x00e5ff;
    if (strstr(color_str, "purple") || strstr(color_str, "紫")) return 0x7c4dff;
    if (strstr(color_str, "gold") || strstr(color_str, "黄") || strstr(color_str, "金")) return 0xffd700;

    if (color_str[0] == '#') {
        unsigned int hex = 0;
        if (sscanf(color_str + 1, "%x", &hex) == 1) {
            return (uint32_t)hex;
        }
    }
    return 0xffd700;
}

static int tool_blink_led_exec(const char *args_json, char *result_out, size_t max_len)
{
    int count = 3;
    uint32_t interval_ms = 200;
    char state_str[16] = "blink";
    char color_str[32] = "gold";

    if (args_json && strlen(args_json) > 0) {
        cJSON *root = cJSON_Parse(args_json);
        if (root) {
            cJSON *c = cJSON_GetObjectItem(root, "count");
            if (c && c->type == cJSON_Number && c->valueint > 0) {
                count = c->valueint;
                if (count > 30) count = 30; /* 保护上限 */
            }

            cJSON *inter = cJSON_GetObjectItem(root, "interval_ms");
            if (inter && inter->type == cJSON_Number && inter->valueint > 0) {
                interval_ms = (uint32_t)inter->valueint;
                if (interval_ms < 50) interval_ms = 50;
                if (interval_ms > 2000) interval_ms = 2000;
            }

            cJSON *s = cJSON_GetObjectItem(root, "state");
            if (s && s->valuestring) {
                strncpy(state_str, s->valuestring, sizeof(state_str) - 1);
                state_str[sizeof(state_str) - 1] = '\0';
            }

            cJSON *col = cJSON_GetObjectItem(root, "color");
            if (col && col->valuestring) {
                strncpy(color_str, col->valuestring, sizeof(color_str) - 1);
                color_str[sizeof(color_str) - 1] = '\0';
            }

            cJSON_Delete(root);
        }
    }

    uint32_t color_rgb = parse_color_rgb(color_str);

    /* 1. 硬件外设控制与状态分发 */
    static char fly_buf[64];
    if (strcmp(state_str, "off") == 0) {
        hal_actuator_set_led(HAL_LED_OFF, 0, 0);
        snprintf(fly_buf, sizeof(fly_buf), "💡 LED指示灯已熄灭");
    } else if (strcmp(state_str, "on") == 0) {
        hal_actuator_set_led(HAL_LED_SOLID, color_rgb, 100);
        snprintf(fly_buf, sizeof(fly_buf), "💡 LED指示灯已常亮");
    } else {
        /* 默认为闪烁模式 */
        hal_actuator_blink_led(count, interval_ms);
        snprintf(fly_buf, sizeof(fly_buf), "💡 LED闪烁 %d 次 (%u ms)", count, (unsigned)interval_ms);
    }

    /* 2. 联动发布 UI 飞字动效与清脆点击音效 */
    phoenix_event_data_t snd_evt;
    memset(&snd_evt, 0, sizeof(snd_evt));
    snd_evt.type = PHOENIX_EVT_PLAY_SOUND;
    snd_evt.data.sound.sound_id = PHOENIX_SOUND_CLICK;
    phoenix_event_publish(&snd_evt);

    phoenix_event_data_t fly_evt;
    memset(&fly_evt, 0, sizeof(fly_evt));
    fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
    fly_evt.data.flying_text.text = fly_buf;
    fly_evt.data.flying_text.color_rgb = color_rgb;
    phoenix_event_publish(&fly_evt);

    /* 3. 构造给大模型的结构化 Observation 文本 */
    if (result_out && max_len > 0) {
        snprintf(result_out, max_len,
                 "{\"success\":true,\"state\":\"%s\",\"count\":%d,\"interval_ms\":%u,\"message\":\"%s\"}",
                 state_str, (strcmp(state_str, "blink") == 0 ? count : 0),
                 interval_ms, fly_buf);
    }
    return 0;
}

const phoenix_tool_desc_t g_tool_blink_led = {
    .name = "blink_led",
    .description = "控制开发板硬件 LED 物理指示灯闪烁或状态切换，用于通知提醒、报警、打招呼或心跳展示。支持设置闪烁次数(count，默认3次)、亮灭间隔(interval_ms，默认200毫秒)及状态模式(state: blink/on/off)。",
    .parameters_schema = "{\"type\":\"object\",\"properties\":{\"count\":{\"type\":\"integer\",\"description\":\"闪烁次数，默认为3次\"},\"interval_ms\":{\"type\":\"integer\",\"description\":\"亮灭时间间隔(毫秒)，默认为200毫秒\"},\"state\":{\"type\":\"string\",\"enum\":[\"blink\",\"on\",\"off\"],\"description\":\"LED状态模式，默认为blink(闪烁)，亦可选on(常亮)或off(熄灭)\"},\"color\":{\"type\":\"string\",\"description\":\"灯光或UI动效颜色，如'#00E5FF'、'gold'、'red'\"}},\"required\":[]}",
    .execute = tool_blink_led_exec
};
