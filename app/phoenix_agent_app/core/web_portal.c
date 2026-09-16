/**
 * @file web_portal.c
 * @brief Embedded Web Configuration & Skill Studio Server Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_portal.h"
#include "tool_registry.h"
#include "store.h"
#include "config.h"
#include "harness/llm_provider.h"
#include "../hal/network_mgr.h"
#include "../utils/log_mgr.h"

#if defined(__has_include) && __has_include("cartridge_mgr.h")
#  include "cartridge_mgr.h"
#  include "../cartridges/cartridge_memo.h"
#else
#  include "../core/cartridge_mgr.h"
#  include "../cartridges/cartridge_memo.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

#include "web_assets.h"

static bool g_server_running = false;
static int g_server_fd = -1;
static uint16_t g_server_port = PHOENIX_DEFAULT_WEB_PORT;
static pthread_t g_server_thread;
static phoenix_agent_ctx_t *g_bound_agent = NULL;

typedef enum {
    WEB_CMD_NONE = 0,
    WEB_CMD_SWITCH_CARTRIDGE,
    WEB_CMD_ADD_MEMO,
    WEB_CMD_ACTION
} web_cmd_type_t;

typedef struct {
    web_cmd_type_t type;
    char param[128];
} web_cmd_t;

#define MAX_WEB_CMDS 16
static web_cmd_t g_web_cmd_queue[MAX_WEB_CMDS];
static size_t g_web_cmd_head = 0;
static size_t g_web_cmd_tail = 0;
static pthread_mutex_t g_web_cmd_lock = PTHREAD_MUTEX_INITIALIZER;

static void enqueue_web_cmd(web_cmd_type_t type, const char *param)
{
    pthread_mutex_lock(&g_web_cmd_lock);
    size_t next = (g_web_cmd_tail + 1) % MAX_WEB_CMDS;
    if (next != g_web_cmd_head) {
        g_web_cmd_queue[g_web_cmd_tail].type = type;
        if (param) {
            strncpy(g_web_cmd_queue[g_web_cmd_tail].param, param, sizeof(g_web_cmd_queue[g_web_cmd_tail].param) - 1);
            g_web_cmd_queue[g_web_cmd_tail].param[sizeof(g_web_cmd_queue[g_web_cmd_tail].param) - 1] = '\0';
        } else {
            g_web_cmd_queue[g_web_cmd_tail].param[0] = '\0';
        }
        g_web_cmd_tail = next;
    }
    pthread_mutex_unlock(&g_web_cmd_lock);
}

void phoenix_web_portal_drain_commands(void)
{
    while (1) {
        web_cmd_t cmd;
        pthread_mutex_lock(&g_web_cmd_lock);
        if (g_web_cmd_head == g_web_cmd_tail) {
            pthread_mutex_unlock(&g_web_cmd_lock);
            break;
        }
        cmd = g_web_cmd_queue[g_web_cmd_head];
        g_web_cmd_head = (g_web_cmd_head + 1) % MAX_WEB_CMDS;
        pthread_mutex_unlock(&g_web_cmd_lock);

        switch (cmd.type) {
            case WEB_CMD_SWITCH_CARTRIDGE:
                cartridge_mgr_switch_to(cmd.param);
                break;
            case WEB_CMD_ADD_MEMO:
                cartridge_memo_add_entry(cmd.param);
                break;
            case WEB_CMD_ACTION:
                if (strcmp(cmd.param, "pet") == 0) {
                    cartridge_mgr_dispatch_knock(1, 1);
                } else if (strcmp(cmd.param, "knock_fish") == 0) {
                    phoenix_tool_execute("knock_wooden_fish", "{\"count\":1}", NULL, 0);
                } else if (strcmp(cmd.param, "pomo_toggle") == 0) {
                    cartridge_mgr_dispatch_knock(1, 1);
                }
                break;
            default:
                break;
        }
    }
}

void phoenix_web_portal_bind_agent(phoenix_agent_ctx_t *agent_ctx)
{
    g_bound_agent = agent_ctx;
}

bool phoenix_web_portal_is_running(void)
{
    return g_server_running;
}

int phoenix_web_portal_handle_request(const char *req_str, char *resp_out, size_t max_len)
{
    if (!req_str || !resp_out || max_len < 128) return -1;

    /* Captive Portal / SoftAP redirect checks */
    bool is_softap_mode = (net_mgr_get_mode() == NET_MODE_SOFTAP_CONFIG);

    if (strncmp(req_str, "GET /setup", 10) == 0 ||
        (is_softap_mode && (strncmp(req_str, "GET / ", 6) == 0 || strncmp(req_str, "GET /hotspot-detect", 19) == 0 || strncmp(req_str, "GET /generate_204", 17) == 0))) {
        const char *setup_html = phoenix_web_asset_get_setup_html();
        size_t setup_len = phoenix_web_asset_get_setup_html_len();
        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html; charset=UTF-8\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n\r\n%s",
                 setup_len, setup_html);
        return (int)strlen(resp_out);
    }

    /* GET /dashboard or GET /index.html -> Companion Studio Dashboard */
    if (strncmp(req_str, "GET /dashboard", 14) == 0 || strncmp(req_str, "GET /index.html", 15) == 0 || (!is_softap_mode && strncmp(req_str, "GET / ", 6) == 0)) {
        const char *dash_html = phoenix_web_asset_get_dashboard_html();
        size_t dash_len = phoenix_web_asset_get_dashboard_html_len();
        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html; charset=UTF-8\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n\r\n%s",
                 dash_len, dash_html);
        return (int)strlen(resp_out);
    }

    /* 1. GET /api/wifi/scan -> Scan surrounding Wi-Fi APs */
    if (strncmp(req_str, "GET /api/wifi/scan", 18) == 0) {
        net_wifi_ap_info_t aps[8];
        int count = net_mgr_scan_wifi(aps, 8);
        if (count < 0) count = 0;

        char json_buf[1024];
        char list_buf[768] = {0};
        size_t offset = 0;
        for (int i = 0; i < count; i++) {
            offset += snprintf(list_buf + offset, sizeof(list_buf) - offset,
                               "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":\"%s\"}",
                               i > 0 ? "," : "",
                               aps[i].ssid, (int)aps[i].rssi, aps[i].auth);
        }
        snprintf(json_buf, sizeof(json_buf),
                 "{\"success\":true,\"count\":%d,\"aps\":[%s]}",
                 count, list_buf);

        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(json_buf), json_buf);
        return (int)strlen(resp_out);
    }

    /* 2. POST /api/wifi/connect */
    if (strncmp(req_str, "POST /api/wifi/connect", 22) == 0) {
        char ssid[32] = {0};
        char psk[64] = {0};
        const char *body = strstr(req_str, "\r\n\r\n");
        if (body) {
            body += 4;
            cJSON *root = cJSON_Parse(body);
            if (root) {
                cJSON *s = cJSON_GetObjectItem(root, "ssid");
                cJSON *p = cJSON_GetObjectItem(root, "psk");
                cJSON *k = cJSON_GetObjectItem(root, "api_key");
                cJSON *pr = cJSON_GetObjectItem(root, "prompt");
                cJSON *m = cJSON_GetObjectItem(root, "model");
                if (s && s->valuestring) strncpy(ssid, s->valuestring, sizeof(ssid) - 1);
                if (p && p->valuestring) strncpy(psk, p->valuestring, sizeof(psk) - 1);
                if (k && k->valuestring && strlen(k->valuestring) > 0) {
                    phoenix_llm_set_api_key(k->valuestring);
                    phoenix_config_set_str(PHOENIX_CFG_API_KEY, k->valuestring);
                }
                if (pr && pr->valuestring && strlen(pr->valuestring) > 0) {
                    phoenix_config_set_str("agent_prompt", pr->valuestring);
                }
                if (m && m->valuestring && strlen(m->valuestring) > 0) {
                    phoenix_config_set_str(PHOENIX_CFG_MODEL, m->valuestring);
                }
                cJSON_Delete(root);
            }
        }

        if (ssid[0] != '\0') {
            net_mgr_connect_sta(ssid, psk);
        }

        const char *resp_json = "{\"success\":true,\"message\":\"connecting to wifi\"}";
        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(resp_json), resp_json);
        return (int)strlen(resp_out);
    }

    /* 2. GET /api/status -> JSON Status including cartridges telemetry */
    if (strncmp(req_str, "GET /api/status", 15) == 0) {
        uint32_t merit = g_bound_agent ? g_bound_agent->stats.merit_count : 0;
        uint32_t tokens = g_bound_agent ? g_bound_agent->stats.total_tokens_used : 0;
        uint32_t latency = g_bound_agent ? g_bound_agent->stats.last_latency_ms : 0;
        bool pomo_active = g_bound_agent ? g_bound_agent->stats.pomodoro_active : false;
        int state = g_bound_agent ? (int)g_bound_agent->state : 0;
        int tool_count = phoenix_tool_get_count();

        /* Current active cartridge */
        cartridge_t *cur = cartridge_mgr_get_current();
        const char *act_id = cur ? cur->ops.id : "home";

        /* Fetch statuses from registered cartridges */
        char fam_buf[128] = "{}";
        char memo_buf[128] = "{}";
        char clk_buf[128] = "{}";
        char zen_buf[128] = "{}";
        char home_buf[128] = "{}";
        char agent_buf[128] = "{}";

        size_t total_c = cartridge_mgr_get_count();
        for (size_t i = 0; i < total_c; i++) {
            cartridge_t *c = cartridge_mgr_get_by_index(i);
            if (!c || !c->ops.get_web_status) continue;
            if (strcmp(c->ops.id, "familiar") == 0) {
                c->ops.get_web_status(c, fam_buf, sizeof(fam_buf));
            } else if (strcmp(c->ops.id, "memo") == 0) {
                c->ops.get_web_status(c, memo_buf, sizeof(memo_buf));
            } else if (strcmp(c->ops.id, "clock") == 0) {
                c->ops.get_web_status(c, clk_buf, sizeof(clk_buf));
            } else if (strcmp(c->ops.id, "zen") == 0) {
                c->ops.get_web_status(c, zen_buf, sizeof(zen_buf));
            } else if (strcmp(c->ops.id, "home") == 0) {
                c->ops.get_web_status(c, home_buf, sizeof(home_buf));
            } else if (strcmp(c->ops.id, "agent") == 0) {
                c->ops.get_web_status(c, agent_buf, sizeof(agent_buf));
            }
        }

        char json_buf[1536];
        snprintf(json_buf, sizeof(json_buf),
                 "{\"device\":\"Gemini-S1\",\"active_cartridge\":\"%s\","
                 "\"stats\":{\"merit\":%u,\"total_tokens\":%u,\"last_latency_ms\":%u,\"pomodoro_active\":%s,\"state\":%d},"
                 "\"cartridges\":{\"familiar\":%s,\"memo\":%s,\"clock\":%s,\"zen\":%s,\"home\":%s,\"agent\":%s},"
                 "\"tools\":%d}",
                 act_id, merit, tokens, latency, pomo_active ? "true" : "false", state,
                 fam_buf, memo_buf, clk_buf, zen_buf, home_buf, agent_buf, tool_count);

        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(json_buf), json_buf);
        return (int)strlen(resp_out);
    }

    /* 3. POST /api/cartridge/switch -> Switch active cartridge */
    if (strncmp(req_str, "POST /api/cartridge/switch", 26) == 0) {
        char target_id[32] = {0};
        const char *body = strstr(req_str, "\r\n\r\n");
        if (body) {
            body += 4;
            cJSON *root = cJSON_Parse(body);
            if (root) {
                cJSON *id_item = cJSON_GetObjectItem(root, "id");
                if (id_item && id_item->valuestring) {
                    strncpy(target_id, id_item->valuestring, sizeof(target_id) - 1);
                }
                cJSON_Delete(root);
            }
        }
        if (target_id[0] != '\0') {
            bool in_server_thread = g_server_running && (pthread_equal(pthread_self(), g_server_thread) != 0);
            if (in_server_thread) {
                enqueue_web_cmd(WEB_CMD_SWITCH_CARTRIDGE, target_id);
            } else {
                cartridge_mgr_switch_to(target_id);
            }
        }
        const char *resp_json = "{\"success\":true,\"message\":\"cartridge switched\"}";
        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(resp_json), resp_json);
        return (int)strlen(resp_out);
    }

    /* 4. POST /api/memo/add -> Push new memo to cartridge_memo */
    if (strncmp(req_str, "POST /api/memo/add", 18) == 0) {
        char memo_text[128] = {0};
        const char *body = strstr(req_str, "\r\n\r\n");
        if (body) {
            body += 4;
            cJSON *root = cJSON_Parse(body);
            if (root) {
                cJSON *content = cJSON_GetObjectItem(root, "content");
                if (content && content->valuestring) {
                    strncpy(memo_text, content->valuestring, sizeof(memo_text) - 1);
                }
                cJSON_Delete(root);
            }
        }
        if (memo_text[0] != '\0') {
            bool in_server_thread = g_server_running && (pthread_equal(pthread_self(), g_server_thread) != 0);
            if (in_server_thread) {
                enqueue_web_cmd(WEB_CMD_ADD_MEMO, memo_text);
            } else {
                cartridge_memo_add_entry(memo_text);
            }
        }
        const char *resp_json = "{\"success\":true,\"message\":\"memo entry added\"}";
        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(resp_json), resp_json);
        return (int)strlen(resp_out);
    }

    /* 5. POST /api/action -> Remote interactions (pet, knock_fish, pomo_toggle) */
    if (strncmp(req_str, "POST /api/action", 16) == 0) {
        char action[32] = {0};
        const char *body = strstr(req_str, "\r\n\r\n");
        if (body) {
            body += 4;
            cJSON *root = cJSON_Parse(body);
            if (root) {
                cJSON *act = cJSON_GetObjectItem(root, "action");
                if (act && act->valuestring) {
                    strncpy(action, act->valuestring, sizeof(action) - 1);
                }
                cJSON_Delete(root);
            }
        }

        bool in_server_thread = g_server_running && (pthread_equal(pthread_self(), g_server_thread) != 0);
        if (in_server_thread) {
            enqueue_web_cmd(WEB_CMD_ACTION, action);
        } else {
            if (strcmp(action, "pet") == 0) {
                cartridge_mgr_dispatch_knock(1, 1);
            } else if (strcmp(action, "knock_fish") == 0) {
                phoenix_tool_execute("knock_wooden_fish", "{\"count\":1}", NULL, 0);
            } else if (strcmp(action, "pomo_toggle") == 0) {
                cartridge_mgr_dispatch_knock(1, 1);
            }
        }

        const char *resp_json = "{\"success\":true,\"message\":\"action dispatched\"}";
        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(resp_json), resp_json);
        return (int)strlen(resp_out);
    }

    /* 6. GET /api/config -> Query current agent configuration */
    if (strncmp(req_str, "GET /api/config", 15) == 0) {
        char key_buf[64] = {0};
        char prompt_buf[256] = {0};
        char model_buf[64] = {0};
        phoenix_config_get_str(PHOENIX_CFG_API_KEY, "", key_buf, sizeof(key_buf));
        phoenix_config_get_str("agent_prompt", "你是一个贴心的赛博桌面极客助手，语气温和简短", prompt_buf, sizeof(prompt_buf));
        phoenix_config_get_str(PHOENIX_CFG_MODEL, "deepseek-chat", model_buf, sizeof(model_buf));

        /* API Key 脱敏显示 */
        char masked_key[64] = {0};
        size_t klen = strlen(key_buf);
        if (klen > 8) {
            snprintf(masked_key, sizeof(masked_key), "%.4s****%.4s", key_buf, key_buf + klen - 4);
        } else if (klen > 0) {
            snprintf(masked_key, sizeof(masked_key), "********");
        }

        char json_buf[1024];
        snprintf(json_buf, sizeof(json_buf),
                 "{\"success\":true,\"has_key\":%s,\"api_key_masked\":\"%s\",\"prompt\":\"%s\",\"model\":\"%s\"}",
                 klen > 0 ? "true" : "false", masked_key, prompt_buf, model_buf);

        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(json_buf), json_buf);
        return (int)strlen(resp_out);
    }

    /* 7. POST /api/config -> Update Agent & LLM Config */
    if (strncmp(req_str, "POST /api/config", 16) == 0) {
        const char *body = strstr(req_str, "\r\n\r\n");
        if (body) {
            body += 4;
            cJSON *root = cJSON_Parse(body);
            if (root) {
                cJSON *key = cJSON_GetObjectItem(root, "api_key");
                cJSON *pr = cJSON_GetObjectItem(root, "prompt");
                cJSON *m = cJSON_GetObjectItem(root, "model");
                if (key && key->valuestring && strlen(key->valuestring) > 0) {
                    phoenix_llm_set_api_key(key->valuestring);
                    phoenix_config_set_str(PHOENIX_CFG_API_KEY, key->valuestring);
                }
                if (pr && pr->valuestring && strlen(pr->valuestring) > 0) {
                    phoenix_config_set_str("agent_prompt", pr->valuestring);
                }
                if (m && m->valuestring && strlen(m->valuestring) > 0) {
                    phoenix_config_set_str(PHOENIX_CFG_MODEL, m->valuestring);
                }
                cJSON_Delete(root);
            }
        }
        const char *resp_json = "{\"success\":true,\"message\":\"config updated\"}";
        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(resp_json), resp_json);
        return (int)strlen(resp_out);
    }

    /* 7. POST /api/wifi/reset -> One-click Reset to SoftAP */
    if (strncmp(req_str, "POST /api/wifi/reset", 20) == 0) {
        net_mgr_reset_to_softap();
        const char *resp_json = "{\"success\":true,\"message\":\"reset to softap\"}";
        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(resp_json), resp_json);
        return (int)strlen(resp_out);
    }

    /* 8. POST /api/proactive -> Trigger Proactive demo */
    if (strncmp(req_str, "POST /api/proactive", 19) == 0) {
        if (g_bound_agent) {
            phoenix_agent_trigger_proactive(g_bound_agent, PROACTIVE_THRESHOLD_FATIGUE, "Web控制台远程演示触发");
        }
        const char *resp_json = "{\"success\":true,\"status\":\"triggered\",\"message\":\"proactive triggered\"}";
        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(resp_json), resp_json);
        return (int)strlen(resp_out);
    }

    /* 9. GET /api/logs -> Query current log level and recent ring buffer logs */
    if (strncmp(req_str, "GET /api/logs", 13) == 0) {
        int lvl = phoenix_log_get_level();
        const char *lvl_name = phoenix_log_level_to_str(lvl);

        char raw_logs[4096];
        size_t nread = phoenix_log_get_recent(raw_logs, sizeof(raw_logs));
        (void)nread;

        /* 构建 cJSON 响应以安全转义换行与引号 */
        cJSON *root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddNumberToObject(root, "level_num", lvl);
        cJSON_AddStringToObject(root, "level_str", lvl_name);
        cJSON_AddStringToObject(root, "logs", raw_logs);

        char *rendered = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        if (rendered) {
            snprintf(resp_out, max_len,
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: application/json; charset=UTF-8\r\n"
                     "Content-Length: %zu\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Connection: close\r\n\r\n%s",
                     strlen(rendered), rendered);
            free(rendered);
            return (int)strlen(resp_out);
        }
    }

    /* 10. POST /api/logs/level -> Dynamically change application log level */
    if (strncmp(req_str, "POST /api/logs/level", 20) == 0 || strncmp(req_str, "POST /api/logs", 14) == 0) {
        const char *body = strstr(req_str, "\r\n\r\n");
        int target_lvl = phoenix_log_get_level();
        if (body) {
            body += 4;
            cJSON *root = cJSON_Parse(body);
            if (root) {
                cJSON *item = cJSON_GetObjectItem(root, "level");
                if (item) {
                    if (item->type == cJSON_String && item->valuestring) {
                        target_lvl = phoenix_log_level_from_str(item->valuestring);
                    } else if (item->type == cJSON_Number) {
                        target_lvl = item->valueint;
                    }
                }
                cJSON_Delete(root);
            }
        }

        phoenix_log_set_level(target_lvl);

        char resp_buf[256];
        snprintf(resp_buf, sizeof(resp_buf),
                 "{\"success\":true,\"level_num\":%d,\"level_str\":\"%s\"}",
                 target_lvl, phoenix_log_level_to_str(target_lvl));

        snprintf(resp_out, max_len,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(resp_buf), resp_buf);
        return (int)strlen(resp_out);
    }

    /* Fallback 404 */
    const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    snprintf(resp_out, max_len, "%s", not_found);
    return (int)strlen(resp_out);
}

static void *server_thread_worker(void *arg)
{
    (void)arg;
    struct sockaddr_in client_addr;
    socklen_t client_len;

    /* 堆上动态分配，避免撑爆嵌入式系统 8KB 默认 pthread 栈 */
    char *req_buf = (char *)malloc(2048);
    char *resp_buf = (char *)malloc(16384);
    if (!req_buf || !resp_buf) {
        printf("[PhoenixWeb] Error: failed to allocate buffers for server worker\n");
        if (req_buf) free(req_buf);
        if (resp_buf) free(resp_buf);
        return NULL;
    }

    while (g_server_running && g_server_fd >= 0) {
        client_len = sizeof(client_addr);
        int client_fd = accept(g_server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!g_server_running) break;
            usleep(10000);
            continue;
        }

        /* 设置 300ms 接收超时，彻底防止 Chrome 等浏览器建立空预连接导致单线程无限死锁 */
        struct timeval tv_client;
        tv_client.tv_sec = 0;
        tv_client.tv_usec = 300000;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_client, sizeof(tv_client));

        ssize_t n = recv(client_fd, req_buf, 2048 - 1, 0);
        if (n > 0) {
            req_buf[n] = '\0';
            /* 提取首行日志输出 */
            char req_summary[64] = {0};
            char *crlf = strstr(req_buf, "\r\n");
            if (crlf) {
                size_t line_len = (size_t)(crlf - req_buf);
                if (line_len >= sizeof(req_summary)) line_len = sizeof(req_summary) - 1;
                strncpy(req_summary, req_buf, line_len);
            } else {
                strncpy(req_summary, req_buf, sizeof(req_summary) - 1);
            }
            printf("[PhoenixWeb] 📥 Client connected: %s\n", req_summary);

            int resp_len = phoenix_web_portal_handle_request(req_buf, resp_buf, 16384);
            if (resp_len > 0) {
                ssize_t total_sent = 0;
                while (total_sent < resp_len) {
                    ssize_t s = send(client_fd, resp_buf + total_sent, (size_t)(resp_len - total_sent), 0);
                    if (s <= 0) break;
                    total_sent += s;
                }
                printf("[PhoenixWeb] 📤 Sent %zd/%d bytes\n", total_sent, resp_len);
            }
        }

        /* 优雅结束：半关闭写端，排空残余接收缓冲以防内核发出 TCP RST 导致浏览器 ERR_EMPTY_RESPONSE */
        shutdown(client_fd, SHUT_WR);
        char drain_buf[128];
        while (recv(client_fd, drain_buf, sizeof(drain_buf), MSG_DONTWAIT) > 0) {
            /* 丢弃未读完的请求冗余头 */
        }
        close(client_fd);
    }

    free(req_buf);
    free(resp_buf);
    return NULL;
}

int phoenix_web_portal_start(uint16_t port, phoenix_agent_ctx_t *agent_ctx)
{
    if (g_server_running) return 0;

    g_bound_agent = agent_ctx;
    g_server_port = port > 0 ? port : PHOENIX_DEFAULT_WEB_PORT;

    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        printf("[PhoenixWeb] Warning: socket creation failed, standalone memory mode available.\n");
        g_server_running = true;
        return 0;
    }

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 设置 1 秒超时避免 accept 在退出时无限阻塞 */
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(g_server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(g_server_port);

    if (bind(g_server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[PhoenixWeb] Port %u bind failed, fallback to memory mode.\n", g_server_port);
        close(g_server_fd);
        g_server_fd = -1;
        g_server_running = true;
        return 0;
    }

    if (listen(g_server_fd, 5) < 0) {
        close(g_server_fd);
        g_server_fd = -1;
        g_server_running = true;
        return 0;
    }

    g_server_running = true;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 16384);
    int ret = pthread_create(&g_server_thread, &attr, server_thread_worker, NULL);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        printf("[PhoenixWeb] Warning: pthread_create failed with code %d\n", ret);
        close(g_server_fd);
        g_server_fd = -1;
        g_server_running = false;
        return -1;
    }

    printf("[PhoenixWeb] 🌐 Web Portal listening at http://0.0.0.0:%u\n", g_server_port);
    return 0;
}

void phoenix_web_portal_stop(void)
{
    if (!g_server_running) return;
    g_server_running = false;
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
    pthread_join(g_server_thread, NULL);
    printf("[PhoenixWeb] 🛑 Web Portal stopped cleanly.\n");
}
