/**
 * @file cartridge_clock.h
 * @brief 番茄专注时钟卡带 (Pomodoro Focus Cartridge)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef CARTRIDGE_CLOCK_H
#define CARTRIDGE_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#else
#  include "../core/cartridge.h"
#endif

/**
 * @brief 创建番茄专注时钟卡带实例
 * @return 卡带结构体指针
 */
cartridge_t *cartridge_clock_create(void);

/**
 * @brief 注册番茄专注时钟卡带到卡带管理器
 * @return 0 成功, 负数失败
 */
int cartridge_clock_register(void);

/**
 * @brief 触控或敲击快速启停/切换番茄专注
 * @return 0 成功
 */
int cartridge_clock_toggle(void);

/**
 * @brief 切换番茄时钟模式 (0: 专注25m, 1: 短休5m, 2: 长休15m)
 * @param mode 模式枚举值
 * @return 0 成功
 */
int cartridge_clock_set_mode(int mode);

#ifdef __cplusplus
}
#endif

#endif /* CARTRIDGE_CLOCK_H */
