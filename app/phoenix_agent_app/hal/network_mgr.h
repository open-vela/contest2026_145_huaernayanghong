/**
 * @file network_mgr.h
 * @brief 网络连接与 SoftAP 双模管理组件 (Network State Machine & SoftAP Portal)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef NETWORK_MGR_H
#define NETWORK_MGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NET_MAX_SSID_LEN 32
#define NET_MAX_PSK_LEN  64
#define NET_MAX_IP_LEN   32

#define NET_DEFAULT_SOFTAP_SSID "Gemini-Agent-Setup"
#define NET_DEFAULT_SOFTAP_IP   "192.168.4.1"

/**
 * @brief 网络工作模式枚举
 */
typedef enum {
    NET_MODE_DISCONNECTED = 0,   /**< 未连接网络 */
    NET_MODE_STA_CONNECTING = 1, /**< 正在握手连接家庭/办公 Wi-Fi */
    NET_MODE_STA_CONNECTED = 2,  /**< 已连入局域网 (获取到路由器分配 IP) */
    NET_MODE_SOFTAP_CONFIG = 3   /**< SoftAP 独立热点配网模式 (广播热点，IP: 192.168.4.1) */
} net_mode_t;

/**
 * @brief 扫描到的周边 Wi-Fi 热点信息
 */
typedef struct {
    char   ssid[NET_MAX_SSID_LEN];  /**< 热点 SSID */
    int8_t rssi;                    /**< 信号强度 dBm，如 -45, -70 */
    char   auth[16];                /**< 加密认证方式: WPA2, WPA3, OPEN 等 */
} net_wifi_ap_info_t;

/**
 * @brief 网络事件回调
 */
typedef void (*net_state_cb_t)(net_mode_t mode, const char *ip, void *user_data);

/**
 * @brief 扫描周围可用的 Wi-Fi 热点列表
 * @param aps_out 输出热点数组
 * @param max_count 数组最大容纳数量
 * @return 实际扫描到的热点数量 (>= 0), 负数失败
 */
int net_mgr_scan_wifi(net_wifi_ap_info_t *aps_out, size_t max_count);

/**
 * @brief 在纯 STA 模式或未拉起 SoftAP 前执行空中 Wi-Fi 预扫描并更新缓存
 *        (避免在 SoftAP 活跃发射时跳频打断 Beacon 导致手机断网)
 * @return 扫描到的热点数量 (>= 0), 负数失败
 */
int net_mgr_prescan_wifi(void);

/**
 * @brief 初始化网络管理器并根据本地存储配置决定联网或进入配网
 * @return 0 成功, 负数失败
 */
int net_mgr_init(void);

/**
 * @brief 反初始化网络管理器
 */
void net_mgr_deinit(void);

/**
 * @brief 启动 SoftAP 独立配网热点
 * @param custom_ssid 自定义热点名称 (传 NULL 则使用默认 "Gemini-Agent-S1")
 * @return 0 成功, 负数失败
 */
int net_mgr_start_softap(const char *custom_ssid);

/**
 * @brief 关闭 SoftAP 独立热点与内嵌 DHCP 服务
 * @return 0 成功, 负数失败
 */
int net_mgr_stop_softap(void);

/**
 * @brief 在 BLE 配网握手期间挂起 SoftAP 射频广播，避免单天线同频干扰 (Coex 仲裁)
 * @return 0 成功
 */
int net_mgr_suspend_softap(void);

/**
 * @brief 当 BLE 配网中断或需要热点备用时恢复 SoftAP 热点广播
 * @return 0 成功
 */
int net_mgr_resume_softap(void);

/**
 * @brief 连接指定 Wi-Fi 路由热点 (STA 模式)
 * @param ssid Wi-Fi 名称
 * @param psk 密码 (可为空)
 * @return 0 成功, 负数失败
 */
int net_mgr_connect_sta(const char *ssid, const char *psk);

/**
 * @brief 断开网络
 */
void net_mgr_disconnect(void);

/**
 * @brief 清空本地 Wi-Fi 配置并重置为断开态 (不自动拉起热点)
 * @return 0 成功
 */
int net_mgr_clear_config(void);

/**
 * @brief 一键清除 Wi-Fi 配置并重置切回 SoftAP 配网热点
 * @return 0 成功
 */
int net_mgr_reset_to_softap(void);

/**
 * @brief 校验 IP 是否为真实有效的路由器分配 STA 局域网 IP
 *        (自动过滤 0.0.0.0, 127.0.0.1, 192.168.4.1 热点以及内核默认静态占位 10.0.0.2, 10.0.2.15 等)
 * @param ip IP 字符串
 * @return true 真实有效, false 属于占位/未联网/热点网段
 */
bool net_is_valid_sta_ip(const char *ip);

/**
 * @brief 获取当前网络模式
 * @return net_mode_t
 */
net_mode_t net_mgr_get_mode(void);

/**
 * @brief 获取当前设备 IP 地址
 * @param buf 存储 IP 字符串缓冲区
 * @param max_len 缓冲区大小
 * @return 0 成功, 负数失败
 */
int net_mgr_get_ip(char *buf, size_t max_len);

/**
 * @brief 获取当前连接的 SSID 或广播的热点名
 * @param buf 缓冲区
 * @param max_len 缓冲区大小
 * @return 0 成功, 负数失败
 */
int net_mgr_get_ssid(char *buf, size_t max_len);

/**
 * @brief 注册网络状态变动监听回调
 * @param cb 回调函数
 * @param user_data 回调私有数据
 */
void net_mgr_register_state_cb(net_state_cb_t cb, void *user_data);

/**
 * @brief 开关局域网 Web 伴侣服务 (按需开启省电防扫描)
 * @param enabled true 开启, false 关闭
 * @return 0 成功
 */
int net_mgr_set_web_enabled(bool enabled);

/**
 * @brief 查询 Web 伴侣服务是否允许开启
 * @return true 允许, false 禁用
 */
bool net_mgr_is_web_enabled(void);

/**
 * @brief 触发后台异步 Wi-Fi 硬件扫描 (非阻塞，扫描结果将写入内部缓存)
 * @return 0 成功启动扫描或已有扫描进行中, 负数失败
 */
int net_mgr_trigger_async_scan(void);

/**
 * @brief 查询当前是否正在后台执行 Wi-Fi 扫描
 * @return true 正在扫描中, false 空闲
 */
bool net_mgr_is_scanning(void);

/**
 * @brief 获取最新扫描到的周边 Wi-Fi 热点缓存快照 (立即返回，绝不阻塞)
 * @param aps_out 输出热点数组
 * @param max_count 数组最大容纳数量
 * @return 实际返回的热点数量 (>= 0)
 */
int net_mgr_get_cached_scan_results(net_wifi_ap_info_t *aps_out, size_t max_count);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_MGR_H */
