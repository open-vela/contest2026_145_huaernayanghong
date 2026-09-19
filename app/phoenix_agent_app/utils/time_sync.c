/**
 * @file time_sync.c
 * @brief NTP / SNTP 网络时间同步客户端实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "time_sync.h"
#include "time_utils.h"
#include "log_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>

#define TAG "TimeSync"

/* NTP 时间起始点：1900-01-01 与 Unix 起始点 1970-01-01 之间的秒数差 */
#define NTP2UNIX_TRANSLATION 2208988800ULL

/* 优选国内稳定快速的高精度 NTP 服务器列表 */
static const char *const s_ntp_servers[] = {
    "ntp.aliyun.com",       /* 阿里云公共 NTP 域名 */
    "203.107.6.88",        /* 阿里云公共 NTP 直连 IP (防 DNS 故障) */
    "cn.pool.ntp.org",      /* 中国 NTP 池域名 */
    "time.asia.apple.com",  /* 苹果亚洲 NTP 域名 */
    "time1.cloud.tencent.com" /* 腾讯云公共 NTP */
};
#define NTP_SERVER_COUNT (sizeof(s_ntp_servers) / sizeof(s_ntp_servers[0]))

static pthread_mutex_t s_sync_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile bool   s_is_syncing = false;
static pthread_t       s_sync_tid = 0;

/**
 * @brief 执行单次 SNTP UDP 报文收发
 * @param server_host 域名或 IP 字符串
 * @param timeout_ms 超时毫秒数
 * @param out_unix_sec 输出解析后的 Unix 秒级时间戳
 * @return 0 成功, 负数失败
 */
static int sntp_query_single(const char *server_host, int timeout_ms, time_t *out_unix_sec)
{
    if (!server_host || !out_unix_sec) return -1;

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    char port_str[8] = "123";
    int ret = getaddrinfo(server_host, port_str, &hints, &res);
    if (ret != 0 || !res) {
        return -1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return -2;
    }

    /* 构造 48 字节 SNTP 客户端请求报文 (LI=0, VN=4, Mode=3 -> 0x23) */
    uint8_t packet[48];
    memset(packet, 0, sizeof(packet));
    packet[0] = 0x23;

    ssize_t sent = sendto(sock, packet, sizeof(packet), 0, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    res = NULL;

    if (sent != (ssize_t)sizeof(packet)) {
        close(sock);
        return -3;
    }

    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLIN;
    int pret = poll(&pfd, 1, timeout_ms);
    if (pret <= 0 || !(pfd.revents & POLLIN)) {
        close(sock);
        return -4; /* 超时或无响应 */
    }

    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    ssize_t n = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&from_addr, &from_len);
    close(sock);

    if (n < 48) {
        return -5; /* 报文残缺 */
    }

    /* 提取 Transmit Timestamp (字节 40..43 为整秒数) */
    uint32_t ntp_sec = ntohl(*(uint32_t *)&packet[40]);
    if (ntp_sec <= NTP2UNIX_TRANSLATION) {
        return -6; /* 无效时间 */
    }

    uint64_t unix_sec = (uint64_t)ntp_sec - NTP2UNIX_TRANSLATION;
    *out_unix_sec = (time_t)unix_sec;
    return 0;
}

static void* time_sync_worker(void *arg)
{
    (void)arg;
    LOG_I(TAG, "⏱️ 启动后台 NTP 时间校准任务 (服务器候选: %zu 个)...", NTP_SERVER_COUNT);

    bool success = false;
    time_t acquired_time = 0;
    const char *success_server = NULL;

    for (size_t i = 0; i < NTP_SERVER_COUNT; i++) {
        const char *srv = s_ntp_servers[i];
        LOG_I(TAG, "⏱️ 正在向 NTP 节点 [%s:123] 发起时钟对齐...", srv);
        int qret = sntp_query_single(srv, 2500, &acquired_time);
        if (qret == 0 && acquired_time >= 1704067200) {
            success = true;
            success_server = srv;
            break;
        }
    }

    if (success) {
        time_utils_set_time(acquired_time);
        char tstr[32] = {0};
        time_utils_get_datetime_str(tstr, sizeof(tstr));
        LOG_I(TAG, "🎉 [TimeSync] NTP 对时成功! 物理时钟已校准: [%s] (授时源: %s)", tstr, success_server);
    } else {
        LOG_W(TAG, "⚠️ [TimeSync] 本轮所有 NTP 服务器均无响应，稍后自动重试或等待 Web 授时");
    }

    pthread_mutex_lock(&s_sync_lock);
    s_is_syncing = false;
    pthread_mutex_unlock(&s_sync_lock);
    return NULL;
}

int time_sync_init(void)
{
    time_utils_init();
    return 0;
}

int time_sync_trigger_ntp(void)
{
    pthread_mutex_lock(&s_sync_lock);
    if (s_is_syncing) {
        pthread_mutex_unlock(&s_sync_lock);
        return 0; /* 任务已在运行，避免并发重复发起 */
    }
    s_is_syncing = true;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int ret = pthread_create(&s_sync_tid, &attr, time_sync_worker, NULL);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        s_is_syncing = false;
        LOG_E(TAG, "无法创建 NTP 同步工作线程: %d", ret);
        pthread_mutex_unlock(&s_sync_lock);
        return -1;
    }

    pthread_mutex_unlock(&s_sync_lock);
    return 0;
}

bool time_sync_is_running(void)
{
    pthread_mutex_lock(&s_sync_lock);
    bool b = s_is_syncing;
    pthread_mutex_unlock(&s_sync_lock);
    return b;
}
