/**
 * @file cartridge_home.h
 * @brief 主界面卡带：高信息密度时间、极客天气仪表盘与待办事项 (Home Dashboard Cartridge)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef CARTRIDGE_HOME_H
#define CARTRIDGE_HOME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl/lvgl.h>
#include <stdbool.h>
#include <stdint.h>

#define HOME_MAX_TODOS 16

/**
 * @brief 注册主界面卡带至卡带管理引擎
 * @return 0 成功, 负数失败
 */
int cartridge_home_register(void);

/**
 * @brief 动态添加待办事项 (对接到系统 todo_mgr)
 * @param title 任务标题
 * @param time_str 时间描述 (如 "16:00")
 * @return 新建待办 ID (>=1), 负数失败
 */
int cartridge_home_add_todo(const char *title, const char *time_str);

/**
 * @brief 切换指定索引的待办完成状态
 * @param index 待办项目本地索引 (0 ~ count-1)
 * @return 0 成功, 负数失败
 */
int cartridge_home_toggle_todo(int index);

/**
 * @brief 触发即时天气网络刷新
 * @return 0 成功
 */
int cartridge_home_refresh_weather(void);

/**
 * @brief 触发或停止番茄钟专注流 (系统服务兼容接口)
 * @return 0 成功
 */
int cartridge_home_toggle_pomodoro(void);

#ifdef __cplusplus
}
#endif

#endif /* CARTRIDGE_HOME_H */
