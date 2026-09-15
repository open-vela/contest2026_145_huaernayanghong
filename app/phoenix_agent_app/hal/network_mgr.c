/**
 * @file network_mgr.c
 * @brief 网络连接与 SoftAP 双模管理组件实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "network_mgr.h"
#include "../core/config.h"
#include "../core/web_portal.h"
#include "../core/event_bus.h"
#include "../utils/log_utils.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define TAG "NetMgr"

static net_mode_t       s_mode = NET_MODE_DISCONNECTED;
static char             s_current_ip[NET_MAX_IP_LEN] = {0};
static char             s_current_ssid[NET_MAX_SSID_LEN] = {0};
static bool             s_web_enabled = true;
static net_state_cb_t   s_state_cb = NULL;
static void            *s_state_user_data = NULL;
static pthread_mutex_t  s_lock = PTHREAD_MUTEX_INITIALIZER;
static bool             s_initialized = false;

static void notify_state_changed_unlocked(void)
{
    if (s_state_cb) {
        s_state_cb(s_mode, s_current_ip, s_state_user_data);
    }
}

int net_mgr_init(void)
{
    pthread_mutex_lock(&s_lock);
    if (s_initialized) {
        pthread_mutex_unlock(&s_lock);
        return 0;
    }

    s_initialized = true;

    /* 读取 Web 服务偏好设置 (默认开启) */
    s_web_enabled = (phoenix_config_get_int("web_portal_en", 1) != 0);

    /* 检查本地是否保存了 Wi-Fi SSID */
    char saved_ssid[NET_MAX_SSID_LEN] = {0};
    char saved_psk[NET_MAX_PSK_LEN] = {0};
    phoenix_config_get_str(PHOENIX_CFG_WIFI_SSID, "", saved_ssid, sizeof(saved_ssid));
    phoenix_config_get_str(PHOENIX_CFG_WIFI_PSK, "", saved_psk, sizeof(saved_psk));

    pthread_mutex_unlock(&s_lock);

    if (saved_ssid[0] != '\0') {
        LOG_I(TAG, "检测到已保存的 Wi-Fi 配置: [%s]，尝试连入局域网...", saved_ssid);
        return net_mgr_connect_sta(saved_ssid, saved_psk);
    } else {
        LOG_I(TAG, "本地无 Wi-Fi 配置，自动启动 SoftAP 独立热点配网模式...");
        return net_mgr_start_softap(NULL);
    }
}

void net_mgr_deinit(void)
{
    pthread_mutex_lock(&s_lock);
    if (!s_initialized) {
        pthread_mutex_unlock(&s_lock);
        return;
    }

    s_mode = NET_MODE_DISCONNECTED;
    s_current_ip[0] = '\0';
    s_current_ssid[0] = '\0';
    s_initialized = false;
    pthread_mutex_unlock(&s_lock);

    phoenix_web_portal_stop();
    LOG_I(TAG, "网络管理器已安全销毁");
}

int net_mgr_start_softap(const char *custom_ssid)
{
    pthread_mutex_lock(&s_lock);
    const char *ssid = (custom_ssid && custom_ssid[0]) ? custom_ssid : NET_DEFAULT_SOFTAP_SSID;

    s_mode = NET_MODE_SOFTAP_CONFIG;
    s_web_enabled = true;
    snprintf(s_current_ssid, sizeof(s_current_ssid), "%s", ssid);
    snprintf(s_current_ip, sizeof(s_current_ip), "%s", NET_DEFAULT_SOFTAP_IP);

    LOG_I(TAG, "📡 SoftAP 广播开启: SSID=[%s], 配网地址=[http://%s:8080]", 
          s_current_ssid, s_current_ip);

    notify_state_changed_unlocked();
    pthread_mutex_unlock(&s_lock);

    /* 启动 Web 配网服务 */
    phoenix_web_portal_start(8080, NULL);
    return 0;
}

int net_mgr_connect_sta(const char *ssid, const char *psk)
{
    if (!ssid || !ssid[0]) return -1;

    pthread_mutex_lock(&s_lock);
    /* 防并发频繁重入 */
    if (s_mode == NET_MODE_STA_CONNECTING) {
        pthread_mutex_unlock(&s_lock);
        LOG_W(TAG, "Wi-Fi 正在连接中，忽略重复连接请求");
        return -2;
    }

    s_mode = NET_MODE_STA_CONNECTING;
    snprintf(s_current_ssid, sizeof(s_current_ssid), "%.31s", ssid);
    LOG_I(TAG, "正在连接 Wi-Fi: [%s]...", s_current_ssid);
    notify_state_changed_unlocked();

    /* 持久化 Wi-Fi 凭证 */
    phoenix_config_set_str(PHOENIX_CFG_WIFI_SSID, s_current_ssid);
    if (psk) {
        char safe_psk[NET_MAX_PSK_LEN] = {0};
        snprintf(safe_psk, sizeof(safe_psk), "%.63s", psk);
        phoenix_config_set_str(PHOENIX_CFG_WIFI_PSK, safe_psk);
    }

    /* 模拟连接成功，分配局域网内网 IP (真机底层由 DHCP 客户端回填) */
    s_mode = NET_MODE_STA_CONNECTED;
    snprintf(s_current_ip, sizeof(s_current_ip), "192.168.1.108");

    LOG_I(TAG, "✅ Wi-Fi 连接成功! 分配 IP: [%s]", s_current_ip);
    notify_state_changed_unlocked();

    bool web_en = s_web_enabled;
    pthread_mutex_unlock(&s_lock);

    /* 按需管理 Web 伴侣服务 */
    if (web_en) {
        phoenix_web_portal_start(8080, NULL);
        LOG_I(TAG, "🌐 局域网 Web 伴侣已待命: http://%s:8080", s_current_ip);
    } else {
        phoenix_web_portal_stop();
        LOG_I(TAG, "🔒 根据配置，局域网 Web 伴侣已禁用");
    }

    return 0;
}

void net_mgr_disconnect(void)
{
    pthread_mutex_lock(&s_lock);
    s_mode = NET_MODE_DISCONNECTED;
    s_current_ip[0] = '\0';
    s_current_ssid[0] = '\0';
    notify_state_changed_unlocked();
    pthread_mutex_unlock(&s_lock);

    phoenix_web_portal_stop();
    LOG_I(TAG, "已断开网络连接");
}

int net_mgr_reset_to_softap(void)
{
    LOG_I(TAG, "🔄 重置 Wi-Fi 配置并返回 SoftAP 独立热点配网模式...");
    phoenix_config_set_str(PHOENIX_CFG_WIFI_SSID, "");
    phoenix_config_set_str(PHOENIX_CFG_WIFI_PSK, "");
    return net_mgr_start_softap(NULL);
}

net_mode_t net_mgr_get_mode(void)
{
    pthread_mutex_lock(&s_lock);
    net_mode_t mode = s_mode;
    pthread_mutex_unlock(&s_lock);
    return mode;
}

int net_mgr_get_ip(char *buf, size_t max_len)
{
    if (!buf || max_len == 0) return -1;
    pthread_mutex_lock(&s_lock);
    if (s_current_ip[0] == '\0') {
        pthread_mutex_unlock(&s_lock);
        buf[0] = '\0';
        return -2;
    }
    snprintf(buf, max_len, "%s", s_current_ip);
    pthread_mutex_unlock(&s_lock);
    return 0;
}

int net_mgr_get_ssid(char *buf, size_t max_len)
{
    if (!buf || max_len == 0) return -1;
    pthread_mutex_lock(&s_lock);
    if (s_current_ssid[0] == '\0') {
        pthread_mutex_unlock(&s_lock);
        buf[0] = '\0';
        return -2;
    }
    snprintf(buf, max_len, "%s", s_current_ssid);
    pthread_mutex_unlock(&s_lock);
    return 0;
}

void net_mgr_register_state_cb(net_state_cb_t cb, void *user_data)
{
    pthread_mutex_lock(&s_lock);
    s_state_cb = cb;
    s_state_user_data = user_data;
    pthread_mutex_unlock(&s_lock);
}

int net_mgr_set_web_enabled(bool enabled)
{
    pthread_mutex_lock(&s_lock);
    s_web_enabled = enabled;
    phoenix_config_set_int("web_portal_en", enabled ? 1 : 0);

    bool is_connected = (s_mode == NET_MODE_STA_CONNECTED || s_mode == NET_MODE_SOFTAP_CONFIG);
    pthread_mutex_unlock(&s_lock);

    if (enabled && is_connected) {
        phoenix_web_portal_start(8080, NULL);
        LOG_I(TAG, "Web 伴侣服务已手动启动");
    } else {
        phoenix_web_portal_stop();
        LOG_I(TAG, "Web 伴侣服务已手动关闭");
    }

    return 0;
}

bool net_mgr_is_web_enabled(void)
{
    pthread_mutex_lock(&s_lock);
    bool en = s_web_enabled;
    pthread_mutex_unlock(&s_lock);
    return en;
}

int net_mgr_scan_wifi(net_wifi_ap_info_t *aps_out, size_t max_count)
{
    if (!aps_out || max_count == 0) return -1;

    static const net_wifi_ap_info_t s_default_aps[] = {
        {"Office-5G",          -45, "WPA2"},
        {"Home-Mesh-2.4G",     -58, "WPA2"},
        {"GeekLab-Wi-Fi6",     -68, "WPA3"},
        {"Guest-Free-WiFi",    -78, "OPEN"},
        {"Coffee-Bar-Hotspot", -85, "WPA2"}
    };

    size_t count = sizeof(s_default_aps) / sizeof(s_default_aps[0]);
    if (count > max_count) count = max_count;

    for (size_t i = 0; i < count; i++) {
        aps_out[i] = s_default_aps[i];
    }

    LOG_I(TAG, "📡 扫描周边 Wi-Fi 完成，发现 %zu 个热点", count);
    return (int)count;
}
