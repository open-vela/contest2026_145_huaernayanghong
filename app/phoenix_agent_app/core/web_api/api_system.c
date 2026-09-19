/**
 * @file api_system.c
 * @brief System, Dashboard & Captive Portal Web API Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_api.h"
#include "../web_assets.h"
#include "../tool_registry.h"
#include "../cartridge_mgr.h"
#include "../../hal/network_mgr.h"
#include "../../hal/hal_system.h"
#include "../../hal/hal_sdcard.h"
#include "../../utils/time_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static phoenix_agent_ctx_t *g_bound_agent = NULL;

void web_api_set_bound_agent(phoenix_agent_ctx_t *agent)
{
    g_bound_agent = agent;
}

phoenix_agent_ctx_t* web_api_get_bound_agent(void)
{
    return g_bound_agent;
}

int handle_system_root(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    bool is_softap_mode = (net_mgr_get_mode() == NET_MODE_SOFTAP_CONFIG);
    if (is_softap_mode) {
        http_resp_html(resp, 200, phoenix_web_asset_get_setup_html(), phoenix_web_asset_get_setup_html_len());
    } else {
        http_resp_html(resp, 200, phoenix_web_asset_get_dashboard_html(), phoenix_web_asset_get_dashboard_html_len());
    }
    return 0;
}

int handle_system_setup(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    http_resp_html(resp, 200, phoenix_web_asset_get_setup_html(), phoenix_web_asset_get_setup_html_len());
    return 0;
}

int handle_system_ble_setup(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    http_resp_html(resp, 200, phoenix_web_asset_get_ble_setup_html(), phoenix_web_asset_get_ble_setup_html_len());
    return 0;
}

int handle_system_dashboard(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    http_resp_html(resp, 200, phoenix_web_asset_get_dashboard_html(), phoenix_web_asset_get_dashboard_html_len());
    return 0;
}

int handle_system_captive_probe(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    http_resp_html(resp, 200, phoenix_web_asset_get_setup_html(), phoenix_web_asset_get_setup_html_len());
    return 0;
}

int handle_system_captive_probe_android(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    http_resp_redirect(resp, 302, "http://192.168.4.1/");
    return 0;
}

int handle_system_status(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
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

    /* Storage & TF Card info */
    hal_sdcard_info_t sd_info;
    memset(&sd_info, 0, sizeof(sd_info));
    hal_sdcard_get_info(&sd_info);
    bool sd_mounted = hal_sdcard_is_mounted();
    const char *data_dir = hal_system_get_storage_base_path();
    const char *temp_dir = hal_system_get_temp_base_path();

    /* System Telemetry */
    hal_system_telemetry_t sys;
    memset(&sys, 0, sizeof(sys));
    hal_system_get_telemetry(&sys);

    /* Network status */
    char net_ip[32] = {0};
    char net_ssid[32] = {0};
    net_mode_t net_mode = net_mgr_get_mode();
    net_mgr_get_ip(net_ip, sizeof(net_ip));
    net_mgr_get_ssid(net_ssid, sizeof(net_ssid));
    const char *mode_str = (net_mode == NET_MODE_STA_CONNECTED) ? "STA_CONNECTED" :
                           (net_mode == NET_MODE_STA_CONNECTING) ? "CONNECTING" :
                           (net_mode == NET_MODE_SOFTAP_CONFIG) ? "SOFTAP" : "DISCONNECTED";

    char json_buf[2048];
    snprintf(json_buf, sizeof(json_buf),
             "{\"device\":\"Gemini-S1\",\"active_cartridge\":\"%s\","
             "\"stats\":{\"merit\":%u,\"total_tokens\":%u,\"last_latency_ms\":%u,\"pomodoro_active\":%s,\"state\":%d},"
             "\"network\":{\"mode\":\"%s\",\"ip\":\"%s\",\"ssid\":\"%s\",\"connected\":%s},"
             "\"storage\":{\"data_dir\":\"%s\",\"temp_dir\":\"%s\",\"sdcard_mounted\":%s,\"sdcard_mount_point\":\"%s\",\"sdcard_used_pct\":%u,\"sdcard_total_mb\":%u,\"sdcard_free_mb\":%u},"
             "\"telemetry\":{\"cpu_freq_mhz\":%u,\"cpu_load_pct\":%u,\"temp_c\":%d,\"mem_total_kb\":%u,\"mem_free_kb\":%u,\"mem_used_pct\":%u,\"uptime\":%llu,\"fps\":%u,\"os\":\"%s\"},"
             "\"cartridges\":{\"familiar\":%s,\"memo\":%s,\"clock\":%s,\"zen\":%s,\"home\":%s,\"agent\":%s},"
             "\"tools\":%d}",
             act_id, merit, tokens, latency, pomo_active ? "true" : "false", state,
             mode_str, net_ip, net_ssid, (net_mode == NET_MODE_STA_CONNECTED) ? "true" : "false",
             data_dir, temp_dir, sd_mounted ? "true" : "false", sd_info.mount_point, (unsigned int)sd_info.used_pct,
             (unsigned int)sd_info.total_mb, (unsigned int)sd_info.free_mb,
             (unsigned int)sys.cpu_freq_mhz, (unsigned int)sys.cpu_load_pct, (int)sys.cpu_temperature_c,
             (unsigned int)sys.mem_total_kb, (unsigned int)sys.mem_free_kb, (unsigned int)sys.mem_used_pct,
             (unsigned long long)sys.uptime_seconds, (unsigned int)sys.fps, sys.os_version,
             fam_buf, memo_buf, clk_buf, zen_buf, home_buf, agent_buf, tool_count);

    http_resp_json(resp, 200, json_buf);
    return 0;
}

int handle_system_proactive(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    if (g_bound_agent) {
        phoenix_agent_trigger_proactive(g_bound_agent, PROACTIVE_THRESHOLD_FATIGUE, "Web控制台远程演示触发");
    }
    http_resp_json(resp, 200, "{\"success\":true,\"status\":\"triggered\",\"message\":\"proactive triggered\"}");
    return 0;
}

int handle_system_get_time(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    char dt_str[32] = {0};
    time_utils_get_datetime_str(dt_str, sizeof(dt_str));
    bool synced = time_utils_is_synced();
    time_t now = time_utils_get_epoch();

    char json[128];
    snprintf(json, sizeof(json), "{\"synced\":%s,\"datetime\":\"%s\",\"timestamp\":%lld}",
             synced ? "true" : "false", dt_str, (long long)now);
    http_resp_json(resp, 200, json);
    return 0;
}

int handle_system_set_time(const http_req_t *req, http_resp_t *resp)
{
    if (!req || !req->body || req->body_len == 0) {
        http_resp_json(resp, 400, "{\"error\":\"Empty request body\"}");
        return -1;
    }

    int64_t ts = 0;
    const char *p = strstr(req->body, "\"timestamp_ms\"");
    if (p) {
        p = strchr(p, ':');
        if (p) ts = atoll(p + 1) / 1000;
    } else {
        p = strstr(req->body, "\"timestamp\"");
        if (p) {
            p = strchr(p, ':');
            if (p) ts = atoll(p + 1);
        }
    }

    if (ts < 1704067200) {
        http_resp_json(resp, 400, "{\"error\":\"Invalid timestamp\"}");
        return -1;
    }

    time_utils_set_time((time_t)ts);

    char dt_str[32] = {0};
    time_utils_get_datetime_str(dt_str, sizeof(dt_str));

    char json[128];
    snprintf(json, sizeof(json), "{\"success\":true,\"datetime\":\"%s\",\"timestamp\":%lld}",
             dt_str, (long long)ts);
    http_resp_json(resp, 200, json);
    return 0;
}
