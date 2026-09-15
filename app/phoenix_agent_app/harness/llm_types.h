/**
 * @file llm_types.h
 * @brief Common Message and Configuration Types for LLM Harness
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_LLM_TYPES_H
#define PHOENIX_LLM_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PHOENIX_MAX_MESSAGES   24
#define PHOENIX_MAX_URL_LEN    256
#define PHOENIX_MAX_KEY_LEN    128
#define PHOENIX_MAX_MODEL_LEN  64

/**
 * @brief Message role in chat conversation
 */
typedef enum {
    PHOENIX_ROLE_SYSTEM = 0,
    PHOENIX_ROLE_USER,
    PHOENIX_ROLE_ASSISTANT,
    PHOENIX_ROLE_TOOL
} phoenix_msg_role_t;

/**
 * @brief Single chat message
 */
typedef struct {
    phoenix_msg_role_t role;
    char *content;           /**< Text content (heap allocated) */
    char *tool_call_id;      /**< Non-NULL if role == TOOL or assistant tool call */
    char *tool_name;         /**< Tool name invoked by assistant */
    char *reasoning_content; /**< Thinking mode reasoning content (heap allocated) */
} phoenix_chat_msg_t;

/**
 * @brief Chat completion response structure
 */
typedef struct {
    char *content;           /**< Assistant reply text (heap allocated) */
    char *tool_call_id;      /**< Tool call identifier */
    char *tool_name;         /**< Name of tool to execute */
    char *tool_input;        /**< Arguments JSON string for tool */
    char *reasoning_content; /**< Thinking mode reasoning content (heap allocated) */
    bool is_tool_use;        /**< True if LLM requested a Tool Call */
    uint32_t prompt_tokens;      /**< Tokens in prompt */
    uint32_t completion_tokens;  /**< Tokens in completion */
    uint32_t total_tokens;       /**< Total tokens consumed */
    uint32_t latency_ms;         /**< LLM round-trip latency in milliseconds */
} phoenix_chat_resp_t;

/**
 * @brief LLM Provider Configuration
 */
typedef struct {
    char api_key[PHOENIX_MAX_KEY_LEN];
    char base_url[PHOENIX_MAX_URL_LEN];
    char model_name[PHOENIX_MAX_MODEL_LEN];
    int  temperature;        /**< 0 - 100, e.g. 70 = 0.7 */
    const char *system_prompt;
} phoenix_llm_config_t;

/**
 * @brief Free resources allocated in chat response
 * @param resp Pointer to response structure
 */
void phoenix_llm_resp_free(phoenix_chat_resp_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_LLM_TYPES_H */
