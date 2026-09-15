/**
 * @file hal_driver_mock.h
 * @brief Phoenix HoloDesk-S1 Mock HAL Driver (for Host Simulation & Unit Testing)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef HAL_DRIVER_MOCK_H
#define HAL_DRIVER_MOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../hal_types.h"
#include "../hal_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const hal_driver_t g_hal_driver_mock;

/* ========================================================================= */
/* Mock 驱动测试注入与观测接口 (供单元测试直接控制与校验)                     */
/* ========================================================================= */

/** 模拟注入物理桌面敲击事件 */
void hal_mock_inject_tap(hal_tap_intensity_t intensity);

/** 模拟设定环境光照度 (Lux) */
void hal_mock_set_light(uint32_t lux);

/** 模拟设定电池电量与充电状态 */
void hal_mock_set_battery(uint8_t percentage, bool is_charging);

/** 模拟设定硬件温度 */
void hal_mock_set_temperature(float temp_c);

/** 模拟注入一帧麦克风拾音数据 */
void hal_mock_inject_pcm_frame(const int16_t *samples, size_t count, bool is_speech);

/** 获取最近一次播放的音效类型 */
int hal_mock_get_last_sound(void);

/** 获取最近一次触发的马达模式 */
int hal_mock_get_last_haptic_pattern(void);

/** 获取最近一次启动的应用名称 */
const char* hal_mock_get_last_launched_app(void);

/** 重置 Mock 驱动内部测试状态 */
void hal_mock_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DRIVER_MOCK_H */
