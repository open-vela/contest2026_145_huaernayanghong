/**
 * @file web_router.h
 * @brief Phoenix HoloDesk-S1 Micro REST Router & HTTP Context Definition
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_WEB_ROUTER_H
#define PHOENIX_WEB_ROUTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HTTP_METHOD_UNKNOWN = 0,
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_HEAD
} http_method_t;

/** HTTP 结构化请求上下文 */
typedef struct {
    http_method_t method;
    char          path[128];       /**< 规范化请求路径，如 "/api/sdcard/list" */
    char          query[256];      /**< 原始 Query 字符串，如 "path=/sounds&page=1" */
    const char   *raw_http;        /**< 完整原始请求指针 */
    const char   *body;            /**< 请求体起始指针 */
    size_t        body_len;        /**< 请求体长度 */
    cJSON        *json;            /**< 若 Body 为合法 JSON 则自动解析（分发完毕后自动销毁） */
} http_req_t;

/** HTTP 结构化响应上下文 */
typedef struct {
    char   *buf;                   /**< 输出报文缓冲区 */
    size_t  max_len;               /**< 缓冲区最大可用字节 */
    size_t  written_len;           /**< 实际写入字节数 */
    int     status_code;           /**< HTTP 响应状态码 */
} http_resp_t;

/** 统一 API 处理函数原型 */
typedef int (*http_handler_fn)(const http_req_t *req, http_resp_t *resp);

/** 路由表项定义 */
typedef struct {
    http_method_t   method;
    const char     *path;          /**< 路径，如 "/api/wifi/scan" */
    bool            prefix_match;  /**< 是否前缀匹配 (如对于文件下载或重定向) */
    http_handler_fn handler;       /**< 对应的控制器处理函数 */
} http_route_t;

/* =========================================================================
 * 统一响应辅助函数
 * ========================================================================= */

/** 输出纯字符串 JSON 响应 */
void http_resp_json(http_resp_t *resp, int status_code, const char *json_str);

/** 输出由 cJSON 对象构建的 JSON 响应 (输出后自动销毁 cJSON 对象) */
void http_resp_json_obj(http_resp_t *resp, int status_code, cJSON *root);

/** 输出通用错误 JSON 响应: {"success":false, "error":"..."} */
void http_resp_error(http_resp_t *resp, int status_code, const char *error_msg);

/** 输出 HTML 页面响应 */
void http_resp_html(http_resp_t *resp, int status_code, const char *html_str, size_t html_len);

/** 输出文件流 / 附件下载响应 (含 Content-Disposition) */
void http_resp_file_stream(http_resp_t *resp, int status_code, const char *content_type,
                           const char *filename, const void *data, size_t len);

/* =========================================================================
 * 请求参数辅助工具
 * ========================================================================= */

/** 从 Query 字符串中提取指定参数并自动 URL 解码 */
bool http_req_get_query_param(const http_req_t *req, const char *key, char *out_val, size_t max_len);

/** URL 字符串就地或拷贝解码 (%20 -> 空格, %2F -> /) */
void http_urldecode(char *dst, const char *src, size_t dst_len);

/* =========================================================================
 * 路由器引擎生命周期与注册
 * ========================================================================= */

/** 初始化路由器 */
void web_router_init(void);

/** 销毁路由器 */
void web_router_deinit(void);

/**
 * @brief 注册单条 API 路由
 * @param method HTTP 方法
 * @param path 匹配路径
 * @param prefix_match 是否前缀匹配
 * @param handler 回调处理函数
 */
int web_router_register(http_method_t method, const char *path, bool prefix_match, http_handler_fn handler);

/**
 * @brief 批量注册路由表
 * @param routes 路由表数组
 * @param count 路由条目数
 */
int web_router_register_table(const http_route_t *routes, size_t count);

/**
 * @brief 解析原始 HTTP 报文并进行路由匹配分发
 * @param raw_http 客户端发送的原始 HTTP 字符串
 * @param resp_out 写入响应报文的目标缓冲区
 * @param max_len 目标缓冲区大小
 * @return 写入的总字节数，负值表示失败
 */
int web_router_dispatch(const char *raw_http, char *resp_out, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_WEB_ROUTER_H */
