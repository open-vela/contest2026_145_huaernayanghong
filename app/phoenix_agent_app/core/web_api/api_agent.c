/**
 * @file api_agent.c
 * @brief Phoenix HoloDesk-S1 Web Companion Agent REST API Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "api_agent.h"
#include "web_api.h"
#include "../agent_core.h"
#include "../tool_registry.h"
#if defined(__has_include) && __has_include("../../utils/log_utils.h")
#  include "../../utils/log_utils.h"
#else
#  include "utils/log_utils.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "PhoenixWebAPI"

static phoenix_agent_ctx_t* get_effective_agent(void)
{
    phoenix_agent_ctx_t *agent = web_api_get_bound_agent();
    if (!agent) {
        agent = phoenix_agent_get_instance();
    }
    return agent;
}

int handle_agent_chat(const http_req_t *req, http_resp_t *resp)
{
    if (!req || !resp) return -1;

    phoenix_agent_ctx_t *agent = get_effective_agent();
    if (!agent) {
        http_resp_error(resp, 503, "Agent Core not initialized");
        return 0;
    }

    const char *prompt = NULL;
    if (req->json) {
        cJSON *prompt_item = cJSON_GetObjectItem(req->json, "prompt");
        if (prompt_item && prompt_item->valuestring) {
            prompt = prompt_item->valuestring;
        }
    }

    if (!prompt || prompt[0] == '\0') {
        http_resp_error(resp, 400, "Missing 'prompt' in request body JSON");
        return 0;
    }

    /* 护城河 2: 前置忙闲互斥闸门，0ms 快速返回 HTTP 429，防止打爆 TCP 连接池与 Socket 堆积 */
    if (phoenix_agent_is_busy(agent)) {
        LOG_W(TAG, "⚠️ 灵眸正处于思考/执行忙碌状态，快速返回 HTTP 429 拒绝重入");
        cJSON *root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "灵眸正在深度思考中，请稍候...");
        cJSON_AddStringToObject(root, "answer", "⚠️ 灵眸正在深度思考上一条指令，请稍候再试...");
        cJSON_AddStringToObject(root, "state", "THINKING");
        http_resp_json_obj(resp, 429, root);
        return 0;
    }

    LOG_I(TAG, "💬 收到 Web 伴侣对话请求: \"%s\"", prompt);

    /* 关键栈保护：trace 结构体占用 ~5.5KB，改由堆动态分配彻底杜绝工作线程栈溢出与 TCB 损坏 */
    phoenix_agent_trace_t *trace = (phoenix_agent_trace_t *)calloc(1, sizeof(phoenix_agent_trace_t));
    if (!trace) {
        LOG_E(TAG, "❌ 内存不足，无法分配 Agent Trace 追踪缓冲");
        http_resp_error(resp, 500, "Out of memory for agent trace");
        return 0;
    }

    int ret = phoenix_agent_chat_with_trace(agent, prompt, trace);
    if (ret == -2) {
        LOG_W(TAG, "⚠️ Agent 核心返回 -EBUSY，返回 HTTP 429");
        cJSON *root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "灵眸正在深度思考中，请稍候...");
        cJSON_AddStringToObject(root, "answer", trace->final_answer[0] ? trace->final_answer : "⚠️ 灵眸正在深度思考上一条指令，请稍候再试...");
        cJSON_AddStringToObject(root, "state", "THINKING");
        free(trace);
        http_resp_json_obj(resp, 429, root);
        return 0;
    }

    const char *state_str = "IDLE";
    switch (trace->end_state) {
        case AGENT_STATE_IDLE:        state_str = "IDLE"; break;
        case AGENT_STATE_LISTENING:   state_str = "LISTENING"; break;
        case AGENT_STATE_THINKING:    state_str = "THINKING"; break;
        case AGENT_STATE_EXECUTING:   state_str = "EXECUTING"; break;
        case AGENT_STATE_CELEBRATING: state_str = "CELEBRATING"; break;
        case AGENT_STATE_POMODORO:    state_str = "POMODORO"; break;
        case AGENT_STATE_ALERT:       state_str = "ALERT"; break;
        default:                      state_str = "UNKNOWN"; break;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", ret == 0 && trace->success);
    cJSON_AddStringToObject(root, "prompt", trace->user_prompt);
    cJSON_AddStringToObject(root, "state", state_str);
    cJSON_AddStringToObject(root, "reasoning_content", trace->reasoning_content);

    if (trace->has_tool_call && trace->tool_name[0] != '\0') {
        cJSON *action = cJSON_CreateObject();
        cJSON_AddStringToObject(action, "tool_name", trace->tool_name);
        cJSON_AddStringToObject(action, "arguments", trace->tool_args);
        cJSON_AddStringToObject(action, "observation", trace->tool_observation);
        cJSON_AddItemToObject(root, "action", action);
    } else {
        cJSON_AddNullToObject(root, "action");
    }

    cJSON_AddStringToObject(root, "answer", trace->final_answer);

    cJSON *metrics = cJSON_CreateObject();
    cJSON_AddNumberToObject(metrics, "latency_ms", trace->latency_ms);
    cJSON_AddNumberToObject(metrics, "prompt_tokens", trace->prompt_tokens);
    cJSON_AddNumberToObject(metrics, "completion_tokens", trace->completion_tokens);
    cJSON_AddNumberToObject(metrics, "total_tokens", trace->total_tokens);
    cJSON_AddItemToObject(root, "metrics", metrics);

    LOG_I(TAG, "🤖 Web 伴侣对话完成: 状态=%s, 耗时=%ums, Tokens=%u, Success=%d",
          state_str, trace->latency_ms, trace->total_tokens, trace->success);

    free(trace);
    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_agent_tools(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    if (!resp) return -1;

    size_t count = phoenix_tool_get_count();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddNumberToObject(root, "count", (double)count);

    cJSON *tools_arr = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        const phoenix_tool_desc_t *tool = phoenix_tool_get_at(i);
        if (!tool) continue;

        cJSON *tobj = cJSON_CreateObject();
        cJSON_AddStringToObject(tobj, "name", tool->name ? tool->name : "");
        cJSON_AddStringToObject(tobj, "description", tool->description ? tool->description : "");

        if (tool->parameters_schema && tool->parameters_schema[0] != '\0') {
            cJSON *params = cJSON_Parse(tool->parameters_schema);
            if (params) {
                cJSON_AddItemToObject(tobj, "parameters", params);
            } else {
                cJSON_AddStringToObject(tobj, "parameters_raw", tool->parameters_schema);
            }
        } else {
            cJSON_AddItemToObject(tobj, "parameters", cJSON_CreateObject());
        }
        cJSON_AddItemToArray(tools_arr, tobj);
    }
    cJSON_AddItemToObject(root, "tools", tools_arr);

    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_agent_tool_exec(const http_req_t *req, http_resp_t *resp)
{
    if (!req || !resp) return -1;

    if (!req->json) {
        http_resp_error(resp, 400, "Expected JSON body");
        return 0;
    }

    cJSON *name_item = cJSON_GetObjectItem(req->json, "name");
    if (!name_item || !name_item->valuestring || name_item->valuestring[0] == '\0') {
        http_resp_error(resp, 400, "Missing 'name' field in request JSON");
        return 0;
    }

    char args_buf[512] = "{}";
    cJSON *args_item = cJSON_GetObjectItem(req->json, "arguments");
    if (args_item) {
        if (cJSON_IsString(args_item) && args_item->valuestring) {
            strncpy(args_buf, args_item->valuestring, sizeof(args_buf) - 1);
            args_buf[sizeof(args_buf) - 1] = '\0';
        } else if (cJSON_IsObject(args_item)) {
            char *s = cJSON_PrintUnformatted(args_item);
            if (s) {
                strncpy(args_buf, s, sizeof(args_buf) - 1);
                args_buf[sizeof(args_buf) - 1] = '\0';
                free(s);
            }
        }
    }

    char result_buf[1024] = {0};
    int ret = phoenix_tool_execute(name_item->valuestring, args_buf, result_buf, sizeof(result_buf));

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", ret == 0);
    cJSON_AddStringToObject(root, "name", name_item->valuestring);
    cJSON_AddStringToObject(root, "arguments", args_buf);

    cJSON *parsed_res = cJSON_Parse(result_buf);
    if (parsed_res) {
        cJSON_AddItemToObject(root, "result", parsed_res);
    } else {
        cJSON_AddStringToObject(root, "result", result_buf);
    }

    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_agent_memory(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    if (!resp) return -1;

    phoenix_agent_ctx_t *agent = get_effective_agent();
    if (!agent) {
        http_resp_error(resp, 503, "Agent Core not initialized");
        return 0;
    }

    pthread_mutex_lock(&agent->core_lock);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddNumberToObject(root, "history_count", (double)agent->history_count);
    cJSON_AddNumberToObject(root, "total_bytes", (double)agent->total_history_bytes);
    cJSON_AddStringToObject(root, "context_summary", agent->context_summary ? agent->context_summary : "");

    cJSON *msgs_arr = cJSON_CreateArray();
    for (size_t i = 0; i < agent->history_count; i++) {
        const phoenix_chat_msg_t *m = &agent->history[i];
        cJSON *mobj = cJSON_CreateObject();
        const char *role_str = "user";
        switch (m->role) {
            case PHOENIX_ROLE_SYSTEM:    role_str = "system"; break;
            case PHOENIX_ROLE_USER:      role_str = "user"; break;
            case PHOENIX_ROLE_ASSISTANT: role_str = "assistant"; break;
            case PHOENIX_ROLE_TOOL:      role_str = "tool"; break;
            default:                     role_str = "unknown"; break;
        }
        cJSON_AddStringToObject(mobj, "role", role_str);
        cJSON_AddStringToObject(mobj, "content", m->content ? m->content : "");
        if (m->tool_name) {
            cJSON_AddStringToObject(mobj, "tool_name", m->tool_name);
        }
        if (m->tool_call_id) {
            cJSON_AddStringToObject(mobj, "tool_call_id", m->tool_call_id);
        }
        if (m->reasoning_content) {
            cJSON_AddStringToObject(mobj, "reasoning_content", m->reasoning_content);
        }
        cJSON_AddItemToArray(msgs_arr, mobj);
    }
    cJSON_AddItemToObject(root, "messages", msgs_arr);
    pthread_mutex_unlock(&agent->core_lock);

    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_agent_memory_clear(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    if (!resp) return -1;

    phoenix_agent_ctx_t *agent = get_effective_agent();
    if (!agent) {
        http_resp_error(resp, 503, "Agent Core not initialized");
        return 0;
    }

    phoenix_agent_clear_memory(agent);
    http_resp_json(resp, 200, "{\"success\":true,\"message\":\"Agent context and conversation memory cleared\"}");
    return 0;
}
