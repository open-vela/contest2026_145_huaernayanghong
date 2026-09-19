/**
 * @file llm_backend.h
 * @brief LLM Backend Adapter Strategy Interface
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_LLM_BACKEND_H
#define PHOENIX_LLM_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "llm_types.h"

typedef struct phoenix_llm_backend_s phoenix_llm_backend_t;

/**
 * @brief LLM Driver/Backend Interface
 */
struct phoenix_llm_backend_s {
    const char *name;

    /**
     * @brief Initialize backend instance with config
     */
    int (*init)(phoenix_llm_backend_t *self, const phoenix_llm_config_t *config);

    /**
     * @brief Send chat messages to the backend
     */
    int (*chat)(phoenix_llm_backend_t *self,
                const phoenix_chat_msg_t *messages,
                size_t msg_count,
                const char *tools_json,
                phoenix_chat_resp_t *resp_out);

    /**
     * @brief Check if backend has active connectivity (e.g. VelaClaw or Network)
     */
    bool (*is_connected)(phoenix_llm_backend_t *self);

    /**
     * @brief Send lightweight ping to test backend connectivity and auth
     */
    int (*ping)(phoenix_llm_backend_t *self,
                uint32_t *latency_ms,
                int *http_status,
                char *err_buf,
                size_t err_sz);

    /**
     * @brief De-initialize backend resources
     */
    void (*deinit)(phoenix_llm_backend_t *self);

    void *user_data;
};

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_LLM_BACKEND_H */
