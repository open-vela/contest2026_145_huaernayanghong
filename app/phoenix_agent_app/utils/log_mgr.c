/**
 * @file log_mgr.c
 * @brief Phoenix HoloDesk-S1 统一日志管理器实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "log_mgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

#if !defined(HOST_TEST_RUNNER) && (defined(__NuttX__) || defined(__openvela__))
#  include <syslog.h>
#  define HAS_OPENVELA_SYSLOG 1
#else
#  define HAS_OPENVELA_SYSLOG 0
#endif

/* 默认环形追踪缓冲区大小: 16KB */
#define PHOENIX_LOG_RECENT_CAPACITY 16384
/* 单条日志最大排版长度: 512字节 */
#define PHOENIX_LOG_LINE_MAX 512
/* 动态限流跟踪槽位数 */
#define PHOENIX_LOG_RATELIMIT_SLOTS 16

/* ANSI 颜色代码定义 (终端输出) */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_PURPLE  "\033[35m"
#define COLOR_GRAY    "\033[90m"

typedef struct {
    const char *tag;
    uint32_t    last_time_ms;
    uint32_t    suppressed_count;
} ratelimit_entry_t;

static int                 s_current_level = PHOENIX_LOG_INFO;
static pthread_mutex_t     s_log_lock = PTHREAD_MUTEX_INITIALIZER;
static bool                s_initialized = false;
static uint64_t            s_boot_time_ms = 0;

/* 内存环形追踪日志缓冲区 (供 Web 伴侣展示) */
static char                s_recent_buf[PHOENIX_LOG_RECENT_CAPACITY];
static size_t              s_recent_head = 0;
static size_t              s_recent_len = 0;
static uint64_t            s_total_written_bytes = 0;

/* 限流状态记录表 */
static ratelimit_entry_t   s_ratelimit_entries[PHOENIX_LOG_RATELIMIT_SLOTS];

/* 持久化 Flash 文件日志与控制台分流状态 */
static bool                s_console_enabled = true;
static FILE               *s_file_fp = NULL;
static char                s_file_path[256] = {0};
static size_t              s_file_max_bytes = 65536; /* 默认 64KB */
static size_t              s_file_current_bytes = 0;

static uint64_t get_system_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}

int phoenix_log_init(void)
{
    pthread_mutex_lock(&s_log_lock);
    if (s_initialized) {
        pthread_mutex_unlock(&s_log_lock);
        return 0;
    }

    s_boot_time_ms = get_system_ms();
    s_current_level = PHOENIX_LOG_INFO;
    s_console_enabled = true;
    s_recent_head = 0;
    s_recent_len = 0;
    s_total_written_bytes = 0;
    memset(s_recent_buf, 0, sizeof(s_recent_buf));
    memset(s_ratelimit_entries, 0, sizeof(s_ratelimit_entries));

#if HAS_OPENVELA_SYSLOG
    /* OpenVela 平台联动 setlogmask 默认开启至 INFO 级别 */
    setlogmask(LOG_UPTO(LOG_INFO));
    syslog(LOG_INFO, "[PhoenixLog] OpenVela Syslog Logging Engine Initialized\n");
#endif

    s_initialized = true;
    pthread_mutex_unlock(&s_log_lock);
    return 0;
}

void phoenix_log_deinit(void)
{
    pthread_mutex_lock(&s_log_lock);
    if (s_file_fp) {
        fflush(s_file_fp);
        fclose(s_file_fp);
        s_file_fp = NULL;
    }
    s_file_path[0] = '\0';
    s_file_current_bytes = 0;
    s_initialized = false;
    pthread_mutex_unlock(&s_log_lock);
}

void phoenix_log_set_level(int level)
{
    if (level < PHOENIX_LOG_NONE) level = PHOENIX_LOG_NONE;
    if (level > PHOENIX_LOG_VERBOSE) level = PHOENIX_LOG_VERBOSE;

    pthread_mutex_lock(&s_log_lock);
    s_current_level = level;

#if HAS_OPENVELA_SYSLOG
    int syslog_mask = LOG_UPTO(LOG_INFO);
    switch (level) {
        case PHOENIX_LOG_NONE:    syslog_mask = 0; break;
        case PHOENIX_LOG_ERROR:   syslog_mask = LOG_UPTO(LOG_ERR); break;
        case PHOENIX_LOG_WARN:    syslog_mask = LOG_UPTO(LOG_WARNING); break;
        case PHOENIX_LOG_INFO:    syslog_mask = LOG_UPTO(LOG_INFO); break;
        case PHOENIX_LOG_DEBUG:
        case PHOENIX_LOG_VERBOSE: syslog_mask = LOG_UPTO(LOG_DEBUG); break;
        default:                  syslog_mask = LOG_UPTO(LOG_INFO); break;
    }
    setlogmask(syslog_mask);
#endif

    pthread_mutex_unlock(&s_log_lock);
}

int phoenix_log_get_level(void)
{
    return s_current_level;
}

int phoenix_log_level_from_str(const char *name)
{
    if (!name) return PHOENIX_LOG_INFO;
    if (strcasecmp(name, "none") == 0 || strcasecmp(name, "off") == 0) return PHOENIX_LOG_NONE;
    if (strcasecmp(name, "error") == 0 || strcasecmp(name, "err") == 0) return PHOENIX_LOG_ERROR;
    if (strcasecmp(name, "warn") == 0 || strcasecmp(name, "warning") == 0) return PHOENIX_LOG_WARN;
    if (strcasecmp(name, "info") == 0) return PHOENIX_LOG_INFO;
    if (strcasecmp(name, "debug") == 0) return PHOENIX_LOG_DEBUG;
    if (strcasecmp(name, "verbose") == 0 || strcasecmp(name, "trace") == 0) return PHOENIX_LOG_VERBOSE;
    return PHOENIX_LOG_INFO;
}

const char *phoenix_log_level_to_str(int level)
{
    switch (level) {
        case PHOENIX_LOG_NONE:    return "NONE";
        case PHOENIX_LOG_ERROR:   return "ERROR";
        case PHOENIX_LOG_WARN:    return "WARN";
        case PHOENIX_LOG_INFO:    return "INFO";
        case PHOENIX_LOG_DEBUG:   return "DEBUG";
        case PHOENIX_LOG_VERBOSE: return "VERBOSE";
        default:                  return "UNKNOWN";
    }
}

static void append_to_recent_buffer(const char *line, size_t line_len)
{
    if (!line || line_len == 0) return;

    s_total_written_bytes += (uint64_t)line_len;

    if (line_len >= PHOENIX_LOG_RECENT_CAPACITY) {
        line += (line_len - PHOENIX_LOG_RECENT_CAPACITY + 1);
        line_len = PHOENIX_LOG_RECENT_CAPACITY - 1;
    }

    for (size_t i = 0; i < line_len; i++) {
        s_recent_buf[s_recent_head] = line[i];
        s_recent_head = (s_recent_head + 1) % PHOENIX_LOG_RECENT_CAPACITY;
        if (s_recent_len < PHOENIX_LOG_RECENT_CAPACITY) {
            s_recent_len++;
        }
    }
}

uint64_t phoenix_log_get_cursor(void)
{
    pthread_mutex_lock(&s_log_lock);
    uint64_t cur = s_total_written_bytes;
    pthread_mutex_unlock(&s_log_lock);
    return cur;
}

size_t phoenix_log_get_since(uint64_t *inout_cursor, char *buffer, size_t max_len)
{
    if (!buffer || max_len == 0) return 0;
    buffer[0] = '\0';

    pthread_mutex_lock(&s_log_lock);
    if (!inout_cursor) {
        pthread_mutex_unlock(&s_log_lock);
        return 0;
    }

    uint64_t cur = *inout_cursor;
    uint64_t total = s_total_written_bytes;

    /* 首次调用或未指定偏移 (cur == 0): 返回当前有效的所有最近日志，并将游标更新为当前最新 */
    if (cur == 0) {
        if (s_recent_len == 0) {
            *inout_cursor = total;
            pthread_mutex_unlock(&s_log_lock);
            return 0;
        }
        size_t copy_len = (s_recent_len < max_len - 1) ? s_recent_len : (max_len - 1);
        size_t start_idx = (s_recent_head + PHOENIX_LOG_RECENT_CAPACITY - s_recent_len) % PHOENIX_LOG_RECENT_CAPACITY;
        for (size_t i = 0; i < copy_len; i++) {
            buffer[i] = s_recent_buf[(start_idx + i) % PHOENIX_LOG_RECENT_CAPACITY];
        }
        buffer[copy_len] = '\0';
        *inout_cursor = total;
        pthread_mutex_unlock(&s_log_lock);
        return copy_len;
    }

    /* 已经是最新，无新增日志 */
    if (cur >= total) {
        *inout_cursor = total;
        pthread_mutex_unlock(&s_log_lock);
        return 0;
    }

    /* 游标落后超过当前环形缓冲容量，快进到最旧可用位置 */
    uint64_t earliest = (total > (uint64_t)s_recent_len) ? (total - (uint64_t)s_recent_len) : 0;
    if (cur < earliest) {
        cur = earliest;
    }

    size_t need_len = (size_t)(total - cur);
    if (need_len > max_len - 1) {
        need_len = max_len - 1;
    }

    size_t delta_from_end = (size_t)(total - cur);
    size_t start_idx = (s_recent_head + PHOENIX_LOG_RECENT_CAPACITY - delta_from_end) % PHOENIX_LOG_RECENT_CAPACITY;

    for (size_t i = 0; i < need_len; i++) {
        buffer[i] = s_recent_buf[(start_idx + i) % PHOENIX_LOG_RECENT_CAPACITY];
    }
    buffer[need_len] = '\0';
    *inout_cursor = cur + need_len;

    pthread_mutex_unlock(&s_log_lock);
    return need_len;
}

size_t phoenix_log_get_recent(char *buffer, size_t max_len)
{
    uint64_t cursor = 0;
    return phoenix_log_get_since(&cursor, buffer, max_len);
}

void phoenix_log_clear_recent(void)
{
    pthread_mutex_lock(&s_log_lock);
    s_recent_head = 0;
    s_recent_len = 0;
    memset(s_recent_buf, 0, sizeof(s_recent_buf));
    pthread_mutex_unlock(&s_log_lock);
}

static void ensure_parent_dir(const char *path)
{
    if (!path) return;
    char tmp[256];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *p = strrchr(tmp, '/');
    if (p && p != tmp) {
        *p = '\0';
        for (char *c = tmp + 1; *c; c++) {
            if (*c == '/') {
                *c = '\0';
                mkdir(tmp, 0755);
                *c = '/';
            }
        }
        mkdir(tmp, 0755);
    }
}

static void write_to_file_locked(const char *line, size_t len)
{
    if (!s_file_fp || !line || len == 0) return;

    /* 检查并执行自动轮转 (Rotation): 达到上限则重命名为 .old 并新建 */
    if (s_file_current_bytes + len > s_file_max_bytes) {
        fflush(s_file_fp);
        fclose(s_file_fp);
        s_file_fp = NULL;

        char old_path[280];
        snprintf(old_path, sizeof(old_path), "%s.old", s_file_path);
        unlink(old_path);
        rename(s_file_path, old_path);

        s_file_fp = fopen(s_file_path, "w+");
        s_file_current_bytes = 0;
        if (!s_file_fp) return;
    }

    size_t written = fwrite(line, 1, len, s_file_fp);
    s_file_current_bytes += written;
    fflush(s_file_fp);
}

void phoenix_log_vwrite(int level, const char *tag, const char *fmt, va_list args)
{
    if (level <= PHOENIX_LOG_NONE || level > s_current_level) {
        return;
    }

    if (!tag) tag = "Phoenix";

    char msg_buf[PHOENIX_LOG_LINE_MAX];
    int written = vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    if (written < 0) return;

    /* 获取相对启动时间戳 (秒.毫秒) */
    uint64_t now_ms = get_system_ms();
    uint64_t rel_ms = now_ms >= s_boot_time_ms ? (now_ms - s_boot_time_ms) : 0;
    uint32_t sec = (uint32_t)(rel_ms / 1000ULL);
    uint32_t msec = (uint32_t)(rel_ms % 1000ULL);

    const char *lvl_char = "I";
    const char *color_code = COLOR_GREEN;

    switch (level) {
        case PHOENIX_LOG_ERROR:
            lvl_char = "E";
            color_code = COLOR_RED;
            break;
        case PHOENIX_LOG_WARN:
            lvl_char = "W";
            color_code = COLOR_YELLOW;
            break;
        case PHOENIX_LOG_INFO:
            lvl_char = "I";
            color_code = COLOR_GREEN;
            break;
        case PHOENIX_LOG_DEBUG:
            lvl_char = "D";
            color_code = COLOR_CYAN;
            break;
        case PHOENIX_LOG_VERBOSE:
            lvl_char = "V";
            color_code = COLOR_PURPLE;
            break;
        default:
            break;
    }

    char full_line[PHOENIX_LOG_LINE_MAX + 64];
    int line_len = snprintf(full_line, sizeof(full_line),
                            "[%5u.%03u] [%s:%s] %s\n",
                            (unsigned int)sec, (unsigned int)msec, tag, lvl_char, msg_buf);

    pthread_mutex_lock(&s_log_lock);

    /* 1. 保存至环形诊断缓冲区 (保留纯文本，去除 ANSI 转义码) */
    if (line_len > 0) {
        append_to_recent_buffer(full_line, (size_t)line_len);
    }

    /* 2. 多通道并发分流 */

#if HAS_OPENVELA_SYSLOG
    /* 2.1 嵌入式 OpenVela 平台系统日志: 经 POSIX syslog 分流至 Serial / Ramlog */
    int syslog_prio = LOG_INFO;
    switch (level) {
        case PHOENIX_LOG_ERROR:   syslog_prio = LOG_ERR; break;
        case PHOENIX_LOG_WARN:    syslog_prio = LOG_WARNING; break;
        case PHOENIX_LOG_INFO:    syslog_prio = LOG_INFO; break;
        case PHOENIX_LOG_DEBUG:
        case PHOENIX_LOG_VERBOSE: syslog_prio = LOG_DEBUG; break;
        default:                  syslog_prio = LOG_INFO; break;
    }
    syslog(syslog_prio, "[%s:%s] %s\n", tag, lvl_char, msg_buf);
#endif

    /* 2.2 控制台/终端标准输出 (确保 adb shell 前台运行或 host 运行均有彩色高亮实时日志) */
    if (s_console_enabled) {
        char ansi_line[PHOENIX_LOG_LINE_MAX + 128];
        snprintf(ansi_line, sizeof(ansi_line),
                 COLOR_GRAY "[%5u.%03u]" COLOR_RESET " %s[%s:%s]%s %s\n",
                 (unsigned int)sec, (unsigned int)msec, color_code, tag, lvl_char, COLOR_RESET, msg_buf);
        fputs(ansi_line, stdout);
        fflush(stdout);
    }

    /* 2.3 Flash 持久化滚动黑匣子文件 (供设备断电或异常崩溃后通过 adb pull 调阅排障) */
    if (s_file_fp && line_len > 0) {
        write_to_file_locked(full_line, (size_t)line_len);
    }

    pthread_mutex_unlock(&s_log_lock);
}

int phoenix_log_enable_file(const char *file_path, size_t max_bytes)
{
    pthread_mutex_lock(&s_log_lock);

    if (s_file_fp) {
        fflush(s_file_fp);
        fclose(s_file_fp);
        s_file_fp = NULL;
    }

    if (!file_path || strlen(file_path) == 0) {
        s_file_path[0] = '\0';
        s_file_current_bytes = 0;
        pthread_mutex_unlock(&s_log_lock);
        return 0;
    }

    strncpy(s_file_path, file_path, sizeof(s_file_path) - 1);
    s_file_path[sizeof(s_file_path) - 1] = '\0';
    s_file_max_bytes = (max_bytes >= 256) ? max_bytes : 65536;

    ensure_parent_dir(s_file_path);

    s_file_fp = fopen(s_file_path, "a+");
    if (!s_file_fp) {
        pthread_mutex_unlock(&s_log_lock);
        return -1;
    }

    fseek(s_file_fp, 0, SEEK_END);
    long sz = ftell(s_file_fp);
    s_file_current_bytes = (sz > 0) ? (size_t)sz : 0;

    pthread_mutex_unlock(&s_log_lock);
    return 0;
}

const char *phoenix_log_get_file_path(void)
{
    return (s_file_fp && s_file_path[0]) ? s_file_path : NULL;
}

void phoenix_log_enable_console(bool enable)
{
    pthread_mutex_lock(&s_log_lock);
    s_console_enabled = enable;
    pthread_mutex_unlock(&s_log_lock);
}

void phoenix_log_flush(void)
{
    pthread_mutex_lock(&s_log_lock);
    if (s_file_fp) {
        fflush(s_file_fp);
    }
    fflush(stdout);
    pthread_mutex_unlock(&s_log_lock);
}

void phoenix_log_write(int level, const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    phoenix_log_vwrite(level, tag, fmt, args);
    va_end(args);
}

void phoenix_log_write_ratelimited(int level, const char *tag, uint32_t interval_ms, const char *fmt, ...)
{
    if (level <= PHOENIX_LOG_NONE || level > s_current_level) {
        return;
    }

    if (!tag) tag = "Phoenix";
    uint64_t now_ms = get_system_ms();

    pthread_mutex_lock(&s_log_lock);

    /* 查找或分配限流跟踪槽 */
    int match_idx = -1;
    int empty_idx = -1;
    for (int i = 0; i < PHOENIX_LOG_RATELIMIT_SLOTS; i++) {
        if (s_ratelimit_entries[i].tag == tag ||
            (s_ratelimit_entries[i].tag && strcmp(s_ratelimit_entries[i].tag, tag) == 0)) {
            match_idx = i;
            break;
        }
        if (empty_idx < 0 && s_ratelimit_entries[i].tag == NULL) {
            empty_idx = i;
        }
    }

    int slot = (match_idx >= 0) ? match_idx : empty_idx;
    if (slot < 0) {
        /* 表满时简单轮转复用第 0 槽 */
        slot = 0;
    }

    ratelimit_entry_t *entry = &s_ratelimit_entries[slot];
    uint32_t elapsed = (uint32_t)(now_ms - entry->last_time_ms);

    if (entry->tag && elapsed < interval_ms) {
        entry->suppressed_count++;
        pthread_mutex_unlock(&s_log_lock);
        return;
    }

    uint32_t suppressed = entry->suppressed_count;
    entry->tag = tag;
    entry->last_time_ms = (uint32_t)now_ms;
    entry->suppressed_count = 0;

    pthread_mutex_unlock(&s_log_lock);

    /* 输出日志并在必要时追加抑制说明 */
    char buf[PHOENIX_LOG_LINE_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (suppressed > 0) {
        char final_msg[PHOENIX_LOG_LINE_MAX + 64];
        snprintf(final_msg, sizeof(final_msg), "%s (抑制了 %u 条频繁日志)", buf, (unsigned int)suppressed);
        phoenix_log_write(level, tag, "%s", final_msg);
    } else {
        phoenix_log_write(level, tag, "%s", buf);
    }
}
