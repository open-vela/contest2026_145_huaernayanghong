/**
 * @file hal_sensor.h
 * @brief Phoenix HoloDesk-S1 Sensor HAL Public Interface
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef HAL_SENSOR_H
#define HAL_SENSOR_H

#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化传感器子系统 */
int hal_sensor_init(void);

/** 析构传感器子系统 */
int hal_sensor_deinit(void);

/** 轮询检测桌面敲击/振动事件 (非阻塞) */
bool hal_sensor_poll_tap(hal_tap_event_t *out_tap);

/** 获取当前环境光线照度及环境判定 */
int hal_sensor_read_light(hal_light_data_t *out_light);

/** 获取当前系统电池/电源状态 */
int hal_sensor_read_battery(hal_battery_data_t *out_battery);

/** 获取当前板载环境温湿度数据 (板载 SHTC3 等器件) */
int hal_sensor_read_env(hal_env_data_t *out_env);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SENSOR_H */
