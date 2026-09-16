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
    (void)req;
    int lvl = phoenix_log_get_level();
    const char *lvl_name = phoenix_log_level_to_str(lvl);

    char raw_logs[4096];
    size_t nread = phoenix_log_get_recent(raw_logs, sizeof(raw_logs));
    (void)nread;

    /* 构建 cJSON 响应以安全转义换行与引号 */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddNumberToObject(root, "level_num", lvl);
    cJSON_AddStringToObject(root, "level_str", lvl_name);
    cJSON_AddStringToObject(root, "logs", raw_logs);

    http_resp_json_obj(resp, 200, root);
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
