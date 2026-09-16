/**
 * @file api_wifi.c
 * @brief Wi-Fi Scanning, Pairing & Reset Web API Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_api.h"
#include "../../hal/network_mgr.h"
#include "../config.h"
#include "../../harness/llm_provider.h"
#include <stdio.h>
#include <string.h>

int handle_wifi_scan(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    net_wifi_ap_info_t aps[8];
    int count = net_mgr_scan_wifi(aps, 8);
    if (count < 0) count = 0;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddNumberToObject(root, "count", count);

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", aps[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", (double)aps[i].rssi);
        cJSON_AddStringToObject(item, "auth", aps[i].auth);
        cJSON_AddItemToArray(arr, item);
    }
    cJSON_AddItemToObject(root, "aps", arr);

    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_wifi_connect(const http_req_t *req, http_resp_t *resp)
{
    char ssid[32] = {0};
    char psk[64] = {0};

    if (req->json) {
        cJSON *s = cJSON_GetObjectItem(req->json, "ssid");
        cJSON *p = cJSON_GetObjectItem(req->json, "psk");
        cJSON *k = cJSON_GetObjectItem(req->json, "api_key");
        cJSON *pr = cJSON_GetObjectItem(req->json, "prompt");
        cJSON *m = cJSON_GetObjectItem(req->json, "model");
        if (s && s->valuestring) strncpy(ssid, s->valuestring, sizeof(ssid) - 1);
        if (p && p->valuestring) strncpy(psk, p->valuestring, sizeof(psk) - 1);
        if (k && k->valuestring && strlen(k->valuestring) > 0) {
            phoenix_llm_set_api_key(k->valuestring);
            phoenix_config_set_str(PHOENIX_CFG_API_KEY, k->valuestring);
        }
        if (pr && pr->valuestring && strlen(pr->valuestring) > 0) {
            phoenix_config_set_str("agent_prompt", pr->valuestring);
        }
        if (m && m->valuestring && strlen(m->valuestring) > 0) {
            phoenix_config_set_str(PHOENIX_CFG_MODEL, m->valuestring);
        }
    }

    if (ssid[0] != '\0') {
        net_mgr_connect_sta(ssid, psk);
    }

    http_resp_json(resp, 200, "{\"success\":true,\"message\":\"connecting to wifi\"}");
    return 0;
}

int handle_wifi_reset(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    net_mgr_reset_to_softap();
    http_resp_json(resp, 200, "{\"success\":true,\"message\":\"reset to softap\"}");
    return 0;
}
