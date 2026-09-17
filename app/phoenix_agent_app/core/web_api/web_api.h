/**
 * @file web_api.h
 * @brief Phoenix HoloDesk-S1 Web Companion REST API Handlers Declaration
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_WEB_API_H
#define PHOENIX_WEB_API_H

#include "../web_router.h"
#include "../agent_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 绑定 Agent 核心实例供各 API 获取遥测 */
void web_api_set_bound_agent(phoenix_agent_ctx_t *agent);
phoenix_agent_ctx_t* web_api_get_bound_agent(void);

/* =========================================================================
 * 模块 1: 系统、仪表盘与 SoftAP 配网重定向 (api_system.c)
 * ========================================================================= */
int handle_system_root(const http_req_t *req, http_resp_t *resp);
int handle_system_setup(const http_req_t *req, http_resp_t *resp);
int handle_system_ble_setup(const http_req_t *req, http_resp_t *resp);
int handle_system_dashboard(const http_req_t *req, http_resp_t *resp);
int handle_system_captive_probe(const http_req_t *req, http_resp_t *resp);
int handle_system_captive_probe_android(const http_req_t *req, http_resp_t *resp);
int handle_system_status(const http_req_t *req, http_resp_t *resp);
int handle_system_proactive(const http_req_t *req, http_resp_t *resp);

/* =========================================================================
 * 模块 2: Wi-Fi 扫描、连接与重置 (api_wifi.c)
 * ========================================================================= */
int handle_wifi_scan(const http_req_t *req, http_resp_t *resp);
int handle_wifi_status(const http_req_t *req, http_resp_t *resp);
int handle_wifi_connect(const http_req_t *req, http_resp_t *resp);
int handle_wifi_reset(const http_req_t *req, http_resp_t *resp);

/* =========================================================================
 * 模块 3: 卡带调度、灵感外脑与桌面动作 (api_cartridge.c)
 * ========================================================================= */
int handle_cartridge_switch(const http_req_t *req, http_resp_t *resp);
int handle_memo_add(const http_req_t *req, http_resp_t *resp);
int handle_action_dispatch(const http_req_t *req, http_resp_t *resp);

/* =========================================================================
 * 模块 4: 大模型与 Agent 配置热加载 (api_config.c)
 * ========================================================================= */
int handle_config_get(const http_req_t *req, http_resp_t *resp);
int handle_config_post(const http_req_t *req, http_resp_t *resp);

/* =========================================================================
 * 模块 5: 统一日志检索与等级调整 (api_logs.c)
 * ========================================================================= */
int handle_logs_get(const http_req_t *req, http_resp_t *resp);
int handle_logs_level_post(const http_req_t *req, http_resp_t *resp);
int handle_logs_clear_post(const http_req_t *req, http_resp_t *resp);

/* =========================================================================
 * 模块 6: 外置 TF 卡读写与文件系统管理 (api_sdcard.c)
 * ========================================================================= */
int handle_sdcard_status(const http_req_t *req, http_resp_t *resp);
int handle_sdcard_list(const http_req_t *req, http_resp_t *resp);
int handle_sdcard_download(const http_req_t *req, http_resp_t *resp);
int handle_sdcard_upload(const http_req_t *req, http_resp_t *resp);
int handle_sdcard_delete(const http_req_t *req, http_resp_t *resp);
int handle_sdcard_mkdir(const http_req_t *req, http_resp_t *resp);

/** 批量将上述所有 API 控制器注册到 web_router */
void web_api_register_all(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_WEB_API_H */
