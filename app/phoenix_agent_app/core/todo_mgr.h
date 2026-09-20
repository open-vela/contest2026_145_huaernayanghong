/**
 * @file todo_mgr.h
 * @brief Phoenix HoloDesk-S1 待办事项管理引擎 (Persistent Todo Manager)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_TODO_MGR_H
#define PHOENIX_TODO_MGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TODO_MAX_ITEMS 16
#define TODO_TITLE_MAX 64
#define TODO_TIME_MAX  16

/**
 * @brief 单条待办事项数据模型
 */
typedef struct {
    int      id;                       /**< 唯一数字编号 (>=1) */
    char     title[TODO_TITLE_MAX];    /**< 待办事项标题 */
    char     time_str[TODO_TIME_MAX];  /**< 关联时间 (如 "09:30", "今日") */
    bool     done;                     /**< 完成状态 */
    int64_t  created_at;               /**< 创建时的时间戳 (秒) */
} todo_item_t;

/**
 * @brief 初始化待办事项管理器并从文件载入历史数据
 * @param data_dir 持久化目录 (如 "/data/phoenix")
 * @return 0 成功, 负数失败
 */
int todo_mgr_init(const char *data_dir);

/**
 * @brief 反初始化待办事项管理器并保存脏数据
 */
void todo_mgr_deinit(void);

/**
 * @brief 获取当前所有待办事项列表
 * @param out_items 输出数组缓冲区
 * @param max_count 数组最大容量
 * @return 实际拷贝出的待办数量
 */
size_t todo_mgr_get_all(todo_item_t *out_items, size_t max_count);

/**
 * @brief 获取待办统计数据
 * @param out_done 输出已完成数量 (可为 NULL)
 * @return 总待办数量
 */
size_t todo_mgr_get_counts(size_t *out_done);

/**
 * @brief 新增一条待办事项并触发持久化与事件通知
 * @param title 标题
 * @param time_str 时间或提醒标签 (若为 NULL 则默认 "今日")
 * @return 新建项的 ID (>=1), 负数失败 (如已满)
 */
int todo_mgr_add(const char *title, const char *time_str);

/**
 * @brief 根据待办 ID 切换完成状态
 * @param id 待办项目唯一 ID
 * @return 0 成功, 负数未找到
 */
int todo_mgr_toggle(int id);

/**
 * @brief 根据待办 ID 删除条目
 * @param id 待办项目唯一 ID
 * @return 0 成功, 负数未找到
 */
int todo_mgr_delete(int id);

/**
 * @brief 一键清理所有已完成的待办条目
 * @return 清理掉的条目数量
 */
int todo_mgr_clear_done(void);

/**
 * @brief 强制保存内存数据到磁盘 JSON 文件
 * @return 0 成功, 负数失败
 */
int todo_mgr_save(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_TODO_MGR_H */
