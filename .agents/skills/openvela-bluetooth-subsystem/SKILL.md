---
name: openvela-bluetooth-subsystem
description: "OpenVela / 全志 R528-S3 (Gemini-S1) 蓝牙子系统架构、bttool 命令行调试、应用层 API 开发与 RTL8723FS 蓝牙/Coex 驱动适配实战指南。"
---

# OpenVela 蓝牙子系统开发与驱动适配实践指南

本指南根据 OpenVela 官方《蓝牙适配指南》深度整理，面向全志 R528-S3 (Gemini-S1 / RTL8723FS) 平台，系统阐述蓝牙协议栈分层架构、Framework 与 IPC 通信机制、板端终端命令行工具调试、应用层 C 语言开发流程、以及底半部驱动与 Wi-Fi/BT 共存适配规范。

---

## 1. 核心协议栈架构与源码布局

OpenVela 蓝牙子系统已通过 Bluetooth 5.4 认证，全面支持 BR/EDR、BLE、GAP/GATT、A2DP、AVRCP、HFP、HID、SPP、PAN、LEA（低功耗音频）及 Mesh 网络。

```
┌─────────────────────────────────────────────────────────────┐
│                 Bluetooth Host (蓝牙主机层)                  │
│  ├── 业务应用 / Profile 协议 (A2DP / HFP / GATT / SPP)      │
│  ├── Framework 框架与服务 (bt_service, sal_*, bt_adapter)   │
│  └── Upper HCI (协议栈上层接口，如 external/zblue)            │
├──────────────────────────────┬──────────────────────────────┤
│                              │  HCI 传输接口 (H4 / H5 协议) │
├──────────────────────────────┴──────────────────────────────┤
│               BR/EDR/LE Controller (蓝牙控制器)              │
│  ├── Lower HCI (下层 HCI 驱动与固件命令解析)                 │
│  ├── Link Manager / Firmware (链路管理协议 LMP/LL，固件逻辑) │
│  └── Baseband Controller (基带控制器，2.4GHz 射频物理收发)   │
├─────────────────────────────────────────────────────────────┤
│                 Physical Bus (物理总线层)                   │
│  ├── 硬件总线：全志 R528 UART1 串口 (/dev/ttyS1) + CTS/RTS   │
│  └── 芯片平台：Realtek RTL8723FS Wi-Fi/BT Combo 模块        │
└─────────────────────────────────────────────────────────────┘
```

### 1.1 OpenVela 仓库分工与关键路径

| 仓库 / 目录路径 | 职能分工与核心内容 |
| :--- | :--- |
| `frameworks/connectivity/bluetooth/` | **Bluetooth Framework**：提供 Client API 接口层、后台 `bt_service` 守护进程、`sal` 协议栈适配层、HAL 抽象层以及 `bttool` 命令行调试工具。 |
| `external/zblue/` | **开源协议栈引擎**：源自 Zephyr 蓝牙协议栈，OpenVela 进行了性能优化与功能扩展，负责 HCI 上层到 L2CAP/ATT/SMP 的数据解析。 |
| `vendor/allwinnertech/chips/r528/components/bluetooth/` | **全志 R528 蓝牙芯片驱动**：包含 `rtk_hci.c`（UART/H5 传输层）、`rtk_coex.c`（Wi-Fi/BT 智能共存仲裁）与 `rtk_hci_board.c`（电源/复位管理）。 |
| `vendor/allwinnertech/apps/bt_instance/` | **板端自测与功能验证应用**：板载 `bt_start`、`bt_lightCtrl`、`gattc_op` 等测试用例。 |
| `/data/etc/bt/8723fs_btaddr.txt` | **持久化 MAC 地址配置**：板端保存出厂蓝牙 MAC 地址的文件路径。 |

---

## 2. Framework 框架层总体通信流程

OpenVela 蓝牙架构采用**客户端/服务端（C/S）分离与 IPC 通信设计**：

```
[ 蓝牙应用 Client ]
       │
       │  IPC (POSIX 消息队列 / 命名管道)
       ▼
[ bt_service (后台服务守护进程) ]
       │
       │  do_service_loop 任务轮询与事件分发
       ▼
[ SAL 协议抽象层 (sal_* interface) ]
       │
       │  sal_send_req / service_loop_work
       ▼
[ zblue 协议栈 ]
       │
       │  Upper HCI
       ▼
[ struct bt_driver_s (HCI 驱动层) ]
```

1. **IPC 通信机制**：各业务进程（如媒体播放器、智慧生活配网、桌面数字人）不直接操作蓝牙硬件，而是通过 IPC 与系统级后台常驻服务 `bt_service` 通信，避免多进程并发调用导致的状态错乱与硬件死锁。
2. **服务层 (`bt_service`)**：维护蓝牙全局状态机（开/关、发现中、连接中），统一管理各种 Profile（A2DP/HFP/GATT）的生命周期。
3. **SAL 抽象接口层 (`sal_*`)**：屏蔽底层协议栈差异（支持 Zephyr zblue、BlueZ、Bluedroid 等多种协议栈）。通过 `do_service_loop` 循环处理事件，利用 `sal_send_req` 将业务请求转化为协议栈请求。
4. **HCI 交互**：SAL 通过统一的 `struct bt_driver_s` 结构将打包后的 HCI 原始数据送交底半部驱动。

---

## 3. 板端终端命令行调试工具 (`bttool`)

在 NSH 调试终端中，使用内置的 `bttool` 交互式命令可以快速验证蓝牙硬件、射频连接与各种 Profile：

### 3.1 启动与常用控制命令

进入交互控制台：
```bash
nsh> bttool
bttool> help
```

常用基础命令：
```bash
# 1. 打开/使能蓝牙适配器
bttool> enable

# 2. 查询当前适配器状态 (0:关闭, 1:开启, 2:正在开启, 3:正在关闭)
bttool> get_state

# 3. 获取本地蓝牙 MAC 地址
bttool> get_local_addr

# 4. 获取与设置本地设备名称
bttool> get_local_name
bttool> set_local_name "Gemini-S1-Agent"

# 5. 设置可被发现与连接状态 (ScanMode: 0=NONE, 1=CONNECTABLE, 2=CONNECTABLE_DISCOVERABLE)
bttool> set_scanmode 2 1

# 6. 关闭蓝牙适配器
bttool> disable
```

### 3.2 扫描、配对与设备连接

```bash
# 1. 开启经典蓝牙/BLE 综合扫描 (开始向终端输出搜索到的设备信息)
bttool> discovery start

# 扫描输出示例:
# Inquiring: device [11:22:33:44:55:66], name: MyPhone, cod: 005a020c, is HEADSET: false, rssi: -65

# 2. 停止扫描
bttool> discovery stop

# 3. 发起配对 / 绑定
bttool> pair 11:22:33:44:55:66
bttool> create_bond 11:22:33:44:55:66

# 4. 建立基础连接 / 断开连接
bttool> connect 11:22:33:44:55:66
bttool> disconnect 11:22:33:44:55:66

# 5. 查看已配对设备列表与当前已连接设备
bttool> get_bonded_devices
bttool> get_connected_devices
```

### 3.3 Profile 子协议测试命令

`bttool` 内置了各 Profile 的专属子命令集：
```bash
# A2DP 音频测试 (作为音频接收端 / 扬声器)
bttool> a2dp_sink connect 11:22:33:44:55:66

# BLE 广播测试 (发射指定广播包)
bttool> adv start

# BLE 扫描测试
bttool> scan start

# GATT Server / Client 调试
bttool> gatt_server register
bttool> gatt_client connect 11:22:33:44:55:66
```

---

## 4. 在 OpenVela 中开发蓝牙应用

对于基于 C 语言的应用开发者，遵循**实例管理、异步回调注册、事件循环解耦**三原则。

### 4.1 核心 API 概览 (`framework/include/`)

* `#include "bluetooth.h"`：基础实例管理 `bluetooth_create_instance()`、`bluetooth_delete_instance()`
* `#include "bt_adapter.h"`：适配器使能与状态控制 `bt_adapter_enable()`、`bt_adapter_register_callback()`
* `#include "bt_device.h"`：设备发现与绑定管理 `bt_gap_create_bond()`、`bt_adapter_start_discovery()`

### 4.2 标准应用开发步骤与模板

> [!IMPORTANT]
> **线程安全原则**：`bt_adapter_register_callback` 传入的回调函数由底层的 `bt_client` 线程调度执行。**严禁在回调函数内部直接调用阻塞式蓝牙 API**，必须将事件封装为消息放入队列，交由应用自己的主线程或工作线程处理。

#### 完整开发模板 (`sample_bt_app.c`)
```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#include "bluetooth.h"
#include "bt_adapter.h"
#include "bt_device.h"

static bt_instance_t* g_bt_ins = NULL;
static void* g_adapter_cb_handle = NULL;
static sem_t g_event_sem;
static volatile bool g_app_running = true;

/* 1. 适配器状态变更回调 (运行于底层通信线程) */
static void on_adapter_state_changed_cb(void* cookie, bt_adapter_state_t state)
{
    printf("[BT_APP] Adapter state changed: %d\n", state);
    if (state == BT_ADAPTER_STATE_ON) {
        printf("[BT_APP] Bluetooth is ON, waking up main loop...\n");
        sem_post(&g_event_sem);
    } else if (state == BT_ADAPTER_STATE_OFF) {
        printf("[BT_APP] Bluetooth is OFF.\n");
        g_app_running = false;
        sem_post(&g_event_sem);
    }
}

/* 2. 设备搜索发现结果回调 */
static void on_discovery_result_cb(void* cookie, bt_discovery_result_t* result)
{
    if (result) {
        char addr_str[18] = {0};
        bt_addr_ba2str(&result->addr, addr_str);
        printf("[BT_APP] Discovered: [%s], Name: %s, RSSI: %d\n",
               addr_str, result->name ? result->name : "Unknown", result->rssi);
    }
}

/* 回调函数映射表 */
static const adapter_callbacks_t g_adapter_cbs = {
    .on_adapter_state_changed = on_adapter_state_changed_cb,
    .on_discovery_result      = on_discovery_result_cb,
};

int main(int argc, char* argv[])
{
    int ret;
    sem_init(&g_event_sem, 0, 0);

    /* 步骤 1: 创建蓝牙客户端实例 */
    g_bt_ins = bluetooth_create_instance();
    if (!g_bt_ins) {
        fprintf(stderr, "Failed to create bluetooth instance!\n");
        return -1;
    }

    /* 步骤 2: 注册状态与事件回调 */
    g_adapter_cb_handle = bt_adapter_register_callback(g_bt_ins, &g_adapter_cbs);
    if (!g_adapter_cb_handle) {
        fprintf(stderr, "Failed to register adapter callback!\n");
        goto cleanup;
    }

    /* 步骤 3: 打开/使能蓝牙 */
    ret = bt_adapter_enable(g_bt_ins);
    if (ret != BT_STATUS_SUCCESS) {
        fprintf(stderr, "bt_adapter_enable failed with error: %d\n", ret);
        goto cleanup;
    }

    /* 步骤 4: 等待蓝牙成功开启 */
    sem_wait(&g_event_sem);

    /* 步骤 5: 配置可发现性并开始扫描 */
    bt_adapter_set_local_name(g_bt_ins, "Gemini-S1-Demo");
    bt_adapter_set_scan_mode(g_bt_ins, BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE, 60);
    bt_adapter_start_discovery(g_bt_ins);

    /* 主业务循环 (监听退出信号或超时) */
    while (g_app_running) {
        sleep(1);
    }

cleanup:
    /* 步骤 6: 反初始化与资源销毁 */
    if (g_adapter_cb_handle) {
        bt_adapter_unregister_callback(g_bt_ins, g_adapter_cb_handle);
    }
    if (g_bt_ins) {
        bt_adapter_disable(g_bt_ins);
        bluetooth_delete_instance(g_bt_ins);
        g_bt_ins = NULL;
    }
    sem_destroy(&g_event_sem);
    return 0;
}
```

---

## 5. 在 OpenVela 中适配蓝牙驱动

OpenVela 采用标准的 `struct bt_driver_s`（定义于 `nuttx/include/nuttx/wireless/bluetooth/bt_driver.h`）驱动模型。在全志 R528 (Gemini-S1) 开发板上，采用了 **Coex 共存包装层 + HCI UART 底层**的双层驱动架构。

### 5.1 驱动模型核心接口 (`struct bt_driver_s`)

```c
struct bt_driver_s {
    size_t head_reserve;  /* 预留 HCI 包头空间 */
    CODE int (*open)(FAR struct bt_driver_s *btdev);
    CODE int (*send)(FAR struct bt_driver_s *btdev, enum bt_buf_type_e type,
                     FAR void *data, size_t len);
    CODE void (*close)(FAR struct bt_driver_s *btdev);
    CODE int (*receive)(FAR struct bt_driver_s *btdev, enum bt_buf_type_e type,
                        FAR void *data, size_t len); /* 由协议栈/框架自动绑定 */
    CODE int (*ioctl)(FAR struct bt_driver_s *btdev, int cmd, unsigned long arg);
};
```

### 5.2 `open` 接口实现规范

#### 1. Coex 包装层：`rtk_coex_open()` (`rtk_coex.c`)
* 校验参数有效性，提取真实的硬件驱动句柄 `drv`。
* 调用 `rtk_coex_init()`：重置共存运行态结构体 `btpf`，初始化互斥锁 `nxmutex_init(&btpf->coexlock)`，并初始化 `conn_list` 与 `pending_cmd_list` 双向链表。
* 透传调用下级 `drv->open(drv)`。

#### 2. HCI UART 层：`bthci_open()` (`rtk_hci.c`)
* 打开基础串口设备 `/dev/ttyS1`。
* 启动异步任务 `btuart_fw_task()`（防止阻塞调用线程）：
  1. `file_open(&dev->filep_h5, CONFIG_BT_UART_ON_DEV_NAME, O_RDWR)`：建立 H5 协议通信；
  2. `hci_check_local_ver()` & `hci_check_local_rom_ver()`：握手并读取芯片 ROM 版本；
  3. `hci_update_baudrate()`：将串口波特率提升至 1500000 高速模式；
  4. `hci_load_firmware()`：向 Realtek 芯片下载蓝牙 Patch 固件；
  5. `hci_reset()`：发送 HCI_Reset 命令重置控制器；
  6. `hci_read_local_addr()`：读取 MAC 地址（若配置了 `/data/etc/bt/8723fs_btaddr.txt` 则自动烧录覆盖）。

### 5.3 `send` 接口实现规范

#### 1. Coex 包装层：`rtk_coex_send()`
* 判断 HCI 数据类型：若是发送给控制器的 ACL 数据流 (`BT_ACL_OUT`)，调用 `l2_process_frame_out(drv, data, len)`：
  * 解析 L2CAP 层协议特征（识别当前是 A2DP 音频流还是 PAN 网络数据流）；
  * 动态更新共存状态机中的引用计数 (`pf_refs`)、协议状态位图 (`pf_bits`)；
  * 若 Wi-Fi 当前正处于高吞吐状态，由 Coex 调度算法生成协同决策命令，动态向蓝牙芯片下发优先级调整包；
* 最终透传调用硬件层发送函数 `drv->send(drv, type, data, len)`。

#### 2. HCI UART 层：`bthci_send()`
* 针对中断与重入安全进行保护：
  ```c
  do {
      ret = file_write(&dev->filep_uart, data, len);
  } while (ret < 0 && errno == EINTR);
  ```
* 将标准 HCI 帧（Command / ACL / SCO / ISO）经由 UART 流控引脚（CTS/RTS）发送给外部无线芯片。

### 5.4 `receive` 数据上送机制
驱动工程师**无需**自行实现 `receive` 成员函数。
当底层 UART 串口中断接收到 Controller 返回的完整 HCI 帧时，直接调用系统 API：
```c
bt_netdev_receive(btdev, type, data, len);
```
该函数会将数据包投递给 OpenVela 蓝牙接收工作队列，送入 `zblue` 上层协议栈解析。

### 5.5 驱动注册
在板级初始化代码（如 `boards/r528/.../gemini_s1_bringup.c`）中：
```c
FAR struct bt_driver_s *hcidrv = bthci_register();
FAR struct bt_driver_s *coexdrv = rtk_coex_register(hcidrv);
bt_driver_register(coexdrv);
```

---

## 6. 常见问题排查与避坑指南

1. **`bluetooth_create_instance()` 返回 NULL**：
   * 检查 `CONFIG_FRAMEWORKS_CONNECTIVITY_BLUETOOTH=y` 是否使能；
   * 确认后台 `bt_service` 守护进程是否已随系统引导启动，IPC 通信管道权限是否正确。
2. **`bt_adapter_enable()` 失败或超时卡死**：
   * **固件未加载成功**：全志 R528 依赖 Realtek 专有蓝牙 Patch 固件。检查串口波特率切换过程日志，若卡在 `hci_load_firmware()`，通常是串口 RTS/CTS 硬件流控接线错误或引脚配置失步。
   * **电源复位时序不对**：查看 `rtkbt_board_poweron()`，确保拉高 BT_EN 引脚后有充足的毫秒级时延（通常至少延时 100ms）再发送第一个 HCI 命令。
3. **Wi-Fi 与蓝牙同时开启时，蓝牙音乐 (A2DP) 卡顿或 Wi-Fi 丢包**：
   * 全志 RTL8723FS 为 2.4GHz 单天线/单射频 Combo 芯片。必须在内核配置中启用 `CONFIG_R528_BT_RTK_COEX=y`。
   * 严禁绕过 `rtk_coex_send()` 直接向 `bthci_send()` 灌数据，否则共存管理器无法感知蓝牙 A2DP 的高频突发流量，导致 Wi-Fi 抢占射频信道引发蓝牙丢包。
4. **应用在 Callback 中直接调用 API 导致死锁**：
   * Framework 回调运行于单线程 IPC 队列上下文中。如果在 `on_adapter_state_changed` 内部直接调用同步阻塞的 `bt_adapter_start_discovery()`，会导致客户端等待服务端回复、而服务端正被阻塞在当前回调调度的恶性死锁。
   * **解决方案**：回调只负责向本地队列或管道 `post` 信号，由独立的工作线程消费执行具体的业务调用。
5. **重启后蓝牙 MAC 地址每次变化**：
   * 检查 `/data/etc/bt/8723fs_btaddr.txt` 是否存在。若缺失该文件且芯片内部 eFuse 未烧录 MAC，系统会在每次上电时生成随机地址。可通过执行 `echo "11:22:33:44:55:66" > /data/etc/bt/8723fs_btaddr.txt` 固定本地地址。
