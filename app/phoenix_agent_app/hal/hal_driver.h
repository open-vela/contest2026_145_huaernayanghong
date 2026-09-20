/**
 * @file hal_driver.h
 * @brief Phoenix HoloDesk-S1 HAL Driver Operations (VTable) Interface
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef HAL_DRIVER_H
#define HAL_DRIVER_H

#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 传感器驱动操作虚表 */
typedef struct {
    int (*init)(void);
    int (*deinit)(void);
    bool (*poll_tap)(hal_tap_event_t *out_tap);
    int (*read_light)(hal_light_data_t *out_light);
    int (*read_battery)(hal_battery_data_t *out_battery);
    int (*read_env)(hal_env_data_t *out_env);
} hal_sensor_ops_t;

/** 执行器驱动操作虚表 */
typedef struct {
    int (*init)(const char *sound_root);
    int (*deinit)(void);
    int (*play_sound)(hal_sound_type_t sound_type);
    void (*set_volume)(uint8_t volume_pct);
    int (*trigger_haptic)(hal_haptic_pattern_t pattern);
    int (*set_led)(hal_led_mode_t mode, uint32_t rgb, uint8_t brightness);
    int (*blink_led)(int count, uint32_t interval_ms);
} hal_actuator_ops_t;

/** 系统服务驱动操作虚表 */
typedef struct {
    int (*init)(void);
    int (*deinit)(void);
    int (*get_telemetry)(hal_system_telemetry_t *out_telem);
    int (*launch_app)(const char *app_package_or_alias);
    const char* (*get_storage_base_path)(void);
} hal_system_ops_t;

/** 音频输入驱动操作虚表 */
typedef struct {
    int (*init)(uint32_t sample_rate, uint8_t channels);
    int (*deinit)(void);
    int (*start_stream)(void);
    int (*read_frame)(hal_audio_pcm_frame_t *frame_out, uint32_t timeout_ms);
    int (*stop_stream)(void);
} hal_audio_in_ops_t;

/** 完整 HAL 驱动提供者定义 */
typedef struct {
    const char *driver_name;
    hal_sensor_ops_t sensor_ops;
    hal_actuator_ops_t actuator_ops;
    hal_system_ops_t system_ops;
    hal_audio_in_ops_t audio_in_ops;
} hal_driver_t;

/** 注册并激活 HAL 硬件驱动 */
int hal_register_driver(const hal_driver_t *driver);

/** 获取当前激活的 HAL 驱动 */
const hal_driver_t* hal_get_active_driver(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DRIVER_H */
