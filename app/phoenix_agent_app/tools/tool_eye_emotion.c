/**
 * @file tool_eye_emotion.c
 * @brief Living Cyber-Eye Emotion Control Tool Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "core/tool_registry.h"
#include "core/event_bus.h"
#include "ui/eye_anim.h"
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

static int tool_eye_emotion_exec(const char *args_json, char *result_out, size_t max_len)
{
    char emotion_str[32] = "happy";
    if (args_json && strlen(args_json) > 0) {
        cJSON *root = cJSON_Parse(args_json);
        if (root) {
            cJSON *e = cJSON_GetObjectItem(root, "emotion");
            if (e && e->valuestring) {
                strncpy(emotion_str, e->valuestring, sizeof(emotion_str) - 1);
                emotion_str[sizeof(emotion_str) - 1] = '\0';
            }
            cJSON_Delete(root);
        }
    }

    int state_val = 0;
    if (strcmp(emotion_str, "listening") == 0) {
        state_val = 1;
    } else if (strcmp(emotion_str, "thinking") == 0) {
        state_val = 2;
    } else if (strcmp(emotion_str, "happy") == 0) {
        state_val = 3;
    } else if (strcmp(emotion_str, "alert") == 0) {
        state_val = 6;
    } else {
        state_val = 0; /* idle */
    }

    phoenix_event_data_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_STATE_CHANGED;
    evt.data.state.new_state = state_val;
    evt.data.state.message = "灵眸情绪色彩已随对话意图切换";
    phoenix_event_publish(&evt);

    if (result_out && max_len > 0) {
        snprintf(result_out, max_len, "{\"success\":true,\"current_emotion\":\"%s\"}", emotion_str);
    }
    return 0;
}

const phoenix_tool_desc_t g_tool_eye_emotion = {
    .name = "set_eye_emotion",
    .description = "控制桌面灵眸眼球微表情色彩与生命感状态（如开心、思考、警戒、倾听或待命）",
    .parameters_schema = "{\"type\":\"object\",\"properties\":{\"emotion\":{\"type\":\"string\",\"enum\":[\"idle\",\"listening\",\"thinking\",\"happy\",\"alert\"],\"description\":\"情绪类型\"}},\"required\":[\"emotion\"]}",
    .execute = tool_eye_emotion_exec
};
