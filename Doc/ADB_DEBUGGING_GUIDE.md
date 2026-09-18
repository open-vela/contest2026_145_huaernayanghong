# Phoenix HoloDesk-S1 运行期间 ADB 全功能调试与测试实战指南

> **适用平台**：全志 R528-S3 (Gemini-S1 开发板) / OpenVela 实时操作系统 / NuttX RTOS  
> **面向对象**：嵌入式开发者、系统集成测试工程师、大赛评审专家  
> **更新时间**：2026 年

---

## 目录
1. [ADB 环境准备与系统基线检查](#1-adb-环境准备与系统基线检查)
2. [全通道日志调阅与黑匣子排障 (Logging Subsystem)](#2-全通道日志调阅与黑匣子排障-logging-subsystem)
3. [Wi-Fi 网络与 Captive Portal 强制门户测试](#3-wi-fi-网络与-captive-portal-强制门户测试)
4. [BLE 蓝牙全双工配网测试 (BLE Provisioning)](#4-ble-蓝牙全双工配网测试-ble-provisioning)
5. [音频链路、录音拾音与扬声器播放调试 (Audio Pipeline)](#5-音频链路录音拾音与扬声器播放调试-audio-pipeline)
6. [传感器采集与微敲击感知测试 (Perception & IIO)](#6-传感器采集与微敲击感知测试-perception--iio)
7. [LVGL 屏幕渲染与触控输入调试 (Display & Touch)](#7-lvgl-屏幕渲染与触控输入调试-display--touch)
8. [业务卡带调度与具身工具测试 (Cartridges & Tool Calling)](#8-业务卡带调度与具身工具测试-cartridges--tool-calling)
9. [全自动化回归与集成测试套件 (Test Suites)](#9-全自动化回归与集成测试套件-test-suites)

---

## 1. ADB 环境准备与系统基线检查

开发板通过 Type-C 数据线连接开发机（PC / Mac / Linux），板载 USB 将被识别为标准的 ADB 复合设备。

### 1.1 检查设备在线状态
在开发机终端执行：
```bash
adb devices
```
正常输出示例：
```text
List of devices attached
0123456789ABCDEF    device
```

### 1.2 进入板端 NSH 交互 Shell
```bash
adb shell
```
进入后终端提示符形如 `nsh>`，输入 `help` 可查看系统所有内置命令。

### 1.3 核心硬件设备节点与运行环境核验
```bash
# 检查关键驱动字符设备
ls -l /dev/lcd0 /dev/input0 /dev/ramlog /dev/audio*

# 查看系统开机运行时长与 CPU 运行态
uptime
cat /proc/uptime

# 查看系统可用内存堆占用
free
```

---

## 2. 全通道日志调阅与黑匣子排障 (Logging Subsystem)

Phoenix HoloDesk-S1 采用**四重并发分流日志架构**：
- **通道 1**：控制台终端（标准输出 `stdout`，带 ANSI 色彩高亮）；
- **通道 2**：系统级内核日志（`POSIX syslog` / `Ramlog`）；
- **通道 3**：Flash 滚动黑匣子持久化文件（`/data/phoenix/logs/phoenix.log`，超过 64KB 自动截断轮转备份为 `.old`）；
- **通道 4**：Web 伴侣诊断环形内存缓冲（16KB 内存环形，供 Web 端实时拉取）。

```text
[业务日志 LOG_I/W/E/D]
        │
        ▼
[Phoenix Log Mgr] ──┬──> 终端 stdout (ANSI 彩色直出，adb shell 实时可见)
                    ├──> POSIX syslog / Ramlog (/dev/ramlog，dmesg 调阅)
                    ├──> Flash 滚动黑匣子 (/data/phoenix/logs/phoenix.log，断电持久化)
                    └──> 内存环形诊断缓冲 (供 Web 控制台 /api/logs 查看)
```

### 2.1 终端实时彩色日志直出
通过 `adb shell` 前台直接启动主程序或 CLI 时，终端将实时打印格式化高亮日志：
```bash
adb shell phoenix_agent_app
```
每条日志均包含：`[相对启动秒.毫秒] [模块标签:严重级别] 日志详情`，异常错误红字标出，警告黄字高亮。

### 2.2 内核与系统级 Ramlog 日志调阅
当应用在后台常驻运行时，可通过以下命令捕获由 `syslog()` 写入的内核环形日志：
```bash
# 方式 A: 打印当前内核环形日志
adb shell dmesg

# 方式 B: 直接读取 ramlog 节点
adb shell cat /dev/ramlog
```

### 2.3 动态调节运行时日志过滤级别
无需重新编译固件，可在运行期间通过 ADB 动态调节输出粒度：
```bash
# 查看当前级别与日志文件路径
adb shell phoenix_agent_app log

# 动态切换为全量调试级别 (Verbose / Debug 流水)
adb shell phoenix_agent_app log level debug

# 切换为仅输出警告与严重错误 (减少高频刷屏)
adb shell phoenix_agent_app log level warn

# 恢复默认 INFO 业务级别
adb shell phoenix_agent_app log level info
```

### 2.4 Flash 滚动黑匣子日志提取 (断电不丢失)
当设备发生意外断电、看门狗复位或偶发性崩溃时，通过 `adb pull` 将板端持久化日志抓取到开发机进行事后故障定界：
```bash
# 提取当前主日志
adb pull /data/phoenix/logs/phoenix.log ./crash_analysis.log

# 提取轮转备份日志 (.old 包含更早期的开机与运行流水)
adb pull /data/phoenix/logs/phoenix.log.old ./crash_analysis_prev.log
```

---

## 3. Wi-Fi 网络与 Captive Portal 强制门户测试

开发板支持 **SoftAP 独立配网热点** 与 **STA 客户端局域网连网** 双模自动跃迁。

### 3.1 查询当前网络模式与 IP
```bash
adb shell phoenix_agent_app wifi status
```
- **未配置或首次开机**：显示 `工作模式: SoftAP 独立配网热点, SSID: Gemini-Agent-S1, 本机 IP: 192.168.4.1`
- **连网成功后**：显示 `工作模式: STA 已连网, SSID: <你的WiFi名称>, 本机 IP: 192.168.x.x`

### 3.2 扫描周边 Wi-Fi 热点
```bash
adb shell phoenix_agent_app wifi scan
```
输出格式示例：
```text
[CLI:WiFi] 正在扫描周边 Wi-Fi 热点...
[CLI:WiFi] 扫描完成，发现 5 个可用热点:
  [ 1] Office-5G                (RSSI: -45 dBm, Auth: WPA2)
  [ 2] Home-Mesh-2.4G           (RSSI: -58 dBm, Auth: WPA2)
  [ 3] GeekLab-Wi-Fi6           (RSSI: -68 dBm, Auth: WPA3)
  [ 4] Guest-Free-WiFi          (RSSI: -78 dBm, Auth: OPEN)
```

### 3.3 验证 Captive Portal 强制门户弹窗
当开发板处于 SoftAP 模式时，内嵌 MiniDHCP 与 Captive DNS (UDP 53) 服务已自动启动：
1. 用手机或笔记本 Wi-Fi 连接板端热点：`Gemini-Agent-S1`；
2. 在开发机使用 ADB 测试 DNS 拦截与 HTTP 302 弹窗探测：
```bash
# 验证 HTTP 探测是否返回 302 弹窗重定向到 http://192.168.4.1/
adb shell "curl -I -s http://192.168.4.1/generate_204"
adb shell "curl -I -s http://192.168.4.1/hotspot-detect.html"
```
响应头包含 `HTTP/1.1 302 Found` 与 `Location: http://192.168.4.1/` 即表示强制门户弹窗机制完全就绪。

### 3.4 手动执行 Wi-Fi 配网（多途径实战指南）

开发板提供了针对不同应用场景的手动配网方案：

#### 方案 A：SoftAP Web 门户配网（推荐，用户免接电脑）
1. **连接热点**：手机/电脑连接板端热点 `Gemini-Agent-S1`（无密码，网关为 `192.168.4.1`）；
2. **访问页面**：等待系统自动弹窗，或打开浏览器访问 `http://192.168.4.1/`；
3. **提交凭证**：在网页的热点下拉列表中选择目标 Wi-Fi，输入密码，点击 **“保存凭证并连接 Wi-Fi”**。设备收到指令后会自动持久化配置并切换至 STA 模式连入路由器。

#### 方案 B：ADB 命令行免界面一键配网（开发调试最快捷）
无需连接热点或打开网页，通过 ADB 管道直接下发账密：

```bash
# 途径 1: 通过板载 HTTP REST 接口触发连接
adb shell "curl -X POST http://127.0.0.1/api/wifi/connect -H 'Content-Type: application/json' -d '{\"ssid\":\"你的WiFi名称\",\"psk\":\"你的WiFi密码\"}'"

# 途径 2: 通过配置管理工具 ble_prov 统一写入并持久化
adb shell ble_prov set_config '{"wifi":{"ssid":"你的WiFi名称","psk":"你的WiFi密码"}}'

# 途径 3: 直接写入底层持久化配置文件并重载
adb shell "mkdir -p /data/etc/wifi && cat << 'EOF' > /data/etc/wifi/wapi.conf
{
  \"ssid\": \"你的WiFi名称\",
  \"psk\": \"你的WiFi密码\",
  \"bssid\": \"\"
}
EOF"
```

#### 方案 C：串口 / NSH 终端原生 `wapi` 命令行（底层极客模式）
在板端 NSH 串口终端中，直接调用系统原生无线管理工具配置 STA 接口：
```bash
# 1. 断开并配置目标 SSID 与 WPA2 密码 (末尾 3 代表 WPA2-PSK 加密)
wapi disconnect wlan0
wapi essid wlan0 "你的WiFi名称" 1
wapi psk wlan0 "你的WiFi密码" 1 3

# 2. 保存配置并触发重连
wapi save_config wlan0
wapi reconnect wlan0

# 3. 申请路由器分配局域网 IP
renew wlan0
```

#### 3.5 验证配网结果与重置热点
```bash
# 验证当前网络状态与分配的内网 IP
adb shell phoenix_agent_app wifi

# 若需清除当前 Wi-Fi 配置重回 SoftAP 配网热点
adb shell "curl -X POST http://127.0.0.1/api/wifi/reset"
```

---

## 4. BLE 蓝牙全双工配网测试 (BLE Provisioning)

除了 Web 配网页面，板端提供独立的 `ble_prov` 命令行工具，支持全双工双向交互测试。

### 4.1 检查蓝牙子系统与广播状态
```bash
adb shell ble_prov status
```
输出示例：
```text
📊 BLE 配网服务状态报告:
   - 运行状态: ADVERTISING (正在广播中...)
   - 广播名称: Phoenix-Agent-BLE
   - 设备平台: OpenVela Gemini-S1
```

### 4.2 模拟回读系统与智能体配置 (双向 Uplink 测试)
```bash
adb shell ble_prov get_config
```
板端将格式化打印出返回给 Web 客户端的 JSON 报文（包含 Wi-Fi 状态、模型配置、脱敏后的 API Key 等）。

### 4.3 模拟客户端下发配置并触发持久化 (双向 Downlink 测试)
```bash
# 下发 Wi-Fi 账密
adb shell ble_prov set_config '{"wifi":{"ssid":"MyTestHome","psk":"88888888"}}'

# 下发大模型 API Key 与系统提示词
adb shell ble_prov set_config '{"agent":{"api_key":"sk-9876543210","model":"deepseek-chat","prompt":"你是桌宠使魔"}}'
```
下发后板端自动落盘至 `/data/phoenix/phoenix_config.txt`。

---

## 5. 音频链路、录音拾音与扬声器播放调试 (Audio Pipeline)

OpenVela 采用 ALSA 兼容架构，开发板引出一路单声道扬声器功放和一路高灵敏麦克风。

### 5.1 扬声器播放测试 (PCM / WAV)
```bash
# 播放内置木鱼清脆音效
adb shell nxplayer -p /data/sounds/wooden_fish.pcm

# 播放番茄钟完成提示音
adb shell nxplayer -p /data/sounds/pomodoro_bell.pcm
```

### 5.2 麦克风录音与底噪分析
```bash
# 录制 5 秒、16kHz、16-bit 单声道音频到 /tmp/mic_test.pcm
adb shell nxrecorder -c 1 -r 16000 -b 16 -d 5 /tmp/mic_test.pcm

# 将录音文件拉取到开发机试听与查看声波频谱
adb pull /tmp/mic_test.pcm ./mic_test.pcm
```

---

## 6. 传感器采集与微敲击感知测试 (Perception & IIO)

Phoenix 感知引擎以 20Hz（50ms）周期对板载光敏传感器、敲击振动传感器与电池电量进行滤波去抖。

### 6.1 实时巡检环境光照、敲击计次与电池
```bash
adb shell phoenix_agent_app sensor status
```
输出示例：
```text
[CLI:Sensor] 环境光 ALS: 380 Lux (暗光: 否), 电池电量: 92% (电压: 4120mV, 充电: 是)
```

### 6.2 命令行模拟桌面物理敲击 (Inject Tap)
无需手敲物理桌面，直接通过 ADB 注入敲击信号：
```bash
adb shell phoenix_agent_app sensor knock
```
**观察反馈**：
- 控制台打印功德增加；
- 若连接了扬声器，将听到木鱼音效；
- 若屏幕点亮，灵眸眼球将出现眨眼拟人反应并向上漂浮 `+1 功德` 飞字。

---

## 7. LVGL 屏幕渲染与触控输入调试 (Display & Touch)

屏幕采用 2.8 寸/7 寸高清屏，底层驱动挂载在 `/dev/lcd0`，触控驱动挂载在 `/dev/input0`。

### 7.1 验证显存 64 字节硬件对齐与硬件加速
由于全志 R528-S3 启用了 G2D 硬件加速器，显存必须保证 64 字节物理地址对齐：
```bash
adb shell "dmesg | grep -E 'lcd|display|g2d'"
```
若有对齐违规底层会报 `Assert failed in lv_display_set_buffers`；正常运行将看到：
`[PhoenixApp] Initializing 64-byte aligned LVGL display (/dev/lcd0)... SUCCESS`。

### 7.2 触控坐标与滑动事件抓包 (getevent)
```bash
adb shell getevent /dev/input0
```
手指触摸屏幕时，终端将实时打印触控中断事件（EV_ABS / ABS_X / ABS_Y / EV_KEY），松手时打印 `UP` 事件。

---

## 8. 业务卡带调度与具身工具测试 (Cartridges & Tool Calling)

Phoenix HoloDesk-S1 采用模块化卡带架构，内建 6 大独立卡带。

### 8.1 巡检已挂载卡带与当前活跃态
```bash
adb shell phoenix_agent_app cartridge list
```
输出示例：
```text
[CLI:Cartridge] 当前活跃卡带: [familiar] (使魔萌宠)
--- 已挂载业务卡带清单 (6 个) ---
  [0] familiar   : 🐱 使魔萌宠 (当前活跃)
  [1] home       : 🏠 主页看板 
  [2] clock      : ⏰ 翻页时钟 
  [3] zen        : 🪷 极客木鱼 
  [4] memo       : 💡 灵感外脑 
  [5] agent      : 🤖 灵眸AI   
```

### 8.2 动态切换卡带
```bash
# 切换到极客木鱼卡带
adb shell phoenix_agent_app cartridge switch zen

# 切换到翻页时钟卡带
adb shell phoenix_agent_app cartridge switch clock

# 切换回使魔萌宠
adb shell phoenix_agent_app cartridge switch familiar
```

### 8.3 具身外设工具独立触发 (Tool Calling)
```bash
# 1. 查询所有已注册具身工具及其 JSON Schema
adb shell phoenix_agent_app tools

# 2. 独立触发赛博木鱼工具
adb shell phoenix_agent_app tool knock_wooden_fish '{"count":3}'

# 3. 独立触发番茄专注流工具
adb shell phoenix_agent_app tool manage_pomodoro '{"action":"start","duration_min":25}'

# 4. 独立触发灵眸眼球微表情情绪调节
adb shell phoenix_agent_app tool set_eye_emotion '{"emotion":"happy"}'
```

### 8.4 模拟大模型端云协同对话
```bash
adb shell phoenix_agent_app ask "帮我开启25分钟沉浸专注番茄钟"
```
控制台将输出 ReAct 思维链、决策自动触发 `manage_pomodoro` 工具，并打印智能体回答。

---

## 9. 全自动化回归与集成测试套件 (Test Suites)

### 9.1 宿主机极速离线单元测试 (0.5 秒完成 28 项单测)
在开发机代码根目录下直接运行：
```bash
./app/phoenix_agent_app/test/run_host_test.sh
```
该测试涵盖：
1. 事件总线多模块订阅分发；
2. 具身 Tool Registry 动态注册与 Schema 生成；
3. 持久化数据存储原子性测试；
4. 具身工具沙箱执行；
5. ReAct Agent 思考循环；
6. 思维链解析与历史滑动窗口压缩；
7. 原生应用调度；
8. 统一应用门面生命周期；
9. 大模型 Mock/云端双后端热切换；
10. 主动心智引擎演进；
11. Web 控制台与 REST API 路由；
12. Token 消耗与系统遥测；
13. HAL 硬件抽象与统一虚表；
14. 20Hz 具身感知与微敲击滤波；
15. 异步事件队列高并发写入；
16. 灵眸眼球拟人表情变换；
17. 意图分发极速通道；
18. 配置子系统持久化回读；
19. 麦克风音频采集与 PCM 流分发；
20. 环形追踪缓冲区动态覆盖；
21. 高精单调时钟与格式化日志；
22. 场景卡带容器生命周期与手势轮换；
23. SoftAP 热点与 Captive Portal 强制门户；
24. 多卡带独立业务数据与 Web 导出；
25. TF 卡热插拔与文件管理 Web API；
26. 灵眸使魔亲密度交互；
27. BLE 蓝牙极速双向配网协议；
28. **持久化 Flash 文件滚动黑匣子与控制台多通道分流**。

### 9.2 板端无头自动化端到端测试
在板端通过 ADB 一键运行：
```bash
adb shell phoenix_agent_app --autotest-cli
```
系统将自动巡检各个硬件驱动并输出测试通过状态报告。
