---
name: openvela-wifi-network
description: "OpenVela / 全志 R528-S3 (Gemini-S1) Wi-Fi 网络配置、WAPI 命令行调试、SoftAP 热点配网与 STA 自动连接开发指南。"
---

# OpenVela Wi-Fi 网络配置与 SoftAP/STA 配网开发指南

本指南专为运行在全志 R528-S3 (Gemini-S1) 平台上的 OpenVela 实时操作系统定制，涵盖 Wi-Fi 驱动架构、WAPI 命令行工具、SoftAP 独立配网与 STA 模式自动切换流程。

---

## 1. 核心架构与配置文件

Gemini-S1 开发板板载全志无线芯片，通过 `WAPI` (Wireless API) 适配层对接 `wpa_supplicant` 和 NuttX 网络协议栈。

### 1.1 关键配置文件与路径
* **持久化配置文件**：`/data/etc/wifi/wapi.conf`（以 JSON 格式存储 SSID、密码及 BSSID）
* **启动脚本**：`/etc/wifi/start_wifi.sh`（源码位于 `vendor/allwinnertech/boards/r528/r528s3-gemini-s1/src/etc/wifi/start_wifi.sh`）
* **工程网络管理器**：`app/phoenix_agent_app/hal/network_mgr.c`

### 1.2 `wapi.conf` 配置规范
```json
{
  "ssid": "MyHome_WiFi",
  "psk": "password123456",
  "bssid": ""
}
```

---

## 2. 常用终端调试命令 (NSH)

在板端 NSH 串口终端中，使用 `wapi` 工具管理无线网卡 `wlan0`：

### 2.1 查看网络状态与网卡信息
```bash
# 查看 wlan0 当前连接状态、信号强度与 IP
wapi show wlan0

# 查看 IP 地址与网卡配置
ifconfig wlan0
```

### 2.2 手动连接 Wi-Fi (STA 模式)
```bash
# 1. 断开当前连接
wapi disconnect wlan0
sleep 1

# 2. 设置目标 SSID (末尾 1 表示开启)
wapi essid wlan0 "MyHome_WiFi" 1

# 3. 设置密码 (参数: 接口名 密码 开启标识 加密类型; 3 代表 WPA2-PSK)
wapi psk wlan0 "password123456" 1 3

# 4. (可选) 锁定 5GHz 频点或指定 AP BSSID
# wapi ap wlan0 12:34:56:78:9a:bc
# wapi freq wlan0 5220 1

# 5. 关闭自适应与省电模式（提升长连接稳定性）
wapi private wlan0 adaptivity 0
wapi power_save wlan0 off

# 6. 保存并重连
wapi save_config wlan0
wapi reconnect wlan0

# 7. 等待 3~5 秒后申请 DHCP IP
renew wlan0
```

### 2.3 快速断网与重置
```bash
wapi disconnect wlan0
wapi power_save wlan0 on
```

---

## 3. SoftAP 热点配网与 STA 双模流转

为实现设备免串口开箱配网，系统支持两阶段自适应状态机：

```text
[开机检测]
   │
   ├─> 本地有保存的 SSID? ─(是)─> [STA 模式: 尝试连路由器] ─(成功)─> [正常工作: 显示内网 IP]
   │                                           │ (多次重试失败)
   │                                           ▼
   └─(否)──────────────────────────────> [SoftAP 独立热点配网模式]
                                               │ (发射 SSID: Gemini-Agent-XX)
                                               │ (网关 IP: 192.168.4.1)
                                               ▼
                                      [手机连热点访问 Web Setup]
                                               │ (提交目标 Wi-Fi 与密码)
                                               ▼
                                      [写入 /data 配置文件并重启网络]
```

### 3.1 代码层集成 API (`network_mgr.h`)
在业务应用代码中通过统一的网络管理器进行控制：

```c
#include "hal/network_mgr.h"

// 1. 初始化网络管理器 (自动根据配置启动 STA 或 SoftAP)
net_mgr_init();

// 2. 注册状态变更回调 (通知 UI 胶囊状态或屏幕弹窗)
net_mgr_register_cb(on_network_status_change, NULL);

// 3. 手动切换至 SoftAP 热点配网
net_mgr_start_softap("Gemini-Agent-Setup");

// 4. 用户提交配网后，切换连入局域网
net_mgr_connect_sta("Office-WiFi", "office-pwd-2026");
```

---

## 4. 常见问题排查与避坑指南

1. **`renew wlan0` 返回错误或无法获取 IP**：
   - 驱动与 AP 完成关联通常需要 3~5 秒，关联未完成时执行 `renew` 会失败。建议编写重试循环（每次 sleep 2 秒，最多重试 3 次）。
2. **5GHz 频段容易断连**：
   - 全志无线芯片在部分弱信号场景自适应算法不稳定，可通过 `wapi private wlan0 adaptivity 0` 关闭自适应模式，提升 TCP/WebSocket 长连接质量。
3. **断电重启后 Wi-Fi 配置丢失**：
   - 确保配置文件写入路径为 `/data`（掉电不丢失的只读/读写挂载区），不可保存在临时目录 `/tmp` 或 `/var`。写入完成后建议执行 `sync` 刷盘。
