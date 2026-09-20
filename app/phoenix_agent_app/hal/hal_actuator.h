/**
 * @file hal_actuator.h
 * @brief Phoenix HoloDesk-S1 Actuator HAL Public Interface
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef HAL_ACTUATOR_H
#define HAL_ACTUATOR_H

#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化执行器子系统 (包含音频、触觉与 LED) */
int hal_actuator_init(const char *sound_data_root);

/** 析构执行器子系统 */
int hal_actuator_deinit(void);

/** 播放物理音效 */
int hal_actuator_play_sound(hal_sound_type_t sound_type);

/** 设置全局主音量 (0-100) */
void hal_actuator_set_volume(uint8_t volume_percent);

/** 触发触觉马达物理振动反馈 */
int hal_actuator_trigger_haptic(hal_haptic_pattern_t pattern);

/** 设置灵眸外环 RGB/LED 状态 */
int hal_actuator_set_led(hal_led_mode_t mode, uint32_t rgb, uint8_t brightness);

/** 控制板载物理 LED 指示灯闪烁 */
int hal_actuator_blink_led(int count, uint32_t interval_ms);

/* ========================================================================= */
/* 兼容宏与别名 (与原 audio_ctl.h 平滑兼容)                           */
/* ========================================================================= */
typedef hal_sound_type_t phoenix_sound_type_t;

#define PHOENIX_SOUND_WOODEN_FISH   HAL_SOUND_WOODEN_FISH
#define PHOENIX_SOUND_WAKEUP        HAL_SOUND_WAKEUP
#define PHOENIX_SOUND_CELEBRATE     HAL_SOUND_CELEBRATE
#define PHOENIX_SOUND_ALERT         HAL_SOUND_ALERT
#define PHOENIX_SOUND_CLICK         HAL_SOUND_CLICK

static inline int phoenix_audio_init(const char *root) {
    return hal_actuator_init(root);
}
static inline void phoenix_audio_deinit(void) {
    hal_actuator_deinit();
}
static inline void phoenix_audio_play(phoenix_sound_type_t st) {
    hal_actuator_play_sound(st);
}
static inline void phoenix_audio_set_volume(uint8_t vol) {
    hal_actuator_set_volume(vol);
}

#ifdef __cplusplus
}
#endif

#endif /* HAL_ACTUATOR_H */
