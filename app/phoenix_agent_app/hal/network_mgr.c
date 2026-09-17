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
int  wapi_scan_init(int sock, const char *ifname);
int  wapi_scan_stat(int sock, const char *ifname);
int  wapi_scan_coll(int sock, const char *ifname, struct wapi_list_s *list);
void wapi_scan_coll_free(struct wapi_list_s *list);
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

    /* 1. 先关闭 softap 并断开旧连接 */
    net_mgr_stop_softap();
    system("wapi disconnect wlan0 > /dev/null 2>&1");
    usleep(300000);

    pthread_mutex_lock(&s_lock);
    s_mode = NET_MODE_STA_CONNECTING;
    notify_state_changed_with_msg_unlocked("热点已关闭，正在关联 Wi-Fi...");
    pthread_mutex_unlock(&s_lock);

    /* 2. 原生 WAPI C API 下发 STA 连接序列 */
    int sock = wapi_make_socket();
    if (sock >= 0) {
        wapi_set_mode(sock, "wlan0", WAPI_MODE_MANAGED);
        if (target_psk[0] != '\0') {
            wpa_driver_wext_set_auth_param(sock, "wlan0", IW_AUTH_WPA_VERSION, IW_AUTH_WPA_VERSION_WPA2);
            wpa_driver_wext_set_auth_param(sock, "wlan0", IW_AUTH_CIPHER_PAIRWISE, IW_AUTH_CIPHER_CCMP);
            wpa_driver_wext_set_key_ext(sock, "wlan0", WPA_ALG_CCMP, target_psk, strlen(target_psk));
        }
        wapi_set_essid(sock, "wlan0", target_ssid, WAPI_ESSID_ON);
        close(sock);
        LOG_I(TAG, "⚡ [Worker] 已通过原生 WAPI C API 触发 STA 握手");
    }

    /* 3. 补全标准命令行指令序列 (关闭自适应与省电，保存并重连) */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wapi essid wlan0 \"%s\" 1 > /dev/null 2>&1", target_ssid);
    system(cmd);
    if (target_psk[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "wapi psk wlan0 \"%s\" 1 3 > /dev/null 2>&1", target_psk);
        system(cmd);
    }
    system("wapi private wlan0 adaptivity 0 > /dev/null 2>&1");
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

        if (query_interface_ip("wlan0", acquired_ip, sizeof(acquired_ip)) == 0) {
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

#if !defined(HOST_TEST_RUNNER)
    /* 1. 优先检查网卡 wlan0 是否已经由系统脚本分配了 IP */
    char existing_ip[NET_MAX_IP_LEN] = {0};
    if (query_interface_ip("wlan0", existing_ip, sizeof(existing_ip)) == 0 &&
        strcmp(existing_ip, NET_DEFAULT_SOFTAP_IP) != 0) {
        s_mode = NET_MODE_STA_CONNECTED;
        snprintf(s_current_ip, sizeof(s_current_ip), "%s", existing_ip);
        phoenix_config_get_str(PHOENIX_CFG_WIFI_SSID, "Connected-WiFi", s_current_ssid, sizeof(s_current_ssid));
        LOG_I(TAG, "检测到 wlan0 已经就绪并持有局域网 IP: [%s]", s_current_ip);
        notify_state_changed_unlocked();

        bool web_en = s_web_enabled;
        pthread_mutex_unlock(&s_lock);

        if (web_en) {
            phoenix_web_portal_start(80, NULL);
            LOG_I(TAG, "🌐 局域网 Web 伴侣已启动: http://%s/ (或 :8080)", existing_ip);
        }
        return 0;
    }
#endif

    /* 2. 检查本地是否保存了 Wi-Fi SSID */
    char saved_ssid[NET_MAX_SSID_LEN] = {0};
    char saved_psk[NET_MAX_PSK_LEN] = {0};
    phoenix_config_get_str(PHOENIX_CFG_WIFI_SSID, "", saved_ssid, sizeof(saved_ssid));
    phoenix_config_get_str(PHOENIX_CFG_WIFI_PSK, "", saved_psk, sizeof(saved_psk));

#if !defined(HOST_TEST_RUNNER)
    /* 3. 若本地 config 无配置，尝试从全志持久化文件 /data/etc/wifi/wapi.conf 读取 */
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

static void* mini_dhcpd_thread(void *arg)
{
    (void)arg;
    LOG_I(TAG, "🐣 [MiniDHCP] 轻量级 DHCP 服务启动 (监听 0.0.0.0:67)...");

    s_dhcp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_dhcp_sock < 0) {
        LOG_W(TAG, "⚠️ [MiniDHCP] 无法创建 UDP 套接字");
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

    /* 设置 1 秒超时以便响应退出信号 */
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(s_dhcp_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

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

    uint8_t buf[576];
    while (s_dhcp_running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        ssize_t n = recvfrom(s_dhcp_sock, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (n < (ssize_t)(sizeof(mini_dhcp_msg_t) - 308)) {
            continue; /* 超时或非标准报文 */
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
        resp.flags = req->flags;
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

        /* 广播发送回给客户端端口 68 */
        struct sockaddr_in bcast_addr;
        memset(&bcast_addr, 0, sizeof(bcast_addr));
        bcast_addr.sin_family = AF_INET;
        bcast_addr.sin_port = htons(68);
        bcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

        sendto(s_dhcp_sock, &resp, resp_len, 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));
        LOG_I(TAG, "📡 [MiniDHCP] 已响应 %s 给客户端 MAC [%02X:%02X:%02X:%02X:%02X:%02X]，分配 IP: 192.168.4.100",
              (msg_type == 1) ? "DHCPOFFER" : "DHCPACK",
              req->chaddr[0], req->chaddr[1], req->chaddr[2],
              req->chaddr[3], req->chaddr[4], req->chaddr[5]);
    }

    if (s_dhcp_sock >= 0) {
        close(s_dhcp_sock);
        s_dhcp_sock = -1;
    }
    LOG_I(TAG, "💤 [MiniDHCP] 轻量级 DHCP 服务已正常退出");
    return NULL;
}

static void mini_dhcpd_start(void)
{
    if (s_dhcp_running) return;
    s_dhcp_running = true;
    pthread_create(&s_dhcp_tid, NULL, mini_dhcpd_thread, NULL);
    pthread_detach(s_dhcp_tid);
}

static void mini_dhcpd_stop(void)
{
    if (!s_dhcp_running) return;
    s_dhcp_running = false;
    if (s_dhcp_sock >= 0) {
        close(s_dhcp_sock);
        s_dhcp_sock = -1;
    }
}
#endif

int net_mgr_stop_softap(void)
{
    LOG_I(TAG, "🛑 关闭 SoftAP 独立热点与内嵌 DHCP 服务...");
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

#if !defined(HOST_TEST_RUNNER)
static void* softap_worker_thread(void *arg)
{
    (void)arg;
    char ssid[NET_MAX_SSID_LEN];
    pthread_mutex_lock(&s_lock);
    strncpy(ssid, s_current_ssid, sizeof(ssid) - 1);
    pthread_mutex_unlock(&s_lock);

    LOG_I(TAG, "[SoftAP:Worker] 正在后台配置 SoftAP 物理网卡与射频 (SSID: %s)...", ssid);

    /* 1. 先静默 wlan0 客户端并断开连接，避免与 SoftAP 争抢单天线物理射频 */
    system("wapi disconnect wlan0 > /dev/null 2>&1");
    system("wapi private wlan0 adaptivity 0 > /dev/null 2>&1");
    system("wapi power_save wlan0 off > /dev/null 2>&1");

    net_mgr_stop_softap();
    usleep(100000);

    /* 2. 原生 WAPI C API 配置 wlan1 Master 与固定 2.4GHz 信道 6 (2437MHz) */
    int sock = wapi_make_socket();
    if (sock >= 0) {
        /* 设置 Master 模式 (AP) */
        wapi_set_mode(sock, "wlan1", WAPI_MODE_MASTER);
        usleep(50000);

        /* 核心关键点 1: 显式锁定 2.4GHz 黄金信道 Channel 6 (2437MHz)，杜绝信道非法为 0 */
        wapi_set_freq(sock, "wlan1", 2437, 1);
        usleep(50000);

        /* 配置物理网卡 IP 192.168.4.1 与掩码 255.255.255.0 并激活接口 */
        struct in_addr ip, mask;
        inet_aton("192.168.4.1", &ip);
        inet_aton("255.255.255.0", &mask);
        wapi_set_ip(sock, "wlan1", &ip);
        wapi_set_netmask(sock, "wlan1", &mask);
        wapi_set_ifup(sock, "wlan1");
        usleep(50000);

        /* 核心关键点 2: 彻底清除 wlan1 历史残留加密算法与密钥，确立纯净 OPEN 无密码模式 */
        wpa_driver_wext_set_auth_param(sock, "wlan1", 0, 0);
        wpa_driver_wext_set_key_ext(sock, "wlan1", 0, NULL, 0);

        /* 设置广播 SSID 并启动射频发射 */
        wapi_set_essid(sock, "wlan1", ssid, WAPI_ESSID_ON);
        close(sock);
        LOG_I(TAG, "⚡ [Native WAPI] SoftAP (SSID: %s, Ch: 6, IP: 192.168.4.1, Mode: OPEN) 射频已就绪", ssid);
    }

    /* 3. 补充标准命令行确保全志底层 Realtek 驱动属性生效 */
    system("ifconfig wlan1 192.168.4.1 netmask 255.255.255.0 up > /dev/null 2>&1");
    system("wapi mode wlan1 3 > /dev/null 2>&1");
    system("wapi freq wlan1 2437 1 > /dev/null 2>&1");
    system("wapi psk wlan1 \"\" 0 0 > /dev/null 2>&1");
    system("wapi private wlan1 adaptivity 0 > /dev/null 2>&1");
    system("wapi power_save wlan1 off > /dev/null 2>&1");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wapi essid wlan1 \"%s\" 1 > /dev/null 2>&1", ssid);
    system(cmd);

    mini_dhcpd_start();
    phoenix_web_portal_start(80, NULL);
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
    pthread_create(&softap_tid, NULL, softap_worker_thread, NULL);
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
        pthread_create(&s_connect_tid, NULL, sta_connect_worker_thread, param);
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

int net_mgr_scan_wifi(net_wifi_ap_info_t *aps_out, size_t max_count)
{
    if (!aps_out || max_count == 0) return -1;

#if !defined(HOST_TEST_RUNNER)
    int sock = wapi_make_socket();
    if (sock >= 0) {
        LOG_I(TAG, "正在通过物理网卡 wlan0 发起实时空中 Wi-Fi 扫描...");
        int ret = wapi_scan_init(sock, "wlan0", NULL);
        if (ret >= 0) {
            /* 轮询等待驱动空中抓包完成 (通常耗时 300ms ~ 1.2s) */
            int tries = 15;
            while (--tries > 0) {
                ret = wapi_scan_stat(sock, "wlan0");
                if (ret == 0) {
                    /* 扫描完成 */
                    break;
                }
                usleep(100 * 1000);
            }

            struct wapi_list_s list;
            memset(&list, 0, sizeof(list));
            ret = wapi_scan_coll(sock, "wlan0", &list);
            if (ret == 0 && list.head.scan != NULL) {
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
                        const char *auth_type = "OPEN";
                        if (info->has_encode && info->encode != 0) {
                            auth_type = "WPA2";
                        }

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
                close(sock);

                if (real_count > 0) {
                    /* 按信号强度从强到弱排序 (RSSI 降序) */
                    for (size_t i = 0; i < real_count - 1; i++) {
                        for (size_t j = 0; j < real_count - 1 - i; j++) {
                            if (aps_out[j].rssi < aps_out[j + 1].rssi) {
                                net_wifi_ap_info_t tmp = aps_out[j];
                                aps_out[j] = aps_out[j + 1];
                                aps_out[j + 1] = tmp;
                            }
                        }
                    }

                    LOG_I(TAG, "📡 真实 Wi-Fi 扫描成功，捕获周边 %zu 个活跃热点", real_count);
                    return (int)real_count;
                }
            } else {
                close(sock);
            }
        } else {
            close(sock);
        }
        LOG_W(TAG, "物理网卡扫描暂无空中数据，回退至安全模式");
    }
#endif

    /* 宿主机仿真测试 (HOST_TEST_RUNNER) 或无线驱动初始化未就绪时的备用数据 */
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

    LOG_I(TAG, "📡 Wi-Fi 扫描完成 (宿主机/备用模式)，返回 %zu 个测试热点", count);
    return (int)count;
}
