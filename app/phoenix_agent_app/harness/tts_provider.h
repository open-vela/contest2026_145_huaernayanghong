/**
 * @file tts_provider.h
 * @brief Unified TTS (Text-to-Speech) Harness Facade for Phoenix Agent
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_TTS_PROVIDER_H
#define PHOENIX_TTS_PROVIDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PHOENIX_TTS_MAX_URL_LEN   256
#define PHOENIX_TTS_MAX_KEY_LEN   128
#define PHOENIX_TTS_MAX_MODEL_LEN 64
#define PHOENIX_TTS_MAX_VOICE_LEN 64

/**
 * @brief TTS Configuration structure
 */
typedef struct {
    char backend[16];                           /**< "cloud" | "mock" */
    char base_url[PHOENIX_TTS_MAX_URL_LEN];     /**< TTS REST API endpoint (e.g. OpenAI /v1/audio/speech) */
    char api_key[PHOENIX_TTS_MAX_KEY_LEN];      /**< Bearer API Key */
    char model_name[PHOENIX_TTS_MAX_MODEL_LEN]; /**< e.g. "tts-1", "cosyvoice-v1" */
    char voice_name[PHOENIX_TTS_MAX_VOICE_LEN]; /**< e.g. "alloy", "echo", "zh-CN-XiaoxiaoNeural" */
} phoenix_tts_config_t;

/**
 * @brief Initialize TTS Provider Facade
 * @param config Pointer to configuration struct, or NULL for defaults
 * @return 0 on success
 */
int phoenix_tts_init(const phoenix_tts_config_t *config);

/**
 * @brief Synthesize text to WAV audio buffer
 * @param text Input text to synthesize
 * @param out_buf Destination buffer for complete WAV data (including 44-byte header)
 * @param max_len Size of destination buffer
 * @param out_len Destination pointer to receive actual WAV byte length
 * @return 0 on success, negative error code on failure
 */
int phoenix_tts_synthesize(const char *text, uint8_t *out_buf, size_t max_len, size_t *out_len);

/**
 * @brief Update TTS API Key dynamically
 * @param api_key New API Key string
 */
void phoenix_tts_set_api_key(const char *api_key);

/**
 * @brief Update TTS Base URL endpoint dynamically
 * @param base_url New Base URL endpoint
 */
void phoenix_tts_set_base_url(const char *base_url);

/**
 * @brief Update TTS Model name dynamically
 * @param model_name New model name (e.g. tts-1)
 */
void phoenix_tts_set_model(const char *model_name);

/**
 * @brief Update TTS Voice name dynamically
 * @param voice_name New voice name (e.g. alloy)
 */
void phoenix_tts_set_voice(const char *voice_name);

/**
 * @brief Switch TTS backend dynamically ("cloud" | "mock")
 * @param backend Backend type string
 */
void phoenix_tts_set_backend(const char *backend);

/**
 * @brief Test TTS backend connectivity and auth
 * @param latency_ms Output roundtrip latency in ms
 * @param http_status Output HTTP status code
 * @param err_buf Output error description buffer
 * @param err_sz Buffer size
 * @return 0 on success (HTTP 200)
 */
int phoenix_tts_ping(uint32_t *latency_ms, int *http_status, char *err_buf, size_t err_sz);

/**
 * @brief De-initialize TTS Provider cleanly
 */
void phoenix_tts_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_TTS_PROVIDER_H */
