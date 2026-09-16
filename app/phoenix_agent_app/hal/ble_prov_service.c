/**
 * @file ble_prov_service.c
 * @brief 基于 Web Bluetooth / BLE GATT 的配网与智能体配置服务实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "ble_prov_service.h"
#include "network_mgr.h"
#include "../core/config.h"
#include "../harness/llm_provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

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

#define TAG "[BLE_PROV]"

#define BLE_PROV_INFO_STR "OpenVela Gemini-S1 (Phoenix Agent S1) FW-v1.2.0"

static ble_prov_state_t s_ble_state = BLE_PROV_STATE_IDLE;
static char s_dev_name[64] = BLE_PROV_DEFAULT_DEV_NAME;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;

#if (defined(CONFIG_BLUETOOTH_SERVER) || defined(CONFIG_BLUETOOTH)) && !defined(HOST_TEST_RUNNER)
#include "bluetooth.h"
#include "bt_gatts.h"
#include "bt_adapter.h"
#include "bt_le_advertiser.h"

enum {
    PROV_SERVICE_ID = 1,
    PROV_CHAR_NOTIFY_ID,
    PROV_CHAR_NOTIFY_CCC_ID,
    PROV_CHAR_WRITE_ID,
    PROV_CHAR_INFO_ID
};

static gatts_handle_t s_gatts_handle = NULL;
static bt_address_t s_connected_addr;
static bool s_has_connected_client = false;
static uint8_t s_cccd_notify_enabled = 0;

static void handle_rx_payload(const uint8_t *payload, uint16_t length);

static uint16_t prov_notify_ccc_changed(void *srv_handle, bt_address_t *addr,
                                        uint16_t attr_handle, const uint8_t *value,
                                        uint16_t length, uint16_t offset)
{
    (void)srv_handle;
    (void)offset;
    if (length > 0) {
        s_cccd_notify_enabled = value[0];
        printf("%s CCCD updated: 0x%02x, remote addr valid\n", TAG, s_cccd_notify_enabled);
        if (addr) {
            memcpy(&s_connected_addr, addr, sizeof(bt_address_t));
            s_has_connected_client = true;
            s_ble_state = BLE_PROV_STATE_CONNECTED;
        }
    }
    return length;
}

static uint16_t prov_write_callback(void *srv_handle, bt_address_t *addr,
                                    uint16_t attr_handle, const uint8_t *value,
                                    uint16_t length, uint16_t offset)
{
    (void)srv_handle;
    (void)attr_handle;
    (void)offset;
    if (addr) {
        memcpy(&s_connected_addr, addr, sizeof(bt_address_t));
        s_has_connected_client = true;
        s_ble_state = BLE_PROV_STATE_CONNECTED;
    }

    if (value && length > 0) {
        handle_rx_payload(value, length);
    }
    return length;
}

static gatt_attr_db_t s_prov_attr_db[] = {
    /* 1. Primary Service: 0xFFE0 */
    GATT_H_PRIMARY_SERVICE(BT_UUID_DECLARE_16(BLE_PROV_SERVICE_UUID_16), PROV_SERVICE_ID),

    /* 2. Notify Characteristic: 0xFFE2 */
    GATT_H_CHARACTERISTIC_AUTO_RSP(BT_UUID_DECLARE_16(BLE_PROV_CHAR_NOTIFY_UUID_16),
                                  GATT_PROP_NOTIFY | GATT_PROP_READ,
                                  GATT_PERM_READ,
                                  NULL, 0,
                                  PROV_CHAR_NOTIFY_ID),
    GATT_H_CCCD(GATT_PERM_READ | GATT_PERM_WRITE,
                prov_notify_ccc_changed,
                PROV_CHAR_NOTIFY_CCC_ID),

    /* 3. Write Characteristic: 0xFFE1 */
    GATT_H_CHARACTERISTIC_USER_RSP(BT_UUID_DECLARE_16(BLE_PROV_CHAR_WRITE_UUID_16),
                                  GATT_PROP_WRITE | GATT_PROP_WRITE_NR,
                                  GATT_PERM_WRITE,
                                  NULL,
                                  prov_write_callback,
                                  PROV_CHAR_WRITE_ID),

    /* 4. Device Info Characteristic: 0xFFE3 */
    GATT_H_CHARACTERISTIC_AUTO_RSP(BT_UUID_DECLARE_16(BLE_PROV_CHAR_INFO_UUID_16),
                                  GATT_PROP_READ,
                                  GATT_PERM_READ,
                                  (uint8_t *)BLE_PROV_INFO_STR,
                                  sizeof(BLE_PROV_INFO_STR),
                                  PROV_CHAR_INFO_ID)
};

static gatt_srv_db_t s_prov_service_db = {
    .attr_db  = s_prov_attr_db,
    .attr_num = sizeof(s_prov_attr_db) / sizeof(gatt_attr_db_t),
};

static void prov_connected_cb(gatts_handle_t srv_handle, bt_address_t *addr)
{
    (void)srv_handle;
    pthread_mutex_lock(&s_lock);
    if (addr) {
        memcpy(&s_connected_addr, addr, sizeof(bt_address_t));
        s_has_connected_client = true;
    }
    s_ble_state = BLE_PROV_STATE_CONNECTED;
    pthread_mutex_unlock(&s_lock);
    printf("%s Web client connected over BLE GATT\n", TAG);
}

static void prov_disconnected_cb(gatts_handle_t srv_handle, bt_address_t *addr)
{
    (void)srv_handle;
    (void)addr;
    pthread_mutex_lock(&s_lock);
    s_has_connected_client = false;
    s_cccd_notify_enabled = 0;
    if (s_ble_state != BLE_PROV_STATE_PROVISIONED) {
        s_ble_state = BLE_PROV_STATE_ADVERTISING;
    }
    pthread_mutex_unlock(&s_lock);
    printf("%s Web client disconnected from BLE GATT\n", TAG);
}

static gatts_callbacks_t s_gatts_cbs = {
    .size            = sizeof(gatts_callbacks_t),
    .on_connected    = prov_connected_cb,
    .on_disconnected = prov_disconnected_cb,
};

static int send_raw_notify(const char *json_str)
{
    if (!s_gatts_handle || !s_has_connected_client || !s_cccd_notify_enabled) {
        return -1;
    }
    size_t len = strlen(json_str);
    bt_status_t status = bt_gatts_notify(s_gatts_handle, &s_connected_addr,
                                         PROV_CHAR_NOTIFY_ID,
                                         (uint8_t *)json_str, (uint16_t)len);
    return (status == BT_STATUS_SUCCESS) ? 0 : -1;
}

#else
/* 模拟桩实现 (HOST_TEST_RUNNER / 桌面环境) */
static int send_raw_notify(const char *json_str)
{
    printf("%s [MOCK NOTIFY -> Web] %s\n", TAG, json_str);
    return 0;
}
#endif

/**
 * @brief 解析来自 Web 客户端写入的 JSON 指令
 */
static void handle_rx_payload(const uint8_t *payload, uint16_t length)
{
    if (!payload || length == 0) return;

    /* 保证 null-terminate */
    char *buf = (char *)malloc(length + 1);
    if (!buf) return;
    memcpy(buf, payload, length);
    buf[length] = '\0';

    printf("%s Received Web BLE command: %s\n", TAG, buf);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        printf("%s Failed to parse JSON command\n", TAG);
        return;
    }

    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
    const char *cmd = cmd_item ? cmd_item->valuestring : "";

    if (strcmp(cmd, "scan") == 0) {
        /* Web 触发扫描周围 Wi-Fi */
        ble_prov_service_notify_wifi_scan();
    }
    else if (strcmp(cmd, "config") == 0) {
        /* 配网与参数下发 */
        char ssid[NET_MAX_SSID_LEN] = {0};
        char psk[NET_MAX_PSK_LEN] = {0};

        cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
        if (wifi) {
            cJSON *s = cJSON_GetObjectItem(wifi, "ssid");
            cJSON *p = cJSON_GetObjectItem(wifi, "psk");
            if (s && s->valuestring) strncpy(ssid, s->valuestring, sizeof(ssid) - 1);
            if (p && p->valuestring) strncpy(psk, p->valuestring, sizeof(psk) - 1);
        }

        cJSON *agent = cJSON_GetObjectItem(root, "agent");
        if (agent) {
            cJSON *k = cJSON_GetObjectItem(agent, "api_key");
            cJSON *pr = cJSON_GetObjectItem(agent, "prompt");
            cJSON *m = cJSON_GetObjectItem(agent, "model");

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

        if (strlen(ssid) > 0) {
            s_ble_state = BLE_PROV_STATE_PROVISIONING;
            ble_prov_service_notify_net_status("connecting", ssid, "", "正在连接 Wi-Fi 路由...");
            net_mgr_connect_sta(ssid, psk);
        }
    }
    else if (strcmp(cmd, "status") == 0) {
        char ip[NET_MAX_IP_LEN] = {0};
        char ssid[NET_MAX_SSID_LEN] = {0};
        net_mode_t mode = net_mgr_get_mode();
        net_mgr_get_ip(ip, sizeof(ip));
        net_mgr_get_ssid(ssid, sizeof(ssid));

        const char *st = (mode == NET_MODE_STA_CONNECTED) ? "connected" :
                         (mode == NET_MODE_STA_CONNECTING) ? "connecting" : "disconnected";
        ble_prov_service_notify_net_status(st, ssid, ip, "状态同步");
    }
    else if (strcmp(cmd, "reset") == 0) {
        net_mgr_reset_to_softap();
        ble_prov_service_notify_net_status("disconnected", "", "", "已重置网络");
    }

    cJSON_Delete(root);
}

int ble_prov_service_init(const char *custom_dev_name)
{
    pthread_mutex_lock(&s_lock);
    if (custom_dev_name && strlen(custom_dev_name) > 0) {
        strncpy(s_dev_name, custom_dev_name, sizeof(s_dev_name) - 1);
    }

#if (defined(CONFIG_BLUETOOTH_SERVER) || defined(CONFIG_BLUETOOTH)) && !defined(HOST_TEST_RUNNER)
    bt_status_t ret = bt_gatts_register_service(NULL, &s_gatts_handle, &s_gatts_cbs);
    if (ret != BT_STATUS_SUCCESS) {
        printf("%s Failed to register GATT service, ret: %d\n", TAG, ret);
        pthread_mutex_unlock(&s_lock);
        return -1;
    }

    ret = bt_gatts_add_attr_table(s_gatts_handle, &s_prov_service_db);
    if (ret != BT_STATUS_SUCCESS) {
        printf("%s Failed to add GATT attribute table, ret: %d\n", TAG, ret);
        bt_gatts_unregister_service(s_gatts_handle);
        s_gatts_handle = NULL;
        pthread_mutex_unlock(&s_lock);
        return -1;
    }

    printf("%s BLE Provisioning GATT service started successfully\n", TAG);
#else
    printf("%s BLE Provisioning service initialized (Mock / Host Test Mode)\n", TAG);
#endif

    s_ble_state = BLE_PROV_STATE_ADVERTISING;
    pthread_mutex_unlock(&s_lock);
    return 0;
}

void ble_prov_service_deinit(void)
{
    pthread_mutex_lock(&s_lock);
#if (defined(CONFIG_BLUETOOTH_SERVER) || defined(CONFIG_BLUETOOTH)) && !defined(HOST_TEST_RUNNER)
    if (s_gatts_handle) {
        bt_gatts_remove_attr_table(s_gatts_handle, PROV_SERVICE_ID);
        bt_gatts_unregister_service(s_gatts_handle);
        s_gatts_handle = NULL;
    }
#endif
    s_ble_state = BLE_PROV_STATE_STOPPED;
    pthread_mutex_unlock(&s_lock);
    printf("%s BLE Provisioning service stopped\n", TAG);
}

ble_prov_state_t ble_prov_service_get_state(void)
{
    return s_ble_state;
}

int ble_prov_service_notify_net_status(const char *state, const char *ssid, const char *ip, const char *msg)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;

    cJSON_AddStringToObject(root, "event", "net_status");
    cJSON_AddStringToObject(root, "state", state ? state : "unknown");
    cJSON_AddStringToObject(root, "ssid", ssid ? ssid : "");
    cJSON_AddStringToObject(root, "ip", ip ? ip : "");
    cJSON_AddStringToObject(root, "msg", msg ? msg : "");

    if (ip && strlen(ip) > 0 && strcmp(ip, "0.0.0.0") != 0 && strncmp(ip, "192.168.4.", 10) != 0) {
        char dash_url[64];
        snprintf(dash_url, sizeof(dash_url), "http://%s:8080/", ip);
        cJSON_AddStringToObject(root, "dashboard_url", dash_url);
        s_ble_state = BLE_PROV_STATE_PROVISIONED;
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return -1;

    int ret = send_raw_notify(out);
    free(out);
    return ret;
}

int ble_prov_service_notify_wifi_scan(void)
{
    net_wifi_ap_info_t aps[8];
    int count = net_mgr_scan_wifi(aps, 8);
    if (count < 0) count = 0;

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;

    cJSON_AddStringToObject(root, "event", "scan_result");
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

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return -1;

    int ret = send_raw_notify(out);
    free(out);
    return ret;
}
