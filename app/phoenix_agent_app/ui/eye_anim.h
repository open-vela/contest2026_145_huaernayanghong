/**
 * @file eye_anim.h
 * @brief Living Cyber-Eye Animation System for Phoenix HoloDesk-S1 (LVGL 9)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_EYE_ANIM_H
#define PHOENIX_EYE_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl/lvgl.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Eye emotional & expressive states
 */
typedef enum {
    PHOENIX_EYE_IDLE = 0,       /**< Normal breathing, occasional random blink & gaze shifts */
    PHOENIX_EYE_LISTENING,      /**< Wide awake, focused pupil, warm amber halo */
    PHOENIX_EYE_THINKING,       /**< Rotating cyber pulse ring, slightly looking up/aside */
    PHOENIX_EYE_HAPPY,          /**< Upward-curved crescent eye, golden glow (PR accepted/merit) */
    PHOENIX_EYE_ALERT,          /**< Rapid pulse, crimson red glow (CI failed/error) */
    PHOENIX_EYE_SLEEPY,         /**< Half-closed eyelids, slow breathing, dimmed cyan */
    PHOENIX_EYE_WINK            /**< Playful wink */
} phoenix_eye_emotion_t;

/**
 * @brief Eye geometry and animation context
 */
typedef struct {
    lv_obj_t *container;        /**< Outer container widget */
    lv_obj_t *outer_halo;       /**< Pulsing glow ring */
    lv_obj_t *iris;             /**< Main eye sphere / iris */
    lv_obj_t *pupil;            /**< Center dynamic pupil */
    lv_obj_t *highlight;        /**< Specular reflection highlight */
    lv_obj_t *upper_eyelid;     /**< Upper eyelid for blinking */
    lv_obj_t *lower_eyelid;     /**< Lower eyelid */
    
    lv_timer_t *anim_timer;     /**< Dynamic micro-motion & breath timer */
    lv_timer_t *blink_timer;    /**< Autonomous random blink timer */
    
    phoenix_eye_emotion_t emotion; /**< Current emotion */
    
    int16_t target_x;           /**< Target gaze X offset */
    int16_t target_y;           /**< Target gaze Y offset */
    int16_t current_x;          /**< Current pupil X offset */
    int16_t current_y;          /**< Current pupil Y offset */
    
    uint16_t base_size;         /**< Diameter of the eye sphere */
    uint32_t tick_count;        /**< Internal animation tick counter */
    bool is_blinking;           /**< Blinking in progress */
} phoenix_eye_t;

/**
 * @brief Create and initialize the cyber eye widget
 * @param parent Parent LVGL container
 * @param size Base diameter in pixels (e.g. 100~160px)
 * @return Pointer to allocated phoenix_eye_t
 */
phoenix_eye_t* phoenix_eye_create(lv_obj_t *parent, uint16_t size);

/**
 * @brief Set emotional state of the eye
 * @param eye Eye context
 * @param emotion Target emotion
 */
void phoenix_eye_set_emotion(phoenix_eye_t *eye, phoenix_eye_emotion_t emotion);

/**
 * @brief Smoothly move gaze towards specified direction
 * @param eye Eye context
 * @param offset_x X offset in range [-20, 20]
 * @param offset_y Y offset in range [-15, 15]
 */
void phoenix_eye_look_at(phoenix_eye_t *eye, int16_t offset_x, int16_t offset_y);

/**
 * @brief Trigger a manual blink animation
 * @param eye Eye context
 */
void phoenix_eye_trigger_blink(phoenix_eye_t *eye);

/**
 * @brief Destroy eye widget and free timers
 * @param eye Eye context
 */
void phoenix_eye_destroy(phoenix_eye_t *eye);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_EYE_ANIM_H */
