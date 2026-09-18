/**
 * @file agent_core.c
 * @brief Master Agent Orchestrator & State Engine Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "agent_core.h"
#include "event_bus.h"
#include "tool_registry.h"
#include "store.h"
#include "web_portal.h"
#include "intent_router.h"
#include "expression.h"
#if defined(__has_include) && __has_include("../harness/llm_provider.h")
#  include "../harness/llm_provider.h"
#else
#  include "harness/llm_provider.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define AGENT_MAX_TURNS 5
#define AGENT_MAX_HISTORY_BYTES (32 * 1024)

static phoenix_agent_ctx_t *s_agent_core_instance = NULL;

static void history_free_entry(phoenix_agent_ctx_t *ctx, phoenix_chat_msg_t *m)
{
    if (!m) return;
    size_t freed_bytes = 0;
    if (m->content) {
        freed_bytes += strlen(m->content);
        free(m->content);
    }
    if (m->tool_call_id) {
        freed_bytes += strlen(m->tool_call_id);
        free(m->tool_call_id);
    }
    if (m->tool_name) {
        freed_bytes += strlen(m->tool_name);
        free(m->tool_name);
    }
    if (m->reasoning_content) {
        freed_bytes += strlen(m->reasoning_content);
        free(m->reasoning_content);
    }
    if (ctx && ctx->total_history_bytes >= freed_bytes) {
        ctx->total_history_bytes -= freed_bytes;
    }
    memset(m, 0, sizeof(phoenix_chat_msg_t));
}

/**
 * @brief 两级记忆压缩：将即将丢弃的历史消息提炼为摘要卡片回填至 context_summary
 */
static void history_compact(phoenix_agent_ctx_t *ctx)
{
    size_t target_keep = PHOENIX_MAX_MESSAGES / 2;
    size_t drop = ctx->history_count > target_keep ? (ctx->history_count - target_keep) : 1;

    /* Ensure tool calls and responses remain paired */
    while (drop > 0 && ctx->history[drop].role == PHOENIX_ROLE_TOOL) {
        drop--;
    }

    if (drop == 0) return;

    /* 提取丢弃会话中的核心语义构建增量记忆摘要 */
    char summary_buf[512] = {0};
    size_t sum_len = 0;
    for (size_t i = 0; i < drop && sum_len < sizeof(summary_buf) - 64; i++) {
        if (ctx->history[i].role == PHOENIX_ROLE_USER && ctx->history[i].content) {
            sum_len += snprintf(summary_buf + sum_len, sizeof(summary_buf) - sum_len,
                                "问:%.32s; ", ctx->history[i].content);
        } else if (ctx->history[i].role == PHOENIX_ROLE_ASSISTANT && ctx->history[i].content) {
            sum_len += snprintf(summary_buf + sum_len, sizeof(summary_buf) - sum_len,
                                "答:%.32s; ", ctx->history[i].content);
        }
    }

    if (sum_len > 0) {
        if (!ctx->context_summary) {
            ctx->context_summary = strdup(summary_buf);
        } else {
            /* 追加更新前情记忆 (保持在 512 字节以内) */
            char merged[512];
            snprintf(merged, sizeof(merged), "%.240s | %.240s", ctx->context_summary, summary_buf);
            free(ctx->context_summary);
            ctx->context_summary = strdup(merged);
        }
        printf("[PhoenixCore] 🧠 两级记忆滚动更新: %s\n", ctx->context_summary);
    }

    size_t keep = ctx->history_count - drop;
    for (size_t i = 0; i < drop; i++) {
        history_free_entry(ctx, &ctx->history[i]);
    }

    memmove(ctx->history, ctx->history + drop, keep * sizeof(phoenix_chat_msg_t));
    ctx->history_count = keep;
    printf("[PhoenixCore] Compacted history: dropped %zu, kept %zu messages (paired tools preserved, bytes: %zu)\n",
           drop, keep, ctx->total_history_bytes);
}

static void history_add(phoenix_agent_ctx_t *ctx,
                        phoenix_msg_role_t role,
                        const char *content,
                        const char *tool_call_id,
                        const char *tool_name,
                        const char *reasoning_content)
{
    if (ctx->history_count >= PHOENIX_MAX_MESSAGES || ctx->total_history_bytes >= AGENT_MAX_HISTORY_BYTES) {
        history_compact(ctx);
    }

    phoenix_chat_msg_t *m = &ctx->history[ctx->history_count++];
    m->role = role;
    m->content = content ? strdup(content) : NULL;
    m->tool_call_id = tool_call_id ? strdup(tool_call_id) : NULL;
    m->tool_name = tool_name ? strdup(tool_name) : NULL;
    m->reasoning_content = reasoning_content ? strdup(reasoning_content) : NULL;

    if (m->content) ctx->total_history_bytes += strlen(m->content);
    if (m->tool_call_id) ctx->total_history_bytes += strlen(m->tool_call_id);
    if (m->tool_name) ctx->total_history_bytes += strlen(m->tool_name);
    if (m->reasoning_content) ctx->total_history_bytes += strlen(m->reasoning_content);
}

static void on_event_bus_msg(const phoenix_event_data_t *event, void *user_data)
{
    phoenix_agent_ctx_t *ctx = (phoenix_agent_ctx_t *)user_data;
    if (!ctx || !event) return;

    if (event->type == PHOENIX_EVT_MERIT_UPDATED) {
        pthread_mutex_lock(&ctx->core_lock);
        uint32_t old_merit = ctx->stats.merit_count;
        ctx->stats.merit_count = event->data.stats.total_merit;
        bool trigger = (ctx->stats.merit_count > 0 && ctx->stats.merit_count % 10 == 0 && old_merit != ctx->stats.merit_count);
        pthread_mutex_unlock(&ctx->core_lock);

        if (trigger) {
            phoenix_agent_trigger_proactive(ctx, PROACTIVE_CONTEXT_MILESTONE, "功德突破新高度！灵眸为你闪耀祝贺！");
        }
    } else if (event->type == PHOENIX_EVT_POMODORO_TICK) {
        pthread_mutex_lock(&ctx->core_lock);
        ctx->stats.pomodoro_active = event->data.stats.is_active;
        ctx->stats.pomodoro_remaining_s = event->data.stats.remaining_s;
        pthread_mutex_unlock(&ctx->core_lock);
    }
}

phoenix_agent_ctx_t* phoenix_agent_core_init(void)
{
    phoenix_agent_ctx_t *ctx = (phoenix_agent_ctx_t *)malloc(sizeof(phoenix_agent_ctx_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(phoenix_agent_ctx_t));

    pthread_mutex_init(&ctx->core_lock, NULL);
    ctx->state = AGENT_STATE_IDLE;
    ctx->stats.merit_count = 0;
    ctx->stats.interaction_count = 0;
    ctx->stats.uptime_seconds = 0;
    ctx->stats.pomodoro_remaining_s = 0;
    ctx->stats.pomodoro_active = false;
    ctx->history_count = 0;
    ctx->context_summary = NULL;
    ctx->total_history_bytes = 0;

    /* Subscribe to Event Bus */
    phoenix_event_subscribe(PHOENIX_EVT_MERIT_UPDATED, on_event_bus_msg, ctx);
    phoenix_event_subscribe(PHOENIX_EVT_POMODORO_TICK, on_event_bus_msg, ctx);

    /* Bind agent context to Web Portal */
    phoenix_web_portal_bind_agent(ctx);

    s_agent_core_instance = ctx;

    printf("[PhoenixCore] Master Agent Orchestrator initialized with Two-Tier Memory & Core Mutex.\n");
    return ctx;
}

void phoenix_agent_set_state(phoenix_agent_ctx_t *ctx, phoenix_core_state_t new_state, const char *message)
{
    if (!ctx) return;
    pthread_mutex_lock(&ctx->core_lock);
    int old_state = ctx->state;
    ctx->state = new_state;
    pthread_mutex_unlock(&ctx->core_lock);

    /* Broadcast State Change Event via Event Bus (UI & Eye observe this event) */
    phoenix_event_data_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_STATE_CHANGED;
    evt.data.state.old_state = old_state;
    evt.data.state.new_state = new_state;
    evt.data.state.message = message;
    phoenix_event_publish(&evt);
}

int phoenix_agent_chat(phoenix_agent_ctx_t *ctx, const char *user_input)
{
    if (!ctx || !user_input || strlen(user_input) == 0) {
        return -1;
    }
    ctx->stats.interaction_count++;

    printf("[PhoenixCore] 💬 User Input: \"%s\"\n", user_input);

    /* 0. Fast-path Intent Routing */
    phoenix_intent_result_t intent = phoenix_intent_route(user_input);
    if (intent.category == INTENT_TYPE_FASTPATH && intent.tool_name) {
        printf("[PhoenixCore] ⚡ Fast-path Intent Hit: Tool [%s], Args [%s]\n",
               intent.tool_name, intent.tool_args_json ? intent.tool_args_json : "{}");
        history_add(ctx, PHOENIX_ROLE_USER, user_input, NULL, NULL, NULL);

        char tool_res[512] = {0};
        phoenix_tool_execute(intent.tool_name, intent.tool_args_json, tool_res, sizeof(tool_res));

        ctx->stats.last_latency_ms = 0;
        const char *reply = intent.fast_reply ? intent.fast_reply : tool_res;
        history_add(ctx, PHOENIX_ROLE_ASSISTANT, reply, NULL, NULL, "【极速槽位】0ms本地直达执行");

        phoenix_event_data_t text_evt;
        memset(&text_evt, 0, sizeof(text_evt));
        text_evt.type = PHOENIX_EVT_LLM_FINISHED;
        text_evt.data.llm_text.text = reply;
        text_evt.data.llm_text.is_final = true;
        phoenix_event_publish(&text_evt);

        phoenix_agent_set_state(ctx, AGENT_STATE_IDLE, "极速直达指令执行完毕。");
        return 0;
    }

    if (intent.category == INTENT_TYPE_WORKFLOW) {
        printf("[PhoenixCore] 🚀 Macro Workflow Hit: [%s]\n", intent.workflow_name ? intent.workflow_name : "");
        history_add(ctx, PHOENIX_ROLE_USER, user_input, NULL, NULL, NULL);

        char tool_res[512] = {0};
        phoenix_tool_execute("manage_pomodoro", "{\"action\":\"start\",\"minutes\":25}", tool_res, sizeof(tool_res));
        phoenix_expression_play(PHOENIX_EXPR_THINKING, 8, "专注工作流已启动");

        ctx->stats.last_latency_ms = 0;
        const char *reply = intent.fast_reply;
        history_add(ctx, PHOENIX_ROLE_ASSISTANT, reply, NULL, NULL, "【宏工作流】复合任务协同执行完成");

        phoenix_event_data_t text_evt;
        memset(&text_evt, 0, sizeof(text_evt));
        text_evt.type = PHOENIX_EVT_LLM_FINISHED;
        text_evt.data.llm_text.text = reply;
        text_evt.data.llm_text.is_final = true;
        phoenix_event_publish(&text_evt);

        phoenix_agent_set_state(ctx, AGENT_STATE_POMODORO, "极客伴工流进行中...");
        return 0;
    }

    /* 1. Add User Input to History */
    history_add(ctx, PHOENIX_ROLE_USER, user_input, NULL, NULL, NULL);

    /* 2. Set State to Thinking */
    phoenix_agent_set_state(ctx, AGENT_STATE_THINKING, "DeepSeek / MiMo 正在深度思考与规划工具链...");

    /* 3. ReAct Execution Loop */
    char *tools_schema = phoenix_tool_build_schema_json();

    for (int turn = 0; turn < AGENT_MAX_TURNS; turn++) {
        phoenix_chat_resp_t resp;
        memset(&resp, 0, sizeof(resp));

        /* 构造注入两级前情记忆卡片的消息序列 */
        phoenix_chat_msg_t send_msgs[PHOENIX_MAX_MESSAGES + 1];
        size_t send_count = 0;
        char summary_item_buf[600];

        if (ctx->context_summary && ctx->context_summary[0]) {
            snprintf(summary_item_buf, sizeof(summary_item_buf), "【前情长程记忆摘要】%s", ctx->context_summary);
            send_msgs[0].role = PHOENIX_ROLE_SYSTEM;
            send_msgs[0].content = summary_item_buf;
            send_msgs[0].tool_call_id = NULL;
            send_msgs[0].tool_name = NULL;
            send_msgs[0].reasoning_content = NULL;
            send_count = 1;
        }

        for (size_t i = 0; i < ctx->history_count && send_count < (PHOENIX_MAX_MESSAGES + 1); i++) {
            send_msgs[send_count++] = ctx->history[i];
        }

        int ret = phoenix_llm_provider_chat(send_msgs, send_count, tools_schema, &resp);
        if (ret < 0) {
            phoenix_agent_set_state(ctx, AGENT_STATE_ALERT, "端云协同通信异常，请检查网络设置！");
            history_add(ctx, PHOENIX_ROLE_ASSISTANT, "抱歉，端云协同通信遇到问题。", NULL, NULL, NULL);
            phoenix_llm_resp_free(&resp);
            break;
        }

        /* Broadcast thinking process if reasoning_content is available */
        if (resp.reasoning_content && strlen(resp.reasoning_content) > 0) {
            printf("[PhoenixCore] 🧠 Thinking: %s\n", resp.reasoning_content);

            phoenix_event_data_t think_evt;
            memset(&think_evt, 0, sizeof(think_evt));
            think_evt.type = PHOENIX_EVT_LLM_THINKING;
            think_evt.data.thinking.reasoning_snippet = resp.reasoning_content;
            phoenix_event_publish(&think_evt);
        }

        ctx->stats.total_tokens_used += resp.total_tokens;
        ctx->stats.last_latency_ms = resp.latency_ms;

        if (resp.is_tool_use && resp.tool_name) {
            /* Assistant requested Tool Call */
            printf("[PhoenixCore] 🛠️ Tool Invocation: %s, Args: %s\n",
                   resp.tool_name, resp.tool_input ? resp.tool_input : "{}");

            phoenix_agent_set_state(ctx, AGENT_STATE_EXECUTING, "正在调度具身工具执行...");

            history_add(ctx, PHOENIX_ROLE_ASSISTANT, resp.content, resp.tool_call_id, resp.tool_name, resp.reasoning_content);

            /* Execute Tool */
            char tool_result[512] = {0};
            phoenix_tool_execute(resp.tool_name, resp.tool_input, tool_result, sizeof(tool_result));

            /* Add Tool result back to history */
            history_add(ctx, PHOENIX_ROLE_TOOL, tool_result, resp.tool_call_id, NULL, NULL);

            phoenix_llm_resp_free(&resp);
            continue; /* Loop back to LLM for final synthesis */
        }

        /* Final Assistant Answer */
        if (resp.content) {
            history_add(ctx, PHOENIX_ROLE_ASSISTANT, resp.content, NULL, NULL, resp.reasoning_content);
            const char *saved_content = (ctx->history_count > 0 && ctx->history[ctx->history_count - 1].content) ?
                                         ctx->history[ctx->history_count - 1].content : resp.content;

            /* Broadcast final text (使用持久化副本，避免异步消费时被 phoenix_llm_resp_free 提前释放) */
            phoenix_event_data_t text_evt;
            memset(&text_evt, 0, sizeof(text_evt));
            text_evt.type = PHOENIX_EVT_LLM_FINISHED;
            text_evt.data.llm_text.text = saved_content;
            text_evt.data.llm_text.is_final = true;
            phoenix_event_publish(&text_evt);

            phoenix_agent_set_state(ctx, AGENT_STATE_IDLE, saved_content);
        }

        phoenix_llm_resp_free(&resp);
        break;
    }

    if (tools_schema) {
        free(tools_schema);
    }
    return 0;
}

typedef struct {
    phoenix_agent_ctx_t *ctx;
    char user_input[256];
} agent_async_req_t;

static void *agent_async_chat_worker(void *arg)
{
    agent_async_req_t *req = (agent_async_req_t *)arg;
    if (req) {
        phoenix_agent_chat(req->ctx, req->user_input);
        free(req);
    }
    return NULL;
}

int phoenix_agent_chat_async(phoenix_agent_ctx_t *ctx, const char *user_input)
{
    if (!ctx || !user_input || !user_input[0]) return -1;

    agent_async_req_t *req = (agent_async_req_t *)malloc(sizeof(agent_async_req_t));
    if (!req) {
        return phoenix_agent_chat(ctx, user_input);
    }
    req->ctx = ctx;
    strncpy(req->user_input, user_input, sizeof(req->user_input) - 1);
    req->user_input[sizeof(req->user_input) - 1] = '\0';

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int rc = pthread_create(&tid, &attr, agent_async_chat_worker, req);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        int ret = phoenix_agent_chat(ctx, user_input);
        free(req);
        return ret;
    }
    return 0;
}

void phoenix_agent_set_proactive_threshold(phoenix_agent_ctx_t *ctx, uint32_t focus_timeout_s)
{
    if (!ctx) return;
    ctx->proactive_threshold_s = focus_timeout_s > 0 ? focus_timeout_s : 2700;
}

int phoenix_agent_trigger_proactive(phoenix_agent_ctx_t *ctx, phoenix_proactive_type_t type, const char *custom_reason)
{
    if (!ctx) return -1;

    phoenix_event_data_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_PROACTIVE_INTERVENE;
    evt.data.proactive.proactive_type = (int)type;

    switch (type) {
        case PROACTIVE_THRESHOLD_FATIGUE: {
            phoenix_agent_set_state(ctx, AGENT_STATE_ALERT, "【主动干预】检测到连续编码专注超时，灵眸提醒您休息！");
            evt.data.proactive.title = "沉浸超时防猝死干预";
            evt.data.proactive.suggestion = "主人，你已连续专注编码较长时间，为你敲响赛博木鱼积攒功德，请闭目休息5分钟！";
            evt.data.proactive.auto_action = "launch_system_app:白噪音";

            /* 自动执行木鱼功德积累与飞字动效 */
            char tool_res[128];
            phoenix_tool_execute("knock_wooden_fish", "{\"count\":1}", tool_res, sizeof(tool_res));

            phoenix_event_data_t fly_evt;
            memset(&fly_evt, 0, sizeof(fly_evt));
            fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
            fly_evt.data.flying_text.text = "主动关怀: 劳逸结合 功德+1";
            fly_evt.data.flying_text.color_rgb = 0xffa500;
            phoenix_event_publish(&fly_evt);
            break;
        }

        case PROACTIVE_PERIODIC_BRIEFING: {
            phoenix_agent_set_state(ctx, AGENT_STATE_THINKING, "【定时主动】晨间开工 Briefing 生成中...");
            evt.data.proactive.title = "晨间智能工作简报";
            evt.data.proactive.suggestion = custom_reason ? custom_reason : "早上好，灵眸已为你巡检系统状态，准备好开启充满创造力的一天！";
            evt.data.proactive.auto_action = "launch_system_app:日历";

            char tool_res[128];
            phoenix_tool_execute("launch_system_app", "{\"app_name\":\"日历\"}", tool_res, sizeof(tool_res));
            break;
        }

        case PROACTIVE_CONTEXT_MILESTONE: {
            phoenix_agent_set_state(ctx, AGENT_STATE_CELEBRATING, "【上下文主动】达成功德里程碑！");
            evt.data.proactive.title = "功德里程碑达成";
            evt.data.proactive.suggestion = custom_reason ? custom_reason : "太棒了！今日代码功德已突破里程碑，愿全量构建无Bug！";
            evt.data.proactive.auto_action = "set_eye_emotion:happy";

            phoenix_event_data_t fly_evt;
            memset(&fly_evt, 0, sizeof(fly_evt));
            fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
            fly_evt.data.flying_text.text = "功德突破里程碑！";
            fly_evt.data.flying_text.color_rgb = 0xffd700;
            phoenix_event_publish(&fly_evt);
            break;
        }

        case PROACTIVE_SYSTEM_HEALTH:
        default: {
            phoenix_agent_set_state(ctx, AGENT_STATE_EXECUTING, "【系统主动】自愈与健康遥测巡检中...");
            evt.data.proactive.title = "系统健康守护巡检";
            evt.data.proactive.suggestion = custom_reason ? custom_reason : "正在自动巡检嵌入式软硬件遥测指标与运行水位...";
            evt.data.proactive.auto_action = "query_system_health";

            char tool_res[256];
            phoenix_tool_execute("query_system_health", "{}", tool_res, sizeof(tool_res));
            break;
        }
    }

    phoenix_event_publish(&evt);
    printf("[PhoenixCore] 🚨 Proactive intervention triggered: [%s] -> %s\n",
           evt.data.proactive.title, evt.data.proactive.suggestion);
    return 0;
}

void phoenix_agent_tick_1s(phoenix_agent_ctx_t *ctx)
{
    if (!ctx) return;
    ctx->stats.uptime_seconds++;

    /* 累计连续专注时长与主动超时干预检测 */
    if (ctx->stats.proactive_enabled) {
        ctx->stats.continuous_focus_s++;
        if (ctx->proactive_threshold_s > 0 &&
            ctx->stats.continuous_focus_s >= ctx->proactive_threshold_s) {
            ctx->stats.continuous_focus_s = 0; /* Reset counter */
            phoenix_agent_trigger_proactive(ctx, PROACTIVE_THRESHOLD_FATIGUE, "连续工作超时主动防猝死干预");
        }
    }

    /* Check Pomodoro */
    if (ctx->stats.pomodoro_active && ctx->stats.pomodoro_remaining_s > 0) {
        ctx->stats.pomodoro_remaining_s--;

        /* Broadcast tick */
        phoenix_event_data_t tick_evt;
        memset(&tick_evt, 0, sizeof(tick_evt));
        tick_evt.type = PHOENIX_EVT_POMODORO_TICK;
        tick_evt.data.stats.is_active = true;
        tick_evt.data.stats.remaining_s = ctx->stats.pomodoro_remaining_s;
        phoenix_event_publish(&tick_evt);

        if (ctx->stats.pomodoro_remaining_s == 0) {
            ctx->stats.pomodoro_active = false;
            phoenix_store_add_pomodoro(25 * 60);
            phoenix_agent_set_state(ctx, AGENT_STATE_CELEBRATING,
                                    "25分钟专注达成！灵眸提醒主人休息片刻");

            phoenix_event_data_t fly_evt;
            memset(&fly_evt, 0, sizeof(fly_evt));
            fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
            fly_evt.data.flying_text.text = "专注达成！休息片刻";
            fly_evt.data.flying_text.color_rgb = 0xffd700;
            phoenix_event_publish(&fly_evt);
        }
    }
}

void phoenix_agent_get_stats(const phoenix_agent_ctx_t *ctx, phoenix_agent_stats_t *out_stats)
{
    if (ctx && out_stats) {
        *out_stats = ctx->stats;
    }
}

void phoenix_agent_core_destroy(phoenix_agent_ctx_t *ctx)
{
    if (!ctx) return;
    if (s_agent_core_instance == ctx) {
        s_agent_core_instance = NULL;
    }
    phoenix_web_portal_bind_agent(NULL);
    for (size_t i = 0; i < ctx->history_count; i++) {
        history_free_entry(ctx, &ctx->history[i]);
    }
    if (ctx->context_summary) {
        free(ctx->context_summary);
        ctx->context_summary = NULL;
    }
    pthread_mutex_destroy(&ctx->core_lock);
    free(ctx);
}

phoenix_agent_ctx_t* phoenix_agent_get_instance(void)
{
    return s_agent_core_instance;
}
