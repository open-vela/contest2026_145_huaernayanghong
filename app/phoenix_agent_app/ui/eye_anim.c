/**
 * @file eye_anim.c
 * @brief Living Cyber-Eye Animation System Implementation (LVGL 9)
 * @author OpenVela Contest 2026 Team 145
 */

#include "eye_anim.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define COLOR_BG_DARK          lv_color_hex(0x0a0e17)
#define COLOR_CYAN_GLOW        lv_color_hex(0x00e5ff)
#define COLOR_AMBER_GLOW       lv_color_hex(0xff9800)
#define COLOR_PURPLE_GLOW      lv_color_hex(0xd500f9)
#define COLOR_GOLD_GLOW        lv_color_hex(0xffc107)
#define COLOR_CRIMSON_GLOW     lv_color_hex(0xff1744)
#define COLOR_INDIGO_GLOW      lv_color_hex(0x3949ab)
#define COLOR_PUPIL_CORE       lv_color_hex(0x05070c)
#define COLOR_HIGHLIGHT        lv_color_hex(0xffffff)

static void blink_anim_cb(void *var, int32_t val)
{
    phoenix_eye_t *eye = (phoenix_eye_t *)var;
    if (!eye || !eye->upper_eyelid || !eye->lower_eyelid) return;
    
    // val goes from 0 (open) to 100 (fully closed)
    int32_t height = (eye->base_size * val) / 200;
    lv_obj_set_height(eye->upper_eyelid, height);
    lv_obj_set_height(eye->lower_eyelid, height);
}

static void blink_anim_ready_cb(lv_anim_t *a)
{
    phoenix_eye_t *eye = (phoenix_eye_t *)a->var;
    if (eye) {
        eye->is_blinking = false;
    }
}

void phoenix_eye_trigger_blink(phoenix_eye_t *eye)
{
    if (!eye || eye->is_blinking) return;
    if (eye->emotion == PHOENIX_EYE_SLEEPY || eye->emotion == PHOENIX_EYE_HAPPY) return;

    eye->is_blinking = true;
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, eye);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_duration(&a, 120);
    lv_anim_set_playback_duration(&a, 140);
    lv_anim_set_exec_cb(&a, blink_anim_cb);
    lv_anim_set_completed_cb(&a, blink_anim_ready_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void autonomous_blink_timer_cb(lv_timer_t *timer)
{
    phoenix_eye_t *eye = (phoenix_eye_t *)timer->user_data;
    if (!eye) return;

    // Trigger random blink
    phoenix_eye_trigger_blink(eye);

    // Random gaze shift (micro-saccades) in idle state
    if (eye->emotion == PHOENIX_EYE_IDLE && !eye->is_blinking) {
        int r = rand() % 100;
        if (r < 40) {
            // Slight look left/right/center
            int16_t nx = (rand() % 17) - 8;
            int16_t ny = (rand() % 11) - 5;
            phoenix_eye_look_at(eye, nx, ny);
        } else if (r < 70) {
            phoenix_eye_look_at(eye, 0, 0); // back to center
        }
    }

    // Set next random blink period (2.5s ~ 6.0s)
    uint32_t next_period = 2500 + (rand() % 3500);
    lv_timer_set_period(timer, next_period);
}

static void eye_render_tick_cb(lv_timer_t *timer)
{
    phoenix_eye_t *eye = (phoenix_eye_t *)timer->user_data;
    if (!eye) return;

    eye->tick_count++;

    // 1. Smooth gaze interpolation (lerp towards target)
    if (eye->current_x != eye->target_x || eye->current_y != eye->target_y) {
        eye->current_x += (eye->target_x - eye->current_x) * 0.35f;
        eye->current_y += (eye->target_y - eye->current_y) * 0.35f;
        if (abs(eye->target_x - eye->current_x) < 2) eye->current_x = eye->target_x;
        if (abs(eye->target_y - eye->current_y) < 2) eye->current_y = eye->target_y;
        
        lv_obj_align(eye->pupil, LV_ALIGN_CENTER, eye->current_x, eye->current_y);
        lv_obj_align(eye->highlight, LV_ALIGN_CENTER, eye->current_x + 8, eye->current_y - 8);
    }

    // 2. Halo breathing animation
    float phase = (eye->tick_count % 60) * (3.14159f / 30.0f);
    int32_t breathe_offset = (int32_t)(sinf(phase) * 6.0f);
    
    if (eye->emotion == PHOENIX_EYE_THINKING) {
        // Faster pulsing in thinking state
        phase = (eye->tick_count % 30) * (3.14159f / 15.0f);
        breathe_offset = (int32_t)(sinf(phase) * 10.0f);
    } else if (eye->emotion == PHOENIX_EYE_ALERT) {
        // Sharp heart-beat pulse
        phase = (eye->tick_count % 20) * (3.14159f / 10.0f);
        breathe_offset = (int32_t)(sinf(phase) * 8.0f);
    }

    int32_t halo_size = eye->base_size + 16 + breathe_offset;
    lv_obj_set_size(eye->outer_halo, halo_size, halo_size);
    lv_obj_set_style_radius(eye->outer_halo, halo_size / 2, LV_PART_MAIN);
}

phoenix_eye_t* phoenix_eye_create(lv_obj_t *parent, uint16_t size)
{
    if (!parent) return NULL;
    if (size < 60) size = 100;

    phoenix_eye_t *eye = (phoenix_eye_t *)malloc(sizeof(phoenix_eye_t));
    if (!eye) return NULL;
    memset(eye, 0, sizeof(phoenix_eye_t));

    eye->base_size = size;
    eye->emotion = PHOENIX_EYE_IDLE;

    // 1. Master Eye Container
    eye->container = lv_obj_create(parent);
    lv_obj_set_size(eye->container, size + 36, size + 36);
    lv_obj_set_style_bg_opa(eye->container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_opa(eye->container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(eye->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(eye->container);

    // 2. Outer Halo (Luminous Pulsing Aura - Lightweight without heavy software shadow)
    eye->outer_halo = lv_obj_create(eye->container);
    lv_obj_set_size(eye->outer_halo, size + 16, size + 16);
    lv_obj_center(eye->outer_halo);
    lv_obj_set_style_radius(eye->outer_halo, (size + 16) / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(eye->outer_halo, COLOR_CYAN_GLOW, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(eye->outer_halo, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_border_width(eye->outer_halo, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(eye->outer_halo, COLOR_CYAN_GLOW, LV_PART_MAIN);
    lv_obj_set_style_border_opa(eye->outer_halo, LV_OPA_60, LV_PART_MAIN);
    lv_obj_remove_flag(eye->outer_halo, LV_OBJ_FLAG_SCROLLABLE);

    // 3. Iris (Main Vibrant Sphere)
    eye->iris = lv_obj_create(eye->container);
    lv_obj_set_size(eye->iris, size, size);
    lv_obj_center(eye->iris);
    lv_obj_set_style_radius(eye->iris, size / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(eye->iris, COLOR_CYAN_GLOW, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(eye->iris, lv_color_hex(0x005577), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(eye->iris, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_width(eye->iris, 0, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(eye->iris, true, LV_PART_MAIN);
    lv_obj_remove_flag(eye->iris, LV_OBJ_FLAG_SCROLLABLE);

    // 4. Pupil (Deep Cyber-Core)
    uint16_t pupil_size = size * 0.48f;
    eye->pupil = lv_obj_create(eye->iris);
    lv_obj_set_size(eye->pupil, pupil_size, pupil_size);
    lv_obj_center(eye->pupil);
    lv_obj_set_style_radius(eye->pupil, pupil_size / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(eye->pupil, COLOR_PUPIL_CORE, LV_PART_MAIN);
    lv_obj_set_style_border_color(eye->pupil, COLOR_CYAN_GLOW, LV_PART_MAIN);
    lv_obj_set_style_border_width(eye->pupil, 1, LV_PART_MAIN);
    lv_obj_remove_flag(eye->pupil, LV_OBJ_FLAG_SCROLLABLE);

    // 5. Highlight (Specular Glistening Point)
    eye->highlight = lv_obj_create(eye->iris);
    lv_obj_set_size(eye->highlight, size * 0.14f, size * 0.14f);
    lv_obj_align(eye->highlight, LV_ALIGN_CENTER, 8, -8);
    lv_obj_set_style_radius(eye->highlight, (size * 0.14f) / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(eye->highlight, COLOR_HIGHLIGHT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(eye->highlight, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(eye->highlight, 0, LV_PART_MAIN);
    lv_obj_remove_flag(eye->highlight, LV_OBJ_FLAG_SCROLLABLE);

    // 6. Blinking Eyelids (Upper & Lower)
    eye->upper_eyelid = lv_obj_create(eye->iris);
    lv_obj_set_size(eye->upper_eyelid, size, 0);
    lv_obj_align(eye->upper_eyelid, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(eye->upper_eyelid, COLOR_BG_DARK, LV_PART_MAIN);
    lv_obj_set_style_border_width(eye->upper_eyelid, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(eye->upper_eyelid, 0, LV_PART_MAIN);
    lv_obj_remove_flag(eye->upper_eyelid, LV_OBJ_FLAG_SCROLLABLE);

    eye->lower_eyelid = lv_obj_create(eye->iris);
    lv_obj_set_size(eye->lower_eyelid, size, 0);
    lv_obj_align(eye->lower_eyelid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(eye->lower_eyelid, COLOR_BG_DARK, LV_PART_MAIN);
    lv_obj_set_style_border_width(eye->lower_eyelid, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(eye->lower_eyelid, 0, LV_PART_MAIN);
    lv_obj_remove_flag(eye->lower_eyelid, LV_OBJ_FLAG_SCROLLABLE);

    // 7. Dynamic Animation Timers
    eye->anim_timer = lv_timer_create(eye_render_tick_cb, 50, eye); // ~20 FPS (smooth & low CPU)
    eye->blink_timer = lv_timer_create(autonomous_blink_timer_cb, 3500, eye);

    return eye;
}

void phoenix_eye_set_emotion(phoenix_eye_t *eye, phoenix_eye_emotion_t emotion)
{
    if (!eye) return;
    eye->emotion = emotion;

    lv_color_t main_color;
    lv_color_t grad_color;
    uint16_t pupil_size = eye->base_size * 0.48f;

    switch (emotion) {
        case PHOENIX_EYE_LISTENING:
            main_color = COLOR_AMBER_GLOW;
            grad_color = lv_color_hex(0x663d00);
            pupil_size = eye->base_size * 0.54f; // Dilated pupil in focus
            phoenix_eye_look_at(eye, 0, 0);
            lv_obj_set_height(eye->upper_eyelid, 0);
            lv_obj_set_height(eye->lower_eyelid, 0);
            break;

        case PHOENIX_EYE_THINKING:
            main_color = COLOR_PURPLE_GLOW;
            grad_color = lv_color_hex(0x4a0072);
            pupil_size = eye->base_size * 0.40f; // Constricted pupil
            phoenix_eye_look_at(eye, 8, -6);    // Contemplative gaze upwards
            lv_obj_set_height(eye->upper_eyelid, 0);
            lv_obj_set_height(eye->lower_eyelid, 0);
            break;

        case PHOENIX_EYE_HAPPY:
            main_color = COLOR_GOLD_GLOW;
            grad_color = lv_color_hex(0x7a4b00);
            pupil_size = eye->base_size * 0.50f;
            phoenix_eye_look_at(eye, 0, 0);
            // Happy crescent shape (eyelids curved upward)
            lv_obj_set_height(eye->lower_eyelid, eye->base_size * 0.35f);
            lv_obj_set_height(eye->upper_eyelid, eye->base_size * 0.10f);
            break;

        case PHOENIX_EYE_ALERT:
            main_color = COLOR_CRIMSON_GLOW;
            grad_color = lv_color_hex(0x5c0000);
            pupil_size = eye->base_size * 0.58f; // Wide open
            phoenix_eye_look_at(eye, 0, 0);
            lv_obj_set_height(eye->upper_eyelid, 0);
            lv_obj_set_height(eye->lower_eyelid, 0);
            break;

        case PHOENIX_EYE_SLEEPY:
            main_color = COLOR_INDIGO_GLOW;
            grad_color = lv_color_hex(0x1a237e);
            pupil_size = eye->base_size * 0.38f;
            phoenix_eye_look_at(eye, 0, 4);
            // Half closed eyelids
            lv_obj_set_height(eye->upper_eyelid, eye->base_size * 0.40f);
            lv_obj_set_height(eye->lower_eyelid, eye->base_size * 0.15f);
            break;

        case PHOENIX_EYE_IDLE:
        default:
            main_color = COLOR_CYAN_GLOW;
            grad_color = lv_color_hex(0x005577);
            pupil_size = eye->base_size * 0.48f;
            lv_obj_set_height(eye->upper_eyelid, 0);
            lv_obj_set_height(eye->lower_eyelid, 0);
            break;
    }

    // Apply color palette updates
    lv_obj_set_style_bg_color(eye->outer_halo, main_color, LV_PART_MAIN);
    lv_obj_set_style_border_color(eye->outer_halo, main_color, LV_PART_MAIN);

    lv_obj_set_style_bg_color(eye->iris, main_color, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(eye->iris, grad_color, LV_PART_MAIN);
    lv_obj_set_style_border_color(eye->pupil, main_color, LV_PART_MAIN);

    // Apply pupil size
    lv_obj_set_size(eye->pupil, pupil_size, pupil_size);
    lv_obj_set_style_radius(eye->pupil, pupil_size / 2, LV_PART_MAIN);
}

void phoenix_eye_look_at(phoenix_eye_t *eye, int16_t offset_x, int16_t offset_y)
{
    if (!eye) return;
    
    // Clamp to realistic range
    int16_t max_x = eye->base_size * 0.20f;
    int16_t max_y = eye->base_size * 0.15f;

    if (offset_x > max_x) offset_x = max_x;
    if (offset_x < -max_x) offset_x = -max_x;
    if (offset_y > max_y) offset_y = max_y;
    if (offset_y < -max_y) offset_y = -max_y;

    eye->target_x = offset_x;
    eye->target_y = offset_y;
}

void phoenix_eye_destroy(phoenix_eye_t *eye)
{
    if (!eye) return;
    
    /* 立即清理并终止所有以 eye 为宿主的活跃 LVGL 动画，杜绝野指针回调导致内存崩溃 */
    lv_anim_delete(eye, NULL);

    if (eye->anim_timer) {
        lv_timer_delete(eye->anim_timer);
        eye->anim_timer = NULL;
    }
    if (eye->blink_timer) {
        lv_timer_delete(eye->blink_timer);
        eye->blink_timer = NULL;
    }
    if (eye->container) {
        lv_obj_del(eye->container);
        eye->container = NULL;
    }
    free(eye);
}
