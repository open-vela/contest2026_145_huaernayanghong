/**
 * @file log_mgr.h
 * @brief Phoenix HoloDesk-S1 统一日志管理器 (OpenVela Syslog / Host Dual-Backend)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_LOG_MGR_H
#define PHOENIX_LOG_MGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

/**
 * @brief 日志严重级别定义 (数值越小严重程度越高)
 */
typedef enum {
    PHOENIX_LOG_NONE    = 0, /**< 禁用所有日志输出 */
    PHOENIX_LOG_ERROR   = 1, /**< 严重错误 (系统故障、硬件错误、内存耗尽) */
    PHOENIX_LOG_WARN    = 2, /**< 警告信息 (非致命异常、重试、性能抖动) */
    PHOENIX_LOG_INFO    = 3, /**< 关键业务流转通知 (状态机跃迁、初始化完成) */
    PHOENIX_LOG_DEBUG   = 4, /**< 调试细节 (仅开发调试开启) */
    PHOENIX_LOG_VERBOSE = 5  /**< 详细数据包与追踪 (全量流水) */
} phoenix_log_level_t;

/**
 * @brief 初始化 Phoenix 统一日志子系统
 *        - 在嵌入式环境下桥接 OpenVela POSIX syslog 与 Ramlog
 *        - 在宿主机环境下初始化高精时间戳与彩色终端输出
 *        - 初始化环形内存追踪缓冲区供 Web 伴侣远程诊断
 * @return 0 成功，负数表示错误码
 */
int phoenix_log_init(void);

/**
 * @brief 去初始化日志子系统，释放资源并刷新待写缓冲
 */
void phoenix_log_deinit(void);

/**
 * @brief 动态设置当前运行时的日志过滤级别
 * @param level 目标严重级别 (PHOENIX_LOG_ERROR ~ PHOENIX_LOG_VERBOSE)
 */
void phoenix_log_set_level(int level);

/**
 * @brief 获取当前日志级别
 * @return 当前严重级别枚举值
 */
int phoenix_log_get_level(void);

/**
 * @brief 将日志级别名称解析为枚举值 (如 "info", "debug", "error")
 * @param name 字符串形式的级别名称
 * @return 对应的枚举级别，未识别时返回默认 INFO 级别
 */
int phoenix_log_level_from_str(const char *name);

/**
 * @brief 将日志级别枚举值转为可读字符串
 * @param level 日志级别
 * @return 对应的字符串表示 (如 "ERROR", "INFO")
 */
const char *phoenix_log_level_to_str(int level);

/**
 * @brief 底层变参日志写入入口
 */
void phoenix_log_vwrite(int level, const char *tag, const char *fmt, va_list args);

/**
 * @brief 格式化日志写入函数
 */
void phoenix_log_write(int level, const char *tag, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * @brief 防雪崩限流日志写入函数
 * @param level 严重级别
 * @param tag 模块标签
 * @param interval_ms 冷却抑制时间窗口 (毫秒)
 * @param fmt 格式化字符串
 */
void phoenix_log_write_ratelimited(int level, const char *tag, uint32_t interval_ms, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

/**
 * @brief 获取最近的环形内存日志内容 (供 Web 伴侣 /api/logs 调阅)
 * @param buffer 输出缓冲区
 * @param max_len 缓冲区最大字节数
 * @return 实际拷贝出的字符串字节数
 */
size_t phoenix_log_get_recent(char *buffer, size_t max_len);

/**
 * @brief 清空当前环形内存日志
 */
void phoenix_log_clear_recent(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_LOG_MGR_H */
