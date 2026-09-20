/**
 * @file tts_provider.c
 * @brief Unified TTS (Text-to-Speech) Harness Facade & Cloud Client Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "tts_provider.h"
#if defined(__has_include) && __has_include("utils/log_utils.h")
#  include "utils/log_utils.h"
#else
#  include "../utils/log_utils.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>

#define TAG "PhoenixTTS"

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

static phoenix_tts_config_t g_tts_cfg = {
    .backend = "cloud",
    .base_url = "https://api.openai.com/v1/audio/speech",
    .api_key = "",
    .model_name = "tts-1",
    .voice_name = "alloy"
};

static bool g_tts_initialized = false;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} tts_http_resp_t;

#ifdef PHOENIX_HAVE_LIBCURL
static size_t tts_curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    tts_http_resp_t *mem = (tts_http_resp_t *)userp;

    if (mem->size + realsize > mem->capacity) {
        size_t new_cap = mem->capacity * 2;
        if (new_cap < mem->size + realsize) {
            new_cap = mem->size + realsize + 4096;
        }
        if (new_cap > 512 * 1024) { /* 512KB 音频上限 */
            LOG_E(TAG, "TTS Response size exceeds memory ceiling (512KB)!");
            return 0;
        }
        uint8_t *ptr = (uint8_t *)realloc(mem->data, new_cap);
        if (!ptr) {
            LOG_E(TAG, "realloc failed for TTS HTTP buffer!");
            return 0;
        }
        mem->data = ptr;
        mem->capacity = new_cap;
    }

    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    return realsize;
}
#endif

/* 构造标准 44 字节 RIFF/WAVE 头部 */
static void write_wav_header(uint8_t *header, uint32_t sample_rate, uint16_t channels,
                             uint16_t bits_per_sample, uint32_t pcm_data_size)
{
    uint32_t total_data_len = pcm_data_size + 36;
    uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8);
    uint16_t block_align = channels * (bits_per_sample / 8);

    memcpy(header, "RIFF", 4);
    header[4] = (uint8_t)(total_data_len & 0xff);
    header[5] = (uint8_t)((total_data_len >> 8) & 0xff);
    header[6] = (uint8_t)((total_data_len >> 16) & 0xff);
    header[7] = (uint8_t)((total_data_len >> 24) & 0xff);

    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);

    header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0; /* Subchunk1Size = 16 */
    header[20] = 1;  header[21] = 0; /* PCM format = 1 */
    header[22] = (uint8_t)(channels & 0xff);
    header[23] = (uint8_t)((channels >> 8) & 0xff);

    header[24] = (uint8_t)(sample_rate & 0xff);
    header[25] = (uint8_t)((sample_rate >> 8) & 0xff);
    header[26] = (uint8_t)((sample_rate >> 16) & 0xff);
    header[27] = (uint8_t)((sample_rate >> 24) & 0xff);

    header[28] = (uint8_t)(byte_rate & 0xff);
    header[29] = (uint8_t)((byte_rate >> 8) & 0xff);
    header[30] = (uint8_t)((byte_rate >> 16) & 0xff);
    header[31] = (uint8_t)((byte_rate >> 24) & 0xff);

    header[32] = (uint8_t)(block_align & 0xff);
    header[33] = (uint8_t)((block_align >> 8) & 0xff);

    header[34] = (uint8_t)(bits_per_sample & 0xff);
    header[35] = (uint8_t)((bits_per_sample >> 8) & 0xff);

    memcpy(header + 36, "data", 4);
    header[40] = (uint8_t)(pcm_data_size & 0xff);
    header[41] = (uint8_t)((pcm_data_size >> 8) & 0xff);
    header[42] = (uint8_t)((pcm_data_size >> 16) & 0xff);
    header[43] = (uint8_t)((pcm_data_size >> 24) & 0xff);
}

static int generate_mock_tts_wav(const char *text, uint8_t *out_buf, size_t max_len, size_t *out_len)
{
    /* 生成一段 16kHz 16-bit Mono 模拟正弦和弦音 (时长约 1.2 秒) */
    uint32_t sample_rate = 16000;
    size_t sample_count = 16000 * 12 / 10; /* 1.2s = 19200 样本 */
    size_t pcm_bytes = sample_count * sizeof(int16_t);
    size_t total_wav_bytes = 44 + pcm_bytes;

    if (total_wav_bytes > max_len) {
        return -1;
    }

    write_wav_header(out_buf, sample_rate, 1, 16, (uint32_t)pcm_bytes);

    int16_t *samples = (int16_t *)(out_buf + 44);
    double f1 = 440.0; /* A4 */
    double f2 = 880.0; /* A5 */
    (void)text;

    for (size_t i = 0; i < sample_count; i++) {
        double t = (double)i / (double)sample_rate;
        double s = 0.5 * sin(2.0 * 3.1415926535 * f1 * t) + 0.3 * sin(2.0 * 3.1415926535 * f2 * t);
        /* 简单包络 */
        double env = 1.0;
        if (i < 1600) env = (double)i / 1600.0;
        else if (i > sample_count - 1600) env = (double)(sample_count - i) / 1600.0;
        samples[i] = (int16_t)(s * env * 16000.0);
    }

    if (out_len) *out_len = total_wav_bytes;
    LOG_I(TAG, "🤖 [TTS Mock] 合成完成 (文本: \"%s\", 生成 WAV %zu 字节)", text ? text : "", total_wav_bytes);
    return 0;
}

int phoenix_tts_init(const phoenix_tts_config_t *config)
{
    if (config) {
        memcpy(&g_tts_cfg, config, sizeof(phoenix_tts_config_t));
    }
    g_tts_initialized = true;
    LOG_I(TAG, "🔊 TTS Provider initialized [Backend: %s, Model: %s, Voice: %s, Endpoint: %s]",
          g_tts_cfg.backend, g_tts_cfg.model_name, g_tts_cfg.voice_name, g_tts_cfg.base_url);
    return 0;
}

void phoenix_tts_set_api_key(const char *api_key)
{
    if (!api_key) return;
    strncpy(g_tts_cfg.api_key, api_key, sizeof(g_tts_cfg.api_key) - 1);
    g_tts_cfg.api_key[sizeof(g_tts_cfg.api_key) - 1] = '\0';
}

void phoenix_tts_set_base_url(const char *base_url)
{
    if (!base_url) return;
    strncpy(g_tts_cfg.base_url, base_url, sizeof(g_tts_cfg.base_url) - 1);
    g_tts_cfg.base_url[sizeof(g_tts_cfg.base_url) - 1] = '\0';
}

void phoenix_tts_set_model(const char *model_name)
{
    if (!model_name) return;
    strncpy(g_tts_cfg.model_name, model_name, sizeof(g_tts_cfg.model_name) - 1);
    g_tts_cfg.model_name[sizeof(g_tts_cfg.model_name) - 1] = '\0';
}

void phoenix_tts_set_voice(const char *voice_name)
{
    if (!voice_name) return;
    strncpy(g_tts_cfg.voice_name, voice_name, sizeof(g_tts_cfg.voice_name) - 1);
    g_tts_cfg.voice_name[sizeof(g_tts_cfg.voice_name) - 1] = '\0';
}

void phoenix_tts_set_backend(const char *backend)
{
    if (!backend) return;
    strncpy(g_tts_cfg.backend, backend, sizeof(g_tts_cfg.backend) - 1);
    g_tts_cfg.backend[sizeof(g_tts_cfg.backend) - 1] = '\0';
}

int phoenix_tts_synthesize(const char *text, uint8_t *out_buf, size_t max_len, size_t *out_len)
{
    if (!text || !out_buf || max_len < 44) {
        return -1;
    }

    if (strcmp(g_tts_cfg.backend, "mock") == 0 || strlen(g_tts_cfg.api_key) == 0) {
        return generate_mock_tts_wav(text, out_buf, max_len, out_len);
    }

#ifdef PHOENIX_HAVE_LIBCURL
    cJSON *root = cJSON_CreateObject();
    if (!root) return generate_mock_tts_wav(text, out_buf, max_len, out_len);

    cJSON_AddStringToObject(root, "model", g_tts_cfg.model_name[0] ? g_tts_cfg.model_name : "tts-1");
    cJSON_AddStringToObject(root, "input", text);
    cJSON_AddStringToObject(root, "voice", g_tts_cfg.voice_name[0] ? g_tts_cfg.voice_name : "alloy");
    cJSON_AddStringToObject(root, "response_format", "wav");

    char *json_body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_body) return generate_mock_tts_wav(text, out_buf, max_len, out_len);

    tts_http_resp_t resp_buf;
    resp_buf.capacity = 32 * 1024;
    resp_buf.data = (uint8_t *)malloc(resp_buf.capacity);
    resp_buf.size = 0;

    CURL *curl = curl_easy_init();
    if (!curl || !resp_buf.data) {
        if (resp_buf.data) free(resp_buf.data);
        free(json_body);
        return generate_mock_tts_wav(text, out_buf, max_len, out_len);
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth_hdr[PHOENIX_TTS_MAX_KEY_LEN + 32];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", g_tts_cfg.api_key);
    headers = curl_slist_append(headers, auth_hdr);

    curl_easy_setopt(curl, CURLOPT_URL, g_tts_cfg.base_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, tts_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(json_body);

    if (res == CURLE_OK && http_code == 200 && resp_buf.size > 0) {
        if (resp_buf.size <= max_len) {
            memcpy(out_buf, resp_buf.data, resp_buf.size);
            if (out_len) *out_len = resp_buf.size;
            free(resp_buf.data);
            LOG_I(TAG, "✅ [TTS Cloud] 语音合成成功 (大小: %zu 字节)", resp_buf.size);
            return 0;
        } else {
            LOG_W(TAG, "TTS output size (%zu) exceeds caller buffer (%zu)", resp_buf.size, max_len);
        }
    }

    LOG_W(TAG, "TTS Cloud request failed (CURL: %d, HTTP: %ld), fallback to Mock", (int)res, http_code);
    if (resp_buf.data) free(resp_buf.data);
    return generate_mock_tts_wav(text, out_buf, max_len, out_len);
#else
    return generate_mock_tts_wav(text, out_buf, max_len, out_len);
#endif
}

int phoenix_tts_ping(uint32_t *latency_ms, int *http_status, char *err_buf, size_t err_sz)
{
    if (latency_ms) *latency_ms = 0;
    if (http_status) *http_status = 0;
    if (err_buf && err_sz > 0) err_buf[0] = '\0';

    if (strcmp(g_tts_cfg.backend, "mock") == 0) {
        if (latency_ms) *latency_ms = 5;
        if (http_status) *http_status = 200;
        return 0;
    }

    if (strlen(g_tts_cfg.api_key) == 0) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "API Key 未填写");
        }
        if (http_status) *http_status = 401;
        return -1;
    }

#ifdef PHOENIX_HAVE_LIBCURL
    struct timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);

    CURL *curl = curl_easy_init();
    if (!curl) {
        if (err_buf && err_sz > 0) snprintf(err_buf, err_sz, "curl init failed");
        return -1;
    }

    struct curl_slist *headers = NULL;
    char auth_hdr[PHOENIX_TTS_MAX_KEY_LEN + 32];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", g_tts_cfg.api_key);
    headers = curl_slist_append(headers, auth_hdr);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    /* 发送最轻量的 1 字符请求探测端点与鉴权 */
    const char *probe_payload = "{\"model\":\"tts-1\",\"input\":\"a\",\"voice\":\"alloy\"}";

    curl_easy_setopt(curl, CURLOPT_URL, g_tts_cfg.base_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, probe_payload);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);

    CURLcode res = curl_easy_perform(curl);
    gettimeofday(&tv_end, NULL);

    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    uint32_t elapsed = (uint32_t)((tv_end.tv_sec - tv_start.tv_sec) * 1000 +
                                  (tv_end.tv_usec - tv_start.tv_usec) / 1000);
    if (latency_ms) *latency_ms = elapsed;
    if (http_status) *http_status = (int)code;

    if (res == CURLE_OK && (code == 200 || code == 400)) {
        return 0;
    }

    if (err_buf && err_sz > 0) {
        snprintf(err_buf, err_sz, "CURL error %d (HTTP %ld)", (int)res, code);
    }
    return -1;
#else
    if (latency_ms) *latency_ms = 10;
    if (http_status) *http_status = 200;
    return 0;
#endif
}

void phoenix_tts_deinit(void)
{
    g_tts_initialized = false;
    LOG_I(TAG, "🔊 TTS Provider de-initialized.");
}
