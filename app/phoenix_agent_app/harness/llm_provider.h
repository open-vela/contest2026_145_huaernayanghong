/**
 * @file llm_provider.h
 * @brief Unified LLM Harness Facade & Router for Phoenix Agent
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_LLM_PROVIDER_H
#define PHOENIX_LLM_PROVIDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "llm_types.h"
#include "llm_backend.h"

/**
 * @brief Initialize LLM Provider Facade with configuration
 * @param config Pointer to configuration struct, or NULL for defaults
 * @return 0 on success
 */
int phoenix_llm_provider_init(const phoenix_llm_config_t *config);

/**
 * @brief Send chat messages with Tool schemas to active LLM backend
 * @param messages Array of history messages
 * @param msg_count Count of messages
 * @param tools_json Tools JSON Schema string (can be NULL)
 * @param resp_out Output response structure
 * @return 0 on success, negative error code on failure
 */
int phoenix_llm_provider_chat(const phoenix_chat_msg_t *messages,
                              size_t msg_count,
                              const char *tools_json,
                              phoenix_chat_resp_t *resp_out);

/**
 * @brief Set or switch the active LLM backend driver dynamically
 * @param backend Pointer to backend driver instance (e.g. mock or cloud)
 * @return 0 on success, -1 on invalid argument
 */
int phoenix_llm_provider_set_backend(phoenix_llm_backend_t *backend);

/**
 * @brief Get currently active backend driver instance
 * @return Pointer to active backend
 */
phoenix_llm_backend_t *phoenix_llm_provider_get_backend(void);

/**
 * @brief Update API Key dynamically
 * @param api_key New API Key string
 */
void phoenix_llm_set_api_key(const char *api_key);

/**
 * @brief Update Base URL endpoint dynamically
 * @param base_url New Base URL endpoint
 */
void phoenix_llm_set_base_url(const char *base_url);

/**
 * @brief Update Model name dynamically
 * @param model_name New model name (e.g. deepseek-chat)
 */
void phoenix_llm_set_model(const char *model_name);

/**
 * @brief Send lightweight ping to test backend connectivity and auth
 * @param latency_ms Output roundtrip latency in ms
 * @param http_status Output HTTP status code
 * @param err_buf Output error description buffer
 * @param err_sz Buffer size
 * @return 0 on success (HTTP 200)
 */
int phoenix_llm_ping(uint32_t *latency_ms, int *http_status, char *err_buf, size_t err_sz);

/**
 * @brief Check if currently connected to system-level VelaClaw Agent or active network
 */
bool phoenix_llm_is_agent_connected(void);

/**
 * @brief De-initialize LLM Provider and current backend cleanly
 */
void phoenix_llm_provider_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_LLM_PROVIDER_H */
