/**
 * @file tool_todo.c
 * @brief Todo List Management Tool Implementation for Phoenix Agent
 * @author OpenVela Contest 2026 Team 145
 */

#include "tools.h"
#if defined(__has_include) && __has_include("core/tool_registry.h")
#  include "core/tool_registry.h"
#  include "core/todo_mgr.h"
#  include "core/event_bus.h"
#else
#  include "../core/tool_registry.h"
#  include "../core/todo_mgr.h"
#  include "../core/event_bus.h"
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
#  else
#    include <cJSON.h>
#  endif
#else
#  include <netutils/cJSON.h>
#endif

static int tool_todo_exec(const char *args_json, char *result_out, size_t max_len)
{
    char action[32] = "list";
    char title[TODO_TITLE_MAX] = {0};
    char time_str[TODO_TIME_MAX] = {0};
    int item_id = 0;

    if (args_json && strlen(args_json) > 0) {
        cJSON *root = cJSON_Parse(args_json);
        if (root) {
            cJSON *a = cJSON_GetObjectItem(root, "action");
            if (a && a->valuestring) {
                strncpy(action, a->valuestring, sizeof(action) - 1);
                action[sizeof(action) - 1] = '\0';
            }

            cJSON *t = cJSON_GetObjectItem(root, "title");
            if (t && t->valuestring) {
                strncpy(title, t->valuestring, sizeof(title) - 1);
                title[sizeof(title) - 1] = '\0';
            }

            cJSON *tm = cJSON_GetObjectItem(root, "time_str");
            if (tm && tm->valuestring) {
                strncpy(time_str, tm->valuestring, sizeof(time_str) - 1);
                time_str[sizeof(time_str) - 1] = '\0';
            }

            cJSON *id_obj = cJSON_GetObjectItem(root, "id");
            if (id_obj && id_obj->type == cJSON_Number) {
                item_id = id_obj->valueint;
            }
            cJSON_Delete(root);
        }
    }

    /* 1. 添加待办事项 (add) */
    if (strcmp(action, "add") == 0) {
        if (title[0] == '\0') {
            if (result_out && max_len > 0) {
                snprintf(result_out, max_len, "{\"status\":\"error\",\"message\":\"title不能为空\"}");
            }
            return -1;
        }

        int new_id = todo_mgr_add(title, time_str[0] ? time_str : "今日");
        if (new_id < 0) {
            if (result_out && max_len > 0) {
                snprintf(result_out, max_len, "{\"status\":\"error\",\"message\":\"待办列表已满或添加失败\"}");
            }
            return -1;
        }

        size_t done_cnt = 0;
        size_t total_cnt = todo_mgr_get_counts(&done_cnt);

        /* 触发全局飞字动效 */
        phoenix_event_data_t fly_evt;
        memset(&fly_evt, 0, sizeof(fly_evt));
        fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
        fly_evt.data.flying_text.text = "📝 待办已添加";
        fly_evt.data.flying_text.color_rgb = 0x00e676;
        phoenix_event_publish(&fly_evt);

        if (result_out && max_len > 0) {
            snprintf(result_out, max_len,
                     "{\"status\":\"success\",\"action\":\"add\",\"id\":%d,\"title\":\"%s\",\"time_str\":\"%s\",\"total_count\":%zu,\"done_count\":%zu}",
                     new_id, title, time_str[0] ? time_str : "今日", total_cnt, done_cnt);
        }
        return 0;
    }

    /* 2. 切换待办状态 (toggle) */
    if (strcmp(action, "toggle") == 0 || strcmp(action, "complete") == 0) {
        if (item_id <= 0) {
            if (result_out && max_len > 0) {
                snprintf(result_out, max_len, "{\"status\":\"error\",\"message\":\"未指定有效的待办ID\"}");
            }
            return -1;
        }

        int ret = todo_mgr_toggle(item_id);
        if (ret < 0) {
            if (result_out && max_len > 0) {
                snprintf(result_out, max_len, "{\"status\":\"error\",\"message\":\"未找到对应编号的待办\"}");
            }
            return -1;
        }

        size_t done_cnt = 0;
        size_t total_cnt = todo_mgr_get_counts(&done_cnt);

        phoenix_event_data_t fly_evt;
        memset(&fly_evt, 0, sizeof(fly_evt));
        fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
        fly_evt.data.flying_text.text = "✅ 待办状态已更新";
        fly_evt.data.flying_text.color_rgb = 0x00bcd4;
        phoenix_event_publish(&fly_evt);

        if (result_out && max_len > 0) {
            snprintf(result_out, max_len,
                     "{\"status\":\"success\",\"action\":\"toggle\",\"id\":%d,\"total_count\":%zu,\"done_count\":%zu}",
                     item_id, total_cnt, done_cnt);
        }
        return 0;
    }

    /* 3. 删除指定待办 (delete) */
    if (strcmp(action, "delete") == 0) {
        if (item_id <= 0) {
            if (result_out && max_len > 0) {
                snprintf(result_out, max_len, "{\"status\":\"error\",\"message\":\"未指定有效的待办ID\"}");
            }
            return -1;
        }

        int ret = todo_mgr_delete(item_id);
        if (ret < 0) {
            if (result_out && max_len > 0) {
                snprintf(result_out, max_len, "{\"status\":\"error\",\"message\":\"未找到对应编号的待办\"}");
            }
            return -1;
        }

        size_t done_cnt = 0;
        size_t total_cnt = todo_mgr_get_counts(&done_cnt);

        if (result_out && max_len > 0) {
            snprintf(result_out, max_len,
                     "{\"status\":\"success\",\"action\":\"delete\",\"id\":%d,\"total_count\":%zu,\"done_count\":%zu}",
                     item_id, total_cnt, done_cnt);
        }
        return 0;
    }

    /* 4. 清理已完成待办 (clear_done) */
    if (strcmp(action, "clear_done") == 0) {
        int cleared = todo_mgr_clear_done();
        size_t done_cnt = 0;
        size_t total_cnt = todo_mgr_get_counts(&done_cnt);

        if (result_out && max_len > 0) {
            snprintf(result_out, max_len,
                     "{\"status\":\"success\",\"action\":\"clear_done\",\"cleared_count\":%d,\"remaining_count\":%zu}",
                     cleared, total_cnt);
        }
        return 0;
    }

    /* 5. 默认: 查询待办列表 (list) */
    todo_item_t items[TODO_MAX_ITEMS];
    size_t count = todo_mgr_get_all(items, TODO_MAX_ITEMS);
    size_t done_count = 0;
    todo_mgr_get_counts(&done_count);

    cJSON *resp_json = cJSON_CreateObject();
    cJSON_AddStringToObject(resp_json, "status", "success");
    cJSON_AddNumberToObject(resp_json, "total_count", (double)count);
    cJSON_AddNumberToObject(resp_json, "done_count", (double)done_count);

    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        cJSON *it = cJSON_CreateObject();
        cJSON_AddNumberToObject(it, "id", items[i].id);
        cJSON_AddStringToObject(it, "title", items[i].title);
        cJSON_AddStringToObject(it, "time_str", items[i].time_str);
        cJSON_AddBoolToObject(it, "done", items[i].done);
        cJSON_AddItemToArray(arr, it);
    }
    cJSON_AddItemToObject(resp_json, "items", arr);

    char *json_str = cJSON_PrintUnformatted(resp_json);
    if (json_str) {
        if (result_out && max_len > 0) {
            strncpy(result_out, json_str, max_len - 1);
            result_out[max_len - 1] = '\0';
        }
        free(json_str);
    } else {
        if (result_out && max_len > 0) {
            snprintf(result_out, max_len, "{\"status\":\"success\",\"total_count\":%zu,\"done_count\":%zu,\"items\":[]}",
                     count, done_count);
        }
    }
    cJSON_Delete(resp_json);

    return 0;
}

const phoenix_tool_desc_t g_tool_todo = {
    .name = "manage_todo",
    .description = "管理桌面开发者待办清单（支持查看待办列表list、新增待办add、切换完成状态toggle、删除待办delete、清理已完成项clear_done）",
    .parameters_schema = "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"list\",\"add\",\"toggle\",\"delete\",\"clear_done\"],\"description\":\"动作类型\"},\"title\":{\"type\":\"string\",\"description\":\"待办事项标题 (用于 add)\"},\"time_str\":{\"type\":\"string\",\"description\":\"关联时间标签，如'14:00'或'今日' (用于 add)\"},\"id\":{\"type\":\"integer\",\"description\":\"待办事项唯一编号 (用于 toggle/delete)\"}},\"required\":[]}",
    .execute = tool_todo_exec
};
