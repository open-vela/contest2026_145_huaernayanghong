/**
 * @file store.c
 * @brief Phoenix HoloDesk-S1 Persistent Data Store Engine Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "store.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if defined(HOST_TEST_RUNNER)
#include "cJSON.h"
#else
#include <netutils/cJSON.h>
#endif
#include "../utils/time_utils.h"

#define STORE_FLUSH_DEBOUNCE_MS 3000
static uint64_t s_last_dirty_ms = 0;

#define STORE_PATH_MAX 256
#define STORE_FILENAME "stats.json"
#define STORE_TMP_FILENAME "stats.json.tmp"

typedef struct {
    phoenix_stats_t stats;
    char data_dir[STORE_PATH_MAX];
    char file_path[STORE_PATH_MAX];
    char tmp_path[STORE_PATH_MAX];
    pthread_mutex_t lock;
    bool initialized;
    bool dirty;
} phoenix_store_ctx_t;

static phoenix_store_ctx_t g_store;

static int ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        return -ENOTDIR;
    }
    if (mkdir(path, 0755) != 0) {
        return -errno;
    }
    return 0;
}

int phoenix_store_init(const char *data_dir)
{
    if (g_store.initialized) return 0;

    memset(&g_store, 0, sizeof(g_store));
    pthread_mutex_init(&g_store.lock, NULL);

    if (!data_dir || strlen(data_dir) == 0) {
        data_dir = "/data/phoenix";
    }

    strncpy(g_store.data_dir, data_dir, sizeof(g_store.data_dir) - 1);
    snprintf(g_store.file_path, sizeof(g_store.file_path), "%s/%s", data_dir, STORE_FILENAME);
    snprintf(g_store.tmp_path, sizeof(g_store.tmp_path), "%s/%s", data_dir, STORE_TMP_FILENAME);

    ensure_dir(data_dir);

    g_store.initialized = true;

    /* Load existing data or create defaults */
    phoenix_store_load();

    return 0;
}

void phoenix_store_deinit(void)
{
    if (!g_store.initialized) return;

    phoenix_store_force_flush();

    pthread_mutex_lock(&g_store.lock);
    g_store.initialized = false;
    pthread_mutex_unlock(&g_store.lock);
    pthread_mutex_destroy(&g_store.lock);
}

int phoenix_store_load(void)
{
    if (!g_store.initialized) return -EINVAL;

    pthread_mutex_lock(&g_store.lock);

    FILE *f = fopen(g_store.file_path, "rb");
    if (!f) {
        /* File doesn't exist yet, start with zeroes */
        memset(&g_store.stats, 0, sizeof(g_store.stats));
        g_store.dirty = true;
        pthread_mutex_unlock(&g_store.lock);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0 || len > 64 * 1024) {
        fclose(f);
        pthread_mutex_unlock(&g_store.lock);
        return -EINVAL;
    }

    char *buf = (char *)malloc(len + 1);
    if (!buf) {
        fclose(f);
        pthread_mutex_unlock(&g_store.lock);
        return -ENOMEM;
    }

    size_t read_bytes = fread(buf, 1, len, f);
    fclose(f);
    buf[read_bytes] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        pthread_mutex_unlock(&g_store.lock);
        return -EINVAL;
    }

    cJSON *item = cJSON_GetObjectItem(root, "total_merit");
    if (item && item->type == cJSON_Number) g_store.stats.total_merit = (uint32_t)item->valueint;

    item = cJSON_GetObjectItem(root, "pomodoro_count");
    if (item && item->type == cJSON_Number) g_store.stats.pomodoro_count = (uint32_t)item->valueint;

    item = cJSON_GetObjectItem(root, "total_focus_seconds");
    if (item && item->type == cJSON_Number) g_store.stats.total_focus_seconds = (uint32_t)item->valueint;

    item = cJSON_GetObjectItem(root, "interaction_count");
    if (item && item->type == cJSON_Number) g_store.stats.interaction_count = (uint32_t)item->valueint;

    cJSON_Delete(root);

    g_store.dirty = false;
    pthread_mutex_unlock(&g_store.lock);

    printf("[PhoenixStore] Loaded stats: Merit=%u, Pomodoro=%u, FocusSec=%u\n",
           g_store.stats.total_merit, g_store.stats.pomodoro_count, g_store.stats.total_focus_seconds);
    return 0;
}

int phoenix_store_save(void)
{
    if (!g_store.initialized) return -EINVAL;

    pthread_mutex_lock(&g_store.lock);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        pthread_mutex_unlock(&g_store.lock);
        return -ENOMEM;
    }

    cJSON_AddNumberToObject(root, "total_merit", g_store.stats.total_merit);
    cJSON_AddNumberToObject(root, "pomodoro_count", g_store.stats.pomodoro_count);
    cJSON_AddNumberToObject(root, "total_focus_seconds", g_store.stats.total_focus_seconds);
    cJSON_AddNumberToObject(root, "interaction_count", g_store.stats.interaction_count);
    cJSON_AddNumberToObject(root, "last_saved_time", (double)time(NULL));

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) {
        pthread_mutex_unlock(&g_store.lock);
        return -ENOMEM;
    }

    /* Write to temporary file first for atomic write */
    FILE *f = fopen(g_store.tmp_path, "wb");
    if (!f) {
        free(json_str);
        pthread_mutex_unlock(&g_store.lock);
        return -errno;
    }

    size_t slen = strlen(json_str);
    fwrite(json_str, 1, slen, f);
    fflush(f);
    fclose(f);
    free(json_str);

    /* Atomic rename */
    rename(g_store.tmp_path, g_store.file_path);

    g_store.dirty = false;
    pthread_mutex_unlock(&g_store.lock);

    return 0;
}

void phoenix_store_flush(void)
{
    if (!g_store.initialized) return;

    if (g_store.dirty) {
        uint64_t now_ms = time_utils_get_ms();
        if (now_ms - s_last_dirty_ms >= STORE_FLUSH_DEBOUNCE_MS) {
            phoenix_store_save();
        }
    }
}

void phoenix_store_force_flush(void)
{
    if (!g_store.initialized) return;

    if (g_store.dirty) {
        phoenix_store_save();
    }
}

void phoenix_store_set_dirty(void)
{
    g_store.dirty = true;
    s_last_dirty_ms = time_utils_get_ms();
}

void phoenix_store_get_stats(phoenix_stats_t *out_stats)
{
    if (!out_stats) return;

    pthread_mutex_lock(&g_store.lock);
    *out_stats = g_store.stats;
    pthread_mutex_unlock(&g_store.lock);
}

uint32_t phoenix_store_add_merit(uint32_t delta)
{
    pthread_mutex_lock(&g_store.lock);
    g_store.stats.total_merit += delta;
    g_store.dirty = true;
    s_last_dirty_ms = time_utils_get_ms();
    uint32_t total = g_store.stats.total_merit;
    pthread_mutex_unlock(&g_store.lock);
    return total;
}

void phoenix_store_add_pomodoro(uint32_t duration_seconds)
{
    pthread_mutex_lock(&g_store.lock);
    g_store.stats.pomodoro_count++;
    g_store.stats.total_focus_seconds += duration_seconds;
    g_store.dirty = true;
    s_last_dirty_ms = time_utils_get_ms();
    pthread_mutex_unlock(&g_store.lock);
}

void phoenix_store_inc_interaction(void)
{
    pthread_mutex_lock(&g_store.lock);
    g_store.stats.interaction_count++;
    g_store.dirty = true;
    s_last_dirty_ms = time_utils_get_ms();
    pthread_mutex_unlock(&g_store.lock);
}
