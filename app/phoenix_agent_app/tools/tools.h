/**
 * @file tools.h
 * @brief Embodied Tools Collection for Phoenix HoloDesk-S1
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_TOOLS_H
#define PHOENIX_TOOLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#if defined(__has_include) && __has_include("../core/tool_registry.h")
#  include "../core/tool_registry.h"
#else
#  include "tool_registry.h"
#endif

/**
 * @brief Register all built-in embodied tools into Phoenix Tool Registry
 * @return 0 on success
 */
int phoenix_register_builtin_tools(void);

/* Pomodoro Background Service APIs */
void     pomodoro_service_tick_1s(void);
bool     pomodoro_service_is_active(void);
uint16_t pomodoro_service_get_remaining(void);
int      pomodoro_service_start(uint16_t duration_minutes);
int      pomodoro_service_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_TOOLS_H */
