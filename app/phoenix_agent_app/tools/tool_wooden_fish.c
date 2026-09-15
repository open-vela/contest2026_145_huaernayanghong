/**
 * @file tool_wooden_fish.c
 * @brief Cyber Wooden Fish & Merit Tool Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "core/tool_registry.h"
#include "core/event_bus.h"
#include "core/store.h"
#include "hal/hal_actuator.h"
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

static int tool_wooden_fish_exec(const char *args_json, char *result_out, size_t max_len)
{
    int count = 1;
    if (args_json && strlen(args_json) > 0) {
        cJSON *root = cJSON_Parse(args_json);
        if (root) {
            cJSON *c = cJSON_GetObjectItem(root, "count");
            if (c && c->type == cJSON_Number && c->valueint > 0) {
                count = c->valueint;
            }
            cJSON_Delete(root);
        }
    }

    uint32_t total_merit = phoenix_store_add_merit((uint32_t)count);

    /* 1. Trigger tactile haptic feedback & play wooden fish sound */
    hal_actuator_trigger_haptic(HAL_HAPTIC_CLICK);

    phoenix_event_data_t snd_evt;
    memset(&snd_evt, 0, sizeof(snd_evt));
    snd_evt.type = PHOENIX_EVT_PLAY_SOUND;
    snd_evt.data.sound.sound_id = PHOENIX_SOUND_WOODEN_FISH;
    phoenix_event_publish(&snd_evt);

    /* 2. Publish flying text */
    phoenix_event_data_t fly_evt;
    memset(&fly_evt, 0, sizeof(fly_evt));
    fly_evt.type = PHOENIX_EVT_FLYING_TEXT;

    static char fly_buf[64];
    if (total_merit % 10 == 0) {
        snprintf(fly_buf, sizeof(fly_buf), "功德大圆满 +%d (总计:%lu)", count, (unsigned long)total_merit);
        fly_evt.data.flying_text.text = fly_buf;
        fly_evt.data.flying_text.color_rgb = 0xffd700; /* Gold */

        /* Celebrate sound */
        snd_evt.data.sound.sound_id = PHOENIX_SOUND_CELEBRATE;
        phoenix_event_publish(&snd_evt);
    } else {
        snprintf(fly_buf, sizeof(fly_buf), "功德 +%d / PR +%d", count, count);
        fly_evt.data.flying_text.text = fly_buf;
        fly_evt.data.flying_text.color_rgb = 0x00e5ff; /* Cyan */
    }
    phoenix_event_publish(&fly_evt);

    /* 3. Publish merit update */
    phoenix_event_data_t stat_evt;
    memset(&stat_evt, 0, sizeof(stat_evt));
    stat_evt.type = PHOENIX_EVT_MERIT_UPDATED;
    stat_evt.data.stats.total_merit = total_merit;
    phoenix_event_publish(&stat_evt);

    if (result_out && max_len > 0) {
        snprintf(result_out, max_len,
                 "{\"success\":true,\"merit_added\":%d,\"total_merit\":%lu}",
                 count, (unsigned long)total_merit);
    }
    return 0;
}

const phoenix_tool_desc_t g_tool_wooden_fish = {
    .name = "knock_wooden_fish",
    .description = "敲击桌面木鱼为主人积累赛博功德，触发飞字动效与清脆木鱼音效",
    .parameters_schema = "{\"type\":\"object\",\"properties\":{\"count\":{\"type\":\"integer\",\"description\":\"敲击次数，默认为1\"}},\"required\":[]}",
    .execute = tool_wooden_fish_exec
};
