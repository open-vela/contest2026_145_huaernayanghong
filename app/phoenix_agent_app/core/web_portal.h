/**
 * @file web_portal.h
 * @brief Embedded Web Configuration & Skill Studio Server for Phoenix Agent
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_WEB_PORTAL_H
#define PHOENIX_WEB_PORTAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "agent_core.h"

#define PHOENIX_DEFAULT_WEB_PORT 8080

/**
 * @brief Start the background embedded Web Portal HTTP server
 * @param port TCP port (e.g. 8080), or 0 for default
 * @param agent_ctx Pointer to active agent context for telemetry
 * @return 0 on success, negative on error
 */
int phoenix_web_portal_start(uint16_t port, phoenix_agent_ctx_t *agent_ctx);

/**
 * @brief Stop the Web Portal HTTP server
 */
void phoenix_web_portal_stop(void);

/**
 * @brief Check if the Web Portal server is running
 * @return true if running, false otherwise
 */
bool phoenix_web_portal_is_running(void);

/**
 * @brief Dynamically bind or rebind agent context for telemetry
 * @param agent_ctx Pointer to agent context
 */
void phoenix_web_portal_bind_agent(phoenix_agent_ctx_t *agent_ctx);

/**
 * @brief Process single incoming request synchronously (useful for tests or non-threaded loops)
 * @param req_str Raw HTTP request buffer
 * @param resp_out Buffer to store HTTP response
 * @param max_len Size of resp_out buffer
 * @return Response length on success, negative on error
 */
int phoenix_web_portal_handle_request(const char *req_str, char *resp_out, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_WEB_PORTAL_H */
