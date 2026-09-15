/**
 * @file vela_hal.h
 * @brief 基于 OpenVela 标准子系统接口的硬件抽象层定义
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef __VELA_HAL_H
#define __VELA_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* 电源与电池监控数据结构                                                     */
/* ========================================================================= */

typedef struct {
    float bus_voltage_v;    /* 总线电压 (V) */
    float shunt_voltage_mv; /* 分流电压 (mV) */
    float current_ma;       /* 实时电流 (mA, 正为放电/负为充电) */
    float power_mw;         /* 瞬时功耗 (mW) */
    uint8_t battery_soc;    /* 估算电池剩余容量百分比 (0 - 100%) */
    bool is_charging;       /* 是否正在充电 (通过 OpenVela 电池子系统检测) */
    bool is_online;         /* 外部供电适配器是否在线 */
    char status_str[32];    /* 状态字符串描述 (Charging/Discharging/Full) */
} vela_power_data_t;

/* ========================================================================= */
/* 麦克风与音频状态数据结构                                                   */
/* ========================================================================= */

typedef struct {
    bool is_available;      /* OpenVela 音频输入设备是否就绪 */
    bool is_muted;          /* 麦克风当前是否关闭/静音 */
    uint32_t sample_rate;   /* 采样率 (Hz, 例如 16000 或 48000) */
    uint8_t channels;       /* 通道数 (1: 单声道, 2: 立体声, 4: 阵列) */
    uint8_t gain;           /* 拾音增益 (0 - 100) */
    int16_t vu_level;       /* 当前麦克风音量峰值 (0 - 32767) */
} vela_mic_status_t;

typedef struct {
    bool is_available;      /* OpenVela 音频输出设备是否就绪 */
    bool is_muted;          /* 扬声器是否静音 */
    uint8_t volume;         /* 当前音量百分比 (0 - 100%) */
    bool amplifier_enabled; /* 功放硬件使能状态 */
} vela_speaker_status_t;

/* ========================================================================= */
/* 传感器与系统时钟数据结构                                                   */
/* ========================================================================= */

typedef struct {
    bool is_available;
    float accel_x;          /* 加速度 X (m/s^2 或 g) */
    float accel_y;          /* 加速度 Y (m/s^2 或 g) */
    float accel_z;          /* 加速度 Z (m/s^2 或 g) */
    float gyro_x;           /* 角速度 X (rad/s 或 dps) */
    float gyro_y;           /* 角速度 Y (rad/s 或 dps) */
    float gyro_z;           /* 角速度 Z (rad/s 或 dps) */
} vela_imu_data_t;

typedef struct {
    bool is_available;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} vela_rtc_time_t;

/* ========================================================================= */
/* OpenVela HAL 导出函数声明                                                 */
/* ========================================================================= */

/**
 * @brief 初始化 OpenVela 硬件抽象子系统
 */
int vela_hal_init(void);

/* 电源与电池管理 */
int vela_hal_power_get_data(vela_power_data_t *out_data);
int vela_hal_power_set_charge_enable(bool enable);

/* 麦克风控制与监测 */
int vela_hal_mic_get_status(vela_mic_status_t *out_status);
int vela_hal_mic_set_mute(bool mute);
int vela_hal_mic_read_vu(int16_t *out_level);

/* 扬声器音量与音频播放 */
int vela_hal_speaker_get_status(vela_speaker_status_t *out_status);
int vela_hal_speaker_set_volume(uint8_t volume_pct);
int vela_hal_speaker_set_mute(bool mute);
int vela_hal_speaker_set_amplifier(bool enable);
int vela_hal_speaker_play_tone(uint32_t freq_hz, uint32_t duration_ms);

/* 屏幕亮度与显示测试 */
int vela_hal_display_get_brightness(uint8_t *out_brightness);
int vela_hal_display_set_brightness(uint8_t brightness);
int vela_hal_display_test_pattern(int pattern_id);

/* 传感器与系统 */
int vela_hal_sensors_read_imu(vela_imu_data_t *out_imu);
int vela_hal_sensors_read_rtc(vela_rtc_time_t *out_time);
int vela_hal_periph_set_5v_rail(bool usb_5v, bool ext_5v);

#ifdef __cplusplus
}
#endif

#endif /* __VELA_HAL_H */
