/**
 * @file api_config.c
 * @brief LLM & Agent Configuration Web API Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_api.h"
#include "../config.h"
#include "../../harness/llm_provider.h"
#include "../../harness/asr_provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int handle_config_get(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    char key_buf[128] = {0};
    char url_buf[256] = {0};
    char prompt_buf[512] = {0};
    char model_buf[64] = {0};
    char backend_buf[32] = {0};

    char asr_key_buf[128] = {0};
    char asr_url_buf[256] = {0};
    char asr_model_buf[64] = {0};
    char asr_backend_buf[32] = {0};

    phoenix_config_get_str(PHOENIX_CFG_API_KEY, "", key_buf, sizeof(key_buf));
    phoenix_config_get_str(PHOENIX_CFG_BASE_URL, "https://api.deepseek.com/v1/chat/completions", url_buf, sizeof(url_buf));
    phoenix_config_get_str(PHOENIX_CFG_MODEL, "deepseek-chat", model_buf, sizeof(model_buf));
    phoenix_config_get_str("agent_prompt", "你是一个贴心的赛博桌面极客助手，语气温和简短", prompt_buf, sizeof(prompt_buf));
    phoenix_config_get_str(PHOENIX_CFG_BACKEND, "cloud", backend_buf, sizeof(backend_buf));

    phoenix_config_get_str(PHOENIX_CFG_ASR_API_KEY, "", asr_key_buf, sizeof(asr_key_buf));
    phoenix_config_get_str(PHOENIX_CFG_ASR_BASE_URL, "https://api.groq.com/openai/v1/audio/transcriptions", asr_url_buf, sizeof(asr_url_buf));
    phoenix_config_get_str(PHOENIX_CFG_ASR_MODEL, "whisper-large-v3", asr_model_buf, sizeof(asr_model_buf));
    phoenix_config_get_str(PHOENIX_CFG_ASR_BACKEND, "cloud", asr_backend_buf, sizeof(asr_backend_buf));

    int temp = phoenix_config_get_int("agent_temperature", 70);
    int vol = phoenix_config_get_int(PHOENIX_CFG_VOLUME, 80);
    int bright = phoenix_config_get_int(PHOENIX_CFG_BRIGHTNESS, 90);

    /* LLM API Key 脱敏显示 */
    char masked_key[64] = {0};
    size_t klen = strlen(key_buf);
    if (klen > 8) {
        snprintf(masked_key, sizeof(masked_key), "%.4s****%.4s", key_buf, key_buf + klen - 4);
    } else if (klen > 0) {
        snprintf(masked_key, sizeof(masked_key), "********");
    }

    /* ASR API Key 脱敏显示 */
    char masked_asr_key[64] = {0};
    size_t asr_klen = strlen(asr_key_buf);
    if (asr_klen > 8) {
        snprintf(masked_asr_key, sizeof(masked_asr_key), "%.4s****%.4s", asr_key_buf, asr_key_buf + asr_klen - 4);
    } else if (asr_klen > 0) {
        snprintf(masked_asr_key, sizeof(masked_asr_key), "********");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddBoolToObject(root, "has_key", klen > 0);
    cJSON_AddStringToObject(root, "api_key_masked", masked_key);
    cJSON_AddStringToObject(root, "base_url", url_buf);
    cJSON_AddStringToObject(root, "model", model_buf);
    cJSON_AddStringToObject(root, "prompt", prompt_buf);
    cJSON_AddStringToObject(root, "backend", backend_buf);
    cJSON_AddNumberToObject(root, "temperature", temp);
    cJSON_AddNumberToObject(root, "volume", vol);
    cJSON_AddNumberToObject(root, "brightness", bright);

    /* ASR 字段 */
    cJSON_AddBoolToObject(root, "has_asr_key", asr_klen > 0);
    cJSON_AddStringToObject(root, "asr_api_key_masked", masked_asr_key);
    cJSON_AddStringToObject(root, "asr_base_url", asr_url_buf);
    cJSON_AddStringToObject(root, "asr_model", asr_model_buf);
    cJSON_AddStringToObject(root, "asr_backend", asr_backend_buf);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        http_resp_json(resp, 200, json_str);
        free(json_str);
    } else {
        http_resp_json(resp, 500, "{\"success\":false,\"error\":\"json encode failed\"}");
    }
    cJSON_Delete(root);
    return 0;
}

int handle_config_post(const http_req_t *req, http_resp_t *resp)
{
    if (req->json) {
        cJSON *key = cJSON_GetObjectItem(req->json, "api_key");
        cJSON *url = cJSON_GetObjectItem(req->json, "base_url");
        cJSON *m = cJSON_GetObjectItem(req->json, "model");
        cJSON *pr = cJSON_GetObjectItem(req->json, "prompt");
        cJSON *temp = cJSON_GetObjectItem(req->json, "temperature");
        cJSON *vol = cJSON_GetObjectItem(req->json, "volume");
        cJSON *bright = cJSON_GetObjectItem(req->json, "brightness");

        /* ASR 字段 */
        cJSON *asr_key = cJSON_GetObjectItem(req->json, "asr_api_key");
        cJSON *asr_url = cJSON_GetObjectItem(req->json, "asr_base_url");
        cJSON *asr_m = cJSON_GetObjectItem(req->json, "asr_model");
        cJSON *asr_b = cJSON_GetObjectItem(req->json, "asr_backend");

        if (key && key->valuestring && strlen(key->valuestring) > 0) {
            phoenix_llm_set_api_key(key->valuestring);
            phoenix_config_set_str(PHOENIX_CFG_API_KEY, key->valuestring);
        }
        if (url && url->valuestring && strlen(url->valuestring) > 0) {
            phoenix_llm_set_base_url(url->valuestring);
            phoenix_config_set_str(PHOENIX_CFG_BASE_URL, url->valuestring);
        }
        if (m && m->valuestring && strlen(m->valuestring) > 0) {
            phoenix_llm_set_model(m->valuestring);
            phoenix_config_set_str(PHOENIX_CFG_MODEL, m->valuestring);
        }
        if (pr && pr->valuestring && strlen(pr->valuestring) > 0) {
            phoenix_config_set_str("agent_prompt", pr->valuestring);
        }
        if (temp && cJSON_IsNumber(temp)) {
            phoenix_config_set_int("agent_temperature", temp->valueint);
        }
        if (vol && cJSON_IsNumber(vol)) {
            phoenix_config_set_int(PHOENIX_CFG_VOLUME, vol->valueint);
        }
        if (bright && cJSON_IsNumber(bright)) {
            phoenix_config_set_int(PHOENIX_CFG_BRIGHTNESS, bright->valueint);
        }

        if (asr_key && asr_key->valuestring && strlen(asr_key->valuestring) > 0) {
            phoenix_asr_set_api_key(asr_key->valuestring);
            phoenix_config_set_str(PHOENIX_CFG_ASR_API_KEY, asr_key->valuestring);
        }
        if (asr_url && asr_url->valuestring && strlen(asr_url->valuestring) > 0) {
            phoenix_asr_set_base_url(asr_url->valuestring);
            phoenix_config_set_str(PHOENIX_CFG_ASR_BASE_URL, asr_url->valuestring);
        }
        if (asr_m && asr_m->valuestring && strlen(asr_m->valuestring) > 0) {
            phoenix_asr_set_model(asr_m->valuestring);
            phoenix_config_set_str(PHOENIX_CFG_ASR_MODEL, asr_m->valuestring);
        }
        if (asr_b && asr_b->valuestring && strlen(asr_b->valuestring) > 0) {
            phoenix_asr_set_backend(asr_b->valuestring);
            phoenix_config_set_str(PHOENIX_CFG_ASR_BACKEND, asr_b->valuestring);
        }

        phoenix_config_save();
    }

    http_resp_json(resp, 200, "{\"success\":true,\"message\":\"config updated\"}");
    return 0;
}

int handle_config_test(const http_req_t *req, http_resp_t *resp)
{
    /* 支持临时传入 key, base_url, model 进行即时连通性探测 */
    if (req->json) {
        cJSON *key = cJSON_GetObjectItem(req->json, "api_key");
        cJSON *url = cJSON_GetObjectItem(req->json, "base_url");
        cJSON *m = cJSON_GetObjectItem(req->json, "model");
        if (key && key->valuestring && strlen(key->valuestring) > 0) {
            phoenix_llm_set_api_key(key->valuestring);
        }
        if (url && url->valuestring && strlen(url->valuestring) > 0) {
            phoenix_llm_set_base_url(url->valuestring);
        }
        if (m && m->valuestring && strlen(m->valuestring) > 0) {
            phoenix_llm_set_model(m->valuestring);
        }

        cJSON *asr_key = cJSON_GetObjectItem(req->json, "asr_api_key");
        cJSON *asr_url = cJSON_GetObjectItem(req->json, "asr_base_url");
        cJSON *asr_m = cJSON_GetObjectItem(req->json, "asr_model");
        if (asr_key && asr_key->valuestring && strlen(asr_key->valuestring) > 0) {
            phoenix_asr_set_api_key(asr_key->valuestring);
        }
        if (asr_url && asr_url->valuestring && strlen(asr_url->valuestring) > 0) {
            phoenix_asr_set_base_url(asr_url->valuestring);
        }
        if (asr_m && asr_m->valuestring && strlen(asr_m->valuestring) > 0) {
            phoenix_asr_set_model(asr_m->valuestring);
        }
    }

    uint32_t latency_ms = 0;
    int http_status = 0;
    char err_buf[256] = {0};
    int rc = phoenix_llm_ping(&latency_ms, &http_status, err_buf, sizeof(err_buf));

    uint32_t asr_latency_ms = 0;
    int asr_http_status = 0;
    char asr_err_buf[256] = {0};
    int asr_rc = phoenix_asr_ping(&asr_latency_ms, &asr_http_status, asr_err_buf, sizeof(asr_err_buf));

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", (rc == 0));
    cJSON_AddNumberToObject(root, "latency_ms", latency_ms);
    cJSON_AddNumberToObject(root, "http_status", http_status);
    cJSON_AddStringToObject(root, "error", err_buf[0] ? err_buf : (rc == 0 ? "" : "LLM 连接失败或服务不可达"));

    /* ASR 测试结果 */
    cJSON_AddBoolToObject(root, "asr_success", (asr_rc == 0));
    cJSON_AddNumberToObject(root, "asr_latency_ms", asr_latency_ms);
    cJSON_AddNumberToObject(root, "asr_http_status", asr_http_status);
    cJSON_AddStringToObject(root, "asr_error", asr_err_buf[0] ? asr_err_buf : (asr_rc == 0 ? "" : "ASR 连接失败或端点不可达"));

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        http_resp_json(resp, 200, json_str);
        free(json_str);
    } else {
        http_resp_json(resp, 500, "{\"success\":false,\"error\":\"json error\"}");
    }
    cJSON_Delete(root);
    return 0;
}

