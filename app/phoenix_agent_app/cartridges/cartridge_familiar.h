/**
 * @file cartridge_familiar.h
 * @brief 桌面萌宠使魔卡带 (Desktop Familiar Cyber-Pet Cartridge)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef CARTRIDGE_FAMILIAR_H
#define CARTRIDGE_FAMILIAR_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#else
#  include "../core/cartridge.h"
#endif

/**
 * @brief 创建并获取使魔卡带实例
 * @return 卡带结构体指针
 */
cartridge_t *cartridge_familiar_create(void);

/**
 * @brief 注册萌宠使魔卡带到卡带管理器
 * @return 0 成功, 负数失败
 */
int cartridge_familiar_register(void);

#ifdef __cplusplus
}
#endif

#endif /* CARTRIDGE_FAMILIAR_H */
