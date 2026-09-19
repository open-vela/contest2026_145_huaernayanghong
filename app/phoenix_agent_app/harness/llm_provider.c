/**
 * @file llm_provider.c
 * @brief Unified LLM Harness Facade Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "llm_provider.h"
#include "llm_mock_backend.h"
#include "llm_cloud_backend.h"
#if defined(__has_include) && __has_include("utils/log_utils.h")
#  include "utils/log_utils.h"
#else
#  include "../utils/log_utils.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "PhoenixHarness"

static phoenix_llm_config_t g_config;
static bool g_provider_initialized = false;
static phoenix_llm_backend_t *g_active_backend = NULL;

void phoenix_llm_resp_free(phoenix_chat_resp_t *resp)
{
    if (!resp) return;
    if (resp->content) free(resp->content);
    if (resp->tool_call_id) free(resp->tool_call_id);
    if (resp->tool_name) free(resp->tool_name);
    if (resp->tool_input) free(resp->tool_input);
    if (resp->reasoning_content) free(resp->reasoning_content);
    if (resp->error_msg) free(resp->error_msg);
    memset(resp, 0, sizeof(phoenix_chat_resp_t));
}

int phoenix_llm_provider_set_backend(phoenix_llm_backend_t *backend)
{
    if (!backend) return -1;
    if (g_active_backend && g_active_backend->deinit) {
        g_active_backend->deinit(g_active_backend);
    }
    g_active_backend = backend;
    if (g_active_backend->init) {
        g_active_backend->init(g_active_backend, &g_config);
    }
    g_provider_initialized = true;
    LOG_I(TAG, "🔀 Switched active LLM backend to: [%s]",
          g_active_backend->name ? g_active_backend->name : "CustomBackend");
    return 0;
}

phoenix_llm_backend_t *phoenix_llm_provider_get_backend(void)
{
    return g_active_backend;
}

int phoenix_llm_provider_init(const phoenix_llm_config_t *config)
{
    if (config) {
        memcpy(&g_config, config, sizeof(phoenix_llm_config_t));
    } else {
        memset(&g_config, 0, sizeof(g_config));
        snprintf(g_config.base_url, sizeof(g_config.base_url), "https://api.deepseek.com/v1/chat/completions");
        snprintf(g_config.model_name, sizeof(g_config.model_name), "deepseek-chat");
        g_config.temperature = 70;
    }

    /* 策略路由：若未配置 API Key 则默认采用离线具身仿真驱动；若有 Key 则采用云端/系统级适配器 */
    if (strlen(g_config.api_key) == 0) {
        g_active_backend = phoenix_llm_mock_backend_create();
    } else {
        g_active_backend = phoenix_llm_cloud_backend_create();
    }

    if (g_active_backend && g_active_backend->init) {
        g_active_backend->init(g_active_backend, &g_config);
    }

    g_provider_initialized = true;
    LOG_I(TAG, "✅ Harness Facade initialized. Active backend: [%s], Model: %s, URL: %s",
          g_active_backend ? g_active_backend->name : "None",
          g_config.model_name, g_config.base_url);
    return 0;
}

int phoenix_llm_provider_chat(const phoenix_chat_msg_t *messages,
                              size_t msg_count,
                              const char *tools_json,
                              phoenix_chat_resp_t *resp_out)
{
    if (!g_provider_initialized || !resp_out) {
        return -1;
    }

    if (!g_active_backend || !g_active_backend->chat) {
        /* 紧急安全回退到离线仿真驱动 */
        g_active_backend = phoenix_llm_mock_backend_create();
        if (g_active_backend->init) {
            g_active_backend->init(g_active_backend, &g_config);
        }
    }

    return g_active_backend->chat(g_active_backend, messages, msg_count, tools_json, resp_out);
}

void phoenix_llm_set_api_key(const char *api_key)
{
    if (!api_key) return;
    snprintf(g_config.api_key, sizeof(g_config.api_key), "%s", api_key);
    LOG_I(TAG, "API Key updated (%zu bytes)", strlen(api_key));

    /* 若从无 Key 切换为有 Key，自动平滑升级为 Cloud 驱动 */
    if (strlen(api_key) > 0 && g_active_backend &&
        strcmp(g_active_backend->name, "MockOfflineBackend") == 0) {
        phoenix_llm_provider_set_backend(phoenix_llm_cloud_backend_create());
    }
}

void phoenix_llm_set_base_url(const char *base_url)
{
    if (!base_url || !base_url[0]) return;
    snprintf(g_config.base_url, sizeof(g_config.base_url), "%s", base_url);
    if (g_active_backend && g_active_backend->init) {
        g_active_backend->init(g_active_backend, &g_config);
    }
    LOG_I(TAG, "Base URL updated -> %s", g_config.base_url);
}

void phoenix_llm_set_model(const char *model_name)
{
    if (!model_name || !model_name[0]) return;
    snprintf(g_config.model_name, sizeof(g_config.model_name), "%s", model_name);
    if (g_active_backend && g_active_backend->init) {
        g_active_backend->init(g_active_backend, &g_config);
    }
    LOG_I(TAG, "Model updated -> %s", g_config.model_name);
}

int phoenix_llm_ping(uint32_t *latency_ms, int *http_status, char *err_buf, size_t err_sz)
{
    if (!g_provider_initialized || !g_active_backend) {
        if (err_buf && err_sz > 0) snprintf(err_buf, err_sz, "LLM provider not initialized");
        return -1;
    }

    if (g_active_backend->ping) {
        return g_active_backend->ping(g_active_backend, latency_ms, http_status, err_buf, err_sz);
    }

    if (err_buf && err_sz > 0) snprintf(err_buf, err_sz, "Backend does not implement ping");
    return -2;
}

bool phoenix_llm_is_agent_connected(void)
{
    if (g_active_backend && g_active_backend->is_connected) {
        return g_active_backend->is_connected(g_active_backend);
    }
    return false;
}

void phoenix_llm_provider_deinit(void)
{
    if (g_active_backend && g_active_backend->deinit) {
        g_active_backend->deinit(g_active_backend);
    }
    g_active_backend = NULL;
    g_provider_initialized = false;
    LOG_I(TAG, "🔄 Harness Facade de-initialized.");
}
