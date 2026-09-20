/**
 * @file api_todo.c
 * @brief Phoenix HoloDesk-S1 Web 伴侣待办事项 RESTful API 控制器
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_api.h"
#include "../todo_mgr.h"
#include "../../utils/log_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "ApiTodo"

int handle_todo_list(const http_req_t *req, http_resp_t *resp)
{
    (void)req;

    todo_item_t items[TODO_MAX_ITEMS];
    size_t count = todo_mgr_get_all(items, TODO_MAX_ITEMS);
    size_t done_cnt = 0;
    todo_mgr_get_counts(&done_cnt);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        http_resp_error(resp, 500, "JSON alloc failed");
        return 0;
    }

    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddNumberToObject(root, "total_count", (int)count);
    cJSON_AddNumberToObject(root, "done_count", (int)done_cnt);

    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        cJSON *it = cJSON_CreateObject();
        cJSON_AddNumberToObject(it, "id", items[i].id);
        cJSON_AddStringToObject(it, "title", items[i].title);
        cJSON_AddStringToObject(it, "time", items[i].time_str);
        cJSON_AddBoolToObject(it, "done", items[i].done);
        cJSON_AddNumberToObject(it, "created_at", (double)items[i].created_at);
        cJSON_AddItemToArray(arr, it);
    }
    cJSON_AddItemToObject(root, "todos", arr);

    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_todo_add(const http_req_t *req, http_resp_t *resp)
{
    cJSON *root = req->json;
    bool need_delete = false;
    if (!root && req->body && req->body_len > 0) {
        root = cJSON_Parse(req->body);
        need_delete = true;
    }

    if (!root) {
        http_resp_error(resp, 400, "Missing or invalid JSON body");
        return 0;
    }

    cJSON *t_item = cJSON_GetObjectItem(root, "title");
    cJSON *time_item = cJSON_GetObjectItem(root, "time");

    if (!t_item || !t_item->valuestring || !t_item->valuestring[0]) {
        if (need_delete) cJSON_Delete(root);
        http_resp_error(resp, 400, "Missing title");
        return 0;
    }

    const char *title = t_item->valuestring;
    const char *time_str = (time_item && time_item->valuestring) ? time_item->valuestring : "今日";

    int new_id = todo_mgr_add(title, time_str);
    if (need_delete) cJSON_Delete(root);

    cJSON *res_obj = cJSON_CreateObject();
    if (new_id > 0) {
        cJSON_AddBoolToObject(res_obj, "success", true);
        cJSON_AddNumberToObject(res_obj, "id", new_id);
        cJSON_AddStringToObject(res_obj, "msg", "待办添加成功");
        http_resp_json_obj(resp, 200, res_obj);
    } else {
        cJSON_AddBoolToObject(res_obj, "success", false);
        cJSON_AddStringToObject(res_obj, "error", "待办数量已达上限");
        http_resp_json_obj(resp, 400, res_obj);
    }
    return 0;
}

int handle_todo_toggle(const http_req_t *req, http_resp_t *resp)
{
    cJSON *root = req->json;
    bool need_delete = false;
    if (!root && req->body && req->body_len > 0) {
        root = cJSON_Parse(req->body);
        need_delete = true;
    }

    if (!root) {
        http_resp_error(resp, 400, "Missing or invalid JSON body");
        return 0;
    }

    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    if (!id_item || id_item->type != cJSON_Number) {
        if (need_delete) cJSON_Delete(root);
        http_resp_error(resp, 400, "Missing or invalid id");
        return 0;
    }

    int id = id_item->valueint;
    if (need_delete) cJSON_Delete(root);

    int ret = todo_mgr_toggle(id);

    cJSON *res_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(res_obj, "success", ret == 0);
    cJSON_AddNumberToObject(res_obj, "id", id);
    if (ret != 0) {
        cJSON_AddStringToObject(res_obj, "error", "待办条目不存在");
        http_resp_json_obj(resp, 404, res_obj);
    } else {
        http_resp_json_obj(resp, 200, res_obj);
    }
    return 0;
}

int handle_todo_delete(const http_req_t *req, http_resp_t *resp)
{
    cJSON *root = req->json;
    bool need_delete = false;
    if (!root && req->body && req->body_len > 0) {
        root = cJSON_Parse(req->body);
        need_delete = true;
    }

    if (!root) {
        http_resp_error(resp, 400, "Missing or invalid JSON body");
        return 0;
    }

    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    if (!id_item || id_item->type != cJSON_Number) {
        if (need_delete) cJSON_Delete(root);
        http_resp_error(resp, 400, "Missing or invalid id");
        return 0;
    }

    int id = id_item->valueint;
    if (need_delete) cJSON_Delete(root);

    int ret = todo_mgr_delete(id);

    cJSON *res_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(res_obj, "success", ret == 0);
    cJSON_AddNumberToObject(res_obj, "id", id);
    if (ret != 0) {
        cJSON_AddStringToObject(res_obj, "error", "待办条目不存在");
        http_resp_json_obj(resp, 404, res_obj);
    } else {
        http_resp_json_obj(resp, 200, res_obj);
    }
    return 0;
}

int handle_todo_clear_done(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    int cleared = todo_mgr_clear_done();

    cJSON *res_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(res_obj, "success", true);
    cJSON_AddNumberToObject(res_obj, "cleared_count", cleared);

    http_resp_json_obj(resp, 200, res_obj);
    return 0;
}
