#ifndef PHOENIX_UTILS_TIME_UTILS_H
#define PHOENIX_UTILS_TIME_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化时钟与时区设置 (默认设置为东八区北京时间 CST-8)
 */
void time_utils_init(void);

/**
 * @brief 获取系统单调递增时间戳（毫秒）
 */
uint64_t time_utils_get_ms(void);

/**
 * @brief 获取系统单调递增时间戳（微秒）
 */
uint64_t time_utils_get_us(void);

/**
 * @brief 线程休眠毫秒
 */
void time_utils_sleep_ms(uint32_t ms);

/**
 * @brief 格式化当前时间字符串 (YYYY-MM-DD HH:MM:SS)
 */
void time_utils_get_datetime_str(char *buf, size_t max_len);

/**
 * @brief 判断系统内核时钟是否已有效校准/对时 (基于 NTP 或 Web 授时)
 * @return true 已同步现实时间, false 处于未同步状态 (刚开机或 1970 纪元)
 */
bool time_utils_is_synced(void);

/**
 * @brief 获取当前系统 Unix 秒级时间戳 (带宿主环境补偿)
 */
time_t time_utils_get_epoch(void);

/**
 * @brief 主动设置系统内核时间 (秒级时间戳)
 * @param epoch_sec 自 1970-01-01 00:00:00 UTC 的秒数
 * @return 0 成功, 负数失败
 */
int time_utils_set_time(time_t epoch_sec);

/**
 * @brief 主动设置系统内核时间 (毫秒级时间戳，常用于 Web Date.now() 注入)
 * @param epoch_ms 自 1970-01-01 00:00:00 UTC 的毫秒数
 * @return 0 成功, 负数失败
 */
int time_utils_set_time_ms(uint64_t epoch_ms);

/**
 * @brief 获取本地时间 (东八区 UTC+8)
 *        若已同步，返回真实世界的 struct tm；
 *        若未同步，优雅降级为开机单调时间 (2026-09-08 08:00:00 + 开机时长)，确保 UI 稳定走字。
 * @param out_tm 输出本地分解时间结构体
 * @return 0 成功, 负数失败
 */
int time_utils_get_local_time(struct tm *out_tm);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_UTILS_TIME_UTILS_H */
