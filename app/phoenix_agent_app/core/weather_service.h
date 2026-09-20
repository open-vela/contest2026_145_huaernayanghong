/**
 * @file weather_service.h
 * @brief Phoenix HoloDesk-S1 极客网络天气服务 (Network Weather Subsystem)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_WEATHER_SERVICE_H
#define PHOENIX_WEATHER_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define WEATHER_CITY_DEFAULT "上海"

/**
 * @brief 实时天气指标结构体
 */
typedef struct {
    char    city[32];          /**< 城市名 (如 "上海") */
    char    condition[32];     /**< 天气状况 (如 "晴", "多云", "阴", "小雨", "雷雨") */
    char    icon_symbol[16];   /**< 紧凑状态符号 */
    int     temp_c;            /**< 实时温度 (摄氏度) */
    int     temp_min;          /**< 当日预报最低温 */
    int     temp_max;          /**< 当日预报最高温 */
    int     humidity;          /**< 相对湿度百分比 (0~100) */
    int64_t update_time;       /**< 上次成功同步的本地时间戳 */
    bool    is_valid;          /**< 天气数据是否真实有效 (非全零占位) */
    bool    is_fetching;       /**< 是否正在网络异步抓取中 */
} weather_info_t;

/**
 * @brief 初始化天气服务，监听网络连通事件并载入持久化城市配置
 * @return 0 成功, 负数失败
 */
int weather_service_init(void);

/**
 * @brief 反初始化天气服务
 */
void weather_service_deinit(void);

/**
 * @brief 获取当前天气数据快照
 * @param out_info 输出结构体指针
 * @return 0 成功, 负数未就绪
 */
int weather_service_get_info(weather_info_t *out_info);

/**
 * @brief 设置目标天气城市并存入系统配置 (触发即时网络异步拉取)
 * @param city 城市名称 (中文如 "上海", "北京", 或英文拼音 "Shanghai")
 * @return 0 成功, 负数失败
 */
int weather_service_set_city(const char *city);

/**
 * @brief 获取当前设置的城市名称
 * @return 城市字符串指针
 */
const char* weather_service_get_city(void);

/**
 * @brief 触发后台异步线程拉取最新天气 (非阻塞)
 * @return 0 成功拉起任务, 负数失败
 */
int weather_service_fetch_async(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_WEATHER_SERVICE_H */
