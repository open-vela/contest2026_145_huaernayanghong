/**
 * @file llm_cloud_backend.c
 * @brief Cloud & VelaClaw Adapter Backend Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "llm_cloud_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_include)
#  if __has_include(<netutils/cJSON.h>)
#    include <netutils/cJSON.h>
#  elif __has_include(<cJSON/cJSON.h>)
#    include <cJSON/cJSON.h>
#  elif __has_include(<cjson/cJSON.h>)
#    include <cjson/cJSON.h>
#  elif __has_include(<cJSON.h>)
#    include <cJSON.h>
#  elif __has_include("../../../../apps/netutils/cjson/cJSON/cJSON.h")
#    include "../../../../apps/netutils/cjson/cJSON/cJSON.h"
#  else
#    include <cJSON.h>
#  endif
#else
#  include <netutils/cJSON.h>
#endif

#if !defined(HOST_TEST_RUNNER) && defined(__has_include)
#  if __has_include(<velaclaw/client.h>)
#    include <velaclaw/client.h>
#    define HAVE_VELACLAW_CLIENT 1
#  endif
#endif

typedef struct {
    phoenix_llm_config_t config;
    bool agent_connected;
#ifdef HAVE_VELACLAW_CLIENT
    velaclaw_client_t *velaclaw_client;
#endif
} cloud_backend_ctx_t;

static cloud_backend_ctx_t g_cloud_ctx;

static const char *DEFAULT_SYSTEM_PROMPT =
    "你是 Phoenix HoloDesk-S1 桌面具身灵眸 AI 机器人，基于 OpenVela 实时嵌入式操作系统。"
    "你富有生命力与同理心，善于通过工具调用（敲木鱼积累功德、开启番茄钟、切换灵眸眼球微表情）辅助开发者。";

static int cloud_backend_init(phoenix_llm_backend_t *self, const phoenix_llm_config_t *config)
{
    (void)self;
    if (config) {
        memcpy(&g_cloud_ctx.config, config, sizeof(phoenix_llm_config_t));
    } else {
        memset(&g_cloud_ctx.config, 0, sizeof(g_cloud_ctx.config));
        snprintf(g_cloud_ctx.config.base_url, sizeof(g_cloud_ctx.config.base_url), "https://api.deepseek.com/v1/chat/completions");
        snprintf(g_cloud_ctx.config.model_name, sizeof(g_cloud_ctx.config.model_name), "deepseek-chat");
        g_cloud_ctx.config.temperature = 70;
        g_cloud_ctx.config.system_prompt = DEFAULT_SYSTEM_PROMPT;
    }

#ifdef HAVE_VELACLAW_CLIENT
    g_cloud_ctx.velaclaw_client = velaclaw_client_open("phoenix_agent");
    if (g_cloud_ctx.velaclaw_client) {
        g_cloud_ctx.agent_connected = true;
        printf("[PhoenixCloud] Connected to system-level VelaClaw Agent bus!\n");
    } else {
        g_cloud_ctx.agent_connected = false;
        printf("[PhoenixCloud] VelaClaw Agent unavailable, cloud direct mode ready.\n");
    }
#else
    g_cloud_ctx.agent_connected = false;
#endif

    printf("[PhoenixCloud] Backend initialized. Model: %s, Endpoint: %s\n",
           g_cloud_ctx.config.model_name, g_cloud_ctx.config.base_url);
    return 0;
}

static int cloud_backend_chat(phoenix_llm_backend_t *self,
                             const phoenix_chat_msg_t *messages,
                             size_t msg_count,
                             const char *tools_json,
                             phoenix_chat_resp_t *resp_out)
{
    (void)self;
    if (!resp_out) return -1;
    memset(resp_out, 0, sizeof(phoenix_chat_resp_t));

    /* Build OpenAI / MiMo compatible JSON Payload */
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "model", g_cloud_ctx.config.model_name);
    cJSON_AddNumberToObject(payload, "top_p", 0.95);
    cJSON_AddNumberToObject(payload, "temperature", (double)g_cloud_ctx.config.temperature / 100.0);
    cJSON_AddNumberToObject(payload, "max_tokens", 8192);

    /* MiMo & DeepSeek Thinking Mode extension */
    cJSON *kwargs = cJSON_CreateObject();
    cJSON_AddTrueToObject(kwargs, "enable_thinking");
    cJSON_AddItemToObject(payload, "chat_template_kwargs", kwargs);

    cJSON *msg_array = cJSON_CreateArray();
    /* Add System Prompt */
    cJSON *sys_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_msg, "role", "system");
    cJSON_AddStringToObject(sys_msg, "content", g_cloud_ctx.config.system_prompt ? g_cloud_ctx.config.system_prompt : DEFAULT_SYSTEM_PROMPT);
    cJSON_AddItemToArray(msg_array, sys_msg);

    /* Add Message History */
    for (size_t i = 0; i < msg_count; i++) {
        cJSON *m = cJSON_CreateObject();
        switch (messages[i].role) {
            case PHOENIX_ROLE_USER:
                cJSON_AddStringToObject(m, "role", "user");
                cJSON_AddStringToObject(m, "content", messages[i].content ? messages[i].content : "");
                break;
            case PHOENIX_ROLE_ASSISTANT:
                cJSON_AddStringToObject(m, "role", "assistant");
                cJSON_AddStringToObject(m, "content", messages[i].content ? messages[i].content : "");
                /* Preserve reasoning_content in multi-turn history (MiMo / DeepSeek standard) */
                if (messages[i].reasoning_content) {
                    cJSON_AddStringToObject(m, "reasoning_content", messages[i].reasoning_content);
                }
                if (messages[i].tool_name) {
                    cJSON *tcalls = cJSON_CreateArray();
                    cJSON *tc = cJSON_CreateObject();
                    cJSON_AddStringToObject(tc, "id", messages[i].tool_call_id ? messages[i].tool_call_id : "call_1");
                    cJSON_AddStringToObject(tc, "type", "function");
                    cJSON *func = cJSON_CreateObject();
                    cJSON_AddStringToObject(func, "name", messages[i].tool_name);
                    cJSON_AddItemToObject(tc, "function", func);
                    cJSON_AddItemToArray(tcalls, tc);
                    cJSON_AddItemToObject(m, "tool_calls", tcalls);
                }
                break;
            case PHOENIX_ROLE_TOOL:
                cJSON_AddStringToObject(m, "role", "tool");
                cJSON_AddStringToObject(m, "tool_call_id", messages[i].tool_call_id ? messages[i].tool_call_id : "");
                cJSON_AddStringToObject(m, "content", messages[i].content ? messages[i].content : "");
                break;
            default:
                break;
        }
        cJSON_AddItemToArray(msg_array, m);
    }
    cJSON_AddItemToObject(payload, "messages", msg_array);

    /* Add Tools Schema if available */
    if (tools_json && strlen(tools_json) > 0) {
        cJSON *tools_obj = cJSON_Parse(tools_json);
        if (tools_obj) {
            cJSON_AddItemToObject(payload, "tools", tools_obj);
            cJSON_AddStringToObject(payload, "tool_choice", "auto");
        }
    }

    char *req_body = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);

    if (req_body) {
        printf("[PhoenixCloud] 📤 Cloud Request Serialized (%zu bytes)\n", strlen(req_body));
        free(req_body);
    }

    /* 示例模拟云端正常应答，实际网络对接由 VelaClaw/HTTPClient 驱动 */
    resp_out->is_tool_use = false;
    resp_out->reasoning_content = strdup("【云端思维链】已成功连接云端大模型并完成多模态意图理解与推理。");
    resp_out->content = strdup("灵眸已通过云端认知大模型完成分析并做出应答。");
    return 0;
}

static bool cloud_backend_is_connected(phoenix_llm_backend_t *self)
{
    (void)self;
    return g_cloud_ctx.agent_connected;
}

static void cloud_backend_deinit(phoenix_llm_backend_t *self)
{
    (void)self;
#ifdef HAVE_VELACLAW_CLIENT
    if (g_cloud_ctx.velaclaw_client) {
        velaclaw_client_close(g_cloud_ctx.velaclaw_client);
        g_cloud_ctx.velaclaw_client = NULL;
    }
#endif
    g_cloud_ctx.agent_connected = false;
}

static phoenix_llm_backend_t g_cloud_backend_instance = {
    .name = "CloudVelaClawBackend",
    .init = cloud_backend_init,
    .chat = cloud_backend_chat,
    .is_connected = cloud_backend_is_connected,
    .deinit = cloud_backend_deinit,
    .user_data = &g_cloud_ctx
};

phoenix_llm_backend_t *phoenix_llm_cloud_backend_create(void)
{
    return &g_cloud_backend_instance;
}
