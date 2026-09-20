/**
 * @file asr_provider.h
 * @brief Unified ASR (Speech-to-Text) Harness Facade for Phoenix Agent
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_ASR_PROVIDER_H
#define PHOENIX_ASR_PROVIDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PHOENIX_ASR_MAX_URL_LEN   256
#define PHOENIX_ASR_MAX_KEY_LEN   128
#define PHOENIX_ASR_MAX_MODEL_LEN 64

/**
 * @brief ASR Configuration structure
 */
typedef struct {
    char backend[16];                           /**< "cloud" | "mock" */
    char base_url[PHOENIX_ASR_MAX_URL_LEN];     /**< ASR REST API endpoint */
    char api_key[PHOENIX_ASR_MAX_KEY_LEN];      /**< Bearer API Key */
    char model_name[PHOENIX_ASR_MAX_MODEL_LEN]; /**< e.g. "whisper-large-v3", "sensevoice-v1" */
} phoenix_asr_config_t;

/**
 * @brief Initialize ASR Provider Facade
 * @param config Pointer to configuration struct, or NULL for defaults
 * @return 0 on success
 */
int phoenix_asr_init(const phoenix_asr_config_t *config);

/**
 * @brief Transcribe in-memory WAV audio buffer to text
 * @param wav_data Pointer to complete WAV data (including 44-byte header)
 * @param wav_len Length of WAV data in bytes
 * @param text_out Output buffer for recognized text
 * @param max_len Size of output buffer
 * @return 0 on success, negative error code on failure
 */
int phoenix_asr_transcribe(const uint8_t *wav_data, size_t wav_len, char *text_out, size_t max_len);

/**
 * @brief Update ASR API Key dynamically
 * @param api_key New API Key string
 */
void phoenix_asr_set_api_key(const char *api_key);

/**
 * @brief Update ASR Base URL endpoint dynamically
 * @param base_url New Base URL endpoint
 */
void phoenix_asr_set_base_url(const char *base_url);

/**
 * @brief Update ASR Model name dynamically
 * @param model_name New model name (e.g. whisper-large-v3)
 */
void phoenix_asr_set_model(const char *model_name);

/**
 * @brief Switch ASR backend dynamically ("cloud" | "mock")
 * @param backend Backend type string
 */
void phoenix_asr_set_backend(const char *backend);

/**
 * @brief Test ASR backend connectivity and auth
 * @param latency_ms Output roundtrip latency in ms
 * @param http_status Output HTTP status code
 * @param err_buf Output error description buffer
 * @param err_sz Buffer size
 * @return 0 on success (HTTP 200)
 */
int phoenix_asr_ping(uint32_t *latency_ms, int *http_status, char *err_buf, size_t err_sz);

/**
 * @brief De-initialize ASR Provider cleanly
 */
void phoenix_asr_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_ASR_PROVIDER_H */
