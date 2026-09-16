/**
 * @file log_utils.h
 * @brief Phoenix HoloDesk-S1 日志宏抽象层 (OpenVela Syslog 规范适配)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_UTILS_LOG_UTILS_H
#define PHOENIX_UTILS_LOG_UTILS_H

#include "log_mgr.h"

/* 保持对历史颜色宏的兼容定义 */
#define PHOENIX_LOG_COLOR_RESET   "\033[0m"
#define PHOENIX_LOG_COLOR_RED     "\033[31m"
#define PHOENIX_LOG_COLOR_GREEN   "\033[32m"
#define PHOENIX_LOG_COLOR_YELLOW  "\033[33m"
#define PHOENIX_LOG_COLOR_BLUE    "\033[34m"
#define PHOENIX_LOG_COLOR_CYAN    "\033[36m"
#define PHOENIX_LOG_COLOR_PURPLE  "\033[35m"

/* 编译时最高使能日志级别裁剪 (默认开启全部级别，由运行时过滤) */
#ifndef PHOENIX_LOG_COMPILE_LEVEL
#define PHOENIX_LOG_COMPILE_LEVEL PHOENIX_LOG_VERBOSE
#endif

/* 标准模块级日志宏 (100% 向后兼容全工程历史代码) */
#if (PHOENIX_LOG_COMPILE_LEVEL >= PHOENIX_LOG_ERROR)
#define LOG_E(tag, fmt, ...) phoenix_log_write(PHOENIX_LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_E(tag, fmt, ...) ((void)0)
#endif

#if (PHOENIX_LOG_COMPILE_LEVEL >= PHOENIX_LOG_WARN)
#define LOG_W(tag, fmt, ...) phoenix_log_write(PHOENIX_LOG_WARN, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_W(tag, fmt, ...) ((void)0)
#endif

#if (PHOENIX_LOG_COMPILE_LEVEL >= PHOENIX_LOG_INFO)
#define LOG_I(tag, fmt, ...) phoenix_log_write(PHOENIX_LOG_INFO, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_I(tag, fmt, ...) ((void)0)
#endif

#if (PHOENIX_LOG_COMPILE_LEVEL >= PHOENIX_LOG_DEBUG)
#define LOG_D(tag, fmt, ...) phoenix_log_write(PHOENIX_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_D(tag, fmt, ...) ((void)0)
#endif

#if (PHOENIX_LOG_COMPILE_LEVEL >= PHOENIX_LOG_VERBOSE)
#define LOG_V(tag, fmt, ...) phoenix_log_write(PHOENIX_LOG_VERBOSE, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_V(tag, fmt, ...) ((void)0)
#endif

/* 防雪崩限流宏 (适用于高频传感器读取、网络重连等场景) */
#define LOG_RATE_LIMITED(level, tag, interval_ms, fmt, ...) \
    phoenix_log_write_ratelimited(level, tag, interval_ms, fmt, ##__VA_ARGS__)

#define LOG_W_RATELIMITED(tag, interval_ms, fmt, ...) \
    phoenix_log_write_ratelimited(PHOENIX_LOG_WARN, tag, interval_ms, fmt, ##__VA_ARGS__)

#define LOG_E_RATELIMITED(tag, interval_ms, fmt, ...) \
    phoenix_log_write_ratelimited(PHOENIX_LOG_ERROR, tag, interval_ms, fmt, ##__VA_ARGS__)

/* Android NDK 风格便捷宏 (要求源文件顶部先定义 #define LOG_TAG "模块名") */
#ifdef LOG_TAG
#define ALOGE(fmt, ...) LOG_E(LOG_TAG, fmt, ##__VA_ARGS__)
#define ALOGW(fmt, ...) LOG_W(LOG_TAG, fmt, ##__VA_ARGS__)
#define ALOGI(fmt, ...) LOG_I(LOG_TAG, fmt, ##__VA_ARGS__)
#define ALOGD(fmt, ...) LOG_D(LOG_TAG, fmt, ##__VA_ARGS__)
#define ALOGV(fmt, ...) LOG_V(LOG_TAG, fmt, ##__VA_ARGS__)
#endif

#endif /* PHOENIX_UTILS_LOG_UTILS_H */
