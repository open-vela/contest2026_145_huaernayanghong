/**
 * @file api_sdcard.c
 * @brief TF Card / MicroSD File System Web API Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_api.h"
#include "../../hal/hal_sdcard.h"
#include "../../hal/hal_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

int handle_sdcard_status(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    hal_sdcard_info_t info;
    memset(&info, 0, sizeof(info));
    bool mounted = hal_sdcard_is_mounted();
    if (mounted) {
        hal_sdcard_get_info(&info);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddBoolToObject(root, "mounted", mounted);
    cJSON_AddStringToObject(root, "mount_point", info.mount_point);
    cJSON_AddNumberToObject(root, "total_mb", (double)info.total_mb);
    cJSON_AddNumberToObject(root, "free_mb", (double)info.free_mb);
    cJSON_AddNumberToObject(root, "used_mb", (double)info.used_mb);
    cJSON_AddNumberToObject(root, "used_pct", (double)info.used_pct);

    cJSON *rec_dirs = cJSON_CreateArray();
    cJSON_AddItemToArray(rec_dirs, cJSON_CreateString("sounds"));
    cJSON_AddItemToArray(rec_dirs, cJSON_CreateString("logs"));
    cJSON_AddItemToArray(rec_dirs, cJSON_CreateString("docs"));
    cJSON_AddItemToArray(rec_dirs, cJSON_CreateString("models"));
    cJSON_AddItemToObject(root, "recommend_dirs", rec_dirs);

    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_sdcard_list(const http_req_t *req, http_resp_t *resp)
{
    char req_path[256] = "/";
    http_req_get_query_param(req, "path", req_path, sizeof(req_path));
    if (req_path[0] == '\0') strcpy(req_path, "/");

    char full_path[512] = {0};
    if (hal_sdcard_resolve_path(req_path, full_path, sizeof(full_path)) != 0) {
        http_resp_error(resp, 400, "Path forbidden or invalid");
        return 0;
    }

    DIR *d = opendir(full_path);
    if (!d) {
        http_resp_error(resp, 404, "Cannot open directory (unmounted?)");
        return 0;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "current_path", req_path);
    cJSON *items = cJSON_CreateArray();

    struct dirent *de;
    int count = 0;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        char sub_path[512];
        snprintf(sub_path, sizeof(sub_path), "%s/%s", full_path, de->d_name);
        struct stat st;
        bool is_dir = false;
        size_t size = 0;
        long mtime = 0;
        if (stat(sub_path, &st) == 0) {
            is_dir = S_ISDIR(st.st_mode);
            size = (size_t)st.st_size;
            mtime = (long)st.st_mtime;
        }

        cJSON *it = cJSON_CreateObject();
        cJSON_AddStringToObject(it, "name", de->d_name);
        cJSON_AddBoolToObject(it, "is_dir", is_dir);
        cJSON_AddNumberToObject(it, "size", (double)size);
        cJSON_AddNumberToObject(it, "mtime", (double)mtime);
        cJSON_AddItemToArray(items, it);
        count++;
    }
    closedir(d);

    cJSON_AddItemToObject(root, "items", items);
    cJSON_AddNumberToObject(root, "total", count);

    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_sdcard_download(const http_req_t *req, http_resp_t *resp)
{
    char req_path[256] = {0};
    http_req_get_query_param(req, "path", req_path, sizeof(req_path));
    char full_path[512] = {0};
    if (req_path[0] == '\0' || hal_sdcard_resolve_path(req_path, full_path, sizeof(full_path)) != 0) {
        http_resp_error(resp, 400, "Invalid target file path");
        return 0;
    }

    struct stat st;
    if (stat(full_path, &st) != 0 || S_ISDIR(st.st_mode)) {
        http_resp_error(resp, 404, "File not found or is a directory");
        return 0;
    }

    FILE *fp = fopen(full_path, "rb");
    if (!fp) {
        http_resp_error(resp, 500, "Cannot open file");
        return 0;
    }

    const char *fname = strrchr(req_path, '/');
    fname = fname ? (fname + 1) : req_path;

    /* 读取文件内容并写入响应 */
    size_t file_size = (size_t)st.st_size;
    void *file_data = malloc(file_size + 1);
    if (!file_data) {
        fclose(fp);
        http_resp_error(resp, 500, "Out of memory");
        return 0;
    }

    size_t nread = fread(file_data, 1, file_size, fp);
    fclose(fp);

    http_resp_file_stream(resp, 200, "application/octet-stream", fname, file_data, nread);
    free(file_data);
    return 0;
}

int handle_sdcard_upload(const http_req_t *req, http_resp_t *resp)
{
    char req_path[256] = {0};
    char full_path[512] = {0};
    const char *content_ptr = NULL;
    size_t content_len = 0;

    if (req->json) {
        cJSON *p = cJSON_GetObjectItem(req->json, "path");
        cJSON *c = cJSON_GetObjectItem(req->json, "content");
        if (p && p->valuestring) strncpy(req_path, p->valuestring, sizeof(req_path) - 1);
        if (c && c->valuestring) {
            content_ptr = c->valuestring;
            content_len = strlen(content_ptr);
        }
    }

    if (req_path[0] == '\0') {
        http_req_get_query_param(req, "path", req_path, sizeof(req_path));
    }

    if (req_path[0] == '\0' || hal_sdcard_resolve_path(req_path, full_path, sizeof(full_path)) != 0) {
        http_resp_error(resp, 400, "Invalid target file path");
        return 0;
    }

    /* 确保父目录就绪 */
    char parent_dir[512];
    strncpy(parent_dir, full_path, sizeof(parent_dir) - 1);
    char *slash = strrchr(parent_dir, '/');
    if (slash) {
        *slash = '\0';
        hal_system_mkdir_p(parent_dir, 0755);
    }

    FILE *fp = fopen(full_path, "wb");
    if (!fp) {
        http_resp_error(resp, 500, "Cannot open destination file for writing");
        return 0;
    }

    size_t written = 0;
    if (content_ptr && content_len > 0) {
        written = fwrite(content_ptr, 1, content_len, fp);
    }
    fclose(fp);

    char resp_json[256];
    snprintf(resp_json, sizeof(resp_json),
             "{\"success\":true,\"path\":\"%s\",\"bytes_written\":%zu}",
             req_path, written);
    http_resp_json(resp, 200, resp_json);
    return 0;
}

int handle_sdcard_delete(const http_req_t *req, http_resp_t *resp)
{
    char req_path[256] = {0};
    if (req->json) {
        cJSON *p = cJSON_GetObjectItem(req->json, "path");
        if (p && p->valuestring) strncpy(req_path, p->valuestring, sizeof(req_path) - 1);
    }
    if (req_path[0] == '\0') {
        http_req_get_query_param(req, "path", req_path, sizeof(req_path));
    }

    char full_path[512] = {0};
    if (req_path[0] == '\0' || hal_sdcard_resolve_path(req_path, full_path, sizeof(full_path)) != 0) {
        http_resp_error(resp, 400, "Invalid path");
        return 0;
    }

    if (strcmp(req_path, "/") == 0 || strlen(req_path) <= 1) {
        http_resp_error(resp, 403, "Root directory cannot be deleted");
        return 0;
    }

    struct stat st;
    int ret = -1;
    if (stat(full_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            ret = rmdir(full_path);
        } else {
            ret = unlink(full_path);
        }
    }

    if (ret == 0) {
        http_resp_json(resp, 200, "{\"success\":true,\"message\":\"Deleted successfully\"}");
    } else {
        http_resp_error(resp, 500, "Failed to delete (is non-empty directory?)");
    }
    return 0;
}

int handle_sdcard_mkdir(const http_req_t *req, http_resp_t *resp)
{
    char req_path[256] = {0};
    if (req->json) {
        cJSON *p = cJSON_GetObjectItem(req->json, "path");
        if (p && p->valuestring) strncpy(req_path, p->valuestring, sizeof(req_path) - 1);
    }
    if (req_path[0] == '\0') {
        http_req_get_query_param(req, "path", req_path, sizeof(req_path));
    }

    char full_path[512] = {0};
    if (req_path[0] == '\0' || hal_sdcard_resolve_path(req_path, full_path, sizeof(full_path)) != 0) {
        http_resp_error(resp, 400, "Invalid directory path");
        return 0;
    }

    int ret = hal_system_mkdir_p(full_path, 0755);
    if (ret == 0) {
        http_resp_json(resp, 200, "{\"success\":true,\"message\":\"Directory created\"}");
    } else {
        http_resp_error(resp, 500, "Failed to create directory");
    }
    return 0;
}

/* =========================================================================
 * 统一静态路由表注册
 * ========================================================================= */
static const http_route_t g_web_api_routes[] = {
    /* 1. 系统与静态页面 */
    { HTTP_METHOD_GET,  "/",                             false, handle_system_root },
    { HTTP_METHOD_GET,  "/dashboard",                    false, handle_system_dashboard },
    { HTTP_METHOD_GET,  "/index.html",                   false, handle_system_dashboard },
    { HTTP_METHOD_GET,  "/setup",                        false, handle_system_setup },
    { HTTP_METHOD_GET,  "/setup.html",                   false, handle_system_setup },
    { HTTP_METHOD_GET,  "/ble_setup",                    false, handle_system_ble_setup },
    { HTTP_METHOD_GET,  "/ble_setup.html",               false, handle_system_ble_setup },
    { HTTP_METHOD_GET,  "/hotspot-detect",               false, handle_system_captive_probe },
    { HTTP_METHOD_GET,  "/generate_204",                 false, handle_system_captive_probe },
    { HTTP_METHOD_GET,  "/gen_204",                      false, handle_system_captive_probe },
    { HTTP_METHOD_GET,  "/canonical.html",               false, handle_system_captive_probe },
    { HTTP_METHOD_GET,  "/ncsi.txt",                     false, handle_system_captive_probe },
    { HTTP_METHOD_GET,  "/connecttest.txt",              false, handle_system_captive_probe },
    { HTTP_METHOD_GET,  "/library/test/success.html",     false, handle_system_captive_probe },
    { HTTP_METHOD_GET,  "/api/status",                   false, handle_system_status },
    { HTTP_METHOD_POST, "/api/proactive",                false, handle_system_proactive },

    /* 2. Wi-Fi 配网 */
    { HTTP_METHOD_GET,  "/api/wifi/scan",                false, handle_wifi_scan },
    { HTTP_METHOD_GET,  "/api/wifi/status",              false, handle_wifi_status },
    { HTTP_METHOD_POST, "/api/wifi/connect",             false, handle_wifi_connect },
    { HTTP_METHOD_POST, "/api/wifi/reset",               false, handle_wifi_reset },

    /* 3. 卡带调度与动作 */
    { HTTP_METHOD_POST, "/api/cartridge/switch",         false, handle_cartridge_switch },
    { HTTP_METHOD_POST, "/api/memo/add",                 false, handle_memo_add },
    { HTTP_METHOD_POST, "/api/action",                   false, handle_action_dispatch },

    /* 4. 配置中心 */
    { HTTP_METHOD_GET,  "/api/config",                   false, handle_config_get },
    { HTTP_METHOD_POST, "/api/config",                   false, handle_config_post },

    /* 5. 统一日志 */
    { HTTP_METHOD_GET,  "/api/logs",                     false, handle_logs_get },
    { HTTP_METHOD_POST, "/api/logs/level",               false, handle_logs_level_post },
    { HTTP_METHOD_POST, "/api/logs",                     false, handle_logs_level_post },

    /* 6. TF 卡存储管理 */
    { HTTP_METHOD_GET,  "/api/sdcard/status",            false, handle_sdcard_status },
    { HTTP_METHOD_GET,  "/api/sdcard/list",              false, handle_sdcard_list },
    { HTTP_METHOD_GET,  "/api/sdcard/download",          false, handle_sdcard_download },
    { HTTP_METHOD_POST, "/api/sdcard/upload",            false, handle_sdcard_upload },
    { HTTP_METHOD_POST, "/api/sdcard/delete",            false, handle_sdcard_delete },
    { HTTP_METHOD_POST, "/api/sdcard/mkdir",             false, handle_sdcard_mkdir },
};

void web_api_register_all(void)
{
    web_router_register_table(g_web_api_routes, sizeof(g_web_api_routes) / sizeof(g_web_api_routes[0]));
}
