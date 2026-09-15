#include "time_utils.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>

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
    time_t now = time(NULL);
    struct tm tm_now;
#if defined(_WIN32)
    localtime_s(&tm_now, &now);
#else
    localtime_r(&now, &tm_now);
#endif
    strftime(buf, max_len, "%Y-%m-%d %H:%M:%S", &tm_now);
}
