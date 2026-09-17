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
#include <stdlib.h>
#include <ctype.h>

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

int handle_wifi_status(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    char ip[32] = {0};
    char ssid[32] = {0};
    net_mode_t mode = net_mgr_get_mode();
    net_mgr_get_ip(ip, sizeof(ip));
    net_mgr_get_ssid(ssid, sizeof(ssid));

    const char *mode_str = (mode == NET_MODE_STA_CONNECTED) ? "STA_CONNECTED" :
                           (mode == NET_MODE_STA_CONNECTING) ? "CONNECTING" :
                           (mode == NET_MODE_SOFTAP_CONFIG) ? "SOFTAP" : "DISCONNECTED";

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "mode", mode_str);
    cJSON_AddStringToObject(root, "ip", ip);
    cJSON_AddStringToObject(root, "ssid", ssid);
    cJSON_AddBoolToObject(root, "is_connected", (mode == NET_MODE_STA_CONNECTED));

    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_wifi_connect(const http_req_t *req, http_resp_t *resp)
{
    char ssid[32] = {0};
    char psk[64] = {0};

    cJSON *json_obj = req ? req->json : NULL;
    cJSON *local_parsed = NULL;

    /* 1. 若上层未能自动解析 JSON，尝试使用 req->body 兜底解析 */
    if (!json_obj && req && req->body && req->body[0] != '\0') {
        const char *bp = req->body;
        while (*bp && isspace((unsigned char)*bp)) bp++;
        if (*bp == '{') {
            local_parsed = cJSON_Parse(bp);
            json_obj = local_parsed;
        }
    }

    if (json_obj) {
        cJSON *s = cJSON_GetObjectItem(json_obj, "ssid");
        cJSON *p = cJSON_GetObjectItem(json_obj, "psk");
        cJSON *k = cJSON_GetObjectItem(json_obj, "api_key");
        cJSON *pr = cJSON_GetObjectItem(json_obj, "prompt");
        cJSON *m = cJSON_GetObjectItem(json_obj, "model");
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
        phoenix_config_save();
    }

    if (local_parsed) {
        cJSON_Delete(local_parsed);
    }

    /* 2. 严格校验 SSID 不能为空 */
    if (ssid[0] == '\0') {
        http_resp_error(resp, 400, "SSID cannot be empty");
        return -1;
    }

    /* 3. 触发系统底层连接状态机 */
    printf("[WebApi] 🌐 接收到前端下发 Wi-Fi 凭证: SSID=[%s], PSK=[%s]\n",
           ssid, (psk[0] != '\0') ? "******" : "(NONE)");
    net_mgr_connect_sta(ssid, psk);

    char resp_buf[128];
    snprintf(resp_buf, sizeof(resp_buf),
             "{\"success\":true,\"message\":\"connecting to wifi\",\"ssid\":\"%s\"}", ssid);
    http_resp_json(resp, 200, resp_buf);
    return 0;
}

int handle_wifi_reset(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    net_mgr_reset_to_softap();
    http_resp_json(resp, 200, "{\"success\":true,\"message\":\"reset to softap\"}");
    return 0;
}
