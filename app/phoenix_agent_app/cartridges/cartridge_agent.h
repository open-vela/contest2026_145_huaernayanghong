/**
 * @file cartridge_agent.h
 * @brief 主动式 Agent 语音交互卡带 (Proactive Agent Voice Interaction Cartridge)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef CARTRIDGE_AGENT_H
#define CARTRIDGE_AGENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 注册主动式 Agent 语音交互卡带至卡带管理器
 * @return 0 成功, 负数失败
 */
int cartridge_agent_register(void);

/**
 * @brief 触发一次语音交互 (支持从界面按钮、快捷键或测试调用)
 * @param prompt_override 若非 NULL 则使用该指令，否则使用当前拾取的语音指令
 */
void cartridge_agent_trigger_voice_chat(const char *prompt_override);

#ifdef __cplusplus
}
#endif

#endif /* CARTRIDGE_AGENT_H */
