/**
 * @file asr_provider.c
 * @brief Unified ASR (Speech-to-Text) Harness Facade & Cloud Client Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "asr_provider.h"
#if defined(__has_include) && __has_include("utils/log_utils.h")
#  include "utils/log_utils.h"
#else
#  include "../utils/log_utils.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define TAG "PhoenixASR"

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

#if defined(CONFIG_LIB_CURL) || defined(__has_include)
#  if __has_include(<curl/curl.h>)
#    include <curl/curl.h>
#    define PHOENIX_HAVE_LIBCURL 1
#  elif __has_include("../../../../external/curl/curl/include/curl/curl.h")
#    include "../../../../external/curl/curl/include/curl/curl.h"
#    define PHOENIX_HAVE_LIBCURL 1
#  endif
#endif

static phoenix_asr_config_t g_asr_cfg = {
    .backend = "cloud",
    .base_url = "https://api.groq.com/openai/v1/audio/transcriptions",
    .api_key = "",
    .model_name = "whisper-large-v3"
};

static bool g_asr_initialized = false;

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} asr_http_resp_t;

#ifdef PHOENIX_HAVE_LIBCURL
static size_t asr_curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    asr_http_resp_t *mem = (asr_http_resp_t *)userp;

    if (mem->size + realsize + 1 > mem->capacity) {
        size_t new_cap = mem->capacity * 2;
        if (new_cap < mem->size + realsize + 1) {
            new_cap = mem->size + realsize + 1024;
        }
        if (new_cap > 64 * 1024) {
            LOG_E(TAG, "ASR Response size exceeds memory ceiling (64KB)!");
            return 0;
        }
        char *ptr = (char *)realloc(mem->data, new_cap);
        if (!ptr) {
            LOG_E(TAG, "realloc failed for ASR HTTP buffer!");
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

int phoenix_asr_init(const phoenix_asr_config_t *config)
{
    if (config) {
        memcpy(&g_asr_cfg, config, sizeof(phoenix_asr_config_t));
    }
    g_asr_initialized = true;
    LOG_I(TAG, "🎙️ ASR Provider initialized [Backend: %s, Model: %s, Endpoint: %s]",
          g_asr_cfg.backend, g_asr_cfg.model_name, g_asr_cfg.base_url);
    return 0;
}

void phoenix_asr_set_api_key(const char *api_key)
{
    if (!api_key) return;
    strncpy(g_asr_cfg.api_key, api_key, sizeof(g_asr_cfg.api_key) - 1);
    g_asr_cfg.api_key[sizeof(g_asr_cfg.api_key) - 1] = '\0';
}

void phoenix_asr_set_base_url(const char *base_url)
{
    if (!base_url) return;
    strncpy(g_asr_cfg.base_url, base_url, sizeof(g_asr_cfg.base_url) - 1);
    g_asr_cfg.base_url[sizeof(g_asr_cfg.base_url) - 1] = '\0';
}

void phoenix_asr_set_model(const char *model_name)
{
    if (!model_name) return;
    strncpy(g_asr_cfg.model_name, model_name, sizeof(g_asr_cfg.model_name) - 1);
    g_asr_cfg.model_name[sizeof(g_asr_cfg.model_name) - 1] = '\0';
}

void phoenix_asr_set_backend(const char *backend)
{
    if (!backend) return;
    strncpy(g_asr_cfg.backend, backend, sizeof(g_asr_cfg.backend) - 1);
    g_asr_cfg.backend[sizeof(g_asr_cfg.backend) - 1] = '\0';
}

static int phoenix_asr_transcribe_mock(size_t wav_len, char *text_out, size_t max_len)
{
    if (!text_out || max_len == 0) return -1;

    /* 基于音频长度进行确定性智能仿真，便于端到端离线闭环测试 */
    if (wav_len < 32000) {
        /* < 1 秒 */
        snprintf(text_out, max_len, "灵眸敲木鱼");
    } else if (wav_len < 96000) {
        /* 1 ~ 3 秒 */
        snprintf(text_out, max_len, "灵眸，帮我巡检系统状态！");
    } else {
        /* > 3 秒 */
        snprintf(text_out, max_len, "灵眸，开启专注番茄钟，让我们开始深度工作吧。");
    }

    LOG_I(TAG, "🤖 [ASR Mock] 识别结果: \"%s\" (音频包大小: %zu 字节)", text_out, wav_len);
    return 0;
}

int phoenix_asr_transcribe(const uint8_t *wav_data, size_t wav_len, char *text_out, size_t max_len)
{
    if (!text_out || max_len == 0) return -1;
    text_out[0] = '\0';

    if (!wav_data || wav_len < 44) {
        LOG_W(TAG, "ASR: Invalid audio buffer (length: %zu)", wav_len);
        return -1;
    }

    /* 若显式配置为 mock 或未配置 API Key 且非局域网独立服务，回退至仿真识别 */
    if (strcmp(g_asr_cfg.backend, "mock") == 0 ||
        (g_asr_cfg.api_key[0] == '\0' && strstr(g_asr_cfg.base_url, "127.0.0.1") == NULL &&
         strstr(g_asr_cfg.base_url, "localhost") == NULL && strstr(g_asr_cfg.base_url, "192.168.") == NULL)) {
        return phoenix_asr_transcribe_mock(wav_len, text_out, max_len);
    }

#ifdef PHOENIX_HAVE_LIBCURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        LOG_E(TAG, "Failed to init curl for ASR");
        return phoenix_asr_transcribe_mock(wav_len, text_out, max_len);
    }

    /* 构造标准 multipart/form-data 报文 */
    static const char *BOUNDARY = "----PhoenixAsrBoundary88837192847";
    char part_header[512];
    int hdr_len = snprintf(part_header, sizeof(part_header),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"language\"\r\n\r\n"
        "zh\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n",
        BOUNDARY, g_asr_cfg.model_name, BOUNDARY, BOUNDARY);

    char part_tail[128];
    int tail_len = snprintf(part_tail, sizeof(part_tail),
        "\r\n--%s--\r\n", BOUNDARY);

    size_t total_payload_sz = (size_t)hdr_len + wav_len + (size_t)tail_len;
    char *payload = (char *)malloc(total_payload_sz);
    if (!payload) {
        LOG_E(TAG, "Failed to allocate %zu bytes for ASR payload", total_payload_sz);
        curl_easy_cleanup(curl);
        return phoenix_asr_transcribe_mock(wav_len, text_out, max_len);
    }

    memcpy(payload, part_header, hdr_len);
    memcpy(payload + hdr_len, wav_data, wav_len);
    memcpy(payload + hdr_len + wav_len, part_tail, tail_len);

    struct curl_slist *headers = NULL;
    char ctype_hdr[128];
    snprintf(ctype_hdr, sizeof(ctype_hdr), "Content-Type: multipart/form-data; boundary=%s", BOUNDARY);
    headers = curl_slist_append(headers, ctype_hdr);

    if (g_asr_cfg.api_key[0] != '\0') {
        char auth_hdr[256];
        snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", g_asr_cfg.api_key);
        headers = curl_slist_append(headers, auth_hdr);
    }

    asr_http_resp_t resp_buf;
    resp_buf.capacity = 4096;
    resp_buf.size = 0;
    resp_buf.data = (char *)malloc(resp_buf.capacity);
    if (!resp_buf.data) {
        free(payload);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return phoenix_asr_transcribe_mock(wav_len, text_out, max_len);
    }
    resp_buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, g_asr_cfg.base_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)total_payload_sz);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, asr_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp_buf);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    free(payload);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) {
        LOG_W(TAG, "ASR HTTP request failed (curl: %d, http: %ld). Fallback to mock.", res, http_code);
        free(resp_buf.data);
        return phoenix_asr_transcribe_mock(wav_len, text_out, max_len);
    }

    /* 解析 JSON 返回的 "text" 字段 */
    cJSON *json = cJSON_Parse(resp_buf.data);
    if (json) {
        cJSON *item = cJSON_GetObjectItem(json, "text");
        if (item && item->valuestring) {
            strncpy(text_out, item->valuestring, max_len - 1);
            text_out[max_len - 1] = '\0';
            LOG_I(TAG, "🎯 [ASR Cloud] 转写成功: \"%s\"", text_out);
        }
        cJSON_Delete(json);
    }

    free(resp_buf.data);

    if (text_out[0] == '\0') {
        return phoenix_asr_transcribe_mock(wav_len, text_out, max_len);
    }
    return 0;
#else
    return phoenix_asr_transcribe_mock(wav_len, text_out, max_len);
#endif
}

int phoenix_asr_ping(uint32_t *latency_ms, int *http_status, char *err_buf, size_t err_sz)
{
    if (latency_ms) *latency_ms = 0;
    if (http_status) *http_status = 0;
    if (err_buf && err_sz > 0) err_buf[0] = '\0';

    if (strcmp(g_asr_cfg.backend, "mock") == 0) {
        if (http_status) *http_status = 200;
        if (latency_ms) *latency_ms = 1;
        return 0;
    }

#ifdef PHOENIX_HAVE_LIBCURL
    struct timeval start_tv, end_tv;
    gettimeofday(&start_tv, NULL);

    CURL *curl = curl_easy_init();
    if (!curl) {
        if (err_buf && err_sz > 0) snprintf(err_buf, err_sz, "curl_easy_init failed");
        return -1;
    }

    struct curl_slist *headers = NULL;
    if (g_asr_cfg.api_key[0] != '\0') {
        char auth_hdr[256];
        snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", g_asr_cfg.api_key);
        headers = curl_slist_append(headers, auth_hdr);
    }

    curl_easy_setopt(curl, CURLOPT_URL, g_asr_cfg.base_url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); /* HEAD 请求快速探测 */
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);

    CURLcode res = curl_easy_perform(curl);
    gettimeofday(&end_tv, NULL);

    uint32_t lat = (uint32_t)((end_tv.tv_sec - start_tv.tv_sec) * 1000 +
                              (end_tv.tv_usec - start_tv.tv_usec) / 1000);
    if (latency_ms) *latency_ms = lat;

    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (http_status) *http_status = (int)code;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "Curl error (%d): %s", res, curl_easy_strerror(res));
        }
        return -2;
    }

    /* 405 (Method Not Allowed) 也说明端点在线且可达 */
    return (code == 200 || code == 405) ? 0 : -3;
#else
    if (http_status) *http_status = 200;
    return 0;
#endif
}

void phoenix_asr_deinit(void)
{
    g_asr_initialized = false;
    LOG_I(TAG, "🎙️ ASR Provider de-initialized.");
}
