/**
 * @file hal_sdcard.h
 * @brief Phoenix HoloDesk-S1 MicroSD / TF Card HAL Public Interface
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef HAL_SDCARD_H
#define HAL_SDCARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool     is_mounted;
    char     mount_point[64];
    uint64_t total_bytes;
    uint64_t free_bytes;
    uint64_t used_bytes;
    uint32_t total_mb;
    uint32_t free_mb;
    uint32_t used_mb;
    uint8_t  used_pct;
} hal_sdcard_info_t;

/** 初始化 TF 卡子系统并探测挂载 */
int hal_sdcard_init(void);

/** 析构 TF 卡子系统 */
int hal_sdcard_deinit(void);

/** 检查当前 TF 卡是否已就绪并挂载 */
bool hal_sdcard_is_mounted(void);

/** 获取当前有效的 TF 卡挂载点根路径 (如 "/mnt/sdcard") */
const char* hal_sdcard_get_mount_point(void);

/** 获取 TF 卡容量与使用遥测信息 */
int hal_sdcard_get_info(hal_sdcard_info_t *out_info);

/** 确保 TF 卡上的基础系统目录就绪 (sounds, logs, docs, models) */
int hal_sdcard_ensure_dirs(void);

/**
 * @brief 安全解析 TF 卡相对路径为绝对路径并杜绝目录越界穿越 (Path Traversal Protection)
 * @param rel_path 相对路径, 如 "/sounds/bell.wav" 或 "test.txt"
 * @param out_full_path 输出绝对路径缓冲区
 * @param max_len 缓冲区大小
 * @return 0 成功合法, 负值表示非法越界或错误
 */
int hal_sdcard_resolve_path(const char *rel_path, char *out_full_path, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SDCARD_H */
