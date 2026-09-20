/**
 * @file ble_prov_service.h
 * @brief 基于 Web Bluetooth / BLE GATT 的配网与智能体参数配置服务
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef BLE_PROV_SERVICE_H
#define BLE_PROV_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief BLE 配网服务 UUID 定义 (16-bit 别名映射标准 Bluetooth SIG Base UUID)
 * Base: 0000xxxx-0000-1000-8000-00805F9B34FB
 */
#define BLE_PROV_SERVICE_UUID_16       0xFFE0
#define BLE_PROV_CHAR_WRITE_UUID_16    0xFFE1  /**< Web -> Board: 写入命令与 Wi-Fi/LLM 配置 */
#define BLE_PROV_CHAR_NOTIFY_UUID_16   0xFFE2  /**< Board -> Web: 实时通知连网状态/扫描结果/IP */
#define BLE_PROV_CHAR_INFO_UUID_16     0xFFE3  /**< Board -> Web: 只读设备硬件型号与固件版本 */

#define BLE_PROV_DEFAULT_DEV_NAME      "Phoenix-Setup"

/**
 * @brief 配网服务运行状态
 */
typedef enum {
    BLE_PROV_STATE_IDLE = 0,
    BLE_PROV_STATE_STARTING,       /**< 正在使能底层蓝牙适配器与初始化 GATT */
    BLE_PROV_STATE_ADVERTISING,    /**< 正在广播等待网页连接 */
    BLE_PROV_STATE_CONNECTED,      /**< 网页蓝牙已建立 GATT 连接 */
    BLE_PROV_STATE_PROVISIONING,   /**< 收到配网凭证，正在尝试加入 Wi-Fi */
    BLE_PROV_STATE_PROVISIONED,    /**< 连网成功并获得 IP */
    BLE_PROV_STATE_STOPPED         /**< 广播停止 */
} ble_prov_state_t;

/**
 * @brief 初始化并启动 BLE 配网服务 (同步阻塞等待就绪)
 * @param custom_dev_name 自定义广播设备名 (为 NULL 则使用默认 Phoenix-Setup)
 * @return 0 成功, 负数失败
 */
int ble_prov_service_init(const char *custom_dev_name);

/**
 * @brief 异步启动 BLE 配网服务 (非阻塞，适用于 UI 事件驱动，后台线程等待蓝牙就绪)
 * @param custom_dev_name 自定义广播设备名 (为 NULL 则使用默认 Phoenix-Setup)
 * @return 0 成功启动后台任务, 负数失败
 */
int ble_prov_service_start_async(const char *custom_dev_name);

/**
 * @brief 停止并注销 BLE 配网服务
 */
void ble_prov_service_deinit(void);

/**
 * @brief 获取当前 BLE 配网服务状态
 */
ble_prov_state_t ble_prov_service_get_state(void);

/**
 * @brief 获取当前广播设备名
 */
int ble_prov_service_get_dev_name(char *buf, size_t max_len);

/**
 * @brief 检查配网服务是否处于活动状态 (广播中/已连接/配网中)
 */
bool ble_prov_service_is_active(void);

/**
 * @brief 触发向已连接的 Web 客户端主动推送网络状态与 IP 地址
 * @param state 状态字符串 ("connecting", "connected", "error", "disconnected")
 * @param ssid 当前 Wi-Fi 名称
 * @param ip 当前分配到的 IP 地址
 * @param msg 提示文本
 * @return 0 成功, 负数失败
 */
int ble_prov_service_notify_net_status(const char *state, const char *ssid, const char *ip, const char *msg);

/**
 * @brief 触发向已连接的 Web 客户端主动推送扫描到的 Wi-Fi 列表
 * @return 扫描并推送成功的热点数量
 */
int ble_prov_service_notify_wifi_scan(void);

/**
 * @brief 触发向已连接的客户端回传当前已保存的完整配置信息 (Wi-Fi/API Key掩码/Model/Prompt)
 * @return 0 成功, 负数失败
 */
int ble_prov_service_notify_config(void);

/**
 * @brief 触发向已连接客户端回传设备硬件与系统遥测信息 (固件/架构/TF卡/内存)
 * @return 0 成功, 负数失败
 */
int ble_prov_service_notify_system_info(void);

/**
 * @brief 向客户端发送通用操作回执 ACK
 * @param event_name 事件名称 (如 "config_saved")
 * @param success 是否成功
 * @param msg 说明文本
 * @return 0 成功, 负数失败
 */
int ble_prov_service_notify_ack(const char *event_name, bool success, const char *msg);

/**
 * @brief 独立处理传入的 JSON 指令字符串 (支持命令管道与测试注入)
 * @param cmd_json_str 待解析的 JSON 指令
 * @return 0 成功, 负数失败
 */
int ble_prov_service_handle_command(const char *cmd_json_str);

#ifdef __cplusplus
}
#endif

#endif /* BLE_PROV_SERVICE_H */
