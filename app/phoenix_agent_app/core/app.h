/**
 * @file app.h
 * @brief Phoenix HoloDesk-S1 Master Application Facade
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_APP_H
#define PHOENIX_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Phoenix Application Configuration
 */
typedef struct {
    const char *storage_dir;    /**< Persistent store directory, e.g. "/data/phoenix" */
    const char *sounds_dir;     /**< Sound assets root directory, e.g. "/data/sounds" */
    const char *api_key;        /**< Optional LLM API Key override */
    bool register_tools;        /**< Whether to automatically register built-in embodied tools */
    bool enable_web_portal;     /**< Whether to launch embedded Web Portal server */
    uint16_t web_port;          /**< Web Portal TCP port, 0 for default 8080 */
} phoenix_app_config_t;

/**
 * @brief Initialize all core subsystems via Unified Application Facade
 *        (EventBus, Store, ToolRegistry, Built-in Tools, Audio, LLM Provider)
 * @param config Configuration parameters, or NULL to use defaults
 * @return 0 on success, negative error code on failure
 */
int phoenix_app_init(const phoenix_app_config_t *config);

/**
 * @brief De-initialize all core subsystems cleanly in reverse order
 */
void phoenix_app_deinit(void);

/**
 * @brief Periodic processing tick for facade (drains async events, ticks voice pipeline, flushes storage)
 */
void phoenix_app_tick(void);

/**
 * @brief Check if the application facade is currently initialized
 * @return true if initialized, false otherwise
 */
bool phoenix_app_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_APP_H */
