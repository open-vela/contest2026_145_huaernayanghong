/**
 * @file network_mgr.c
 * @brief 网络连接与 SoftAP 双模管理组件实现 (OpenVela WAPI & Dual-Mode State Machine)
 * @author OpenVela Contest 2026 Team 145
 */

#include "network_mgr.h"
#include "../core/config.h"
#include "../core/web_portal.h"
#include "../utils/log_utils.h"

#if defined(__has_include) && __has_include("core/event_bus.h")
#  include "core/event_bus.h"
#else
#  include "../core/event_bus.h"
#endif
#include "ble_prov_service.h"

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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <poll.h>

#if !defined(HOST_TEST_RUNNER)
#  if defined(__has_include) && __has_include(<wireless/wapi.h>)
#    include <wireless/wapi.h>
#  else
/* WAPI C 语言原生底层接口声明 (链接自 apps/wireless/wapi) */
#define WAPI_ESSID_OFF 0
#define WAPI_ESSID_ON  1
#define WAPI_MODE_MANAGED 2
#define WAPI_MODE_MASTER  3

#define IW_AUTH_WPA_VERSION      0
#define IW_AUTH_CIPHER_PAIRWISE  1
#define IW_AUTH_WPA_VERSION_WPA2 0x00000004
#define IW_AUTH_CIPHER_CCMP      0x00000008
#define WPA_ALG_CCMP             3

struct ether_addr_sub {
    uint8_t ether_addr_octet[6];
};

struct wapi_scan_info_s {
    struct wapi_scan_info_s *next;
    struct ether_addr_sub ap;
    int has_essid;
    char essid[32 + 1];
    int essid_flag;
    int has_freq;
    double freq;
    int has_mode;
    int mode;
    int has_bitrate;
    int bitrate;
    int has_rssi;
    int rssi;
    int has_encode;
    int encode;
};

struct wapi_list_s {
    union {
        void *string;
        struct wapi_scan_info_s *scan;
        void *route;
    } head;
};

int  wapi_make_socket(void);
int  wapi_set_ifup(int sock, const char *ifname);
int  wapi_set_ifdown(int sock, const char *ifname);
int  wapi_set_ip(int sock, const char *ifname, const struct in_addr *addr);
int  wapi_set_netmask(int sock, const char *ifname, const struct in_addr *addr);
int  wapi_set_mode(int sock, const char *ifname, int mode);
int  wapi_set_freq(int sock, const char *ifname, double freq, int flag);
int  wapi_set_essid(int sock, const char *ifname, const char *essid, int flag);
void wpa_driver_wext_disconnect(int sockfd, const char *ifname);
int  wpa_driver_wext_set_auth_param(int sockfd, const char *ifname, int idx, uint32_t value);
int  wpa_driver_wext_set_key_ext(int sockfd, const char *ifname, int alg, const char *key, size_t key_len);
int  wapi_scan_init(int sock, const char *ifname, const char *essid);
int  wapi_scan_stat(int sock, const char *ifname);
int  wapi_scan_coll(int sock, const char *ifname, struct wapi_list_s *list);
void wapi_scan_coll_free(struct wapi_list_s *list);
int  wapi_get_ap(int sock, const char *ifname, void *ap);
#  endif
#endif

#define TAG "NetMgr"

#define WAPI_CONF_DIR  "/data/etc/wifi"
#define WAPI_CONF_FILE "/data/etc/wifi/wapi.conf"

static net_mode_t       s_mode = NET_MODE_DISCONNECTED;
static char             s_current_ip[NET_MAX_IP_LEN] = {0};
static char             s_current_ssid[NET_MAX_SSID_LEN] = {0};
static bool             s_web_enabled = true;
static net_state_cb_t   s_state_cb = NULL;
static void            *s_state_user_data = NULL;
static pthread_mutex_t  s_lock = PTHREAD_MUTEX_INITIALIZER;
static bool             s_initialized = false;
#if !defined(HOST_TEST_RUNNER)
static pthread_t        s_connect_tid = 0;
static pthread_t        s_watchdog_tid = 0;
static volatile bool    s_watchdog_running = false;
#endif
static bool             s_worker_running = false;

static void notify_state_changed_with_msg_unlocked(const char *custom_msg)
{
    if (s_state_cb) {
        s_state_cb(s_mode, s_current_ip, s_state_user_data);
    }

    phoenix_event_data_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_NET_STATUS;
    evt.data.net.mode = (int)s_mode;
    evt.data.net.ssid = s_current_ssid;
    evt.data.net.ip = s_current_ip;
    if (custom_msg && custom_msg[0]) {
        evt.data.net.msg = custom_msg;
    } else if (s_mode == NET_MODE_STA_CONNECTED) {
        evt.data.net.msg = "Wi-Fi 连接成功";
    } else if (s_mode == NET_MODE_STA_CONNECTING) {
        evt.data.net.msg = "正在连接 Wi-Fi";
    } else if (s_mode == NET_MODE_SOFTAP_CONFIG) {
        evt.data.net.msg = "独立热点配网就绪";
    } else {
        evt.data.net.msg = "网络未连接";
    }

#if defined(HOST_TEST_RUNNER)
    phoenix_event_publish(&evt);
#else
    phoenix_event_post_async(&evt);
#endif

    const char *ble_state_str = (s_mode == NET_MODE_STA_CONNECTED) ? "connected" :
                                (s_mode == NET_MODE_STA_CONNECTING) ? "connecting" : "disconnected";
    ble_prov_service_notify_net_status(ble_state_str, s_current_ssid, s_current_ip, evt.data.net.msg);
}

static void notify_state_changed_unlocked(void)
{
    notify_state_changed_with_msg_unlocked(NULL);
}

/**
 * @brief 持久化保存 Wi-Fi 配置至 /data/etc/wifi/wapi.conf
 */
static int save_wapi_conf(const char *ssid, const char *psk)
{
    struct stat st;
    if (stat("/data", &st) != 0) {
        /* /data 分区若未挂载，尝试写入当前工作区或忽略 */
        return -1;
    }

    mkdir("/data/etc", 0755);
    mkdir(WAPI_CONF_DIR, 0755);

    FILE *fp = fopen(WAPI_CONF_FILE, "w");
    if (!fp) {
        LOG_W(TAG, "无法写入 %s，请检查文件权限", WAPI_CONF_FILE);
        return -1;
    }

    fprintf(fp, "{\n  \"ssid\": \"%s\",\n  \"psk\": \"%s\",\n  \"bssid\": \"\"\n}\n",
            ssid ? ssid : "", psk ? psk : "");
    fclose(fp);
    sync();

    LOG_I(TAG, "已成功写入无线凭证至持久化文件: %s", WAPI_CONF_FILE);
    return 0;
}

bool net_is_valid_sta_ip(const char *ip)
{
    if (!ip || ip[0] == '\0') return false;
    if (strcmp(ip, "0.0.0.0") == 0) return false;
    if (strcmp(ip, "127.0.0.1") == 0) return false;
    if (strncmp(ip, "192.168.4.", 10) == 0) return false; /* 排除 SoftAP 独立热点网段 */
    if (strcmp(ip, "10.0.0.2") == 0) return false;        /* 排除 OpenVela 内核 netinit 默认静态占位 IP */
    if (strcmp(ip, "10.0.0.1") == 0) return false;        /* 排除内核默认网关占位 IP */
    if (strcmp(ip, "10.0.2.15") == 0) return false;       /* 排除 QEMU 模拟器虚拟网卡占位 IP */
    if (strncmp(ip, "169.254.", 8) == 0) return false;    /* 排除 APIPA 链路本地未获取到 DHCP 的临时 IP */
    return true;
}

#if !defined(HOST_TEST_RUNNER)
/**
 * @brief 通过网络套接字与 ioctl 查询指定接口的 IPv4 地址
 */
static int query_interface_ip(const char *ifname, char *ip_buf, size_t max_len)
{
    if (!ifname || !ip_buf || max_len == 0) return -1;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
        struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
        char *addr_str = inet_ntoa(sin->sin_addr);
        if (addr_str && strcmp(addr_str, "0.0.0.0") != 0 && strcmp(addr_str, "127.0.0.1") != 0) {
            snprintf(ip_buf, max_len, "%s", addr_str);
            close(sock);
            return 0;
        }
    }
    close(sock);
    return -1;
}

/**
 * @brief 校验底层网络接口是否处于 IFF_RUNNING 活跃态 (Link Up / Carrier Active)
 */
static bool net_is_interface_running(const char *ifname)
{
    if (!ifname || !ifname[0]) return false;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    bool running = false;
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
        running = ((ifr.ifr_flags & IFF_RUNNING) != 0);
    }
    close(sock);
    return running;
}

/**
 * @brief 校验底层网卡是否已真正关联到物理 AP (BSSID 非全 0)
 */
static bool net_is_ap_associated(const char *ifname)
{
    if (!ifname || !ifname[0]) return false;
    int sock = wapi_make_socket();
    if (sock < 0) return false;

    struct ether_addr_sub ap;
    memset(&ap, 0, sizeof(ap));
    int ret = wapi_get_ap(sock, ifname, (void *)&ap);
    close(sock);

    if (ret < 0) return false;
    static const uint8_t zero_mac[6] = {0};
    return (memcmp(ap.ether_addr_octet, zero_mac, 6) != 0);
}

/**
 * @brief 物理链路掉线看门狗守护线程 (Link Watchdog)
 * 周期性检测 wlan0 的 IFF_RUNNING 与 BSSID 状态，掉线时自动触发静默重连自愈
 */
static void* net_link_watchdog_thread(void *arg)
{
    (void)arg;
    int link_down_count = 0;

    while (s_watchdog_running) {
        sleep(5);
        if (!s_watchdog_running) break;

        pthread_mutex_lock(&s_lock);
        net_mode_t cur_mode = s_mode;
        bool in_worker = s_worker_running;
        pthread_mutex_unlock(&s_lock);

        /* 1. 仅在 STA_CONNECTED 且无配网 Worker 运行中时监控物理链路 */
        if (cur_mode == NET_MODE_STA_CONNECTED && !in_worker) {
            bool running = net_is_interface_running("wlan0");
            bool ap_ok   = net_is_ap_associated("wlan0");

            if (!running || !ap_ok) {
                link_down_count++;
                LOG_W(TAG, "[Watchdog] ⚠️ 检测到 wlan0 链路脱网 (Running=%d, AP=%d, 脱网计数 %d/3)...",
                      running, ap_ok, link_down_count);

                /* 立即触发底层静默快速重连自愈 */
                system("wapi reconnect wlan0 > /dev/null 2>&1");

                if (link_down_count >= 3) {
                    LOG_E(TAG, "[Watchdog] ❌ 链路连续 3 次检测脱网，切换为断开态并通知界面...");
                    pthread_mutex_lock(&s_lock);
                    s_mode = NET_MODE_DISCONNECTED;
                    s_current_ip[0] = '\0';
                    notify_state_changed_with_msg_unlocked("Wi-Fi 意外断开，正在尝试重连...");
                    pthread_mutex_unlock(&s_lock);
                    link_down_count = 0;
                }
            } else {
                if (link_down_count > 0) {
                    LOG_I(TAG, "[Watchdog] 🎉 wlan0 链路已自愈恢复正常！");
                    link_down_count = 0;
                }
            }
        } else if (cur_mode == NET_MODE_DISCONNECTED && !in_worker) {
            /* 2. 若当前为断开态，但底层重新恢复了 RUNNING 并且拿到了有效 IP，自动触发状态恢复 */
            char check_ip[NET_MAX_IP_LEN] = {0};
            if (query_interface_ip("wlan0", check_ip, sizeof(check_ip)) == 0 &&
                net_is_valid_sta_ip(check_ip) &&
                net_is_interface_running("wlan0") &&
                net_is_ap_associated("wlan0")) {
                pthread_mutex_lock(&s_lock);
                s_mode = NET_MODE_STA_CONNECTED;
                snprintf(s_current_ip, sizeof(s_current_ip), "%s", check_ip);
                LOG_I(TAG, "🎉 [Watchdog] Wi-Fi 重新自愈连通，物理 IP: [%s]", s_current_ip);
                notify_state_changed_with_msg_unlocked("Wi-Fi 连接成功");
                pthread_mutex_unlock(&s_lock);
            }
        }
    }
    return NULL;
}

typedef struct {
    char ssid[NET_MAX_SSID_LEN];
    char psk[NET_MAX_PSK_LEN];
} connect_param_t;

/**
 * @brief 真机异步连网工作线程
 */
static void* sta_connect_worker_thread(void *arg)
{
    connect_param_t *p = (connect_param_t *)arg;
    char target_ssid[NET_MAX_SSID_LEN];
    char target_psk[NET_MAX_PSK_LEN];
    strncpy(target_ssid, p->ssid, sizeof(target_ssid) - 1);
    strncpy(target_psk, p->psk, sizeof(target_psk) - 1);
    free(p);

    LOG_I(TAG, "[Worker] 开始向底层 WAPI 下发连接序列: SSID=[%s]", target_ssid);

    /* 1. 先关闭 softap 并断开旧连接，清空残留伪 IP */
    net_mgr_stop_softap();
    system("wapi disconnect wlan0 > /dev/null 2>&1");
    system("ifconfig wlan0 0.0.0.0 > /dev/null 2>&1");
    usleep(300000);

    pthread_mutex_lock(&s_lock);
    s_mode = NET_MODE_STA_CONNECTING;
    notify_state_changed_with_msg_unlocked("热点已关闭，正在关联 Wi-Fi...");
    pthread_mutex_unlock(&s_lock);

    /* 2. 切换 STA 模式并执行快速空中扫描，填充驱动底层 scanned_queue 候选列表 */
    system("wapi mode wlan0 2 > /dev/null 2>&1");
    system("wapi scan wlan0 > /dev/null 2>&1");
    usleep(800000);

    /* 3. 规范时序：必须先下发 PSK 加密秘钥 (CCMP+WPA2: 3 2)，再下发 ESSID 触发关联握手 */
    char cmd[256];
    if (target_psk[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "wapi psk wlan0 \"%s\" 3 2 > /dev/null 2>&1", target_psk);
        system(cmd);
    }
    snprintf(cmd, sizeof(cmd), "wapi essid wlan0 \"%s\" 1 > /dev/null 2>&1", target_ssid);
    system(cmd);
    system("wapi power_save wlan0 off > /dev/null 2>&1");
    system("wapi save_config wlan0 > /dev/null 2>&1");
    system("wapi reconnect wlan0 > /dev/null 2>&1");

    /* 等待 4 秒供无线网卡完成 AP 关联与信道对齐 */
    sleep(4);

    pthread_mutex_lock(&s_lock);
    notify_state_changed_with_msg_unlocked("目标 Wi-Fi 已关联，正在申请 DHCP IP 租约...");
    pthread_mutex_unlock(&s_lock);

    /* 4. DHCP 租约重试获取 IP (最多 5 次) */
    char acquired_ip[NET_MAX_IP_LEN] = {0};
    bool connected = false;

    for (int retry = 1; retry <= 5; retry++) {
        LOG_I(TAG, "[Worker] 尝试申请 DHCP 租约 (第 %d/5 次)...", retry);
        system("renew wlan0 > /dev/null 2>&1");
        sleep(2);

        if (query_interface_ip("wlan0", acquired_ip, sizeof(acquired_ip)) == 0 &&
            net_is_valid_sta_ip(acquired_ip) &&
            net_is_interface_running("wlan0") &&
            net_is_ap_associated("wlan0")) {
            connected = true;
            break;
        }
    }

    pthread_mutex_lock(&s_lock);
    s_worker_running = false;

    if (connected && acquired_ip[0] != '\0') {
        s_mode = NET_MODE_STA_CONNECTED;
        snprintf(s_current_ip, sizeof(s_current_ip), "%s", acquired_ip);
        LOG_I(TAG, "🎉 [Worker] Wi-Fi 成功连入局域网! 物理 IP: [%s]", s_current_ip);
        notify_state_changed_with_msg_unlocked("Wi-Fi 连接成功");

        bool web_en = s_web_enabled;
        pthread_mutex_unlock(&s_lock);

        if (web_en) {
            phoenix_web_portal_start(80, NULL);
            LOG_I(TAG, "🌐 局域网 Web 伴侣已启动: http://%s/ (或 :8080)", acquired_ip);
        }
    } else {
        LOG_W(TAG, "⚠️ [Worker] Wi-Fi 握手或 DHCP 超时，通知界面并自动恢复 SoftAP 独立热点");
        s_mode = NET_MODE_DISCONNECTED;
        notify_state_changed_with_msg_unlocked("Wi-Fi 连网失败(请核对密码与信号)，已恢复独立热点");
        pthread_mutex_unlock(&s_lock);

        /* 自动恢复独立热点供用户继续配网 */
        net_mgr_start_softap(NULL);
    }

    return NULL;
}

static void start_link_watchdog(void)
{
    if (!s_watchdog_running) {
        s_watchdog_running = true;
        pthread_attr_t w_attr;
        pthread_attr_init(&w_attr);
        pthread_attr_setstacksize(&w_attr, 8192);
        if (pthread_create(&s_watchdog_tid, &w_attr, net_link_watchdog_thread, NULL) == 0) {
            pthread_detach(s_watchdog_tid);
        }
        pthread_attr_destroy(&w_attr);
    }
}
#endif

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

    /* 1. 优先检查本地是否保存了 Wi-Fi SSID */
    char saved_ssid[NET_MAX_SSID_LEN] = {0};
    char saved_psk[NET_MAX_PSK_LEN] = {0};
    phoenix_config_get_str(PHOENIX_CFG_WIFI_SSID, "", saved_ssid, sizeof(saved_ssid));
    phoenix_config_get_str(PHOENIX_CFG_WIFI_PSK, "", saved_psk, sizeof(saved_psk));

#if !defined(HOST_TEST_RUNNER)
    /* 2. 若本地 config 无配置，尝试从全志持久化文件 /data/etc/wifi/wapi.conf 读取 */
    if (saved_ssid[0] == '\0') {
        FILE *fp = fopen(WAPI_CONF_FILE, "r");
        if (fp) {
            char fbuf[512] = {0};
            size_t n = fread(fbuf, 1, sizeof(fbuf) - 1, fp);
            fclose(fp);
            if (n > 0) {
                cJSON *root = cJSON_Parse(fbuf);
                if (root) {
                    cJSON *s = cJSON_GetObjectItem(root, "ssid");
                    cJSON *p = cJSON_GetObjectItem(root, "psk");
                    if (s && s->valuestring && s->valuestring[0] != '\0') {
                        strncpy(saved_ssid, s->valuestring, sizeof(saved_ssid) - 1);
                        phoenix_config_set_str(PHOENIX_CFG_WIFI_SSID, saved_ssid);
                    }
                    if (p && p->valuestring) {
                        strncpy(saved_psk, p->valuestring, sizeof(saved_psk) - 1);
                        phoenix_config_set_str(PHOENIX_CFG_WIFI_PSK, saved_psk);
                    }
                    cJSON_Delete(root);
                }
            }
        }
    }

    /* 3. 若本地已有凭证，检查物理网卡 wlan0 是否已经由系统开机流程分配了合法局域网 IP */
    if (saved_ssid[0] != '\0') {
        char existing_ip[NET_MAX_IP_LEN] = {0};
        if (query_interface_ip("wlan0", existing_ip, sizeof(existing_ip)) == 0 &&
            net_is_valid_sta_ip(existing_ip) &&
            net_is_interface_running("wlan0") &&
            net_is_ap_associated("wlan0")) {
            s_mode = NET_MODE_STA_CONNECTED;
            snprintf(s_current_ip, sizeof(s_current_ip), "%s", existing_ip);
            snprintf(s_current_ssid, sizeof(s_current_ssid), "%s", saved_ssid);
            LOG_I(TAG, "检测到 wlan0 已经就绪并持有局域网 IP: [%s], SSID: [%s]", s_current_ip, s_current_ssid);
            notify_state_changed_unlocked();

            bool web_en = s_web_enabled;
            pthread_mutex_unlock(&s_lock);

            if (web_en) {
                phoenix_web_portal_start(80, NULL);
                LOG_I(TAG, "🌐 局域网 Web 伴侣已启动: http://%s/ (或 :8080)", existing_ip);
            }
            start_link_watchdog();
            return 0;
        }
    } else {
        /* 本地无凭证，清除 NuttX netinit 默认赋给 wlan0 的 10.0.0.2 伪静态 IP (微秒级原生调用，避免阻塞) */
        int sock = wapi_make_socket();
        if (sock >= 0) {
            struct in_addr zero_ip;
            zero_ip.s_addr = 0;
            wapi_set_ip(sock, "wlan0", &zero_ip);
            close(sock);
        }
    }
    start_link_watchdog();
#endif

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

#if !defined(HOST_TEST_RUNNER)
    s_watchdog_running = false;
#endif

    s_mode = NET_MODE_DISCONNECTED;
    s_current_ip[0] = '\0';
    s_current_ssid[0] = '\0';
    s_initialized = false;
    pthread_mutex_unlock(&s_lock);

    phoenix_web_portal_stop();
    net_mgr_stop_softap();
    LOG_I(TAG, "网络管理器已安全销毁");
}

#if !defined(HOST_TEST_RUNNER)
/* ========================================================================= */
/*                   内嵌微型 DHCP Server 守护服务 (Mini DHCPD)                */
/* ========================================================================= */

#pragma pack(push, 1)
typedef struct {
    uint8_t  op;           /* 1: BOOTREQUEST, 2: BOOTREPLY */
    uint8_t  htype;        /* 1: Ethernet */
    uint8_t  hlen;         /* 6 */
    uint8_t  hops;         /* 0 */
    uint32_t xid;          /* 客户端事务 ID */
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;       /* 客户端分配 IP */
    uint32_t siaddr;       /* 下一阶段服务器 IP */
    uint32_t giaddr;
    uint8_t  chaddr[16];   /* 客户端 MAC 地址 */
    uint8_t  sname[64];
    uint8_t  file[128];
    uint8_t  magic[4];     /* 99, 130, 83, 99 */
    uint8_t  options[308]; /* DHCP 可选字段 */
} mini_dhcp_msg_t;
#pragma pack(pop)

static pthread_t      s_dhcp_tid = 0;
static volatile bool  s_dhcp_running = false;
static int            s_dhcp_sock = -1;
static int            s_dns_sock = -1;

static void handle_dns_packet(void)
{
    if (s_dns_sock < 0) return;

    uint8_t dns_buf[512];
    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    ssize_t dn = recvfrom(s_dns_sock, dns_buf, sizeof(dns_buf), 0, (struct sockaddr *)&caddr, &clen);
    if (dn < 12) return;

    uint16_t flags = ntohs(*(uint16_t *)&dns_buf[2]);
    uint16_t qdcount = ntohs(*(uint16_t *)&dns_buf[4]);

    /* 仅处理标准查询 (Opcode=0) */
    if (qdcount < 1 || ((flags >> 11) & 0x0F) != 0) return;

    /* 跳过 QNAME */
    int pos = 12;
    while (pos < dn && dns_buf[pos] != 0) {
        pos += (int)dns_buf[pos] + 1;
    }
    pos++; /* 跳过 0 字节结尾 */
    if (pos + 4 > dn) return;

    uint16_t qtype = ntohs(*(uint16_t *)&dns_buf[pos]);
    pos += 4;

    uint8_t resp_dns[512];
    memcpy(resp_dns, dns_buf, pos); /* 复制 Header 与 Question */

    uint16_t *r_flags = (uint16_t *)&resp_dns[2];
    uint16_t *r_ancount = (uint16_t *)&resp_dns[6];
    uint16_t *r_nscount = (uint16_t *)&resp_dns[8];
    uint16_t *r_arcount = (uint16_t *)&resp_dns[10];
    *r_nscount = 0;
    *r_arcount = 0;

    if (qtype == 28 /* AAAA (IPv6) */) {
        /* 对 IPv6 AAAA 查询返回 NOERROR 空记录，促使手机立即走 IPv4 访问 */
        *r_flags = htons(0x8180);
        *r_ancount = htons(0);
        sendto(s_dns_sock, resp_dns, pos, 0, (struct sockaddr *)&caddr, clen);
    } else if (qtype == 1 /* A (IPv4) */ || qtype == 255 /* ANY */) {
        /* 劫持全域名解析返回 192.168.4.1 */
        *r_flags = htons(0x8180);
        *r_ancount = htons(1);

        int rpos = pos;
        /* Answer: Name Pointer 0xC00C (指向 Question 中的 QNAME) */
        resp_dns[rpos++] = 0xc0;
        resp_dns[rpos++] = 0x0c;
        /* Type: A (1) */
        resp_dns[rpos++] = 0x00;
        resp_dns[rpos++] = 0x01;
        /* Class: IN (1) */
        resp_dns[rpos++] = 0x00;
        resp_dns[rpos++] = 0x01;
        /* TTL: 60s */
        resp_dns[rpos++] = 0x00;
        resp_dns[rpos++] = 0x00;
        resp_dns[rpos++] = 0x00;
        resp_dns[rpos++] = 0x3c;
        /* RDLENGTH: 4 */
        resp_dns[rpos++] = 0x00;
        resp_dns[rpos++] = 0x04;
        /* RDATA: 192.168.4.1 */
        uint32_t ip4 = inet_addr("192.168.4.1");
        memcpy(&resp_dns[rpos], &ip4, 4);
        rpos += 4;

        sendto(s_dns_sock, resp_dns, rpos, 0, (struct sockaddr *)&caddr, clen);
    }
}

static void* mini_dhcpd_thread(void *arg)
{
    (void)arg;
    LOG_I(TAG, "🐣 [MiniDHCP+DNS] 轻量级 DHCP & Captive DNS 服务启动 (监听 UDP 67 & 53)...");

    s_dhcp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_dhcp_sock < 0) {
        LOG_W(TAG, "⚠️ [MiniDHCP] 无法创建 DHCP UDP 套接字");
        s_dhcp_running = false;
        return NULL;
    }

    int opt = 1;
    setsockopt(s_dhcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(s_dhcp_sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

#if defined(SO_BINDTODEVICE)
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "wlan1", IFNAMSIZ - 1);
    setsockopt(s_dhcp_sock, SOL_SOCKET, SO_BINDTODEVICE, (char *)&ifr, sizeof(ifr));
#endif

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(67);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s_dhcp_sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        LOG_W(TAG, "⚠️ [MiniDHCP] 绑定 67 端口失败 (可能已被占用或权限受限)");
        close(s_dhcp_sock);
        s_dhcp_sock = -1;
        s_dhcp_running = false;
        return NULL;
    }

    /* 初始化 Captive DNS (UDP 53) */
    s_dns_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_dns_sock >= 0) {
        setsockopt(s_dns_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#if defined(SO_BINDTODEVICE)
        setsockopt(s_dns_sock, SOL_SOCKET, SO_BINDTODEVICE, (char *)&ifr, sizeof(ifr));
#endif
        struct sockaddr_in dns_saddr;
        memset(&dns_saddr, 0, sizeof(dns_saddr));
        dns_saddr.sin_family = AF_INET;
        dns_saddr.sin_port = htons(53);
        dns_saddr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s_dns_sock, (struct sockaddr *)&dns_saddr, sizeof(dns_saddr)) < 0) {
            LOG_W(TAG, "⚠️ [CaptiveDNS] 绑定 53 端口失败 (可能权限受限)");
            close(s_dns_sock);
            s_dns_sock = -1;
        } else {
            LOG_I(TAG, "🎯 [CaptiveDNS] UDP 53 就绪，全域名劫持指向 192.168.4.1 (锁定 Wi-Fi)");
        }
    }

    uint8_t buf[576];
    while (s_dhcp_running) {
        struct pollfd pfds[2];
        int pfd_cnt = 0;
        int idx_dhcp = -1, idx_dns = -1;

        if (s_dhcp_sock >= 0) {
            pfds[pfd_cnt].fd = s_dhcp_sock;
            pfds[pfd_cnt].events = POLLIN;
            pfds[pfd_cnt].revents = 0;
            idx_dhcp = pfd_cnt++;
        }
        if (s_dns_sock >= 0) {
            pfds[pfd_cnt].fd = s_dns_sock;
            pfds[pfd_cnt].events = POLLIN;
            pfds[pfd_cnt].revents = 0;
            idx_dns = pfd_cnt++;
        }

        if (pfd_cnt == 0) break;

        int poll_ret = poll(pfds, pfd_cnt, 1000);
        if (poll_ret <= 0) continue;

        /* 1. 处理 DNS 请求 */
        if (idx_dns >= 0 && (pfds[idx_dns].revents & POLLIN)) {
            handle_dns_packet();
        }

        /* 2. 处理 DHCP 请求 */
        if (idx_dhcp >= 0 && (pfds[idx_dhcp].revents & POLLIN)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            ssize_t n = recvfrom(s_dhcp_sock, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &addr_len);
            if (n < (ssize_t)(sizeof(mini_dhcp_msg_t) - 308)) {
                continue;
            }

            mini_dhcp_msg_t *req = (mini_dhcp_msg_t *)buf;
            if (req->op != 1) continue; /* 仅处理 BOOTREQUEST */
            if (req->magic[0] != 99 || req->magic[1] != 130 || req->magic[2] != 83 || req->magic[3] != 99) {
                continue;
            }

            /* 解析 Option 53 (Message Type) */
            uint8_t msg_type = 0;
            int opt_idx = 0;
            int max_opt = n - (int)(sizeof(mini_dhcp_msg_t) - 308);
            if (max_opt > 308) max_opt = 308;

            while (opt_idx < max_opt) {
                uint8_t code = req->options[opt_idx++];
                if (code == 0) continue; /* PAD */
                if (code == 255) break;  /* END */
                if (opt_idx >= max_opt) break;
                uint8_t len = req->options[opt_idx++];
                if (opt_idx + len > max_opt) break;
                if (code == 53 && len >= 1) {
                    msg_type = req->options[opt_idx];
                }
                opt_idx += len;
            }

            /* 仅对 DISCOVER (1) 和 REQUEST (3) 响应 */
            if (msg_type != 1 && msg_type != 3) {
                continue;
            }

            /* 构造回复报文 (DHCPOFFER 或 DHCPACK) */
            mini_dhcp_msg_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.op = 2; /* BOOTREPLY */
            resp.htype = req->htype;
            resp.hlen = req->hlen;
            resp.xid = req->xid;
            resp.flags = htons(0x8000); /* 规范化强制广播标志位 */
            resp.yiaddr = inet_addr("192.168.4.100"); /* 固定向连入设备下发 192.168.4.100 */
            resp.siaddr = inet_addr("192.168.4.1");
            memcpy(resp.chaddr, req->chaddr, 16);
            resp.magic[0] = 99; resp.magic[1] = 130; resp.magic[2] = 83; resp.magic[3] = 99;

            int o = 0;
            /* Option 53: Message Type */
            resp.options[o++] = 53;
            resp.options[o++] = 1;
            resp.options[o++] = (msg_type == 1) ? 2 /* OFFER */ : 5 /* ACK */;

            /* Option 54: Server Identifier (192.168.4.1) */
            resp.options[o++] = 54;
            resp.options[o++] = 4;
            uint32_t server_ip = inet_addr("192.168.4.1");
            memcpy(&resp.options[o], &server_ip, 4);
            o += 4;

            /* Option 51: Lease Time (86400 秒) */
            resp.options[o++] = 51;
            resp.options[o++] = 4;
            uint32_t lease = htonl(86400);
            memcpy(&resp.options[o], &lease, 4);
            o += 4;

            /* Option 1: Subnet Mask (255.255.255.0) */
            resp.options[o++] = 1;
            resp.options[o++] = 4;
            uint32_t mask = inet_addr("255.255.255.0");
            memcpy(&resp.options[o], &mask, 4);
            o += 4;

            /* Option 3: Router / Gateway (192.168.4.1) */
            resp.options[o++] = 3;
            resp.options[o++] = 4;
            memcpy(&resp.options[o], &server_ip, 4);
            o += 4;

            /* Option 6: DNS Server (192.168.4.1) */
            resp.options[o++] = 6;
            resp.options[o++] = 4;
            memcpy(&resp.options[o], &server_ip, 4);
            o += 4;

            /* Option 255: End */
            resp.options[o++] = 255;

            size_t resp_len = (sizeof(mini_dhcp_msg_t) - 308) + o;
            /* 遵循 RFC 1542 规范填充至至少 300 字节 */
            if (resp_len < 300) {
                resp_len = 300;
            }

            /* MiniDHCP 双重广播投递：同时向 255.255.255.255:68 与子网定向 192.168.4.255:68 投递 */
            struct sockaddr_in bcast_addr;
            memset(&bcast_addr, 0, sizeof(bcast_addr));
            bcast_addr.sin_family = AF_INET;
            bcast_addr.sin_port = htons(68);
            bcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            sendto(s_dhcp_sock, &resp, resp_len, 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));

            struct sockaddr_in subnet_bcast;
            memset(&subnet_bcast, 0, sizeof(subnet_bcast));
            subnet_bcast.sin_family = AF_INET;
            subnet_bcast.sin_port = htons(68);
            subnet_bcast.sin_addr.s_addr = inet_addr("192.168.4.255");
            sendto(s_dhcp_sock, &resp, resp_len, 0, (struct sockaddr *)&subnet_bcast, sizeof(subnet_bcast));

            LOG_I(TAG, "📡 [MiniDHCP] 已双重广播响应 %s 给客户端 MAC [%02X:%02X:%02X:%02X:%02X:%02X]，分配 IP: 192.168.4.100",
                  (msg_type == 1) ? "DHCPOFFER" : "DHCPACK",
                  req->chaddr[0], req->chaddr[1], req->chaddr[2],
                  req->chaddr[3], req->chaddr[4], req->chaddr[5]);
        }
    }

    if (s_dhcp_sock >= 0) {
        close(s_dhcp_sock);
        s_dhcp_sock = -1;
    }
    if (s_dns_sock >= 0) {
        close(s_dns_sock);
        s_dns_sock = -1;
    }
    LOG_I(TAG, "💤 [MiniDHCP+DNS] 服务已正常退出");
    return NULL;
}

static void mini_dhcpd_start(void)
{
    if (s_dhcp_running) return;
    s_dhcp_running = true;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8192);
    pthread_t tid = 0;
    if (pthread_create(&tid, &attr, mini_dhcpd_thread, NULL) == 0) {
        pthread_detach(tid);
        s_dhcp_tid = tid;
    }
    pthread_attr_destroy(&attr);
}

static void mini_dhcpd_stop(void)
{
    if (!s_dhcp_running) return;
    s_dhcp_running = false;

    /* 发送本地 UDP 唤醒报文打破 poll() 阻塞，促使 mini_dhcpd_thread 立即优雅退出 */
    int wake_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (wake_sock >= 0) {
        struct sockaddr_in to;
        memset(&to, 0, sizeof(to));
        to.sin_family = AF_INET;
        to.sin_port = htons(53);
        to.sin_addr.s_addr = inet_addr("127.0.0.1");
        char dummy = 0;
        sendto(wake_sock, &dummy, 1, 0, (struct sockaddr *)&to, sizeof(to));
        close(wake_sock);
    }
    s_dhcp_tid = 0;
}
#endif

int net_mgr_stop_softap(void)
{
    LOG_I(TAG, "🛑 关闭 SoftAP 独立热点 (wlan1) 与内嵌 DHCP 服务...");
#if !defined(HOST_TEST_RUNNER)
    mini_dhcpd_stop();

    int sock = wapi_make_socket();
    if (sock >= 0) {
        wapi_set_essid(sock, "wlan1", "", WAPI_ESSID_OFF);
        wapi_set_ifdown(sock, "wlan1");
        close(sock);
    } else {
        system("wapi essid wlan1 \"\" 0 > /dev/null 2>&1");
        system("ifconfig wlan1 down > /dev/null 2>&1");
    }
#endif
    return 0;
}

static bool s_softap_suspended = false;

int net_mgr_suspend_softap(void)
{
    pthread_mutex_lock(&s_lock);
    if (s_mode == NET_MODE_SOFTAP_CONFIG) {
        LOG_I(TAG, "📶 [Coex] BLE 建立 GATT 连接，主动挂起 SoftAP 广播以让出单天线射频...");
        s_softap_suspended = true;
        pthread_mutex_unlock(&s_lock);
        net_mgr_stop_softap();
        return 0;
    }
    pthread_mutex_unlock(&s_lock);
    return 0;
}

int net_mgr_resume_softap(void)
{
    pthread_mutex_lock(&s_lock);
    if (s_softap_suspended && s_mode == NET_MODE_SOFTAP_CONFIG) {
        LOG_I(TAG, "📶 [Coex] 蓝牙连接关闭/降级，恢复 SoftAP 热点广播供备用配网...");
        s_softap_suspended = false;
        char ssid[NET_MAX_SSID_LEN];
        strncpy(ssid, s_current_ssid, sizeof(ssid) - 1);
        ssid[sizeof(ssid) - 1] = '\0';
        pthread_mutex_unlock(&s_lock);
        return net_mgr_start_softap(ssid);
    }
    s_softap_suspended = false;
    pthread_mutex_unlock(&s_lock);
    return 0;
}

#if !defined(HOST_TEST_RUNNER)
static void* softap_worker_thread(void *arg)
{
    (void)arg;
    char ssid[NET_MAX_SSID_LEN];
    pthread_mutex_lock(&s_lock);
    strncpy(ssid, s_current_ssid, sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
    pthread_mutex_unlock(&s_lock);

    LOG_I(TAG, "[SoftAP:Worker] 正在后台配置 wlan1 SoftAP 物理网卡与射频 (SSID: %s)...", ssid);

    /* 1. 先关闭旧的热点 (wlan1) 并保持 wlan0 监听态就绪 */
    net_mgr_stop_softap();

    int sock = wapi_make_socket();
    if (sock >= 0) {
        /* 2. 原生 WAPI C API 配置 wlan1 Master 与固定 IP 192.168.4.1、信道 6 (2437MHz) */
        struct in_addr ip, mask;
        inet_aton("192.168.4.1", &ip);
        inet_aton("255.255.255.0", &mask);
        wapi_set_ip(sock, "wlan1", &ip);
        wapi_set_netmask(sock, "wlan1", &mask);
        wapi_set_ifup(sock, "wlan1");
        wapi_set_mode(sock, "wlan1", WAPI_MODE_MASTER);
        wapi_set_freq(sock, "wlan1", 2437, 1);
        wapi_set_essid(sock, "wlan1", ssid, WAPI_ESSID_ON);
        close(sock);
        LOG_I(TAG, "⚡ [Native WAPI] SoftAP wlan1 (SSID: %s, Ch: 6, IP: 192.168.4.1, Mode: OPEN) 射频已就绪", ssid);
    }

    /* 3. 补充标准命令行确保全志底层 Realtek wlan1 属性与路由生效 */
    system("ifconfig wlan1 192.168.4.1 netmask 255.255.255.0 up > /dev/null 2>&1");
    system("wapi mode wlan1 3 > /dev/null 2>&1");
    system("wapi freq wlan1 2437 1 > /dev/null 2>&1");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wapi essid wlan1 \"%s\" 1 > /dev/null 2>&1", ssid);
    system(cmd);

    /* 确保 wlan0 处于唤醒监听状态，关闭自适应与省电，为空中扫描留出稳定环境 */
    system("wapi private wlan0 adaptivity 0 > /dev/null 2>&1");
    system("wapi power_save wlan0 off > /dev/null 2>&1");

    /* 4. 启动内嵌 MiniDHCP+DNS 服务 (Web 服务在开机时已常驻监听 0.0.0.0:80，无需重启) */
    mini_dhcpd_start();

    LOG_I(TAG, "📡 [SoftAP:Worker] 热点广播与内嵌 MiniDHCP 服务已就绪");
    return NULL;
}
#endif

int net_mgr_start_softap(const char *custom_ssid)
{
    pthread_mutex_lock(&s_lock);
    const char *ssid = (custom_ssid && custom_ssid[0]) ? custom_ssid : NET_DEFAULT_SOFTAP_SSID;

    s_mode = NET_MODE_SOFTAP_CONFIG;
    s_web_enabled = true;
    snprintf(s_current_ssid, sizeof(s_current_ssid), "%s", ssid);
    snprintf(s_current_ip, sizeof(s_current_ip), "%s", NET_DEFAULT_SOFTAP_IP);

    LOG_I(TAG, "📡 启动 SoftAP: SSID=[%s], 网关=[192.168.4.1], Web免端口直达: [http://192.168.4.1/]", 
          s_current_ssid);

#if !defined(HOST_TEST_RUNNER)
    pthread_t softap_tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 16384);
    pthread_create(&softap_tid, &attr, softap_worker_thread, NULL);
    pthread_attr_destroy(&attr);
    pthread_detach(softap_tid);
#else
    phoenix_web_portal_start(80, NULL);
#endif

    notify_state_changed_unlocked();
    pthread_mutex_unlock(&s_lock);
    return 0;
}

int net_mgr_connect_sta(const char *ssid, const char *psk)
{
    if (!ssid || !ssid[0]) return -1;

    pthread_mutex_lock(&s_lock);
    if (s_mode == NET_MODE_STA_CONNECTING || s_worker_running) {
        pthread_mutex_unlock(&s_lock);
        LOG_W(TAG, "Wi-Fi 正在连接中，忽略重复连接请求");
        return -2;
    }

    s_mode = NET_MODE_STA_CONNECTING;
    snprintf(s_current_ssid, sizeof(s_current_ssid), "%.31s", ssid);
    LOG_I(TAG, "正在连接 Wi-Fi: [%s]...", s_current_ssid);
    notify_state_changed_unlocked();

    /* 1. 持久化 Wi-Fi 凭证至系统 Config */
    phoenix_config_set_str(PHOENIX_CFG_WIFI_SSID, s_current_ssid);
    if (psk) {
        char safe_psk[NET_MAX_PSK_LEN] = {0};
        snprintf(safe_psk, sizeof(safe_psk), "%.63s", psk);
        phoenix_config_set_str(PHOENIX_CFG_WIFI_PSK, safe_psk);
    }

    /* 2. 持久化至全志原厂 /data/etc/wifi/wapi.conf 配置文件 */
    save_wapi_conf(s_current_ssid, psk);
    phoenix_config_save();

#if defined(HOST_TEST_RUNNER)
    /* Host 单测模式：同步完成以便单元测试断言 */
    s_mode = NET_MODE_STA_CONNECTED;
    snprintf(s_current_ip, sizeof(s_current_ip), "192.168.1.108");
    LOG_I(TAG, "✅ [Host] Wi-Fi 连接成功! 分配 IP: [%s]", s_current_ip);
    notify_state_changed_unlocked();

    bool web_en = s_web_enabled;
    pthread_mutex_unlock(&s_lock);

    if (web_en) {
        phoenix_web_portal_start(8080, NULL);
    }
    return 0;
#else
    /* 真机模式：启动后台异步工作线程下发 WAPI 序列，防止阻塞 LVGL 界面 */
    s_worker_running = true;
    connect_param_t *param = (connect_param_t *)malloc(sizeof(connect_param_t));
    if (param) {
        strncpy(param->ssid, s_current_ssid, sizeof(param->ssid) - 1);
        if (psk) strncpy(param->psk, psk, sizeof(param->psk) - 1);
        else param->psk[0] = '\0';
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 16384);
        pthread_create(&s_connect_tid, &attr, sta_connect_worker_thread, param);
        pthread_attr_destroy(&attr);
        pthread_detach(s_connect_tid);
    } else {
        s_worker_running = false;
    }
    pthread_mutex_unlock(&s_lock);
    return 0;
#endif
}

void net_mgr_disconnect(void)
{
    pthread_mutex_lock(&s_lock);
    s_mode = NET_MODE_DISCONNECTED;
    s_current_ip[0] = '\0';
    s_current_ssid[0] = '\0';

#if !defined(HOST_TEST_RUNNER)
    int sock = wapi_make_socket();
    if (sock >= 0) {
        wpa_driver_wext_disconnect(sock, "wlan0");
        close(sock);
    } else {
        system("wapi disconnect wlan0 > /dev/null 2>&1");
    }
    net_mgr_stop_softap();
#endif

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
    phoenix_config_save();

#if !defined(HOST_TEST_RUNNER)
    /* 1. 彻底断开 wlan0 当前物理关联，避免后台重试干扰射频 */
    int sock = wapi_make_socket();
    if (sock >= 0) {
        wpa_driver_wext_disconnect(sock, "wlan0");
        close(sock);
    }
    system("wapi disconnect wlan0 > /dev/null 2>&1");
    system("ifconfig wlan0 0.0.0.0 > /dev/null 2>&1");

    /* 2. 彻底清空全志板载持久化文件 /data/etc/wifi/wapi.conf 并刷盘 */
    unlink(WAPI_CONF_FILE);
    FILE *fp = fopen(WAPI_CONF_FILE, "w");
    if (fp) {
        fputs("{\n  \"ssid\": \"\",\n  \"psk\": \"\",\n  \"bssid\": \"\"\n}\n", fp);
        fclose(fp);
    }
    sync();
    LOG_I(TAG, "🧹 已彻底抹除持久化配置 /data/etc/wifi/wapi.conf");
#endif

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

#if !defined(HOST_TEST_RUNNER)
static int do_wapi_scan_on_if(int sock, const char *ifname, net_wifi_ap_info_t *aps_out, size_t max_count)
{
    if (!ifname || !aps_out || max_count == 0) return -1;

    /* 1. 确保目标网卡处于激活 UP 状态与 Managed 模式 */
    wapi_set_mode(sock, ifname, WAPI_MODE_MANAGED);
    wapi_set_ifup(sock, ifname);

    /* 2. 触发空中主动探针扫描 (Active Scan) */
    int ret = wapi_scan_init(sock, ifname, NULL);
    if (ret < 0) {
        LOG_W(TAG, "wapi_scan_init on %s failed: %d", ifname, ret);
        return -1;
    }

    /* 3. 核心时序保护：全志 Realtek 芯片遍历 1~13 信道探针需物理时间，
     * 先强制等待 1.2 秒给底层射频空中抓包，彻底杜绝第 0 毫秒 wapi_scan_stat 早退误判 */
    usleep(1200 * 1000);

    /* 4. 动态轮询等待驱动数据最终落盘就绪 (最多再等 1.5 秒，每 150ms 轮询一次) */
    int tries = 10;
    while (--tries > 0) {
        ret = wapi_scan_stat(sock, ifname);
        if (ret == 0) {
            /* 扫描数据已就绪 */
            break;
        }
        usleep(150 * 1000);
    }

    /* 5. 收集驱动扫描结果 */
    struct wapi_list_s list;
    memset(&list, 0, sizeof(list));
    ret = wapi_scan_coll(sock, ifname, &list);
    if (ret != 0 || list.head.scan == NULL) {
        return 0;
    }

    size_t real_count = 0;
    struct wapi_scan_info_s *info = list.head.scan;
    while (info != NULL && real_count < max_count) {
        if (info->has_essid && info->essid[0] != '\0') {
            /* 检查同名 SSID 去重 (双频 2.4G/5G 保留最强信号) */
            int dup_idx = -1;
            for (size_t i = 0; i < real_count; i++) {
                if (strcmp(aps_out[i].ssid, info->essid) == 0) {
                    dup_idx = (int)i;
                    break;
                }
            }

            int rssi_val = info->has_rssi ? info->rssi : -75;
            const char *auth_type = (info->has_encode && info->encode != 0) ? "WPA2" : "OPEN";

            if (dup_idx >= 0) {
                if (rssi_val > aps_out[dup_idx].rssi) {
                    aps_out[dup_idx].rssi = (int16_t)rssi_val;
                    strncpy(aps_out[dup_idx].auth, auth_type, sizeof(aps_out[dup_idx].auth) - 1);
                }
            } else {
                strncpy(aps_out[real_count].ssid, info->essid, sizeof(aps_out[real_count].ssid) - 1);
                aps_out[real_count].rssi = (int16_t)rssi_val;
                strncpy(aps_out[real_count].auth, auth_type, sizeof(aps_out[real_count].auth) - 1);
                real_count++;
            }
        }
        info = info->next;
    }

    wapi_scan_coll_free(&list);
    return (int)real_count;
}
#endif

int net_mgr_scan_wifi(net_wifi_ap_info_t *aps_out, size_t max_count)
{
    if (!aps_out || max_count == 0) return -1;

#if !defined(HOST_TEST_RUNNER)
    int sock = wapi_make_socket();
    if (sock >= 0) {
        LOG_I(TAG, "🔍 正在通过物理网卡 wlan0 发起实时空中 Wi-Fi 扫描...");

        /* 仅在 STA 网卡 wlan0 上执行全信道物理扫描，保护 SoftAP 网卡 wlan1 射频稳定 */
        int count = do_wapi_scan_on_if(sock, "wlan0", aps_out, max_count);

        close(sock);

        if (count > 0) {
            /* 按信号强度从强到弱排序 (RSSI 降序) */
            for (int i = 0; i < count - 1; i++) {
                for (int j = 0; j < count - 1 - i; j++) {
                    if (aps_out[j].rssi < aps_out[j + 1].rssi) {
                        net_wifi_ap_info_t tmp = aps_out[j];
                        aps_out[j] = aps_out[j + 1];
                        aps_out[j + 1] = tmp;
                    }
                }
            }
            LOG_I(TAG, "📡 真实 Wi-Fi 扫描成功，捕获周边 %d 个活跃真实热点", count);
            return count;
        }

        LOG_W(TAG, "⚠️ 空中周边未捕获到真实 Wi-Fi 信号");
        return 0;
    }
    return 0;
#else
    /* 宿主机仿真测试 (HOST_TEST_RUNNER) 基准测试热点 */
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

    LOG_I(TAG, "📡 Wi-Fi 扫描完成 (宿主机测试模式)，返回 %zu 个测试热点", count);
    return (int)count;
#endif
}
