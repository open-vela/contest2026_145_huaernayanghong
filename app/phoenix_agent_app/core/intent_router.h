/**
 * @file intent_router.h
 * @brief Fast-path Intent Router & Workflow Engine for Phoenix HoloDesk-S1
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_INTENT_ROUTER_H
#define PHOENIX_INTENT_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Categorized Intent Types
 */
typedef enum {
    INTENT_TYPE_NONE = 0,
    INTENT_TYPE_FASTPATH,       /**< Fast-path local hardware/tool control (0ms, 0 Token) */
    INTENT_TYPE_WORKFLOW,       /**< Composite macro workflow */
    INTENT_TYPE_LLM_REASONING   /**< Open-ended chat / complex reasoning requiring LLM */
} phoenix_intent_category_t;

/**
 * @brief Routing Outcome
 */
typedef struct {
    phoenix_intent_category_t category;
    const char *tool_name;       /**< Tool name to invoke if fast-path */
    const char *tool_args_json;  /**< JSON arguments string for tool */
    const char *fast_reply;      /**< Instant response message */
    const char *workflow_name;   /**< Workflow identifier if composite */
} phoenix_intent_result_t;

/**
 * @brief Route user prompt into fast-path or LLM reasoning
 * @param input_text User prompt string
 * @return Routing outcome structure
 */
phoenix_intent_result_t phoenix_intent_route(const char *input_text);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_INTENT_ROUTER_H */
