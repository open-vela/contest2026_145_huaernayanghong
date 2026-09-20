/**
 * @file todo_mgr.c
 * @brief Phoenix HoloDesk-S1 待办事项管理引擎实现 (JSON 持久化与事件广播)
 * @author OpenVela Contest 2026 Team 145
 */

#include "todo_mgr.h"
#include "event_bus.h"
#include "web_router.h"
#include "../utils/log_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>

#define TAG "TodoMgr"

typedef struct {
    todo_item_t     items[TODO_MAX_ITEMS];
    size_t          count;
    int             next_id;
    char            file_path[256];
    bool            initialized;
    pthread_mutex_t lock;
} todo_ctx_t;

static todo_ctx_t s_todo_ctx = {
    .count = 0,
    .next_id = 1,
    .initialized = false,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

static void todo_mgr_notify_event(int action, int target_id)
{
    size_t done_cnt = 0;
    for (size_t i = 0; i < s_todo_ctx.count; i++) {
        if (s_todo_ctx.items[i].done) done_cnt++;
    }

    phoenix_event_data_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_TODO_CHANGED;
    evt.data.todo.total_count = s_todo_ctx.count;
    evt.data.todo.done_count = done_cnt;
    evt.data.todo.last_action = action;
    evt.data.todo.target_id = target_id;
    phoenix_event_publish(&evt);
}

static void todo_mgr_set_defaults(void)
{
    s_todo_ctx.count = 4;
    s_todo_ctx.next_id = 5;

    s_todo_ctx.items[0].id = 1;
    strncpy(s_todo_ctx.items[0].title, "方案评审", TODO_TITLE_MAX - 1);
    strncpy(s_todo_ctx.items[0].time_str, "09:30", TODO_TIME_MAX - 1);
    s_todo_ctx.items[0].done = true;
    s_todo_ctx.items[0].created_at = (int64_t)time(NULL) - 3600;

    s_todo_ctx.items[1].id = 2;
    strncpy(s_todo_ctx.items[1].title, "固件烧录", TODO_TITLE_MAX - 1);
    strncpy(s_todo_ctx.items[1].time_str, "16:00", TODO_TIME_MAX - 1);
    s_todo_ctx.items[1].done = false;
    s_todo_ctx.items[1].created_at = (int64_t)time(NULL) - 1800;

    s_todo_ctx.items[2].id = 3;
    strncpy(s_todo_ctx.items[2].title, "项目周报", TODO_TITLE_MAX - 1);
    strncpy(s_todo_ctx.items[2].time_str, "19:00", TODO_TIME_MAX - 1);
    s_todo_ctx.items[2].done = false;
    s_todo_ctx.items[2].created_at = (int64_t)time(NULL) - 900;

    s_todo_ctx.items[3].id = 4;
    strncpy(s_todo_ctx.items[3].title, "温湿度联调", TODO_TITLE_MAX - 1);
    strncpy(s_todo_ctx.items[3].time_str, "20:30", TODO_TIME_MAX - 1);
    s_todo_ctx.items[3].done = false;
    s_todo_ctx.items[3].created_at = (int64_t)time(NULL);
}

int todo_mgr_save(void)
{
    if (!s_todo_ctx.initialized || s_todo_ctx.file_path[0] == '\0') {
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) return -2;

    cJSON_AddNumberToObject(root, "next_id", s_todo_ctx.next_id);

    cJSON *arr = cJSON_CreateArray();
    if (arr) {
        for (size_t i = 0; i < s_todo_ctx.count; i++) {
            cJSON *item = cJSON_CreateObject();
            if (item) {
                cJSON_AddNumberToObject(item, "id", s_todo_ctx.items[i].id);
                cJSON_AddStringToObject(item, "title", s_todo_ctx.items[i].title);
                cJSON_AddStringToObject(item, "time", s_todo_ctx.items[i].time_str);
                cJSON_AddBoolToObject(item, "done", s_todo_ctx.items[i].done);
                cJSON_AddNumberToObject(item, "created_at", (double)s_todo_ctx.items[i].created_at);
                cJSON_AddItemToArray(arr, item);
            }
        }
        cJSON_AddItemToObject(root, "todos", arr);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) return -3;

    FILE *f = fopen(s_todo_ctx.file_path, "w");
    if (!f) {
        free(json_str);
        return -4;
    }

    fputs(json_str, f);
    fclose(f);
    free(json_str);

    LOG_D(TAG, "已持久化 %zu 条待办事项到 %s", s_todo_ctx.count, s_todo_ctx.file_path);
    return 0;
}

static int todo_mgr_load_from_file(void)
{
    FILE *f = fopen(s_todo_ctx.file_path, "r");
    if (!f) {
        todo_mgr_set_defaults();
        todo_mgr_save();
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 2 || sz > 65536) {
        fclose(f);
        todo_mgr_set_defaults();
        todo_mgr_save();
        return 0;
    }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return -1;
    }

    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    buf[rd] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        todo_mgr_set_defaults();
        todo_mgr_save();
        return 0;
    }

    cJSON *next_id_item = cJSON_GetObjectItem(root, "next_id");
    if (next_id_item && next_id_item->type == cJSON_Number) {
        s_todo_ctx.next_id = next_id_item->valueint;
    }

    cJSON *arr = cJSON_GetObjectItem(root, "todos");
    if (arr && cJSON_IsArray(arr)) {
        s_todo_ctx.count = 0;
        int arr_sz = cJSON_GetArraySize(arr);
        for (int i = 0; i < arr_sz && s_todo_ctx.count < TODO_MAX_ITEMS; i++) {
            cJSON *elem = cJSON_GetArrayItem(arr, i);
            if (!elem) continue;

            todo_item_t *it = &s_todo_ctx.items[s_todo_ctx.count];
            memset(it, 0, sizeof(todo_item_t));

            cJSON *f_id = cJSON_GetObjectItem(elem, "id");
            cJSON *f_title = cJSON_GetObjectItem(elem, "title");
            cJSON *f_time = cJSON_GetObjectItem(elem, "time");
            cJSON *f_done = cJSON_GetObjectItem(elem, "done");
            cJSON *f_cr = cJSON_GetObjectItem(elem, "created_at");

            it->id = f_id ? f_id->valueint : (int)(s_todo_ctx.count + 1);
            if (f_title && f_title->valuestring) {
                strncpy(it->title, f_title->valuestring, TODO_TITLE_MAX - 1);
            }
            if (f_time && f_time->valuestring) {
                strncpy(it->time_str, f_time->valuestring, TODO_TIME_MAX - 1);
            } else {
                strncpy(it->time_str, "今日", TODO_TIME_MAX - 1);
            }
            it->done = f_done ? cJSON_IsTrue(f_done) : false;
            it->created_at = f_cr ? (int64_t)f_cr->valuedouble : (int64_t)time(NULL);

            if (it->id >= s_todo_ctx.next_id) {
                s_todo_ctx.next_id = it->id + 1;
            }

            s_todo_ctx.count++;
        }
    }

    cJSON_Delete(root);
    LOG_I(TAG, "成功载入 %zu 条待办事项 (NextID=%d)", s_todo_ctx.count, s_todo_ctx.next_id);
    return 0;
}

int todo_mgr_init(const char *data_dir)
{
    pthread_mutex_lock(&s_todo_ctx.lock);

    const char *dir = (data_dir && data_dir[0]) ? data_dir : "/data/phoenix";
    struct stat st;
    if (stat(dir, &st) != 0) {
#if defined(_WIN32)
        mkdir(dir);
#else
        mkdir(dir, 0755);
#endif
    }

    snprintf(s_todo_ctx.file_path, sizeof(s_todo_ctx.file_path), "%s/todos.json", dir);
    s_todo_ctx.count = 0;
    s_todo_ctx.next_id = 1;
    s_todo_ctx.initialized = true;

    todo_mgr_load_from_file();

    pthread_mutex_unlock(&s_todo_ctx.lock);
    return 0;
}

void todo_mgr_deinit(void)
{
    pthread_mutex_lock(&s_todo_ctx.lock);
    if (s_todo_ctx.initialized) {
        todo_mgr_save();
        s_todo_ctx.initialized = false;
    }
    pthread_mutex_unlock(&s_todo_ctx.lock);
}

size_t todo_mgr_get_all(todo_item_t *out_items, size_t max_count)
{
    if (!out_items || max_count == 0) return 0;

    pthread_mutex_lock(&s_todo_ctx.lock);
    size_t copy_cnt = (s_todo_ctx.count < max_count) ? s_todo_ctx.count : max_count;
    for (size_t i = 0; i < copy_cnt; i++) {
        memcpy(&out_items[i], &s_todo_ctx.items[i], sizeof(todo_item_t));
    }
    pthread_mutex_unlock(&s_todo_ctx.lock);
    return copy_cnt;
}

size_t todo_mgr_get_counts(size_t *out_done)
{
    pthread_mutex_lock(&s_todo_ctx.lock);
    size_t total = s_todo_ctx.count;
    size_t done = 0;
    for (size_t i = 0; i < total; i++) {
        if (s_todo_ctx.items[i].done) done++;
    }
    pthread_mutex_unlock(&s_todo_ctx.lock);

    if (out_done) *out_done = done;
    return total;
}

int todo_mgr_add(const char *title, const char *time_str)
{
    if (!title || !title[0]) return -1;

    pthread_mutex_lock(&s_todo_ctx.lock);

    if (s_todo_ctx.count >= TODO_MAX_ITEMS) {
        pthread_mutex_unlock(&s_todo_ctx.lock);
        LOG_W(TAG, "待办事项列表已满 (上限 %d 条)", TODO_MAX_ITEMS);
        return -2;
    }

    int new_id = s_todo_ctx.next_id++;
    todo_item_t *it = &s_todo_ctx.items[s_todo_ctx.count];
    memset(it, 0, sizeof(todo_item_t));

    it->id = new_id;
    strncpy(it->title, title, TODO_TITLE_MAX - 1);
    if (time_str && time_str[0]) {
        strncpy(it->time_str, time_str, TODO_TIME_MAX - 1);
    } else {
        strncpy(it->time_str, "今日", TODO_TIME_MAX - 1);
    }
    it->done = false;
    it->created_at = (int64_t)time(NULL);

    s_todo_ctx.count++;
    todo_mgr_save();

    pthread_mutex_unlock(&s_todo_ctx.lock);

    LOG_I(TAG, "已新增待办 [#%d]: %s (时间: %s)", new_id, title, it->time_str);
    todo_mgr_notify_event(1, new_id);
    return new_id;
}

int todo_mgr_toggle(int id)
{
    pthread_mutex_lock(&s_todo_ctx.lock);
    int target_idx = -1;
    for (size_t i = 0; i < s_todo_ctx.count; i++) {
        if (s_todo_ctx.items[i].id == id) {
            target_idx = (int)i;
            break;
        }
    }

    if (target_idx < 0) {
        pthread_mutex_unlock(&s_todo_ctx.lock);
        return -1;
    }

    s_todo_ctx.items[target_idx].done = !s_todo_ctx.items[target_idx].done;
    bool status = s_todo_ctx.items[target_idx].done;
    todo_mgr_save();

    pthread_mutex_unlock(&s_todo_ctx.lock);

    LOG_I(TAG, "切换待办 [#%d] 状态: %s", id, status ? "已达成" : "未完成");
    todo_mgr_notify_event(2, id);
    return 0;
}

int todo_mgr_delete(int id)
{
    pthread_mutex_lock(&s_todo_ctx.lock);
    int target_idx = -1;
    for (size_t i = 0; i < s_todo_ctx.count; i++) {
        if (s_todo_ctx.items[i].id == id) {
            target_idx = (int)i;
            break;
        }
    }

    if (target_idx < 0) {
        pthread_mutex_unlock(&s_todo_ctx.lock);
        return -1;
    }

    for (size_t i = (size_t)target_idx; i + 1 < s_todo_ctx.count; i++) {
        memcpy(&s_todo_ctx.items[i], &s_todo_ctx.items[i + 1], sizeof(todo_item_t));
    }
    s_todo_ctx.count--;
    todo_mgr_save();

    pthread_mutex_unlock(&s_todo_ctx.lock);

    LOG_I(TAG, "已删除待办 [#%d], 剩余: %zu", id, s_todo_ctx.count);
    todo_mgr_notify_event(3, id);
    return 0;
}

int todo_mgr_clear_done(void)
{
    pthread_mutex_lock(&s_todo_ctx.lock);
    size_t write_idx = 0;
    size_t removed = 0;

    for (size_t read_idx = 0; read_idx < s_todo_ctx.count; read_idx++) {
        if (!s_todo_ctx.items[read_idx].done) {
            if (write_idx != read_idx) {
                memcpy(&s_todo_ctx.items[write_idx], &s_todo_ctx.items[read_idx], sizeof(todo_item_t));
            }
            write_idx++;
        } else {
            removed++;
        }
    }

    s_todo_ctx.count = write_idx;
    if (removed > 0) {
        todo_mgr_save();
    }
    pthread_mutex_unlock(&s_todo_ctx.lock);

    if (removed > 0) {
        LOG_I(TAG, "已清理 %zu 条已完成待办，当前保留: %zu", removed, s_todo_ctx.count);
        todo_mgr_notify_event(4, 0);
    }
    return (int)removed;
}
