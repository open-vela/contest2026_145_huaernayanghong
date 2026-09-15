/**
 * @file hal_types.h
 * @brief Phoenix HoloDesk-S1 Hardware Abstraction Layer Data Types
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef HAL_TYPES_H
#define HAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* 1. 传感器数据类型 (Sensors)                                              */
/* ========================================================================= */

/** 敲击/振动强度枚举 */
typedef enum {
    HAL_TAP_LIGHT = 1,       /**< 轻度轻触/微弱振动 */
    HAL_TAP_NORMAL = 2,      /**< 常规桌面敲击/木鱼敲击 */
    HAL_TAP_HEAVY = 3        /**< 猛烈撞击/强烈晃动 */
} hal_tap_intensity_t;

/** 桌面敲击事件数据 */
typedef struct {
    hal_tap_intensity_t intensity; /**< 振动强度 */
    uint64_t timestamp_ms;         /**< 发生时间戳 (ms) */
} hal_tap_event_t;

/** 环境光照传感器数据 */
typedef struct {
    uint32_t lux;                  /**< 环境光照度 (Lux) */
    bool is_dark_environment;      /**< 是否处于暗室/夜间弱光环境 (< 30 Lux) */
    bool is_direct_sunlight;       /**< 是否处于强光/阳光直射环境 (> 1000 Lux) */
} hal_light_data_t;

/** 电池电源监控数据 */
typedef struct {
    uint8_t percentage;            /**< 剩余电量百分比 0-100 */
    bool is_charging;              /**< 是否处于充电状态 */
    bool is_low_power;             /**< 是否处于低电量报警状态 (< 15%) */
    uint16_t voltage_mv;           /**< 电池当前端电压 (mV, 比如 3700-4200) */
} hal_battery_data_t;

/* ========================================================================= */
/* 2. 执行器控制类型 (Actuators)                                            */
/* ========================================================================= */

/** 音效类型枚举 (兼容原 phoenix_sound_type_t) */
typedef enum {
    HAL_SOUND_WOODEN_FISH = 0,    /**< 赛博木鱼敲击声 (功德+1) */
    HAL_SOUND_WAKEUP = 1,         /**< 灵眸唤醒提示音 */
    HAL_SOUND_CELEBRATE = 2,      /**< 目标达成/里程碑庆祝音 */
    HAL_SOUND_ALERT = 3,          /**< 异常警戒/低电提示音 */
    HAL_SOUND_CLICK = 4           /**< 物理微触按键音 */
} hal_sound_type_t;

/** 触觉马达振动模式 */
typedef enum {
    HAL_HAPTIC_CLICK = 0,         /**< 瞬态短振 (50ms，木鱼敲击反馈) */
    HAL_HAPTIC_DOUBLE_CLICK = 1,  /**< 双击短振 (100ms + 50ms) */
    HAL_HAPTIC_PULSE = 2,         /**< 柔和呼吸微振 (专注结束/番茄钟提醒) */
    HAL_HAPTIC_ALERT_BURST = 3    /**< 强力警戒急促振动 (系统异常/超时警告) */
} hal_haptic_pattern_t;

/** 灵眸外环 RGB/LED 控制模式 */
typedef enum {
    HAL_LED_OFF = 0,
    HAL_LED_SOLID = 1,
    HAL_LED_BREATHING = 2,
    HAL_LED_PULSE = 3
} hal_led_mode_t;

/* ========================================================================= */
/* 3. 系统遥测与服务类型 (System Telemetry & App Launcher)                   */
/* ========================================================================= */

/** 系统底层遥测健康数据 */
typedef struct {
    char board_model[64];         /**< 开发板硬件型号 (如 Gemini-S1 Allwinner R528) */
    char os_version[32];          /**< 操作系统与内核版本 (如 OpenVela v1.0.0) */
    float cpu_temperature_c;      /**< 核心温度摄氏度 */
    uint32_t cpu_freq_mhz;        /**< CPU 主频 (MHz) */
    uint32_t mem_total_kb;        /**< 总物理内存 (KB) */
    uint32_t mem_free_kb;         /**< 空闲物理内存 (KB) */
    uint32_t mem_used_pct;        /**< 内存使用百分比 (0-100) */
    uint64_t uptime_seconds;      /**< 系统开机运行时间 (秒) */
} hal_system_telemetry_t;
/* ========================================================================= */
/* 4. 音频输入采集与语音类型 (Audio In & Voice Streaming)                   */
/* ========================================================================= */

/** 音频输入 PCM 帧数据 */
typedef struct {
    uint32_t sample_rate;         /**< 采样率 (如 16000 Hz) */
    uint8_t  channels;            /**< 通道数 (1: 单声道, 2: 立体声) */
    uint8_t  bit_depth;           /**< 位宽 (如 16 bit) */
    size_t   frame_length;        /**< 采样点数量 (如 160点 = 10ms) */
    const int16_t *pcm_data;      /**< PCM 采样缓冲区指针 */
    bool     is_speech;           /**< VAD 语音活动标记 */
} hal_audio_pcm_frame_t;

#ifdef __cplusplus
}
#endif

#endif /* HAL_TYPES_H */
