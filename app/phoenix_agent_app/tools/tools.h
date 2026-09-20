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

extern const phoenix_tool_desc_t g_tool_wooden_fish;
extern const phoenix_tool_desc_t g_tool_pomodoro;
extern const phoenix_tool_desc_t g_tool_eye_emotion;
extern const phoenix_tool_desc_t g_tool_system_health;
extern const phoenix_tool_desc_t g_tool_launch_app;
extern const phoenix_tool_desc_t g_tool_todo;
extern const phoenix_tool_desc_t g_tool_environment;

/**
 * @brief Register all built-in embodied tools into Phoenix Tool Registry
 * @return 0 on success
 */
int phoenix_register_builtin_tools(void);

/* Pomodoro Background Service APIs */
typedef enum {
    POMODORO_MODE_FOCUS = 0,   /* 专注模式 (例如 25 分钟) */
    POMODORO_MODE_SHORT_BREAK, /* 短休息 (例如 5 分钟) */
    POMODORO_MODE_LONG_BREAK   /* 长休息 (例如 15 分钟) */
} pomodoro_mode_t;

void            pomodoro_service_tick_1s(void);
bool            pomodoro_service_is_active(void);
bool            pomodoro_service_is_paused(void);
uint16_t        pomodoro_service_get_remaining(void);
uint16_t        pomodoro_service_get_total_duration(void);
pomodoro_mode_t pomodoro_service_get_mode(void);
void            pomodoro_service_set_mode(pomodoro_mode_t mode);
int             pomodoro_service_start(uint16_t duration_minutes);
int             pomodoro_service_pause(void);
int             pomodoro_service_resume(void);
int             pomodoro_service_stop(void);
int             pomodoro_service_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_TOOLS_H */
