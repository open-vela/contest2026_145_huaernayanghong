/**
 * @file tools.c
 * @brief Embodied Built-in Tools Registration
 * @author OpenVela Contest 2026 Team 145
 */

#include "tools.h"
#if defined(__has_include) && __has_include("utils/log_utils.h")
#  include "utils/log_utils.h"
#else
#  include "../utils/log_utils.h"
#endif
#include <stdio.h>

#define TAG "PhoenixTools"

extern const phoenix_tool_desc_t g_tool_wooden_fish;
extern const phoenix_tool_desc_t g_tool_pomodoro;
extern const phoenix_tool_desc_t g_tool_eye_emotion;
extern const phoenix_tool_desc_t g_tool_system_health;
extern const phoenix_tool_desc_t g_tool_launch_app;

int phoenix_register_builtin_tools(void)
{
    phoenix_tool_register(&g_tool_wooden_fish);
    phoenix_tool_register(&g_tool_pomodoro);
    phoenix_tool_register(&g_tool_eye_emotion);
    phoenix_tool_register(&g_tool_system_health);
    phoenix_tool_register(&g_tool_launch_app);
    phoenix_tool_register(&g_tool_todo);
    phoenix_tool_register(&g_tool_environment);

    LOG_I(TAG, "%zu built-in embodied tools registered.", phoenix_tool_get_count());
    return 0;
}
