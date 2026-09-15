#ifndef PHOENIX_UTILS_TIME_UTILS_H
#define PHOENIX_UTILS_TIME_UTILS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 获取系统单调递增时间戳（毫秒）
 */
uint64_t time_utils_get_ms(void);

/**
 * 获取系统单调递增时间戳（微秒）
 */
uint64_t time_utils_get_us(void);

/**
 * 线程休眠毫秒
 */
void time_utils_sleep_ms(uint32_t ms);

/**
 * 格式化当前时间字符串 (YYYY-MM-DD HH:MM:SS)
 */
void time_utils_get_datetime_str(char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_UTILS_TIME_UTILS_H */
