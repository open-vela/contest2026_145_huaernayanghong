#include "time_utils.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/* 基准对时有效时间阈值：2024-01-01 00:00:00 UTC (1704067200) */
#define TIME_SYNC_VALID_THRESHOLD_SEC 1704067200

/* 东八区秒数偏移 (UTC+8) */
#define TIMEZONE_OFFSET_BEIJING_SEC (8 * 3600)

static volatile bool   s_time_manually_synced = false;
static volatile time_t s_time_offset_sec = 0;

void time_utils_init(void) {
#if !defined(_WIN32)
    setenv("TZ", "CST-8", 1);
    tzset();
#endif
}

uint64_t time_utils_get_ms(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}

uint64_t time_utils_get_us(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return ((uint64_t)ts.tv_sec * 1000000ULL) + ((uint64_t)ts.tv_nsec / 1000ULL);
}

void time_utils_sleep_ms(uint32_t ms) {
    usleep((useconds_t)ms * 1000);
}

void time_utils_get_datetime_str(char *buf, size_t max_len) {
    if (!buf || max_len == 0) return;
    struct tm tm_now;
    time_utils_get_local_time(&tm_now);
    strftime(buf, max_len, "%Y-%m-%d %H:%M:%S", &tm_now);
}

time_t time_utils_get_epoch(void) {
    return time(NULL) + s_time_offset_sec;
}

bool time_utils_is_synced(void) {
    if (s_time_manually_synced) {
        return true;
    }
    time_t now = time_utils_get_epoch();
    return (now >= TIME_SYNC_VALID_THRESHOLD_SEC);
}

int time_utils_set_time(time_t epoch_sec) {
    if (epoch_sec < TIME_SYNC_VALID_THRESHOLD_SEC) {
        return -1;
    }

    struct timespec ts;
    ts.tv_sec = epoch_sec;
    ts.tv_nsec = 0;

    int ret = -1;
#if defined(CLOCK_REALTIME)
    ret = clock_settime(CLOCK_REALTIME, &ts);
#endif
    if (ret != 0) {
        struct timeval tv;
        tv.tv_sec = epoch_sec;
        tv.tv_usec = 0;
        ret = settimeofday(&tv, NULL);
    }

    /* 若运行在非 root 环境 (如宿主机单测)，记录 offset 补偿，保证单测与业务逻辑完全一致 */
    if (ret == 0) {
        s_time_offset_sec = 0;
    } else {
        time_t sys_now = time(NULL);
        s_time_offset_sec = epoch_sec - sys_now;
    }

    s_time_manually_synced = true;
    return 0;
}

int time_utils_set_time_ms(uint64_t epoch_ms) {
    time_t sec = (time_t)(epoch_ms / 1000);
    return time_utils_set_time(sec);
}

int time_utils_get_local_time(struct tm *out_tm) {
    if (!out_tm) return -1;

    if (time_utils_is_synced()) {
        time_t now = time_utils_get_epoch();
        /* 手动叠加 UTC+8 偏移并通过 gmtime_r 分解，确保在缺少 /etc/zoneinfo 时依然 100% 准确 */
        time_t local_sec = now + TIMEZONE_OFFSET_BEIJING_SEC;
        gmtime_r(&local_sec, out_tm);
        return 0;
    }

    /* 未对时状态：优雅回退到开机单调时间 (默认基准：2026-09-08 08:00:00) */
    uint64_t now_ms = time_utils_get_ms();
    int64_t total_sec = (int64_t)(now_ms / 1000) + TIMEZONE_OFFSET_BEIJING_SEC;

    memset(out_tm, 0, sizeof(*out_tm));
    out_tm->tm_year = 2026 - 1900;
    out_tm->tm_mon  = 8;  /* 9月 (0-11) */
    out_tm->tm_mday = 8;
    out_tm->tm_wday = 2;  /* 周二 */
    out_tm->tm_hour = (int)((total_sec / 3600) % 24);
    out_tm->tm_min  = (int)((total_sec / 60) % 60);
    out_tm->tm_sec  = (int)(total_sec % 60);
    return 0;
}
