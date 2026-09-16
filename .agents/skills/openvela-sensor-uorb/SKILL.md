---
name: openvela-sensor-uorb
description: "OpenVela / 全志 R528-S3 (Gemini-S1) IIO 传感器驱动体系、uORB 事件流、sensortest 工具与环境感知/敲击感应开发指南。"
---

# OpenVela 传感器体系与感知开发指南

本指南详细介绍 OpenVela 基于 Linux IIO 设计思想的传感器框架（Sensor Framework）、核心设备节点、`sensortest` 命令行工具使用、以及光感（LTR553）、温湿度（SHTC3）与加速度计物理感知（桌面微敲击检测）的实现方案。

---

## 1. 传感器框架架构与设备模型

OpenVela 采用清晰的上下半区（Upper-Half / Lower-Half）驱动分离设计：
* **上半区 (Upper-Half)**：通用内核层，负责创建管理 `/dev/sensor/*` 设备节点、维护各传感器专用的 Ring Buffer 环形缓冲区、多进程并发订阅与数据降采样。
* **下半区 (Lower-Half)**：芯片硬件抽象层，实现标准 `sensor_ops_s` 接口，通过 I2C/SPI 总线与物理传感器寄存器通信，在定时器或中断触发时调用 `push_event()` 上报数据。

### 1.1 核心设备节点与数据结构
所有结构体统一定义在 `nuttx/include/nuttx/sensors/sensor.h` 中：

| 节点路径 | 传感器类型与结构体 | 典型物理器件 | 单位与核心字段 |
| :--- | :--- | :--- | :--- |
| `/dev/sensor/accel0` | `struct sensor_event_accel` | 三轴加速度计 | `x, y, z` (单位: $m/s^2$)、`temperature` |
| `/dev/sensor/light0` | `struct sensor_event_light` | LTR-553 光照传感器 | `lux` (单位: Lux 勒克斯)、红外强度 |
| `/dev/sensor/humi0` | `struct sensor_event_relative_humidity` | SHTC3 湿度传感器 | `humidity` (单位: % 相对湿度) |
| `/dev/sensor/temp0` | `struct sensor_event_ambient_temperature` | SHTC3 环境温度计 | `temperature` (单位: °C 摄氏度) |

---

## 2. 命令行交互调试：`sensortest`

OpenVela 内置的 `sensortest` 工具可直接在 NSH 中验证传感器驱动是否正常注册并持续输出数据。

### 2.1 命令语法与参数
```bash
sensortest <device_node> [options]
```
* `-h`：查看详细帮助。
* `-i <interval_us>`：设置采样间隔时间（微秒）。
* `-n <count>`：指定读取数据包次数后退出。

### 2.2 典型测试命令
```bash
# 1. 持续读取当前环境光强度 (默认 1Hz / 1秒一次)
sensortest light0

# 2. 以 20Hz (50000 微秒) 高频读取三轴加速度计数据
sensortest accel0 -i 50000

# 3. 读取 10 次温湿度数据后自动退出
sensortest humi0 -n 10
sensortest temp0 -n 10
```

---

## 3. 应用层读取传感器数据的标准流程

应用层通过标准 POSIX 文件操作与 `ioctl` 命令与传感器交互：

```c
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <nuttx/sensors/sensor.h>
#include <nuttx/sensors/ioctl.h>

int fd = open("/dev/sensor/light0", O_RDONLY);
if (fd >= 0) {
    /* 1. 激活传感器 (1: 开启, 0: 待机省电) */
    ioctl(fd, SNIOC_ACTIVATE, 1);

    /* 2. 设置期望采样周期 (例如 100ms = 100000 us) */
    ioctl(fd, SNIOC_SET_INTERVAL, 100000);

    /* 3. 循环读取事件 */
    struct sensor_event_light event;
    while (running) {
        ssize_t ret = read(fd, &event, sizeof(event));
        if (ret == sizeof(event)) {
            printf("[Light] 当前光照度: %.2f Lux, 时间戳: %llu us\n", 
                   event.lux, (unsigned long long)event.timestamp);
        }
        usleep(100000);
    }

    /* 4. 关闭并释放 */
    ioctl(fd, SNIOC_ACTIVATE, 0);
    close(fd);
}
```

---

## 4. 桌面微敲击检测算法实现 (Knock Sensing)

在桌面数字生命体场景中，利用加速度计 $Z$ 轴加速度的高频瞬态突变，可以实现“免触控物理轻敲感知”：

```text
[加速度计 50Hz 采样]
        │
        ▼
[高通/差分滤波: Δz = |z(t) - z(t-1)|]
        │
        ├─> Δz > 阈值_TH (约 4.0 m/s²)? ──(否)──> 忽略日常桌面静止
        │
        └─(是)─> [记录敲击时间戳 t_knock]
                   │
                   ▼
         [500ms 滑动时间窗口状态机]
                   │
                   ├─ 单次轻敲 (Single Knock) ──> 触发赛博木鱼功德 +1 / 翻页钟切换
                   ├─ 快速双敲 (Double Knock) ──> 灵感胶囊立即升起示波器录音
                   └─ 快速三敲 (Triple Knock) ──> 免触控轮换下一个场景卡带
```

在工程代码 `hal/hal_sensor.c` 与 `perception/perception.c` 中，该状态机作为独立后台线程轻量运行，CPU 占用率低于 0.5%。

---

## 5. 开发建议与避坑要点

1. **禁止在应用层频繁使用非阻塞 `fetch` 模式**：
   - 官方推荐驱动使用中断或内核定时器采集并 `push_event` 到环形缓冲区。若在应用层高频以非阻塞方式直读物理寄存器，会导致 I2C 总线争抢拥堵并卡顿 UI 刷新。
2. **环形缓冲区大小设置**：
   - 对于光感、温湿度等低频传感器，`nbuffer` 设为 `1` 即可节省 RAM；
   - 对于加速度计等需要防丢包的高频传感器，建议驱动下半区将 `nbuffer` 设为 `3`。
