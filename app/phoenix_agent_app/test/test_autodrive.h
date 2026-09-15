/**
 * @file test_autodrive.h
 * @brief 模拟器端到端自动化测试与自驱动场景引擎 (Auto-Drive Engine)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef TEST_AUTODRIVE_H
#define TEST_AUTODRIVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <uv.h>
#include <lvgl/lvgl.h>
#include "ui/ui.h"
#include "core/agent_core.h"

/**
 * @brief 启动 GUI 自动化场景自驱动测试流程
 * @param loop Libuv 事件循环指针 (测试完成将调用 uv_stop 优雅退出)
 * @param ui Phoenix UI 句柄
 * @param agent Agent Core 上下文
 * @return 0 成功启动, 负数失败
 */
int phoenix_autodrive_start_gui(uv_loop_t *loop, phoenix_ui_t *ui, phoenix_agent_ctx_t *agent);

/**
 * @brief 运行纯 CLI 自动化集成冒烟测试 (无图形依赖)
 * @return 0 成功全部通过, 负数有失败用例
 */
int phoenix_autodrive_run_cli(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_AUTODRIVE_H */
