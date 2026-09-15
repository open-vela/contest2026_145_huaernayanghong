/**
 * @file cartridge_clock.h
 * @brief 拟物机械翻页钟与专注时钟卡带 (Mechanical Flip Clock & Pomodoro Cartridge)
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
 * @brief 创建拟物翻页钟卡带实例
 * @return 卡带结构体指针
 */
cartridge_t *cartridge_clock_create(void);

/**
 * @brief 注册拟物翻页钟卡带到卡带管理器
 * @return 0 成功, 负数失败
 */
int cartridge_clock_register(void);

#ifdef __cplusplus
}
#endif

#endif /* CARTRIDGE_CLOCK_H */
