/**
 * @file api_logs.c
 * @brief Logging Query & Dynamic Level Setting Web API Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_api.h"
#include "../../utils/log_mgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int handle_logs_get(const http_req_t *req, http_resp_t *resp)
{
    int lvl = phoenix_log_get_level();
    const char *lvl_name = phoenix_log_level_to_str(lvl);

    uint64_t cursor = 0;
    char cursor_str[32] = {0};
    if (http_req_get_query_param(req, "cursor", cursor_str, sizeof(cursor_str)) ||
        http_req_get_query_param(req, "offset", cursor_str, sizeof(cursor_str))) {
        if (cursor_str[0] != '\0') {
            cursor = (uint64_t)strtoull(cursor_str, NULL, 10);
        }
    }

    char raw_logs[8192];
    size_t nread = phoenix_log_get_since(&cursor, raw_logs, sizeof(raw_logs));

    /* 构建 cJSON 响应以安全转义换行与引号 */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddNumberToObject(root, "level_num", lvl);
    cJSON_AddStringToObject(root, "level_str", lvl_name);
    cJSON_AddNumberToObject(root, "cursor", (double)cursor);
    cJSON_AddNumberToObject(root, "bytes", (double)nread);
    cJSON_AddStringToObject(root, "logs", raw_logs);

    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_logs_clear_post(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    phoenix_log_clear_recent();
    http_resp_json(resp, 200, "{\"success\":true,\"message\":\"Logs buffer cleared\"}");
    return 0;
}

int handle_logs_level_post(const http_req_t *req, http_resp_t *resp)
{
    int target_lvl = phoenix_log_get_level();
    if (req->json) {
        cJSON *item = cJSON_GetObjectItem(req->json, "level");
        if (item) {
            if (item->type == cJSON_String && item->valuestring) {
                target_lvl = phoenix_log_level_from_str(item->valuestring);
            } else if (item->type == cJSON_Number) {
                target_lvl = item->valueint;
            }
        }
    }

    phoenix_log_set_level(target_lvl);

    char resp_buf[256];
    snprintf(resp_buf, sizeof(resp_buf),
             "{\"success\":true,\"level_num\":%d,\"level_str\":\"%s\"}",
             target_lvl, phoenix_log_level_to_str(target_lvl));

    http_resp_json(resp, 200, resp_buf);
    return 0;
}
