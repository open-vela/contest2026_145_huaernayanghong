/**
 * @file llm_cloud_backend.c
 * @brief Robust Cloud & VelaClaw Adapter Backend Implementation (libcurl / Socket with Full Diagnostics)
 * @author OpenVela Contest 2026 Team 145
 */

#include "llm_cloud_backend.h"
#if defined(__has_include) && __has_include("utils/log_utils.h")
#  include "utils/log_utils.h"
#else
#  include "../utils/log_utils.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#define TAG "PhoenixCloud"

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

/* 优先引用 OpenVela 系统的 libcurl 支持 HTTPS/TLS 传输 */
#if defined(CONFIG_LIB_CURL) || defined(__has_include)
#  if __has_include(<curl/curl.h>)
#    include <curl/curl.h>
#    define PHOENIX_HAVE_LIBCURL 1
#  elif __has_include("../../../../external/curl/curl/include/curl/curl.h")
#    include "../../../../external/curl/curl/include/curl/curl.h"
#    define PHOENIX_HAVE_LIBCURL 1
#  endif
#endif

#if !defined(HOST_TEST_RUNNER) && defined(__has_include)
#  if __has_include(<velaclaw/client.h>)
#    include <velaclaw/client.h>
#    define HAVE_VELACLAW_CLIENT 1
#  endif
#endif

typedef struct {
    phoenix_llm_config_t config;
    bool agent_connected;
#ifdef HAVE_VELACLAW_CLIENT
    velaclaw_client_t *velaclaw_client;
#endif
} cloud_backend_ctx_t;

static cloud_backend_ctx_t g_cloud_ctx;

static const char *DEFAULT_SYSTEM_PROMPT =
    "你是 Phoenix HoloDesk-S1 桌面具身灵眸 AI 机器人，运行在 OpenVela 实时嵌入式操作系统上。\n"
    "【核心具身准则】\n"
    "1. 你连接着物理实体硬件与嵌入式外设驱动。当用户提出任何动作、控制或查询需求（如开启/暂停/停止番茄钟、敲木鱼积累功德、控制眼球微表情、闪烁板载LED指示灯、查询传感器环境或待办等）时，你必须且只能发起真实的工具调用 (tool_calls) 来执行对应工具！\n"
    "2. 绝对禁止在未实际发起并完成工具调用前，口头宣称已启动或通过括号动作描写角色扮演假装已执行！\n"
    "3. 工具执行后，系统会向你输入真实结果 (role=tool)。你必须基于该真实结果，以灵眸机器人温暖生动的语调向用户进行反馈。\n"
    "4. 若用户仅进行日常闲聊，则直接输出自然语言回复。";

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} http_resp_buf_t;

#ifdef PHOENIX_HAVE_LIBCURL
static size_t curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    http_resp_buf_t *mem = (http_resp_buf_t *)userp;

    if (mem->size + realsize + 1 > mem->capacity) {
        size_t new_cap = mem->capacity * 2;
        if (new_cap < mem->size + realsize + 1) {
            new_cap = mem->size + realsize + 1024;
        }
        if (new_cap > 64 * 1024) {
            LOG_E(TAG, "Response size exceeds memory ceiling (64KB)!");
            return 0;
        }
        char *ptr = (char *)realloc(mem->data, new_cap);
        if (!ptr) {
            LOG_E(TAG, "realloc failed for HTTP buffer!");
            return 0;
        }
        mem->data = ptr;
        mem->capacity = new_cap;
    }

    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
    return realsize;
}
#endif

/**
 * @brief 执行统一 HTTP/HTTPS POST 请求并支持精准诊断输出
 */
static int execute_http_post(const char *url,
                             const char *api_key,
                             const char *req_body,
                             char **out_resp,
                             int *out_status,
                             uint32_t *out_latency_ms,
                             char *err_buf,
                             size_t err_sz)
{
    if (!url || !req_body || !out_resp) return -1;
    *out_resp = NULL;
    if (out_status) *out_status = 0;
    if (out_latency_ms) *out_latency_ms = 0;
    if (err_buf && err_sz > 0) err_buf[0] = '\0';

    struct timeval start_tv, end_tv;
    gettimeofday(&start_tv, NULL);

#ifdef PHOENIX_HAVE_LIBCURL
    CURL *curl = curl_easy_init();
    if (curl) {
        http_resp_buf_t chunk;
        chunk.capacity = 4096;
        chunk.size = 0;
        chunk.data = (char *)malloc(chunk.capacity);
        if (!chunk.data) {
            curl_easy_cleanup(curl);
            if (err_buf && err_sz > 0) snprintf(err_buf, err_sz, "Memory allocation failed");
            return -1;
        }
        chunk.data[0] = '\0';

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        if (api_key && api_key[0] != '\0') {
            char auth_hdr[256];
            snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", api_key);
            headers = curl_slist_append(headers, auth_hdr);
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_body);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        /* 嵌入式设备网络稳定性适配：允许自签名或缺失本地 CA 根证书时安全连通 */
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        /* 护城河 3: 注入网络硬看门狗，快速失败自愈，绝不长期霸占工作线程拖死系统 */
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);

        CURLcode res = curl_easy_perform(curl);

        gettimeofday(&end_tv, NULL);
        uint32_t lat = (uint32_t)((end_tv.tv_sec - start_tv.tv_sec) * 1000 +
                                  (end_tv.tv_usec - start_tv.tv_usec) / 1000);
        if (out_latency_ms) *out_latency_ms = lat;

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (out_status) *out_status = (int)http_code;

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            if (err_buf && err_sz > 0) {
                snprintf(err_buf, err_sz, "Curl error (%d): %s", res, curl_easy_strerror(res));
            }
            free(chunk.data);
            return -2;
        }

        *out_resp = chunk.data;
        return (http_code == 200) ? 0 : -3;
    }
#endif

    /* Fallback: 仅当非 HTTPS 或纯 IP/HTTP 代理时的简易 Socket 通信 */
    const char *p = strstr(url, "://");
    p = p ? (p + 3) : url;

    char host[128] = {0};
    char path[256] = "/";
    const char *slash = strchr(p, '/');
    if (slash) {
        size_t hlen = (size_t)(slash - p);
        if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        strncpy(host, p, hlen);
        strncpy(path, slash, sizeof(path) - 1);
    } else {
        strncpy(host, p, sizeof(host) - 1);
    }

    int port = 80;
    if (strncmp(url, "https://", 8) == 0) {
        port = 443;
    }
    char *colon = strchr(host, ':');
    if (colon) {
        *colon = '\0';
        port = atoi(colon + 1);
    }

    if (port == 443) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "HTTPS requires CONFIG_LIB_CURL with TLS enabled in build config");
        }
        return -4;
    }

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int gai = getaddrinfo(host, port_str, &hints, &res);
    if (gai != 0 || !res) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "DNS lookup failed for %s: %s", host, gai_strerror(gai));
        }
        return -5;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        if (err_buf && err_sz > 0) snprintf(err_buf, err_sz, "Socket create failed");
        return -6;
    }

    struct timeval tv = { .tv_sec = 12, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        close(sock);
        freeaddrinfo(res);
        if (err_buf && err_sz > 0) snprintf(err_buf, err_sz, "Connect to %s:%d timed out or refused", host, port);
        return -7;
    }
    freeaddrinfo(res);

    char header_buf[1024];
    snprintf(header_buf, sizeof(header_buf),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Authorization: Bearer %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n",
             path, host, api_key ? api_key : "", strlen(req_body));

    send(sock, header_buf, strlen(header_buf), 0);
    send(sock, req_body, strlen(req_body), 0);

    char *raw_resp = (char *)malloc(32768);
    if (!raw_resp) {
        close(sock);
        if (err_buf && err_sz > 0) snprintf(err_buf, err_sz, "OOM allocating raw_resp");
        return -8;
    }

    size_t total_n = 0;
    while (total_n < 32767) {
        ssize_t n = recv(sock, raw_resp + total_n, 32767 - total_n, 0);
        if (n <= 0) break;
        total_n += n;
    }
    raw_resp[total_n] = '\0';
    close(sock);

    gettimeofday(&end_tv, NULL);
    if (out_latency_ms) {
        *out_latency_ms = (uint32_t)((end_tv.tv_sec - start_tv.tv_sec) * 1000 +
                                     (end_tv.tv_usec - start_tv.tv_usec) / 1000);
    }

    int code = 0;
    sscanf(raw_resp, "HTTP/%*s %d", &code);
    if (out_status) *out_status = code;

    char *body = strstr(raw_resp, "\r\n\r\n");
    if (body) {
        body += 4;
        *out_resp = strdup(body);
        free(raw_resp);
        return (code == 200) ? 0 : -3;
    }

    *out_resp = raw_resp;
    return -9;
}

static int cloud_backend_init(phoenix_llm_backend_t *self, const phoenix_llm_config_t *config)
{
    (void)self;
    if (config) {
        memcpy(&g_cloud_ctx.config, config, sizeof(phoenix_llm_config_t));
    } else {
        memset(&g_cloud_ctx.config, 0, sizeof(g_cloud_ctx.config));
        snprintf(g_cloud_ctx.config.base_url, sizeof(g_cloud_ctx.config.base_url),
                 "https://api.deepseek.com/v1/chat/completions");
        snprintf(g_cloud_ctx.config.model_name, sizeof(g_cloud_ctx.config.model_name), "deepseek-chat");
        g_cloud_ctx.config.temperature = 70;
        g_cloud_ctx.config.system_prompt = DEFAULT_SYSTEM_PROMPT;
    }

#ifdef HAVE_VELACLAW_CLIENT
    g_cloud_ctx.velaclaw_client = velaclaw_client_open("phoenix_agent");
    if (g_cloud_ctx.velaclaw_client) {
        g_cloud_ctx.agent_connected = true;
        LOG_I(TAG, "Connected to system-level VelaClaw Agent bus!");
    } else {
        g_cloud_ctx.agent_connected = false;
        LOG_I(TAG, "VelaClaw Agent unavailable, cloud direct mode ready.");
    }
#else
    g_cloud_ctx.agent_connected = false;
#endif

    LOG_I(TAG, "Backend initialized. Model: %s, Endpoint: %s",
          g_cloud_ctx.config.model_name, g_cloud_ctx.config.base_url);
    return 0;
}

static int cloud_backend_chat(phoenix_llm_backend_t *self,
                             const phoenix_chat_msg_t *messages,
                             size_t msg_count,
                             const char *tools_json,
                             phoenix_chat_resp_t *resp_out)
{
    (void)self;
    if (!resp_out) return -1;
    memset(resp_out, 0, sizeof(phoenix_chat_resp_t));

    const char *api_key = g_cloud_ctx.config.api_key;
#ifdef HOST_TEST_RUNNER
    /* 宿主机离线自动化测试桩：模拟 Cloud 适配器正常响应 */
    if (!api_key || api_key[0] == '\0' || strncmp(api_key, "mock", 4) == 0 || strncmp(api_key, "sk-test", 7) == 0) {
        resp_out->reasoning_content = strdup("【云端思维链】宿主机回归环境 Cloud 适配器测试通过。");
        resp_out->content = strdup("宿主机 Cloud 适配器测试正常。");
        resp_out->http_status = 200;
        return 0;
    }
#else
    if (!api_key || api_key[0] == '\0') {
        resp_out->reasoning_content = strdup("【云端配置检测】未检测到有效的大模型 API Key，已提供终端配置指引。");
        resp_out->content = strdup("⚠️ 未配置大模型 API Key！请先在 ADB 终端执行:\n"
                                   "  phoenix_agent_app config set api_key <你的DeepSeek-Key>");
        resp_out->error_msg = strdup("API Key is missing in configuration.");
        resp_out->http_status = 401;
        return 0;
    }
#endif

    /* Build OpenAI / MiMo compatible JSON Payload */
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "model", g_cloud_ctx.config.model_name);
    cJSON_AddNumberToObject(payload, "top_p", 0.95);
    cJSON_AddNumberToObject(payload, "temperature", (double)g_cloud_ctx.config.temperature / 100.0);
    cJSON_AddNumberToObject(payload, "max_tokens", 1024); /* 限制在 1024 保护嵌入式堆内存 */

    /* MiMo & DeepSeek Thinking Mode extension */
    cJSON *kwargs = cJSON_CreateObject();
    cJSON_AddTrueToObject(kwargs, "enable_thinking");
    cJSON_AddItemToObject(payload, "chat_template_kwargs", kwargs);

    cJSON *msg_array = cJSON_CreateArray();
    /* Add System Prompt */
    cJSON *sys_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_msg, "role", "system");
    cJSON_AddStringToObject(sys_msg, "content",
                            g_cloud_ctx.config.system_prompt ? g_cloud_ctx.config.system_prompt : DEFAULT_SYSTEM_PROMPT);
    cJSON_AddItemToArray(msg_array, sys_msg);

    /* Add Message History */
    for (size_t i = 0; i < msg_count; i++) {
        cJSON *m = cJSON_CreateObject();
        switch (messages[i].role) {
            case PHOENIX_ROLE_SYSTEM:
                cJSON_AddStringToObject(m, "role", "system");
                cJSON_AddStringToObject(m, "content", messages[i].content ? messages[i].content : "");
                break;
            case PHOENIX_ROLE_USER:
                cJSON_AddStringToObject(m, "role", "user");
                cJSON_AddStringToObject(m, "content", messages[i].content ? messages[i].content : "");
                break;
            case PHOENIX_ROLE_ASSISTANT:
                cJSON_AddStringToObject(m, "role", "assistant");
                cJSON_AddStringToObject(m, "content", messages[i].content ? messages[i].content : "");
                if (messages[i].tool_name) {
                    cJSON *tcalls = cJSON_CreateArray();
                    cJSON *tc = cJSON_CreateObject();
                    cJSON_AddStringToObject(tc, "id", messages[i].tool_call_id ? messages[i].tool_call_id : "call_1");
                    cJSON_AddStringToObject(tc, "type", "function");
                    cJSON *func = cJSON_CreateObject();
                    cJSON_AddStringToObject(func, "name", messages[i].tool_name);
                    cJSON_AddItemToObject(tc, "function", func);
                    cJSON_AddItemToArray(tcalls, tc);
                    cJSON_AddItemToObject(m, "tool_calls", tcalls);
                }
                break;
            case PHOENIX_ROLE_TOOL:
                cJSON_AddStringToObject(m, "role", "tool");
                cJSON_AddStringToObject(m, "tool_call_id", messages[i].tool_call_id ? messages[i].tool_call_id : "");
                cJSON_AddStringToObject(m, "content", messages[i].content ? messages[i].content : "");
                break;
            default:
                break;
        }
        cJSON_AddItemToArray(msg_array, m);
    }
    cJSON_AddItemToObject(payload, "messages", msg_array);

    /* Add Tools Schema if available */
    if (tools_json && strlen(tools_json) > 0) {
        cJSON *tools_obj = cJSON_Parse(tools_json);
        if (tools_obj) {
            cJSON_AddItemToObject(payload, "tools", tools_obj);
            cJSON_AddStringToObject(payload, "tool_choice", "auto");
        }
    }

    char *req_body = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);

    if (!req_body) {
        resp_out->content = strdup("组装请求 JSON Payload 失败");
        resp_out->error_msg = strdup("cJSON_Print failed");
        return -1;
    }

    LOG_D(TAG, "📤 Cloud Request (%zu bytes) -> %s", strlen(req_body), g_cloud_ctx.config.base_url);

    char *raw_resp = NULL;
    int http_status = 0;
    uint32_t lat_ms = 0;
    char err_diag[256] = {0};

    int ret = execute_http_post(g_cloud_ctx.config.base_url,
                                api_key,
                                req_body,
                                &raw_resp,
                                &http_status,
                                &lat_ms,
                                err_diag,
                                sizeof(err_diag));
    free(req_body);

    resp_out->http_status = http_status;
    resp_out->latency_ms = lat_ms;

    if (ret != 0 || !raw_resp) {
        char err_msg_buf[512];
        snprintf(err_msg_buf, sizeof(err_msg_buf),
                 "云端通信失败: %s (HTTP %d, 耗时 %ums)",
                 err_diag[0] ? err_diag : "网络握手异常", http_status, lat_ms);
        resp_out->content = strdup(err_msg_buf);
        resp_out->error_msg = strdup(err_diag[0] ? err_diag : "Network transport error");
        if (raw_resp) free(raw_resp);
        return -1;
    }

    /* Parse JSON Response */
    cJSON *r_json = cJSON_Parse(raw_resp);
    if (!r_json) {
        char err_buf[512];
        snprintf(err_buf, sizeof(err_buf), "解析云端应答 JSON 失败 (HTTP %d): %.128s", http_status, raw_resp);
        resp_out->content = strdup(err_buf);
        resp_out->error_msg = strdup("cJSON parse failed on response body");
        free(raw_resp);
        return -1;
    }
    free(raw_resp);

    /* 检查是否包含服务端错误信息 */
    if (http_status != 200) {
        cJSON *err_obj = cJSON_GetObjectItem(r_json, "error");
        cJSON *err_txt = err_obj ? cJSON_GetObjectItem(err_obj, "message") : NULL;
        char err_buf[512];
        snprintf(err_buf, sizeof(err_buf), "大模型服务拒绝 (HTTP %d): %s",
                 http_status, (err_txt && err_txt->valuestring) ? err_txt->valuestring : "未知服务端错误");
        resp_out->content = strdup(err_buf);
        resp_out->error_msg = strdup((err_txt && err_txt->valuestring) ? err_txt->valuestring : "Server error");
        cJSON_Delete(r_json);
        return -1;
    }

    /* 提取 Usage 信息 */
    cJSON *usage = cJSON_GetObjectItem(r_json, "usage");
    if (usage) {
        cJSON *pt = cJSON_GetObjectItem(usage, "prompt_tokens");
        cJSON *ct = cJSON_GetObjectItem(usage, "completion_tokens");
        cJSON *tt = cJSON_GetObjectItem(usage, "total_tokens");
        if (pt) resp_out->prompt_tokens = (uint32_t)pt->valueint;
        if (ct) resp_out->completion_tokens = (uint32_t)ct->valueint;
        if (tt) resp_out->total_tokens = (uint32_t)tt->valueint;
    }

    /* 提取 Choice 内容与 Tool Calls */
    cJSON *choices = cJSON_GetObjectItem(r_json, "choices");
    cJSON *choice0 = choices ? cJSON_GetArrayItem(choices, 0) : NULL;
    cJSON *msg = choice0 ? cJSON_GetObjectItem(choice0, "message") : NULL;
    if (msg) {
        cJSON *c_txt = cJSON_GetObjectItem(msg, "content");
        cJSON *r_txt = cJSON_GetObjectItem(msg, "reasoning_content");
        cJSON *t_calls = cJSON_GetObjectItem(msg, "tool_calls");

        if (c_txt && c_txt->valuestring && strlen(c_txt->valuestring) > 0) {
            resp_out->content = strdup(c_txt->valuestring);
        }
        if (r_txt && r_txt->valuestring && strlen(r_txt->valuestring) > 0) {
            resp_out->reasoning_content = strdup(r_txt->valuestring);
        }
        if (t_calls && cJSON_GetArraySize(t_calls) > 0) {
            cJSON *call0 = cJSON_GetArrayItem(t_calls, 0);
            cJSON *call_id = call0 ? cJSON_GetObjectItem(call0, "id") : NULL;
            cJSON *fn = call0 ? cJSON_GetObjectItem(call0, "function") : NULL;
            if (fn) {
                cJSON *fname = cJSON_GetObjectItem(fn, "name");
                cJSON *fargs = cJSON_GetObjectItem(fn, "arguments");
                resp_out->is_tool_use = true;
                if (call_id && call_id->valuestring) resp_out->tool_call_id = strdup(call_id->valuestring);
                if (fname && fname->valuestring) resp_out->tool_name = strdup(fname->valuestring);
                if (fargs && fargs->valuestring) resp_out->tool_input = strdup(fargs->valuestring);
            }
        }
    }
    cJSON_Delete(r_json);

    if (!resp_out->content && !resp_out->is_tool_use) {
        resp_out->content = strdup("未能从大模型提取到有效内容。");
    }

    return 0;
}

static int cloud_backend_ping(phoenix_llm_backend_t *self,
                             uint32_t *latency_ms,
                             int *http_status,
                             char *err_buf,
                             size_t err_sz)
{
    (void)self;
    const char *api_key = g_cloud_ctx.config.api_key;
    if (!api_key || api_key[0] == '\0') {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "API Key not configured. Run 'phoenix_agent_app config set api_key <key>'");
        }
        if (http_status) *http_status = 401;
        return -1;
    }

    /* 组装超轻量心跳请求探测连通性 */
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "model", g_cloud_ctx.config.model_name);
    cJSON_AddNumberToObject(payload, "max_tokens", 1);
    cJSON *msg_array = cJSON_CreateArray();
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", "hi");
    cJSON_AddItemToArray(msg_array, msg);
    cJSON_AddItemToObject(payload, "messages", msg_array);

    char *req_body = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);
    if (!req_body) {
        if (err_buf && err_sz > 0) snprintf(err_buf, err_sz, "Failed to build ping payload");
        return -1;
    }

    char *raw_resp = NULL;
    int status = 0;
    uint32_t lat = 0;
    char diag[256] = {0};

    int ret = execute_http_post(g_cloud_ctx.config.base_url,
                                api_key,
                                req_body,
                                &raw_resp,
                                &status,
                                &lat,
                                diag,
                                sizeof(diag));
    free(req_body);

    if (latency_ms) *latency_ms = lat;
    if (http_status) *http_status = status;

    if (raw_resp) {
        /* 若返回非 200，尝试解析错误摘要 */
        if (status != 200) {
            cJSON *r = cJSON_Parse(raw_resp);
            if (r) {
                cJSON *err_obj = cJSON_GetObjectItem(r, "error");
                cJSON *err_txt = err_obj ? cJSON_GetObjectItem(err_obj, "message") : NULL;
                if (err_txt && err_txt->valuestring && err_buf && err_sz > 0) {
                    snprintf(err_buf, err_sz, "%s", err_txt->valuestring);
                }
                cJSON_Delete(r);
            }
        }
        free(raw_resp);
    }

    if (ret != 0) {
        if (err_buf && err_sz > 0 && err_buf[0] == '\0') {
            snprintf(err_buf, err_sz, "%s", diag[0] ? diag : "Connection failed");
        }
        return -1;
    }

    if (status == 200) {
        if (err_buf && err_sz > 0) snprintf(err_buf, err_sz, "OK (HTTP 200)");
        return 0;
    }

    return -1;
}

static bool cloud_backend_is_connected(phoenix_llm_backend_t *self)
{
    (void)self;
    return g_cloud_ctx.agent_connected;
}

static void cloud_backend_deinit(phoenix_llm_backend_t *self)
{
    (void)self;
#ifdef HAVE_VELACLAW_CLIENT
    if (g_cloud_ctx.velaclaw_client) {
        velaclaw_client_close(g_cloud_ctx.velaclaw_client);
        g_cloud_ctx.velaclaw_client = NULL;
    }
#endif
    g_cloud_ctx.agent_connected = false;
}

static phoenix_llm_backend_t g_cloud_backend_instance = {
    .name = "CloudVelaClawBackend",
    .init = cloud_backend_init,
    .chat = cloud_backend_chat,
    .is_connected = cloud_backend_is_connected,
    .ping = cloud_backend_ping,
    .deinit = cloud_backend_deinit,
    .user_data = &g_cloud_ctx
};

phoenix_llm_backend_t *phoenix_llm_cloud_backend_create(void)
{
    return &g_cloud_backend_instance;
}
