/**
 * @file perception.h
 * @brief Phoenix HoloDesk-S1 Embodied Perception Engine
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_PERCEPTION_H
#define PHOENIX_PERCEPTION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t poll_interval_ms;   /**< 传感器轮询扫描周期 (默认 50ms) */
    bool auto_bridge_to_event_bus; /**< 是否自动将感知信号转化为事件总线广播 (默认 true) */
    bool enable_tap_to_wooden_fish; /**< 是否将物理敲击桌面自动联动为木鱼功德 (默认 true) */
} phoenix_perception_config_t;

/**
 * @brief 初始化具身感知引擎
 * @param config 配置选项，传 NULL 使用默认配置
 * @return 0 成功, 负值失败
 */
int phoenix_perception_init(const phoenix_perception_config_t *config);

/**
 * @brief 周期性单步执行感知采样、滤波、去抖与事件提升
 * @note 可在 libuv 定时器或单测循环中主动调用
 */
void phoenix_perception_step(void);

/**
 * @brief 析构感知引擎
 */
void phoenix_perception_deinit(void);

/**
 * @brief 查询感知引擎是否处于运行状态
 */
bool phoenix_perception_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_PERCEPTION_H */
