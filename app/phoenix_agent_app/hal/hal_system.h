/**
 * @file hal_system.h
 * @brief Phoenix HoloDesk-S1 System Capability HAL Public Interface
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef HAL_SYSTEM_H
#define HAL_SYSTEM_H

#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化系统能力子系统 */
int hal_system_init(void);

/** 析构系统能力子系统 */
int hal_system_deinit(void);

/** 采集系统底层软硬件健康遥测数据 (CPU负载、主频、真实内存使用、核心温度、Uptime) */
int hal_system_get_telemetry(hal_system_telemetry_t *out_telem);

/**
 * @brief 在底层操作系统调度拉起原生应用
 * @param app_package_or_alias 应用包名或别名 (如 com.application.x4b.calendar 或 "日历")
 * @return 0 成功拉起, 负值表示失败
 */
int hal_system_launch_app(const char *app_package_or_alias);

/** 获取平台推荐的持久化存储基路径 (如 "/data/phoenix" 或 "/tmp/phoenix") */
const char* hal_system_get_storage_base_path(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SYSTEM_H */
