/**
 * @file tool_launch_app.c
 * @brief OpenVela System App Launcher Tool Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "core/tool_registry.h"
#include "core/event_bus.h"
#include "hal/hal_system.h"
#if defined(__has_include) && __has_include("utils/log_utils.h")
#  include "utils/log_utils.h"
#else
#  include "../utils/log_utils.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "PhoenixTool"

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

typedef struct {
    const char *alias;
    const char *package_name;
    const char *display_name;
} app_mapping_t;

/* OpenVela system application mapping table (aligned with ai_chat launcher apps) */
static const app_mapping_t g_app_mappings[] = {
    {"日历",       "com.application.x4b.calendar",      "系统日历"},
    {"计算器",     "com.vela.xmsdemo.calculator",       "简易计算器"},
    {"亲戚计算器", "com.vela.xmsdemo.relation_calculator", "亲戚称谓计算器"},
    {"会议录音",   "com.vela.system.meeting",           "会议录音纪要"},
    {"录音",       "com.vela.system.meeting",           "会议录音纪要"},
    {"会议纪要",   "com.openvela.meeting",              "智能会议纪要"},
    {"白噪音",     "com.application.x4b.whitenoise",    "白噪音专注助眠"},
    {"雨声",       "com.application.x4b.whitenoise",    "白噪音专注助眠"},
    {"助眠",       "com.application.x4b.whitenoise",    "白噪音专注助眠"},
    {"翻译",       "com.system.translation",            "实时多语种翻译"},
    {"音乐播放器", "com.vela.xmsdemo.music_player",     "音乐播放器"},
    {"音乐",       "com.vela.xmsdemo.music_player",     "音乐播放器"},
    {"电子宠物",   "com.vela.xmsdemo.pet",              "桌面电子宠物"},
    {"塔罗牌",     "com.vela.xmsdemo.tarot",            "塔罗占卜"},
    {"设置",       "com.application.x4b.settings",      "系统控制中心"},
    {"时钟",       "com.application.x4b.clock",         "时钟闹钟"},
    {"闹钟",       "com.application.x4b.clock",         "时钟闹钟"},
    {"打地鼠",     "com.vela.xmsdemo.whackmole",        "解压打地鼠游戏"},
    {NULL, NULL, NULL}
};

static const app_mapping_t* find_app(const char *target)
{
    if (!target) return NULL;
    for (int i = 0; g_app_mappings[i].alias != NULL; i++) {
        if (strstr(target, g_app_mappings[i].alias) != NULL ||
            strstr(g_app_mappings[i].alias, target) != NULL) {
            return &g_app_mappings[i];
        }
    }
    return NULL;
}

static int tool_launch_app_exec(const char *args_json, char *result_out, size_t max_len)
{
    char target_app_name[64] = {0};

    if (args_json && strlen(args_json) > 0) {
        cJSON *root = cJSON_Parse(args_json);
        if (root) {
            cJSON *item = cJSON_GetObjectItem(root, "app_name");
            if (item && item->valuestring) {
                strncpy(target_app_name, item->valuestring, sizeof(target_app_name) - 1);
            }
            cJSON_Delete(root);
        }
    }

    if (strlen(target_app_name) == 0) {
        strncpy(target_app_name, "日历", sizeof(target_app_name) - 1);
    }

    const app_mapping_t *app = find_app(target_app_name);

    if (app) {
        LOG_I(TAG, "🚀 Launching OpenVela App: %s (%s)",
              app->display_name, app->package_name);

        /* Dispatch real app launch via HAL System Service */
        hal_system_launch_app(app->package_name);

        if (result_out && max_len > 0) {
            snprintf(result_out, max_len,
                     "{\"success\":true,\"app_name\":\"%s\",\"package\":\"%s\",\"status\":\"launched\"}",
                     app->display_name, app->package_name);
        }

        /* Show HUD Flying Text */
        char fly_buf[64];
        snprintf(fly_buf, sizeof(fly_buf), "启动应用: %s", app->alias);

        phoenix_event_data_t fly_evt;
        memset(&fly_evt, 0, sizeof(fly_evt));
        fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
        fly_evt.data.flying_text.text = fly_buf;
        fly_evt.data.flying_text.color_rgb = 0x00e5ff;
        phoenix_event_publish(&fly_evt);

        /* Publish tool triggered event */
        phoenix_event_data_t tool_evt;
        memset(&tool_evt, 0, sizeof(tool_evt));
        tool_evt.type = PHOENIX_EVT_TOOL_TRIGGERED;
        tool_evt.data.tool.tool_name = "launch_system_app";
        tool_evt.data.tool.result_summary = app->display_name;
        tool_evt.data.tool.success = true;
        phoenix_event_publish(&tool_evt);
        return 0;
    } else {
        LOG_W(TAG, "⚠️ App not found in mapping: \"%s\"", target_app_name);
        if (result_out && max_len > 0) {
            snprintf(result_out, max_len,
                     "{\"success\":false,\"error\":\"未找到应用\",\"query\":\"%s\","
                     "\"available\":[\"日历\",\"计算器\",\"会议录音\",\"白噪音\",\"翻译\",\"音乐播放器\",\"电子宠物\",\"设置\"]}",
                     target_app_name);
        }
        return 0;
    }
}

const phoenix_tool_desc_t g_tool_launch_app = {
    .name = "launch_system_app",
    .description = "在 OpenVela 嵌入式系统上调起或切换系统级原生应用（如：日历、计算器、会议录音、白噪音、翻译、音乐播放器、电子宠物、设置等）",
    .parameters_schema = "{\"type\":\"object\",\"properties\":{\"app_name\":{\"type\":\"string\",\"description\":\"需要启动的应用名称，如：日历、计算器、会议录音、白噪音、翻译、音乐播放器、设置、电子宠物\"}},\"required\":[\"app_name\"]}",
    .execute = tool_launch_app_exec
};
