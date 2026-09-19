/**
 * @file web_router.c
 * @brief Phoenix HoloDesk-S1 Micro REST Router & Dispatcher Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_router.h"
#include "../hal/network_mgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_ROUTES 128
static http_route_t g_routes[MAX_ROUTES];
static size_t g_route_count = 0;

extern void web_api_register_all(void);

void web_router_init(void)
{
    g_route_count = 0;
    memset(g_routes, 0, sizeof(g_routes));
}

void web_router_deinit(void)
{
    /* 保持静态路由表存在，避免测试中反复多次单测时被清空 */
}

int web_router_register(http_method_t method, const char *path, bool prefix_match, http_handler_fn handler)
{
    if (!path || !handler || g_route_count >= MAX_ROUTES) return -1;

    g_routes[g_route_count].method = method;
    g_routes[g_route_count].path = path;
    g_routes[g_route_count].prefix_match = prefix_match;
    g_routes[g_route_count].handler = handler;
    g_route_count++;
    return 0;
}

int web_router_register_table(const http_route_t *routes, size_t count)
{
    if (!routes) return -1;
    for (size_t i = 0; i < count; i++) {
        if (web_router_register(routes[i].method, routes[i].path, routes[i].prefix_match, routes[i].handler) != 0) {
            return -1;
        }
    }
    return 0;
}

void http_urldecode(char *dst, const char *src, size_t dst_len)
{
    if (!dst || !src || dst_len == 0) return;
    char a, b;
    size_t d = 0;
    while (*src && d + 1 < dst_len) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit((unsigned char)a) && isxdigit((unsigned char)b))) {
            if (a >= 'a') a -= 'a' - 10;
            else if (a >= 'A') a -= 'A' - 10;
            else a -= '0';
            if (b >= 'a') b -= 'a' - 10;
            else if (b >= 'A') b -= 'A' - 10;
            else b -= '0';
            dst[d++] = (char)(16 * a + b);
            src += 3;
        } else if (*src == '+') {
            dst[d++] = ' ';
            src++;
        } else {
            dst[d++] = *src++;
        }
    }
    dst[d] = '\0';
}

bool http_req_get_query_param(const http_req_t *req, const char *key, char *out_val, size_t max_len)
{
    if (!req || !key || !out_val || max_len == 0 || req->query[0] == '\0') {
        if (out_val && max_len > 0) out_val[0] = '\0';
        return false;
    }
    out_val[0] = '\0';

    char search_key[64];
    snprintf(search_key, sizeof(search_key), "%s=", key);
    const char *p = strstr(req->query, search_key);
    if (!p) {
        return false;
    }
    p += strlen(search_key);
    const char *val_end = p;
    while (*val_end != '\0' && *val_end != '&') {
        val_end++;
    }
    size_t raw_len = (size_t)(val_end - p);
    char raw_buf[256];
    if (raw_len >= sizeof(raw_buf)) raw_len = sizeof(raw_buf) - 1;
    memcpy(raw_buf, p, raw_len);
    raw_buf[raw_len] = '\0';

    http_urldecode(out_val, raw_buf, max_len);
    return true;
}

static const char* status_code_to_str(int code)
{
    switch (code) {
        case 200: return "200 OK";
        case 204: return "204 No Content";
        case 302: return "302 Found";
        case 400: return "400 Bad Request";
        case 403: return "403 Forbidden";
        case 404: return "404 Not Found";
        case 429: return "429 Too Many Requests";
        case 500: return "500 Internal Server Error";
        case 503: return "503 Service Unavailable";
        default:  return "200 OK";
    }
}

void http_resp_json(http_resp_t *resp, int status_code, const char *json_str)
{
    if (!resp || !resp->buf || resp->max_len < 64) return;
    if (!json_str) json_str = "{}";

    size_t content_len = strlen(json_str);
    int written = snprintf(resp->buf, resp->max_len,
                           "HTTP/1.1 %s\r\n"
                           "Content-Type: application/json; charset=UTF-8\r\n"
                           "Content-Length: %zu\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Connection: close\r\n\r\n%s",
                           status_code_to_str(status_code), content_len, json_str);
    resp->status_code = status_code;
    if (written > 0) {
        resp->written_len = ((size_t)written < resp->max_len) ? (size_t)written : (resp->max_len - 1);
    } else {
        resp->written_len = 0;
    }
}

void http_resp_json_obj(http_resp_t *resp, int status_code, cJSON *root)
{
    if (!resp) {
        if (root) cJSON_Delete(root);
        return;
    }
    char *rendered = root ? cJSON_PrintUnformatted(root) : NULL;
    http_resp_json(resp, status_code, rendered ? rendered : "{}");
    if (rendered) free(rendered);
    if (root) cJSON_Delete(root);
}

void http_resp_error(http_resp_t *resp, int status_code, const char *error_msg)
{
    char err_json[256];
    snprintf(err_json, sizeof(err_json), "{\"success\":false,\"error\":\"%s\"}",
             error_msg ? error_msg : "Unknown Error");
    http_resp_json(resp, status_code, err_json);
}

void http_resp_html(http_resp_t *resp, int status_code, const char *html_str, size_t html_len)
{
    if (!resp || !resp->buf || resp->max_len < 64) return;
    if (!html_str) {
        html_str = "";
        html_len = 0;
    }
    if (html_len == 0) html_len = strlen(html_str);

    int written = snprintf(resp->buf, resp->max_len,
                           "HTTP/1.1 %s\r\n"
                           "Content-Type: text/html; charset=UTF-8\r\n"
                           "Content-Length: %zu\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Connection: close\r\n\r\n%s",
                           status_code_to_str(status_code), html_len, html_str);
    resp->status_code = status_code;
    if (written > 0) {
        resp->written_len = ((size_t)written < resp->max_len) ? (size_t)written : (resp->max_len - 1);
    } else {
        resp->written_len = 0;
    }
}

void http_resp_file_stream(http_resp_t *resp, int status_code, const char *content_type,
                           const char *filename, const void *data, size_t len)
{
    if (!resp || !resp->buf || resp->max_len < 128) return;
    if (!content_type) content_type = "application/octet-stream";

    char header_buf[512];
    int hlen = 0;
    if (filename && filename[0]) {
        hlen = snprintf(header_buf, sizeof(header_buf),
                        "HTTP/1.1 %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Disposition: attachment; filename=\"%s\"\r\n"
                        "Content-Length: %zu\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Connection: close\r\n\r\n",
                        status_code_to_str(status_code), content_type, filename, len);
    } else {
        hlen = snprintf(header_buf, sizeof(header_buf),
                        "HTTP/1.1 %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %zu\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Connection: close\r\n\r\n",
                        status_code_to_str(status_code), content_type, len);
    }

    if ((size_t)hlen >= resp->max_len) return;

    memcpy(resp->buf, header_buf, (size_t)hlen);
    size_t copy_body = len;
    if ((size_t)hlen + copy_body >= resp->max_len) {
        copy_body = resp->max_len - (size_t)hlen - 1;
    }
    if (data && copy_body > 0) {
        memcpy(resp->buf + hlen, data, copy_body);
    }
    resp->buf[hlen + copy_body] = '\0';
    resp->status_code = status_code;
    resp->written_len = (size_t)hlen + copy_body;
}

void http_resp_redirect(http_resp_t *resp, int status_code, const char *location)
{
    if (!resp || !resp->buf || resp->max_len == 0 || !location) return;

    int written = snprintf(resp->buf, resp->max_len,
                           "HTTP/1.1 %s\r\n"
                           "Location: %s\r\n"
                           "Content-Length: 0\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Connection: close\r\n\r\n",
                           status_code_to_str(status_code), location);
    resp->status_code = status_code;
    resp->written_len = (written > 0) ? (size_t)written : 0;
}

static http_method_t parse_http_method(const char *req)
{
    if (strncmp(req, "GET ", 4) == 0) return HTTP_METHOD_GET;
    if (strncmp(req, "POST ", 5) == 0) return HTTP_METHOD_POST;
    if (strncmp(req, "PUT ", 4) == 0) return HTTP_METHOD_PUT;
    if (strncmp(req, "DELETE ", 7) == 0) return HTTP_METHOD_DELETE;
    if (strncmp(req, "OPTIONS ", 8) == 0) return HTTP_METHOD_OPTIONS;
    if (strncmp(req, "HEAD ", 5) == 0) return HTTP_METHOD_HEAD;
    return HTTP_METHOD_UNKNOWN;
}

int web_router_dispatch(const char *raw_http, char *resp_out, size_t max_len)
{
    if (!raw_http || !resp_out || max_len < 64) return -1;

    if (g_route_count == 0) {
        web_api_register_all();
    }

    http_req_t req;
    memset(&req, 0, sizeof(req));
    req.raw_http = raw_http;
    req.method = parse_http_method(raw_http);

    /* 1. 解析请求行 (Method, URI) */
    const char *p = raw_http;
    while (*p && *p != ' ') p++;
    if (*p == ' ') p++;

    const char *uri_start = p;
    while (*p && *p != ' ' && *p != '\r' && *p != '\n') p++;
    size_t uri_len = (size_t)(p - uri_start);

    char raw_uri[256];
    if (uri_len >= sizeof(raw_uri)) uri_len = sizeof(raw_uri) - 1;
    memcpy(raw_uri, uri_start, uri_len);
    raw_uri[uri_len] = '\0';

    /* 分离 Path 与 Query */
    char *qmark = strchr(raw_uri, '?');
    if (qmark) {
        *qmark = '\0';
        strncpy(req.query, qmark + 1, sizeof(req.query) - 1);
    }
    http_urldecode(req.path, raw_uri, sizeof(req.path));

    /* 1.5 CORS OPTIONS 预检全局快速放行 */
    if (req.method == HTTP_METHOD_OPTIONS) {
        int written = snprintf(resp_out, max_len,
                               "HTTP/1.1 204 No Content\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                               "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n"
                               "Access-Control-Max-Age: 86400\r\n"
                               "Content-Length: 0\r\n"
                               "Connection: close\r\n\r\n");
        return (written > 0) ? written : 0;
    }

    /* 2. 定位 Body 与 JSON 自动解析 */
    const char *body_marker = strstr(raw_http, "\r\n\r\n");
    if (body_marker) {
        req.body = body_marker + 4;
        req.body_len = strlen(req.body);
        const char *bp = req.body;
        while (*bp && isspace((unsigned char)*bp)) bp++;
        if (*bp == '{' || *bp == '[') {
            req.json = cJSON_Parse(bp);
        }
    }

    /* 3. 准备 Response 上下文 */
    http_resp_t resp;
    resp.buf = resp_out;
    resp.max_len = max_len;
    resp.written_len = 0;
    resp.status_code = 200;

    /* 4. 路由匹配与执行 */
    bool handled = false;
    for (size_t i = 0; i < g_route_count; i++) {
        const http_route_t *r = &g_routes[i];
        if (r->method != req.method) continue;

        bool match = false;
        if (r->prefix_match) {
            match = (strncmp(req.path, r->path, strlen(r->path)) == 0);
        } else {
            match = (strcmp(req.path, r->path) == 0);
        }

        if (match && r->handler) {
            r->handler(&req, &resp);
            handled = true;
            break;
        }
    }

    /* 5. 未匹配路由 Fallback (SoftAP 模式下非 API 路径统一 302 重定向到 http://192.168.4.1/) */
    if (!handled) {
        if (net_mgr_get_mode() == NET_MODE_SOFTAP_CONFIG && strncmp(req.path, "/api/", 5) != 0) {
            http_resp_redirect(&resp, 302, "http://192.168.4.1/");
        } else {
            http_resp_error(&resp, 404, "Not Found");
        }
    }

    /* 6. 释放自动解析的 JSON */
    if (req.json) {
        cJSON_Delete(req.json);
        req.json = NULL;
    }

    return (int)resp.written_len;
}
