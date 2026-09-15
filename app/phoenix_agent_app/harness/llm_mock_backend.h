/**
 * @file llm_mock_backend.h
 * @brief Offline Mock & Embody Fallback Backend for LLM Harness
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_LLM_MOCK_BACKEND_H
#define PHOENIX_LLM_MOCK_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "llm_backend.h"

/**
 * @brief Create an offline mock backend instance
 * @return Pointer to initialized backend structure, or NULL
 */
phoenix_llm_backend_t *phoenix_llm_mock_backend_create(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_LLM_MOCK_BACKEND_H */
