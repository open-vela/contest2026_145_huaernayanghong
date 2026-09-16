/**
 * @file hal_manager.h
 * @brief Phoenix HoloDesk-S1 HAL Core Manager & Lifecycle
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef HAL_MANAGER_H
#define HAL_MANAGER_H

#include "hal_driver.h"
#include "hal_sensor.h"
#include "hal_actuator.h"
#include "hal_system.h"
#include "hal_sdcard.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *sound_data_root;   /**< 音频资源路径 (如 "/data/sounds") */
    const hal_driver_t *custom_driver; /**< 自定义驱动，传 NULL 则使用平台默认驱动 */
} hal_config_t;

/**
 * @brief 初始化整个 HAL 硬件抽象层
 * @param config 配置选项，传 NULL 使用默认配置
 * @return 0 成功, 负值失败
 */
int hal_init(const hal_config_t *config);

/**
 * @brief 释放并逆序析构整个 HAL 硬件抽象层
 */
void hal_deinit(void);

/**
 * @brief 查询 HAL 是否已经初始化完成
 */
bool hal_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_MANAGER_H */
