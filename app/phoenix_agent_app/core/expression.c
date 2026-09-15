/**
 * @file expression.c
 * @brief Embodied Expression Engine Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "expression.h"
#include "../hal/hal_actuator.h"
#include "event_bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *g_bound_eye = NULL;
static bool g_expr_initialized = false;

int phoenix_expression_init(void)
{
    g_bound_eye = NULL;
    g_expr_initialized = true;
    printf("[PhoenixExpression] 🎭 Embodied Expression Engine initialized.\n");
    return 0;
}

void phoenix_expression_bind_eye(void *eye_anim)
{
    g_bound_eye = eye_anim;
}

int phoenix_expression_play(phoenix_expr_type_t expr, int intensity, const char *banner_text)
{
    if (!g_expr_initialized || expr < 0 || expr >= PHOENIX_EXPR_COUNT) {
        return -1;
    }

    if (intensity <= 0) intensity = 5;
    if (intensity > 10) intensity = 10;

    uint32_t color = 0x00E5FF;
    uint8_t brightness = (uint8_t)(intensity * 10);
    hal_led_mode_t led_mode = HAL_LED_BREATHING;
    hal_sound_type_t sound = HAL_SOUND_CLICK;
    hal_haptic_pattern_t haptic = HAL_HAPTIC_CLICK;
    bool play_audio = false;

    switch (expr) {
    case PHOENIX_EXPR_STANDBY:
        color = 0x00E5FF; /* Cyan */
        led_mode = HAL_LED_BREATHING;
        haptic = HAL_HAPTIC_CLICK;
        play_audio = false;
        break;

    case PHOENIX_EXPR_JOY:
        color = 0xFFD700; /* Gold */
        led_mode = HAL_LED_BREATHING;
        sound = HAL_SOUND_WOODEN_FISH;
        haptic = HAL_HAPTIC_CLICK;
        play_audio = true;
        break;

    case PHOENIX_EXPR_THINKING:
        color = 0x9933FF; /* Violet */
        led_mode = HAL_LED_BREATHING;
        haptic = HAL_HAPTIC_PULSE;
        play_audio = false;
        break;

    case PHOENIX_EXPR_ALERT:
        color = 0xFF0033; /* Crimson Red */
        led_mode = HAL_LED_PULSE;
        sound = HAL_SOUND_ALERT;
        haptic = HAL_HAPTIC_ALERT_BURST;
        play_audio = true;
        break;

    case PHOENIX_EXPR_SLEEP:
        color = 0xCC6600; /* Amber Warm Dim */
        brightness = (uint8_t)(brightness / 2);
        led_mode = HAL_LED_BREATHING;
        sound = HAL_SOUND_WAKEUP;
        haptic = HAL_HAPTIC_PULSE;
        play_audio = false;
        break;

    case PHOENIX_EXPR_CELEBRATE:
        color = 0xFFD700; /* Golden Flash */
        brightness = 100;
        led_mode = HAL_LED_SOLID;
        sound = HAL_SOUND_CELEBRATE;
        haptic = HAL_HAPTIC_DOUBLE_CLICK;
        play_audio = true;
        break;

    default:
        break;
    }

    /* 1. Physical Actuators Synchronization */
    hal_actuator_set_led(led_mode, color, brightness);
    hal_actuator_trigger_haptic(haptic);
    if (play_audio) {
        hal_actuator_play_sound(sound);
    }

    /* 2. Broadcast Flying Text / Banner via Event Bus if requested */
    if (banner_text && strlen(banner_text) > 0) {
        phoenix_event_data_t evt;
        memset(&evt, 0, sizeof(evt));
        evt.type = PHOENIX_EVT_FLYING_TEXT;
        evt.data.flying_text.text = banner_text;
        evt.data.flying_text.color_rgb = color;
        phoenix_event_post_async(&evt);
    }

    printf("[PhoenixExpression] 🎭 Played Expression #%d (Color=0x%06X, Intensity=%d, Banner=\"%s\")\n",
           expr, color, intensity, banner_text ? banner_text : "");

    return 0;
}

void phoenix_expression_deinit(void)
{
    g_bound_eye = NULL;
    g_expr_initialized = false;
}
