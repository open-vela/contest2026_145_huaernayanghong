/**
 * @file api_cartridge.c
 * @brief Cartridge Switch, Memo Add & Desktop Actions Web API Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_api.h"
#include "../cartridge_mgr.h"
#include "../../cartridges/cartridge_memo.h"
#include "../tool_registry.h"
#if defined(__has_include) && __has_include("web_portal.h")
#  include "web_portal.h"
#else
#  include "../web_portal.h"
#endif
#include <stdio.h>
#include <string.h>

int handle_cartridge_switch(const http_req_t *req, http_resp_t *resp)
{
    char target_id[32] = {0};
    if (req->json) {
        cJSON *id_item = cJSON_GetObjectItem(req->json, "id");
        if (id_item && id_item->valuestring) {
            strncpy(target_id, id_item->valuestring, sizeof(target_id) - 1);
        }
    }

    if (target_id[0] != '\0') {
        if (phoenix_web_portal_in_server_thread()) {
            phoenix_web_portal_enqueue_cmd(WEB_CMD_SWITCH_CARTRIDGE, target_id);
        } else {
            cartridge_mgr_switch_to(target_id);
        }
    }

    http_resp_json(resp, 200, "{\"success\":true,\"message\":\"cartridge switched\"}");
    return 0;
}

int handle_memo_add(const http_req_t *req, http_resp_t *resp)
{
    char memo_text[128] = {0};
    if (req->json) {
        cJSON *content = cJSON_GetObjectItem(req->json, "content");
        if (content && content->valuestring) {
            strncpy(memo_text, content->valuestring, sizeof(memo_text) - 1);
        }
    }

    if (memo_text[0] != '\0') {
        if (phoenix_web_portal_in_server_thread()) {
            phoenix_web_portal_enqueue_cmd(WEB_CMD_ADD_MEMO, memo_text);
        } else {
            cartridge_memo_add_entry(memo_text);
        }
    }

    http_resp_json(resp, 200, "{\"success\":true,\"message\":\"memo entry added\"}");
    return 0;
}

int handle_action_dispatch(const http_req_t *req, http_resp_t *resp)
{
    char action[32] = {0};
    if (req->json) {
        cJSON *act = cJSON_GetObjectItem(req->json, "action");
        if (act && act->valuestring) {
            strncpy(action, act->valuestring, sizeof(action) - 1);
        }
    }

    if (action[0] != '\0') {
        if (phoenix_web_portal_in_server_thread()) {
            phoenix_web_portal_enqueue_cmd(WEB_CMD_ACTION, action);
        } else {
            if (strcmp(action, "pet") == 0) {
                cartridge_mgr_dispatch_knock(1, 1);
            } else if (strcmp(action, "knock_fish") == 0) {
                phoenix_tool_execute("knock_wooden_fish", "{\"count\":1}", NULL, 0);
            } else if (strcmp(action, "pomo_toggle") == 0) {
                cartridge_mgr_dispatch_knock(1, 1);
            }
        }
    }

    http_resp_json(resp, 200, "{\"success\":true,\"message\":\"action dispatched\"}");
    return 0;
}
