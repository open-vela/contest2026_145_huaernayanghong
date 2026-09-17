/**
 * @file api_config.c
 * @brief LLM & Agent Configuration Web API Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_api.h"
#include "../config.h"
#include "../../harness/llm_provider.h"
#include <stdio.h>
#include <string.h>

int handle_config_get(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    char key_buf[64] = {0};
    char prompt_buf[256] = {0};
    char model_buf[64] = {0};

    phoenix_config_get_str(PHOENIX_CFG_API_KEY, "", key_buf, sizeof(key_buf));
    phoenix_config_get_str("agent_prompt", "你是一个贴心的赛博桌面极客助手，语气温和简短", prompt_buf, sizeof(prompt_buf));
    phoenix_config_get_str(PHOENIX_CFG_MODEL, "deepseek-chat", model_buf, sizeof(model_buf));

    /* API Key 脱敏显示 */
    char masked_key[64] = {0};
    size_t klen = strlen(key_buf);
    if (klen > 8) {
        snprintf(masked_key, sizeof(masked_key), "%.4s****%.4s", key_buf, key_buf + klen - 4);
    } else if (klen > 0) {
        snprintf(masked_key, sizeof(masked_key), "********");
    }

    char json_buf[1024];
    snprintf(json_buf, sizeof(json_buf),
             "{\"success\":true,\"has_key\":%s,\"api_key_masked\":\"%s\",\"prompt\":\"%s\",\"model\":\"%s\"}",
             klen > 0 ? "true" : "false", masked_key, prompt_buf, model_buf);

    http_resp_json(resp, 200, json_buf);
    return 0;
}

int handle_config_post(const http_req_t *req, http_resp_t *resp)
{
    if (req->json) {
        cJSON *key = cJSON_GetObjectItem(req->json, "api_key");
        cJSON *pr = cJSON_GetObjectItem(req->json, "prompt");
        cJSON *m = cJSON_GetObjectItem(req->json, "model");
        if (key && key->valuestring && strlen(key->valuestring) > 0) {
            phoenix_llm_set_api_key(key->valuestring);
            phoenix_config_set_str(PHOENIX_CFG_API_KEY, key->valuestring);
        }
        if (pr && pr->valuestring && strlen(pr->valuestring) > 0) {
            phoenix_config_set_str("agent_prompt", pr->valuestring);
        }
        if (m && m->valuestring && strlen(m->valuestring) > 0) {
            phoenix_config_set_str(PHOENIX_CFG_MODEL, m->valuestring);
        }
        phoenix_config_save();
    }

    http_resp_json(resp, 200, "{\"success\":true,\"message\":\"config updated\"}");
    return 0;
}
