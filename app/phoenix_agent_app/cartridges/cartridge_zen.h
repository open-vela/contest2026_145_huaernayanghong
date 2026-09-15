/**
 * @file cartridge_zen.h
 * @brief 极客禅意与赛博木鱼卡带 (Cyber Wooden Fish & Zen Cartridge)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef CARTRIDGE_ZEN_H
#define CARTRIDGE_ZEN_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#else
#  include "../core/cartridge.h"
#endif

/**
 * @brief 创建赛博木鱼卡带实例
 * @return 卡带结构体指针
 */
cartridge_t *cartridge_zen_create(void);

/**
 * @brief 注册赛博木鱼卡带到卡带管理器
 * @return 0 成功, 负数失败
 */
int cartridge_zen_register(void);

#ifdef __cplusplus
}
#endif

#endif /* CARTRIDGE_ZEN_H */
