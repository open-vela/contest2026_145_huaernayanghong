/**
 * @file demo_hal.h
 * @brief M5Stack Tab5 硬件抽象层接口与数据结构定义
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef __DEMO_HAL_H
#define __DEMO_HAL_H

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
    float bus_voltage;      /* 总线电压 (V) */
    float shunt_voltage_mv; /* 分流电阻电压 (mV) */
    float current_ma;       /* 实时电流 (mA, 正为放电/负为充电，取决于采样方向) */
    float power_mw;         /* 瞬时功耗 (mW) */
    uint8_t battery_soc;    /* 估算电池剩余电量 (0 - 100%) */
    bool is_charging;       /* 是否正在充电 */
    bool is_qc_enabled;     /* QC 快充是否使能 */
    bool is_charge_enabled; /* 充电功能是否使能 */
} demo_power_data_t;

/* ========================================================================= */
/* 麦克风与音频状态数据结构                                                   */
/* ========================================================================= */

typedef struct {
    bool is_available;      /* ES7210 芯片是否正常应答 */
    bool is_muted;          /* 麦克风是否已被硬件静音/关闭 */
    uint32_t sample_rate;   /* 采样率 (如 48000 Hz) */
    uint8_t channels;       /* 通道数 (4通道: MIC-L, AEC, MIC-R, MIC-HP) */
    uint8_t gain;           /* 拾音模拟/数字增益 (0-255) */
    int16_t vu_level_left;  /* 左麦克风实时电平 (0 - 32767) */
    int16_t vu_level_right; /* 右麦克风实时电平 (0 - 32767) */
} demo_mic_status_t;

typedef struct {
    bool is_available;      /* ES8388 芯片是否正常应答 */
    bool is_amplifier_on;   /* NS4150B 扬声器功放是否开启 */
    bool is_muted;          /* 扬声器是否静音 */
    uint8_t volume;         /* 当前音量百分比 (0 - 100%) */
} demo_speaker_status_t;

/* ========================================================================= */
/* 六轴 IMU 与 RTC 数据结构                                                 */
/* ========================================================================= */

typedef struct {
    bool is_available;
    float accel_x;          /* 加速度 X (g) */
    float accel_y;          /* 加速度 Y (g) */
    float accel_z;          /* 加速度 Z (g) */
    float gyro_x;           /* 角速度 X (dps) */
    float gyro_y;           /* 角速度 Y (dps) */
    float gyro_z;           /* 角速度 Z (dps) */
} demo_imu_data_t;

typedef struct {
    bool is_available;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} demo_rtc_time_t;

/* ========================================================================= */
/* 外设供电与连接状态                                                         */
/* ========================================================================= */

typedef struct {
    bool usb_5v_enabled;    /* USB-A 5V VBUS 是否供电 */
    bool ext_5v_enabled;    /* Grove / M5-Bus EXT 5V 是否供电 */
    bool wlan_pwr_enabled;  /* ESP32-C6 Wi-Fi 芯片是否供电 */
    bool headphone_plugged; /* 是否检测到 3.5mm 耳机插入 */
    bool rf_external_ant;   /* 是否切换到外置天线 */
} demo_periph_status_t;

/* ========================================================================= */
/* HAL 统一导出函数                                                         */
/* ========================================================================= */

/**
 * @brief 初始化硬件抽象层，探测全部片上与板载外设
 * @return 0: 成功, <0: 失败
 */
int demo_hal_init(void);

/* 电源与电量 */
int demo_hal_power_read(demo_power_data_t *out_data);
int demo_hal_power_set_charge_enable(bool enable);
int demo_hal_power_set_qc_enable(bool enable);

/* 麦克风控制 */
int demo_hal_mic_get_status(demo_mic_status_t *out_status);
int demo_hal_mic_set_mute(bool mute);
int demo_hal_mic_read_vu(int16_t *out_left, int16_t *out_right);

/* 扬声器音量与发声 */
int demo_hal_speaker_get_status(demo_speaker_status_t *out_status);
int demo_hal_speaker_set_volume(uint8_t volume_pct);
int demo_hal_speaker_set_amplifier(bool enable);
int demo_hal_speaker_set_mute(bool mute);
int demo_hal_speaker_play_tone(uint32_t freq_hz, uint32_t duration_ms);

/* 屏幕亮度与显示 */
int demo_hal_display_get_brightness(uint8_t *out_brightness);
int demo_hal_display_set_brightness(uint8_t brightness);
int demo_hal_display_draw_pattern(int pattern_type);

/* 外设供电与 IO 扩展 */
int demo_hal_periph_get_status(demo_periph_status_t *out_status);
int demo_hal_periph_set_usb5v(bool enable);
int demo_hal_periph_set_ext5v(bool enable);
int demo_hal_periph_set_wlan_pwr(bool enable);
int demo_hal_periph_set_antenna(bool external);

/* 传感器 */
int demo_hal_imu_read(demo_imu_data_t *out_imu);
int demo_hal_rtc_read(demo_rtc_time_t *out_time);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_HAL_H */
