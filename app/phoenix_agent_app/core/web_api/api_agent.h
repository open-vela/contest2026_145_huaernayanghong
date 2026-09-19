/**
 * @file api_agent.h
 * @brief Phoenix HoloDesk-S1 Web Companion Agent REST API Handlers Declaration
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_API_AGENT_H
#define PHOENIX_API_AGENT_H

#include "../web_router.h"

#ifdef __cplusplus
extern "C" {
#endif

int handle_agent_chat(const http_req_t *req, http_resp_t *resp);
int handle_agent_tools(const http_req_t *req, http_resp_t *resp);
int handle_agent_tool_exec(const http_req_t *req, http_resp_t *resp);
int handle_agent_memory(const http_req_t *req, http_resp_t *resp);
int handle_agent_memory_clear(const http_req_t *req, http_resp_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_API_AGENT_H */
