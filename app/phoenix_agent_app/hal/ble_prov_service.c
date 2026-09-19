/**
 * @file ble_prov_service.c
 * @brief 基于 Web Bluetooth / BLE GATT 的配网与智能体配置服务实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "ble_prov_service.h"
#include "network_mgr.h"
#include "../core/config.h"
#include "../harness/llm_provider.h"
#if defined(__has_include) && __has_include("utils/log_utils.h")
#  include "utils/log_utils.h"
#else
#  include "../utils/log_utils.h"
#endif
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

#define TAG "BLE_PROV"

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

static void handle_rx_payload(const uint8_t *payload, uint16_t length) __attribute__((unused));

static uint16_t prov_notify_ccc_changed(void *srv_handle, bt_address_t *addr,
                                        uint16_t attr_handle, const uint8_t *value,
                                        uint16_t length, uint16_t offset)
{
    (void)srv_handle;
    (void)offset;
    if (length > 0) {
        s_cccd_notify_enabled = value[0];
        LOG_D(TAG, "CCCD updated: 0x%02x, remote addr valid", s_cccd_notify_enabled);
        if (addr) {
            memcpy(&s_connected_addr, addr, sizeof(bt_address_t));
            s_has_connected_client = true;
            s_ble_state = BLE_PROV_STATE_CONNECTED;
            net_mgr_suspend_softap(); /* Coex 射频仲裁：挂起 SoftAP 广播 */
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
        net_mgr_suspend_softap(); /* Coex 射频仲裁：挂起 SoftAP 广播 */
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
    LOG_I(TAG, "Web client connected over BLE GATT");
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
    LOG_I(TAG, "Web client disconnected from BLE GATT");
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
    LOG_I(TAG, "[MOCK NOTIFY -> Web] %s", json_str);
    return 0;
}
#endif

int ble_prov_service_handle_command(const char *cmd_json_str)
{
    if (!cmd_json_str || strlen(cmd_json_str) == 0) return -1;

    LOG_I(TAG, "Received BLE Provisioning command: %s", cmd_json_str);

    cJSON *root = cJSON_Parse(cmd_json_str);
    if (!root) {
        LOG_W(TAG, "Failed to parse JSON command");
        ble_prov_service_notify_ack("error", false, "Invalid JSON format");
        return -1;
    }

    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
    const char *cmd = cmd_item ? cmd_item->valuestring : "";

    if (strcmp(cmd, "get_config") == 0) {
        /* 双向回读：获取当前全部配置 */
        ble_prov_service_notify_config();
    }
    else if (strcmp(cmd, "get_system_info") == 0) {
        /* 双向回读：获取系统硬件与遥测信息 */
        ble_prov_service_notify_system_info();
    }
    else if (strcmp(cmd, "scan") == 0) {
        /* 触发 Wi-Fi 扫描 */
        ble_prov_service_notify_wifi_scan();
    }
    else if (strcmp(cmd, "set_config") == 0 || strcmp(cmd, "config") == 0) {
        /* 配置更新与修改 */
        bool config_updated = false;
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
                config_updated = true;
            }
            if (pr && pr->valuestring && strlen(pr->valuestring) > 0) {
                phoenix_config_set_str("agent_prompt", pr->valuestring);
                config_updated = true;
            }
            if (m && m->valuestring && strlen(m->valuestring) > 0) {
                phoenix_config_set_str(PHOENIX_CFG_MODEL, m->valuestring);
                config_updated = true;
            }
            if (config_updated) {
                phoenix_config_save();
            }
        }

        /* 若带有 Wi-Fi SSID 则发起连网流程 */
        if (strlen(ssid) > 0) {
            s_ble_state = BLE_PROV_STATE_PROVISIONING;
            ble_prov_service_notify_net_status("connecting", ssid, "", "正在连接 Wi-Fi 路由...");
            net_mgr_connect_sta(ssid, psk);
        } else if (config_updated) {
            /* 仅更新智能体参数，无需重连 Wi-Fi，立即回传成功 ACK 并回传最新配置 */
            ble_prov_service_notify_ack("config_saved", true, "智能体参数已成功保存并持久化");
            ble_prov_service_notify_config();
        }
    }
    else if (strcmp(cmd, "connect") == 0) {
        /* 仅连接指定 Wi-Fi */
        char ssid[NET_MAX_SSID_LEN] = {0};
        char psk[NET_MAX_PSK_LEN] = {0};
        cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
        if (wifi) {
            cJSON *s = cJSON_GetObjectItem(wifi, "ssid");
            cJSON *p = cJSON_GetObjectItem(wifi, "psk");
            if (s && s->valuestring) strncpy(ssid, s->valuestring, sizeof(ssid) - 1);
            if (p && p->valuestring) strncpy(psk, p->valuestring, sizeof(psk) - 1);
        }
        if (strlen(ssid) > 0) {
            s_ble_state = BLE_PROV_STATE_PROVISIONING;
            ble_prov_service_notify_net_status("connecting", ssid, "", "正在连接 Wi-Fi 路由...");
            net_mgr_connect_sta(ssid, psk);
        } else {
            ble_prov_service_notify_ack("error", false, "Missing SSID in connect command");
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
        ble_prov_service_notify_net_status("disconnected", "", "", "已重置网络并恢复出厂热点");
    }
    else {
        LOG_W(TAG, "Unknown command: %s", cmd);
        ble_prov_service_notify_ack("error", false, "Unknown command");
    }

    cJSON_Delete(root);
    return 0;
}

static void handle_rx_payload(const uint8_t *payload, uint16_t length) __attribute__((unused));
static void handle_rx_payload(const uint8_t *payload, uint16_t length)
{
    if (!payload || length == 0) return;

    char *buf = (char *)malloc(length + 1);
    if (!buf) return;
    memcpy(buf, payload, length);
    buf[length] = '\0';

    ble_prov_service_handle_command(buf);
    free(buf);
}

#if (defined(CONFIG_BLUETOOTH_SERVER) || defined(CONFIG_BLUETOOTH)) && !defined(HOST_TEST_RUNNER)
static bt_instance_t *s_bt_ins = NULL;
#endif

int ble_prov_service_init(const char *custom_dev_name)
{
    pthread_mutex_lock(&s_lock);
    if (s_ble_state == BLE_PROV_STATE_ADVERTISING ||
        s_ble_state == BLE_PROV_STATE_CONNECTED ||
        s_ble_state == BLE_PROV_STATE_PROVISIONING) {
        LOG_I(TAG, "BLE Provisioning service is already active");
        pthread_mutex_unlock(&s_lock);
        return 0;
    }

    if (custom_dev_name && strlen(custom_dev_name) > 0) {
        strncpy(s_dev_name, custom_dev_name, sizeof(s_dev_name) - 1);
    }

#if (defined(CONFIG_BLUETOOTH_SERVER) || defined(CONFIG_BLUETOOTH)) && !defined(HOST_TEST_RUNNER)
    if (!s_bt_ins) {
        s_bt_ins = bluetooth_create_instance();
        if (!s_bt_ins) {
            LOG_E(TAG, "bluetooth_create_instance failed");
            pthread_mutex_unlock(&s_lock);
            return -1;
        }
    }

    /* 1. 检查并确保适配器处于开启状态 */
    bt_adapter_state_t state = bt_adapter_get_state(s_bt_ins);
    if (state != BT_ADAPTER_STATE_ON) {
        LOG_I(TAG, "蓝牙适配器尚未开启 (state=%d)，正在使能...", state);
        bt_adapter_enable(s_bt_ins);

        /* 轮询等待底层固件加载与控制器就绪 (最多等待 3 秒) */
        int wait_count = 0;
        while (wait_count < 60) {
            usleep(50000); /* 50ms */
            state = bt_adapter_get_state(s_bt_ins);
            if (state == BT_ADAPTER_STATE_ON) {
                LOG_I(TAG, "蓝牙适配器已就绪 (耗时 %d ms)", (wait_count + 1) * 50);
                break;
            }
            wait_count++;
        }

        if (state != BT_ADAPTER_STATE_ON) {
            LOG_E(TAG, "蓝牙适配器使能超时 (最终状态: %d)，底层驱动可能未就绪", state);
            bluetooth_delete_instance(s_bt_ins);
            s_bt_ins = NULL;
            pthread_mutex_unlock(&s_lock);
            return -1;
        }
    }

    /* 2. 配置广播参数与可见性 (duration 传入 180 秒) */
    bt_adapter_set_name(s_bt_ins, s_dev_name);
    bt_adapter_set_scan_mode(s_bt_ins, BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE, 180);

    /* 3. 适配器就绪后再注册 GATT 服务 */
    bt_status_t ret = bt_gatts_register_service(s_bt_ins, &s_gatts_handle, &s_gatts_cbs);
    if (ret != BT_STATUS_SUCCESS) {
        LOG_E(TAG, "Failed to register GATT service, ret: %d", ret);
        bluetooth_delete_instance(s_bt_ins);
        s_bt_ins = NULL;
        pthread_mutex_unlock(&s_lock);
        return -1;
    }

    ret = bt_gatts_add_attr_table(s_gatts_handle, &s_prov_service_db);
    if (ret != BT_STATUS_SUCCESS) {
        LOG_E(TAG, "Failed to add GATT attribute table, ret: %d", ret);
        bt_gatts_unregister_service(s_gatts_handle);
        s_gatts_handle = NULL;
        bluetooth_delete_instance(s_bt_ins);
        s_bt_ins = NULL;
        pthread_mutex_unlock(&s_lock);
        return -1;
    }

    LOG_I(TAG, "BLE Provisioning GATT service started successfully (Device: %s)", s_dev_name);
#else
    LOG_I(TAG, "BLE Provisioning service initialized (Mock / Host Test Mode: %s)", s_dev_name);
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
    if (s_bt_ins) {
        bluetooth_delete_instance(s_bt_ins);
        s_bt_ins = NULL;
    }
#endif
    s_ble_state = BLE_PROV_STATE_STOPPED;
    pthread_mutex_unlock(&s_lock);
    LOG_I(TAG, "BLE Provisioning service stopped");
}

ble_prov_state_t ble_prov_service_get_state(void)
{
    return s_ble_state;
}

int ble_prov_service_get_dev_name(char *buf, size_t max_len)
{
    if (!buf || max_len == 0) return -1;
    pthread_mutex_lock(&s_lock);
    strncpy(buf, s_dev_name, max_len - 1);
    buf[max_len - 1] = '\0';
    pthread_mutex_unlock(&s_lock);
    return 0;
}

bool ble_prov_service_is_active(void)
{
    pthread_mutex_lock(&s_lock);
    bool active = (s_ble_state == BLE_PROV_STATE_ADVERTISING ||
                   s_ble_state == BLE_PROV_STATE_CONNECTED ||
                   s_ble_state == BLE_PROV_STATE_PROVISIONING);
    pthread_mutex_unlock(&s_lock);
    return active;
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

    if (ip && net_is_valid_sta_ip(ip)) {
        char dash_url[64];
        snprintf(dash_url, sizeof(dash_url), "http://%s:8080/", ip);
        cJSON_AddStringToObject(root, "dashboard_url", dash_url);
        s_ble_state = BLE_PROV_STATE_PROVISIONED;
        net_mgr_stop_softap(); /* 成功入网，关闭热点发射以省电防干扰 */
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

int ble_prov_service_notify_ack(const char *event_name, bool success, const char *msg)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;

    cJSON_AddStringToObject(root, "event", event_name ? event_name : "ack");
    cJSON_AddBoolToObject(root, "success", success);
    cJSON_AddStringToObject(root, "msg", msg ? msg : "");

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return -1;

    int ret = send_raw_notify(out);
    free(out);
    return ret;
}

int ble_prov_service_notify_config(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;

    cJSON_AddStringToObject(root, "event", "config_data");
    cJSON_AddStringToObject(root, "status", "ok");

    /* Wi-Fi 状态 */
    cJSON *wifi = cJSON_CreateObject();
    char ssid[NET_MAX_SSID_LEN] = {0};
    char ip[NET_MAX_IP_LEN] = {0};
    net_mgr_get_ssid(ssid, sizeof(ssid));
    net_mgr_get_ip(ip, sizeof(ip));
    net_mode_t mode = net_mgr_get_mode();
    cJSON_AddStringToObject(wifi, "ssid", ssid);
    cJSON_AddStringToObject(wifi, "ip", ip);
    cJSON_AddBoolToObject(wifi, "connected", (mode == NET_MODE_STA_CONNECTED));
    cJSON_AddNumberToObject(wifi, "mode", (double)mode);
    cJSON_AddItemToObject(root, "wifi", wifi);

    /* 智能体与大模型配置 */
    cJSON *agent = cJSON_CreateObject();
    char api_key[128] = {0};
    char model[64] = {0};
    char prompt[512] = {0};
    phoenix_config_get_str(PHOENIX_CFG_API_KEY, "", api_key, sizeof(api_key));
    phoenix_config_get_str(PHOENIX_CFG_MODEL, "deepseek-chat", model, sizeof(model));
    phoenix_config_get_str("agent_prompt", "", prompt, sizeof(prompt));

    cJSON_AddStringToObject(agent, "model", model);
    cJSON_AddStringToObject(agent, "prompt", prompt);
    bool key_set = (strlen(api_key) > 0);
    cJSON_AddBoolToObject(agent, "api_key_set", key_set);
    if (key_set) {
        /* 生成安全掩码: 如 sk-****1234 */
        char masked[32] = {0};
        size_t klen = strlen(api_key);
        if (klen > 8) {
            snprintf(masked, sizeof(masked), "sk-****%s", api_key + (klen - 4));
        } else {
            snprintf(masked, sizeof(masked), "sk-****");
        }
        cJSON_AddStringToObject(agent, "api_key_masked", masked);
    } else {
        cJSON_AddStringToObject(agent, "api_key_masked", "");
    }
    cJSON_AddItemToObject(root, "agent", agent);

    /* 设备元数据 */
    cJSON *dev = cJSON_CreateObject();
    char dname[64] = {0};
    ble_prov_service_get_dev_name(dname, sizeof(dname));
    cJSON_AddStringToObject(dev, "name", dname);
    cJSON_AddStringToObject(dev, "version", "v1.2.0");
    cJSON_AddStringToObject(dev, "platform", "OpenVela Gemini-S1");
    cJSON_AddItemToObject(root, "device", dev);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return -1;

    int ret = send_raw_notify(out);
    free(out);
    return ret;
}

int ble_prov_service_notify_system_info(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;

    cJSON_AddStringToObject(root, "event", "system_info");
    cJSON_AddStringToObject(root, "status", "ok");
    cJSON_AddStringToObject(root, "soc", "Allwinner R528-S3 (Dual Cortex-A7)");
    cJSON_AddStringToObject(root, "os", "OpenVela / NuttX RTOS");
    cJSON_AddStringToObject(root, "fw_ver", "v1.2.0");
    cJSON_AddStringToObject(root, "bt_profile", "BLE 5.4 GATT Provisioning");

    char ip[NET_MAX_IP_LEN] = {0};
    net_mgr_get_ip(ip, sizeof(ip));
    cJSON_AddStringToObject(root, "ip", ip);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return -1;

    int ret = send_raw_notify(out);
    free(out);
    return ret;
}
