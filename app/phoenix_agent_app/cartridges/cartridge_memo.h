/**
 * @file cartridge_memo.h
 * @brief 灵感外脑速记胶囊卡带 (Mind Memo & Idea Capsule Cartridge)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef CARTRIDGE_MEMO_H
#define CARTRIDGE_MEMO_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#else
#  include "../core/cartridge.h"
#endif

/**
 * @brief 创建并获取灵感外脑卡带实例
 * @return 卡带结构体指针
 */
cartridge_t *cartridge_memo_create(void);

/**
 * @brief 注册灵感外脑卡带到卡带管理器
 * @return 0 成功, 负数失败
 */
int cartridge_memo_register(void);

/**
 * @brief 手动推入一条新灵感速记
 * @param content 灵感内容
 * @return 0 成功
 */
int cartridge_memo_add_entry(const char *content);

#ifdef __cplusplus
}
#endif

#endif /* CARTRIDGE_MEMO_H */
