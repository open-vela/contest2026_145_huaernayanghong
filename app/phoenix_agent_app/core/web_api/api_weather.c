/**
 * @file api_weather.c
 * @brief Phoenix HoloDesk-S1 Web 伴侣天气查询与配置 RESTful API 控制器
 * @author OpenVela Contest 2026 Team 145
 */

#include "web_api.h"
#include "../weather_service.h"
#include "../../utils/log_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "ApiWeather"

int handle_weather_status(const http_req_t *req, http_resp_t *resp)
{
    (void)req;
    weather_info_t info;
    memset(&info, 0, sizeof(info));
    weather_service_get_info(&info);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        http_resp_error(resp, 500, "JSON alloc failed");
        return 0;
    }

    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "city", info.city);
    cJSON_AddStringToObject(root, "condition", info.condition);
    cJSON_AddNumberToObject(root, "temp_c", info.temp_c);
    cJSON_AddNumberToObject(root, "temp_min", info.temp_min);
    cJSON_AddNumberToObject(root, "temp_max", info.temp_max);
    cJSON_AddNumberToObject(root, "humidity", info.humidity);
    cJSON_AddBoolToObject(root, "is_valid", info.is_valid);
    cJSON_AddBoolToObject(root, "is_fetching", info.is_fetching);
    cJSON_AddNumberToObject(root, "update_time", (double)info.update_time);

    http_resp_json_obj(resp, 200, root);
    return 0;
}

int handle_weather_config(const http_req_t *req, http_resp_t *resp)
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

    cJSON *city_item = cJSON_GetObjectItem(root, "city");
    if (!city_item || !city_item->valuestring || !city_item->valuestring[0]) {
        if (need_delete) cJSON_Delete(root);
        http_resp_error(resp, 400, "Missing city string");
        return 0;
    }

    char city_name[64];
    strncpy(city_name, city_item->valuestring, sizeof(city_name) - 1);
    city_name[sizeof(city_name) - 1] = '\0';
    if (need_delete) cJSON_Delete(root);

    weather_service_set_city(city_name);

    cJSON *ret_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(ret_obj, "success", true);
    cJSON_AddStringToObject(ret_obj, "city", city_name);
    cJSON_AddStringToObject(ret_obj, "msg", "城市已更新并触发异步网络拉取");

    http_resp_json_obj(resp, 200, ret_obj);
    return 0;
}
