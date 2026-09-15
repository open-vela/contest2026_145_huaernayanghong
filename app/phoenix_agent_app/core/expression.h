/**
 * @file expression.h
 * @brief Embodied Expression Engine for Phoenix HoloDesk-S1
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_EXPRESSION_H
#define PHOENIX_EXPRESSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief High-level Embodied Expression Semantics
 */
typedef enum {
    PHOENIX_EXPR_STANDBY = 0,    /**< Standby/Idle: Cyan glow, calm breathing, subtle eye saccade */
    PHOENIX_EXPR_JOY,            /**< Joy/Merit: Happy crescent eye, gold glow (0xFFD700), wooden fish sound, haptic click */
    PHOENIX_EXPR_THINKING,       /**< Thinking: Thinking eye, cyan-purple breath (0x9933FF), silent, subtle haptic */
    PHOENIX_EXPR_ALERT,          /**< Alert/Intervention: Alert crimson eye (0xFF0033), alert tone, double buzz */
    PHOENIX_EXPR_SLEEP,          /**< Sleep/Night: Half-closed sleepy eye, amber dim breath (0xCC6600), white noise */
    PHOENIX_EXPR_CELEBRATE,      /**< Celebrate: Golden chase LED, bell chime, double buzz, celebratory banner */
    PHOENIX_EXPR_COUNT
} phoenix_expr_type_t;

/**
 * @brief Initialize Embodied Expression Engine
 * @return 0 on success
 */
int phoenix_expression_init(void);

/**
 * @brief Play an embodied expression across all physical actuators & UI
 * @param expr High-level expression type
 * @param intensity 1 to 10 scale (or 0 for default 5)
 * @param banner_text Optional text for flying text or UI banner (can be NULL)
 * @return 0 on success
 */
int phoenix_expression_play(phoenix_expr_type_t expr, int intensity, const char *banner_text);

/**
 * @brief Optional hook to bind LVGL eye animation context
 * @param eye_anim Pointer to phoenix_eye_anim_t or NULL
 */
void phoenix_expression_bind_eye(void *eye_anim);

/**
 * @brief De-initialize Embodied Expression Engine
 */
void phoenix_expression_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_EXPRESSION_H */
