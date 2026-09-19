/**
 * @file agent_core.h
 * @brief Master Agent Orchestrator & State Engine for Phoenix HoloDesk-S1
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_AGENT_CORE_H
#define PHOENIX_AGENT_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "event_bus.h"
#include "tool_registry.h"
#if defined(__has_include) && __has_include("../harness/llm_provider.h")
#  include "../harness/llm_provider.h"
#else
#  include "harness/llm_provider.h"
#endif

/**
 * @brief Master Agent operational states
 */
typedef enum {
    AGENT_STATE_IDLE = 0,         /**< Standby, cyber eye breathing */
    AGENT_STATE_LISTENING,        /**< Listening for voice / command */
    AGENT_STATE_THINKING,         /**< LLM inference / Planning execution chain */
    AGENT_STATE_EXECUTING,        /**< Tool Calling in progress */
    AGENT_STATE_CELEBRATING,      /**< PR merged / Merit milestone reached */
    AGENT_STATE_POMODORO,         /**< Deep focus / Pomodoro timer running */
    AGENT_STATE_ALERT             /**< CI failure / Abnormal alarm */
} phoenix_core_state_t;

/**
 * @brief Agent runtime statistics & metrics
 */
typedef struct {
    uint32_t merit_count;         /**< Accumulated cyber merit / PR count */
    uint32_t interaction_count;   /**< Total interaction count */
    uint32_t uptime_seconds;      /**< System uptime in seconds */
    uint16_t pomodoro_remaining_s;/**< Remaining Pomodoro seconds */
    bool pomodoro_active;         /**< Whether Pomodoro timer is active */
    uint32_t total_tokens_used;   /**< Total Tokens consumed */
    uint32_t last_latency_ms;     /**< Latency of last inference */
    uint32_t continuous_focus_s;  /**< Continuous focus seconds */
    bool proactive_enabled;       /**< Whether proactive engine is active */
} phoenix_agent_stats_t;

/**
 * @brief Forward declaration of Agent context
 */
typedef struct phoenix_agent_ctx_s phoenix_agent_ctx_t;

#include <pthread.h>

struct phoenix_agent_ctx_s {
    phoenix_core_state_t state;
    phoenix_agent_stats_t stats;
    phoenix_chat_msg_t history[PHOENIX_MAX_MESSAGES];
    size_t history_count;
    char *context_summary;          /**< 两级记忆模型：滚入后台的历史上下文摘要卡片 */
    size_t total_history_bytes;     /**< 动态堆内存水位监测 (字节计数) */
    uint32_t proactive_threshold_s; /**< Focus threshold seconds before intervention */
    pthread_mutex_t core_lock;      /**< 核心上下文与状态机互斥锁 */
};

/**
 * @brief Initialize Master Agent Core
 * @return Allocated agent context pointer
 */
phoenix_agent_ctx_t* phoenix_agent_core_init(void);

/**
 * @brief Configure proactive intervention rules
 * @param ctx Agent context
 * @param focus_timeout_s Focus threshold seconds before triggering intervention (0 for default 2700s)
 */
void phoenix_agent_set_proactive_threshold(phoenix_agent_ctx_t *ctx, uint32_t focus_timeout_s);

/**
 * @brief Manually or externally trigger a proactive intervention
 * @param ctx Agent context
 * @param type Proactive category
 * @param custom_reason Reason string
 * @return 0 on success
 */
int phoenix_agent_trigger_proactive(phoenix_agent_ctx_t *ctx, phoenix_proactive_type_t type, const char *custom_reason);

/**
 * @brief Transition agent to a new state and broadcast via Event Bus
 * @param ctx Agent context
 * @param new_state New state
 * @param message Description message
 */
void phoenix_agent_set_state(phoenix_agent_ctx_t *ctx, phoenix_core_state_t new_state, const char *message);

/**
 * @brief ReAct 单轮执行链路白盒追踪结构体 (供 Web 伴侣与调试终端透明可视化)
 */
typedef struct {
    char user_prompt[256];          /**< 用户请求入参 Prompt */
    char reasoning_content[2048];   /**< 大模型深度思考链 Thinking */
    char tool_name[64];             /**< 命中的具身工具名称 (若有) */
    char tool_args[512];            /**< 传入具身工具的 JSON 参数 */
    char tool_observation[512];     /**< 端侧具身硬件执行结果 Observation */
    char final_answer[2048];        /**< 最终拟人化回复文案 Answer */
    uint32_t latency_ms;            /**< 端到端往返总耗时 (毫秒) */
    uint32_t prompt_tokens;         /**< 输入 Token 数 */
    uint32_t completion_tokens;     /**< 输出 Token 数 */
    uint32_t total_tokens;          /**< 累计消耗 Token 数 */
    bool has_tool_call;             /**< 本轮对话是否调用了端侧工具 */
    bool success;                   /**< 本轮执行是否成功 */
    phoenix_core_state_t end_state; /**< 对话结束后的状态机状态 */
} phoenix_agent_trace_t;

/**
 * @brief Dispatch user prompt into ReAct Agent loop synchronously
 * @param ctx Agent context
 * @param user_input User question or intent string
 * @return 0 on success
 */
int phoenix_agent_chat(phoenix_agent_ctx_t *ctx, const char *user_input);

/**
 * @brief Dispatch user prompt into ReAct Agent loop synchronously and capture full ReAct execution trace
 * @param ctx Agent context
 * @param user_input User question or intent string
 * @param trace_out Destination pointer to receive detailed ReAct execution trace (can be NULL)
 * @return 0 on success
 */
int phoenix_agent_chat_with_trace(phoenix_agent_ctx_t *ctx, const char *user_input, phoenix_agent_trace_t *trace_out);

/**
 * @brief Clear Agent conversation history and reset context memory
 * @param ctx Agent context
 * @return 0 on success
 */
int phoenix_agent_clear_memory(phoenix_agent_ctx_t *ctx);

/**
 * @brief Dispatch user prompt into ReAct Agent loop asynchronously in background pthread
 * @param ctx Agent context
 * @param user_input User question or intent string
 * @return 0 on success
 */
int phoenix_agent_chat_async(phoenix_agent_ctx_t *ctx, const char *user_input);

/**
 * @brief Periodic timer tick (for Pomodoro countdown, stats update)
 * @param ctx Agent context
 */
void phoenix_agent_tick_1s(phoenix_agent_ctx_t *ctx);

/**
 * @brief Get copy of agent statistics
 * @param ctx Agent context
 * @param out_stats Destination stats structure
 */
void phoenix_agent_get_stats(const phoenix_agent_ctx_t *ctx, phoenix_agent_stats_t *out_stats);

/**
 * @brief Cleanup and release Agent Core
 * @param ctx Agent context
 */
void phoenix_agent_core_destroy(phoenix_agent_ctx_t *ctx);

/**
 * @brief 获取当前全局 Agent 核心实例
 * @return 实例指针，未初始化则为 NULL
 */
phoenix_agent_ctx_t* phoenix_agent_get_instance(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_AGENT_CORE_H */
