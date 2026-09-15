# OpenVela 硬件功能调用测试应用 (`app/demo`)

## 一、概述

本应用受到 [M5Tab5-UserDemo](file:///Volumes/LaCie/OpenVela/M5Tab5-UserDemo) 硬件测试思想的启发，是**专门基于 OpenVela 操作系统提供的标准封装与驱动接口**构建的硬件功能测试小应用。

应用脱离了私有寄存器的硬编码耦合，而是面向 OpenVela 平台提供的标准子系统（电池子系统、音频与多媒体子系统、显示子系统、传感器框架）进行规范对接与调用，支持在 NSH 控制台下进行**命令行测试（CLI）**与在屏幕上运行**图形化卡片仪表盘（GUI）**。

---

## 二、支持的硬件功能调用测试

| 功能类别 | 测试目标 | OpenVela 对接接口 | CLI 指令 |
| :--- | :--- | :--- | :--- |
| **电源与电量** | 总线电压、分流电流、功耗、SOC% 估算、充放电状态 | `<nuttx/power/battery_ioctl.h>`, `/dev/charge/*`, `/dev/bat*` | `demo power [采样次数]` |
| **麦克风状态** | 采样率、通道数、增益、可用性状态查询 | `<nuttx/audio/audio.h>`, `/dev/audio/pcm0c` | `demo mic status` |
| **关闭麦克风** | 硬件静音麦克风输入，切断拾音通道 | `AUDIOIOC_SETMUTE` (true) | `demo mic mute` |
| **开启麦克风** | 解除静音，恢复麦克风正常拾音 | `AUDIOIOC_SETMUTE` (false) | `demo mic unmute` |
| **麦克风电平** | 实时采样麦克风音频，输出动态音量柱 (VU Meter) | OpenVela PCM 采样计算 RMS 峰值 | `demo mic vu [秒数]` |
| **屏幕亮度** | 背光亮度查询与 0% ~ 100% 无级调节 | `<nuttx/lcd/lcd.h>`, `board_lcd_set_backlight` | `demo brightness <0-100\|get>` |
| **屏幕色彩** | RGB彩条、渐变灰度、棋盘网格图案渲染测试 | `/dev/fb0` Framebuffer 接口 | `demo pattern [0\|1\|2]` |
| **喇叭音量** | 0% ~ 100% 音量控制、静音、功放使能开关 | `AUDIOIOC_SETVOLUME`, `AUDIOIOC_SETMUTE` | `demo volume <0-100\|mute\|amp>` |
| **音频发声** | 标准正弦波测试音发生 (调音与出音测试) | `/dev/audio/pcm0p` PCM 输出流 | `demo beep [频率] [毫秒]` |
| **姿态传感器** | 读取六轴加速度 (g) 与角速度 (dps) | `<nuttx/sensors/sensor.h>`, IMU 接口 | `demo imu` |
| **实时时钟** | 读取与校验硬件 RTC 真实时钟 | `<time.h>`, RTC 硬件时钟 | `demo rtc` |
| **外设电源** | USB-A 5V 与 Grove EXT 5V 供电轨道启闭 | OpenVela 外设电源控制接口 | `demo rail <usb5v> <ext5v>` |
| **综合自检** | 一键全量硬件全自动流水线诊断与自检报告 | 统一自动化回归引擎 | `demo all` |
| **图形仪表盘** | 现代卡片仪表盘 (触控滑块、波形条、开关) | OpenVela LVGL 图形库 | `demo gui` |

---

## 三、快速开始

### 1. 编译配置
在系统配置或通过 `menuconfig` 使能：
```text
CONFIG_LVX_USE_DEMO_CONTEST2026_145_DEMO=y
```

### 2. 构建与运行
在 OpenVela 工作区根目录执行构建：
```bash
./build.sh <board-config-path>
```

在系统 NSH 终端运行测试：
```bash
# 1. 综合全功能自检
nsh> demo all

# 2. 查看电源电量与电流电压
nsh> demo power 3

# 3. 关闭麦克风
nsh> demo mic mute

# 4. 开启麦克风并查看实时音量跳动
nsh> demo mic unmute
nsh> demo mic vu 5

# 5. 调整屏幕亮度
nsh> demo brightness 70

# 6. 调整喇叭音量并播放测试音
nsh> demo volume 65
nsh> demo beep 520 300

# 7. 启动触摸屏图形控制仪表盘
nsh> demo gui
```
