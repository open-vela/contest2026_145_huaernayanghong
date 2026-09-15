/**
 * @file cartridge_home.h
 * @brief 主界面卡带：高信息密度时间、日历与待办事项 (Home Dashboard Cartridge)
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

#define HOME_MAX_TODOS 6

typedef struct {
    char title[64];
    char time_str[16];
    bool done;
} home_todo_item_t;

/**
 * @brief 注册主界面卡带至卡带管理引擎
 * @return 0 成功, 负数失败
 */
int cartridge_home_register(void);

/**
 * @brief 动态添加待办事项
 * @param title 任务标题
 * @param time_str 时间描述 (如 "16:00")
 * @return 0 成功, 负数失败
 */
int cartridge_home_add_todo(const char *title, const char *time_str);

/**
 * @brief 切换指定索引的待办完成状态
 * @param index 待办索引 (0 ~ HOME_MAX_TODOS - 1)
 * @return 0 成功
 */
int cartridge_home_toggle_todo(int index);

/**
 * @brief 触发或停止内嵌番茄钟专注流
 * @return 0 成功
 */
int cartridge_home_toggle_pomodoro(void);

#ifdef __cplusplus
}
#endif

#endif /* CARTRIDGE_HOME_H */
