/**
 * @file web_portal.c
 * @brief Embedded Web Companion Server Implementation (Powered by Micro REST Router)
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_portal.h"
#include "web_router.h"
#include "web_api/web_api.h"
#include "cartridge_mgr.h"
#include "../cartridges/cartridge_memo.h"
#include "tool_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

static bool g_server_running = false;
static int g_server_fd = -1;
static int g_server_fd_alt = -1; /* 辅助端口 socket (80/8080 双端口并发) */
static uint16_t g_server_port = PHOENIX_STANDARD_HTTP_PORT;
static pthread_t g_server_thread;

/* =========================================================================
 * 跨线程 Web 远程交互命令安全队列
 * ========================================================================= */
typedef struct {
    web_cmd_type_t type;
    char param[128];
} web_cmd_entry_t;

#define MAX_WEB_CMDS 16
static web_cmd_entry_t g_web_cmd_queue[MAX_WEB_CMDS];
static size_t g_web_cmd_head = 0;
static size_t g_web_cmd_tail = 0;
static pthread_mutex_t g_web_cmd_lock = PTHREAD_MUTEX_INITIALIZER;

void phoenix_web_portal_enqueue_cmd(web_cmd_type_t type, const char *param)
{
    pthread_mutex_lock(&g_web_cmd_lock);
    size_t next = (g_web_cmd_tail + 1) % MAX_WEB_CMDS;
    if (next != g_web_cmd_head) {
        g_web_cmd_queue[g_web_cmd_tail].type = type;
        if (param) {
            strncpy(g_web_cmd_queue[g_web_cmd_tail].param, param, sizeof(g_web_cmd_queue[g_web_cmd_tail].param) - 1);
            g_web_cmd_queue[g_web_cmd_tail].param[sizeof(g_web_cmd_queue[g_web_cmd_tail].param) - 1] = '\0';
        } else {
            g_web_cmd_queue[g_web_cmd_tail].param[0] = '\0';
        }
        g_web_cmd_tail = next;
    }
    pthread_mutex_unlock(&g_web_cmd_lock);
}

bool phoenix_web_portal_in_server_thread(void)
{
    return g_server_running && (pthread_equal(pthread_self(), g_server_thread) != 0);
}

void phoenix_web_portal_drain_commands(void)
{
    while (1) {
        web_cmd_entry_t cmd;
        pthread_mutex_lock(&g_web_cmd_lock);
        if (g_web_cmd_head == g_web_cmd_tail) {
            pthread_mutex_unlock(&g_web_cmd_lock);
            break;
        }
        cmd = g_web_cmd_queue[g_web_cmd_head];
        g_web_cmd_head = (g_web_cmd_head + 1) % MAX_WEB_CMDS;
        pthread_mutex_unlock(&g_web_cmd_lock);

        switch (cmd.type) {
            case WEB_CMD_SWITCH_CARTRIDGE:
                cartridge_mgr_switch_to(cmd.param);
                break;
            case WEB_CMD_ADD_MEMO:
                cartridge_memo_add_entry(cmd.param);
                break;
            case WEB_CMD_ACTION:
                if (strcmp(cmd.param, "pet") == 0) {
                    cartridge_mgr_dispatch_knock(1, 1);
                } else if (strcmp(cmd.param, "knock_fish") == 0) {
                    phoenix_tool_execute("knock_wooden_fish", "{\"count\":1}", NULL, 0);
                } else if (strcmp(cmd.param, "pomo_toggle") == 0) {
                    cartridge_mgr_dispatch_knock(1, 1);
                }
                break;
            default:
                break;
        }
    }
}

/* =========================================================================
 * 状态绑定与请求分发代理
 * ========================================================================= */
void phoenix_web_portal_bind_agent(phoenix_agent_ctx_t *agent_ctx)
{
    web_api_set_bound_agent(agent_ctx);
}

bool phoenix_web_portal_is_running(void)
{
    return g_server_running;
}

int phoenix_web_portal_handle_request(const char *req_str, char *resp_out, size_t max_len)
{
    /* 直接委托给微型 REST 路由器执行表驱动匹配与分发 */
    return web_router_dispatch(req_str, resp_out, max_len);
}

/* =========================================================================
 * Socket 监听与后台 Worker 线程
 * ========================================================================= */
static int bind_and_listen_socket(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 5) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static const char* find_content_length_header(const char *haystack, const char *end)
{
    const char *p = haystack;
    const char *key = "content-length:";
    size_t key_len = 15;

    while (p + key_len <= end) {
        bool match = true;
        for (size_t i = 0; i < key_len; i++) {
            char c1 = p[i];
            char c2 = key[i];
            if (c1 >= 'A' && c1 <= 'Z') c1 += ('a' - 'A');
            if (c1 != c2) {
                match = false;
                break;
            }
        }
        if (match) {
            return p + key_len;
        }
        p++;
    }
    return NULL;
}

static void handle_single_client(int client_fd, char *req_buf, char *resp_buf)
{
    /* 增加 poll 1000ms 超时判定，杜绝空预连接挂起 */
    struct pollfd pfd;
    pfd.fd = client_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int pret = poll(&pfd, 1, 1000);
    if (pret <= 0 || !(pfd.revents & POLLIN)) {
        close(client_fd);
        return;
    }

    /* 设置 1000ms 接收超时，防止后续数据传输阻塞 */
    struct timeval tv_client;
    tv_client.tv_sec = 1;
    tv_client.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_client, sizeof(tv_client));

    size_t total_read = 0;
    const size_t max_req = 16384 - 1;
    char *header_end = NULL;
    int content_length = -1;

    while (total_read < max_req) {
        ssize_t n = recv(client_fd, req_buf + total_read, max_req - total_read, 0);
        if (n <= 0) {
            break;
        }
        total_read += (size_t)n;
        req_buf[total_read] = '\0';

        /* 1. 若尚未找到头部结束标记，尝试寻找 \r\n\r\n */
        if (!header_end) {
            header_end = strstr(req_buf, "\r\n\r\n");
            if (header_end) {
                /* 解析 Content-Length */
                const char *cl_pos = find_content_length_header(req_buf, header_end);
                if (cl_pos) {
                    content_length = atoi(cl_pos);
                } else {
                    content_length = 0;
                }
            }
        }

        /* 2. 若头部已就绪，校验 Body 是否已完全读取 */
        if (header_end) {
            size_t body_start_offset = (size_t)(header_end + 4 - req_buf);
            size_t current_body_len = total_read >= body_start_offset ? (total_read - body_start_offset) : 0;
            if (content_length <= 0 || current_body_len >= (size_t)content_length) {
                break; /* 请求已完整就绪 */
            }
        }
    }

    if (total_read > 0) {
        req_buf[total_read] = '\0';
        char req_summary[64] = {0};
        char *crlf = strstr(req_buf, "\r\n");
        if (crlf) {
            size_t line_len = (size_t)(crlf - req_buf);
            if (line_len >= sizeof(req_summary)) line_len = sizeof(req_summary) - 1;
            strncpy(req_summary, req_buf, line_len);
        } else {
            strncpy(req_summary, req_buf, sizeof(req_summary) - 1);
        }
        printf("[PhoenixWeb] 📥 Client connected: %s (Total: %zu bytes, Body: %d bytes)\n",
               req_summary, total_read, content_length > 0 ? content_length : 0);

        int resp_len = phoenix_web_portal_handle_request(req_buf, resp_buf, 32768);
        if (resp_len > 0) {
            ssize_t total_sent = 0;
            while (total_sent < resp_len) {
                ssize_t s = send(client_fd, resp_buf + total_sent, (size_t)(resp_len - total_sent), 0);
                if (s <= 0) break;
                total_sent += s;
            }
            printf("[PhoenixWeb] 📤 Sent %zd/%d bytes\n", total_sent, resp_len);
        }
    }

    /* 优雅半关闭并排空残留缓冲区 */
    shutdown(client_fd, SHUT_WR);
    char drain_buf[128];
    while (recv(client_fd, drain_buf, sizeof(drain_buf), MSG_DONTWAIT) > 0) {
    }
    close(client_fd);
}

static void *server_thread_worker(void *arg)
{
    (void)arg;
    struct sockaddr_in client_addr;
    socklen_t client_len;

    char *req_buf = (char *)malloc(16384);
    char *resp_buf = (char *)malloc(32768);
    if (!req_buf || !resp_buf) {
        printf("[PhoenixWeb] Error: failed to allocate buffers for server worker\n");
        if (req_buf) free(req_buf);
        if (resp_buf) free(resp_buf);
        return NULL;
    }

    while (g_server_running && (g_server_fd >= 0 || g_server_fd_alt >= 0)) {
        struct pollfd pfds[2];
        int pfd_cnt = 0;

        if (g_server_fd >= 0) {
            pfds[pfd_cnt].fd = g_server_fd;
            pfds[pfd_cnt].events = POLLIN;
            pfds[pfd_cnt].revents = 0;
            pfd_cnt++;
        }
        if (g_server_fd_alt >= 0) {
            pfds[pfd_cnt].fd = g_server_fd_alt;
            pfds[pfd_cnt].events = POLLIN;
            pfds[pfd_cnt].revents = 0;
            pfd_cnt++;
        }

        if (pfd_cnt == 0) {
            usleep(20000);
            continue;
        }

        int poll_ret = poll(pfds, pfd_cnt, 500);
        if (poll_ret <= 0) {
            if (!g_server_running) break;
            continue;
        }

        for (int i = 0; i < pfd_cnt; i++) {
            if (pfds[i].revents & POLLIN) {
                client_len = sizeof(client_addr);
                int client_fd = accept(pfds[i].fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd >= 0) {
                    handle_single_client(client_fd, req_buf, resp_buf);
                }
            }
        }
    }

    free(req_buf);
    free(resp_buf);
    return NULL;
}

int phoenix_web_portal_start(uint16_t port, phoenix_agent_ctx_t *agent_ctx)
{
    if (g_server_running) return 0;

    /* 初始化路由引擎并批量注册业务控制器 */
    web_router_init();
    web_api_register_all();

    if (agent_ctx) {
        phoenix_web_portal_bind_agent(agent_ctx);
    }

    g_server_port = (port > 0) ? port : PHOENIX_STANDARD_HTTP_PORT;

    g_server_fd = bind_and_listen_socket(g_server_port);
    uint16_t alt_port = (g_server_port == PHOENIX_STANDARD_HTTP_PORT) ? PHOENIX_DEFAULT_WEB_PORT : PHOENIX_STANDARD_HTTP_PORT;
    g_server_fd_alt = bind_and_listen_socket(alt_port);

    if (g_server_fd < 0 && g_server_fd_alt < 0) {
        printf("[PhoenixWeb] Error: failed to bind both ports %u and %u\n", g_server_port, alt_port);
        return -1;
    }

    g_server_running = true;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 16384);

    int ret = pthread_create(&g_server_thread, &attr, server_thread_worker, NULL);
    pthread_attr_destroy(&attr);
    if (ret != 0) {
        printf("[PhoenixWeb] Error: failed to create server worker thread (16KB stack)\n");
        g_server_running = false;
        if (g_server_fd >= 0) { close(g_server_fd); g_server_fd = -1; }
        if (g_server_fd_alt >= 0) { close(g_server_fd_alt); g_server_fd_alt = -1; }
        return -1;
    }

    printf("[PhoenixWeb] 🌐 Web Portal listening on BOTH http://0.0.0.0:%u (免端口直达) & :%u (兼容)\n",
           g_server_port, alt_port);
    return 0;
}

void phoenix_web_portal_stop(void)
{
    if (!g_server_running) return;

    g_server_running = false;

    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
    if (g_server_fd_alt >= 0) {
        close(g_server_fd_alt);
        g_server_fd_alt = -1;
    }

    pthread_join(g_server_thread, NULL);
    web_router_deinit();
    printf("[PhoenixWeb] 🛑 Web Portal stopped cleanly.\n");
}
