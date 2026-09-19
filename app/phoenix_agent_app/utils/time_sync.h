#ifndef PHOENIX_UTILS_TIME_SYNC_H
#define PHOENIX_UTILS_TIME_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化时间同步子系统
 */
int time_sync_init(void);

/**
 * @brief 异步触发一次 NTP 网络时间校准 (非阻塞后台工作线程)
 * @return 0 成功启动同步或同步已在运行, 负数失败
 */
int time_sync_trigger_ntp(void);

/**
 * @brief 查询当前 NTP 同步任务是否正在后台执行
 */
bool time_sync_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_UTILS_TIME_SYNC_H */
