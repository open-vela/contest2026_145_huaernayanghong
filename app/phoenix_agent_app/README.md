# Phoenix HoloDesk-S1 桌面具身灵眸 AI 机器人 (2026 openvela 大赛作品)

## 📌 一、项目简介
**Phoenix HoloDesk-S1** 是基于 **OpenVela 实时嵌入式操作系统** 与 **润芯微 Gemini-S1（全志 R528-S3 双核 A7 @ 1.2GHz + 128MB DDR3 + HiFi4 DSP）** 硬件平台打造的桌面具身 AI 智能体硬件。

它将拟人化、富有生命力的 **Living Cyber-Eye（灵眸微表情系统）** 与 **云端 LLM 引擎（MiMo / DeepSeek）**、**声明式 Tool Calling 机制**、**发布-订阅轻量事件总线**、**开发者工作流（CI/CD & 功德木鱼）** 深度融合，为开发者提供一个有温度、有交互、能感知的桌面物理伴侣。

---

## 🌟 二、核心功能特性

### 1. 灵眸具身拟人表情系统 (Living Cyber-Eye)
- **矢量平滑渲染**：基于 LVGL 9 矢量对象，呈现发光呼吸外环（Halo）、巩膜渐变、深邃瞳孔与晶莹高光。
- **自主生命感行为**：内置微动（Micro-saccades）插值算法，支持周期性自主眨眼、随情绪变换色彩（青蓝待命、琥珀倾听、赛博紫思考、金辉开心、猩红警戒、幽蓝困倦）。
- **眼球触控反馈**：点击眼球即可触发互动与木鱼敲击。

### 2. 声明式 Tool Calling 工具体系 (Declarative Tools)
- **零负担扩展**：在 `tools/` 目录编写标准函数并调用 `phoenix_tool_register` 即可注册新技能。
- **内置具身工具集**：
  - `knock_wooden_fish`：敲击木鱼，积攒赛博功德，触发飞字与清脆音效；
  - `manage_pomodoro`：25 分钟沉浸专注流番茄钟定时器；
  - `set_eye_emotion`：大模型意图自主控制灵眸微表情与光环色彩；
  - `query_system_health`：OpenVela 嵌入式系统与硬件健康状态巡检。

### 3. 硬件抽象层与双端驱动 (HAL Architecture)
- **多设备驱动虚表 (VTable)**：统一抽象传感器（`hal_sensor.h`：桌面敲击/环境光 ALS/电池）、执行器（`hal_actuator.h`：音频 PCM/触觉马达/LED）与系统能力（`hal_system.h`：真实动态遥测/原生应用调度）。
- **双端无缝解耦**：内建 Gemini-S1 板载真实驱动（`/dev/audio`, procfs, mallinfo）与 Host 宿主机仿真/注入驱动，软硬件彻底解耦。

### 4. 具身感知闭环引擎 (Perception Engine)
- **物理事件驱动心智**：感知引擎负责硬件信号滤波、去抖动与高层事件融合，实现物理敲击桌面自动触发木鱼功德+1、夜间暗光自动推荐伴睡白噪音、低电量自主警戒与省电守护。

### 5. 轻量级事件总线 (Event Bus)
- 彻底解耦 UI 渲染与业务调度。UI 与 Agent 仅需订阅对应事件，无缝驱动多模态交互。

---

## 🧩 三、模块架构与核心文件

```text
app/phoenix_agent_app/
├── core/                                 # 🧠 1. 核心调度与基础设施层 (纯 C，无图形依赖)
│   ├── app.h / .c                #    应用顶层统一门面 (Facade，负责 HAL/Perception/Core 协同初始化)
│   ├── agent_core.h / .c         #    ReAct 思考循环、主动心智引擎与历史压缩
│   ├── event_bus.h / .c          #    轻量级发布-订阅事件总线 (支持 HAL 感知事件)
│   ├── store.h / .c              #    持久化存储引擎 (功德/番茄/交互统计)
│   ├── tool_registry.h / .c      #    声明式 Tool Calling 注册中心与 Schema 提取
│   └── web_portal.h / .c         #    内嵌极客 Web Portal 控制台与 REST API
├── hal/                                  # ⚡ 2. 硬件抽象层 (HAL / PAL 软硬件解耦核心)
│   ├── hal_types.h                       #    传感器、执行器与系统遥测核心数据结构
│   ├── hal_driver.h                      #    统一驱动虚表操作接口 (Ops VTable)
│   ├── hal_manager.h / .c                #    HAL 管理中枢与平台驱动自动装载
│   ├── hal_sensor.h / .c                 #    传感器统一接口 (敲击/环境光/电池)
│   ├── hal_actuator.h / .c               #    执行器统一接口 (音频/触觉马达/LED光环)
│   ├── hal_system.h / .c                 #    系统底层服务 (真实遥测健康度/应用拉起)
│   └── drivers/                          #    双端驱动实现
│       ├── hal_driver_mock.h / .c        #    宿主机仿真驱动 (支持单测物理事件注入)
│       └── hal_driver_openvela.h / .c    #    Gemini-S1 板载真实硬件驱动
├── perception/                           # 👁️ 3. 具身感知引擎 (Perception Layer)
│   └── perception.h / .c         #    物理信号滤波、防抖与具身事件提升引擎
├── harness/                              # 🌐 4. 大模型与通信适配器层 (驱动抽象与分层)
│   ├── llm_types.h               #    消息数据契约、思维链字段与通用内存管理
│   ├── llm_backend.h             #    多后端驱动策略抽象接口
│   ├── llm_mock_backend.h / .c   #    离线仿真驱动 (关键词意图推导与离线思维链)
│   ├── llm_cloud_backend.h / .c  #    云端与 VelaClaw IPC 驱动 (MiMo/OpenAI规范)
│   └── llm_provider.h / .c       #    统一 Provider 调度门面 (支持运行时驱动热切换)
├── tools/                                # 🛠️ 5. 具身外设技能插件层
│   ├── tools.h / .c              #    内置具身工具集中注册入口
│   ├── tool_wooden_fish.c                #    赛博木鱼 (联动触觉马达+音频+功德存储)
│   ├── tool_pomodoro.c                   #    沉浸专注流 25 分钟番茄钟
│   ├── tool_eye_emotion.c                #    灵眸情绪微表情与颜色控制
│   ├── tool_system_health.c              #    基于 HAL 真实系统指标动态遥测
│   └── tool_launch_app.c                 #    基于 HAL 系统服务的原生应用调度
├── ui/                                   # 🎨 6. 视觉交互与多媒体层
│   ├── ui.h / .c                 #    LVGL 桌面看板 UI、飞字动画与事件总线监听
│   ├── eye_anim.h / .c           #    灵眸眼球矢量渲染引擎与微表情动画
│   ├── audio_ctl.h / .c          #    向后兼容的音效控制垫片 (桥接至 HAL)
│   └── lv_font_chinese_16.c              #    中文点阵字库资源
├── test/                                 # 🧪 7. 宿主机全保真单测套件 (14大单元测试)
│   ├── host_runner.c                     #    宿主机 Native 测试驱动与交互式 REPL
│   └── run_host_test.sh                  #    0.2 秒宿主机一键验证脚本
├── main.c              # 顶层 POSIX 应用入口与事件主循环
├── CMakeLists.txt                        # CMake 构建配置
├── Make.defs / Makefile                  # Make 构建配置
└── Kconfig                               # 内核与应用配置选项
```

---

## 🚀 四、编译与运行指南

### 1. 开启构建配置
在 `menuconfig` 中勾选：
`Application Configuration -> Demos -> Contest 2026 team 145 Phoenix Agent App`
或确保 `.config` 中包含：
```ini
CONFIG_LVX_USE_DEMO_CONTEST2026_145_PHOENIX_AGENT_APP=y
```

### 2. 快速构建
```bash
# 在 OpenVela 根目录下执行：
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap --cmake -j8
```

### 3. 运行模拟器
```bash
./emulator.sh vela -skin xiaomi_watch_s1
```

在终端 NSH 命令行中运行：
```bash
goldfish-armv8a-ap> phoenix_agent_app
```

即可在屏幕上看到灵眸发光眼球、状态看板、飞字动效与四个多功能快捷按钮！

---

## 🛠️ 五、真机适配（润芯微 Gemini-S1 / R528-S3）
1. 使用 R528-S3 开发板专用 defconfig 进行构建与烧录；
2. 声音资源（WAV 文件）放置于开发板 `/data/sounds/` 目录；
3. 支持板载 HiFi4 DSP 音频拾音与大尺寸 RGB/MIPI 屏幕硬件 2D 加速。

---

## 🧪 六、端到端自动化测试与 AI 视觉验收套件

本项目内置了一套 **全自动模拟器端到端驱动 + UI 场景自走测 + 屏幕快照采集 + AI 视觉验收** 自动化框架：

### 1. 宿主机秒级全保真单测 (0.2秒极速排查)
```bash
./app/phoenix_agent_app/test/run_host_test.sh
```

### 2. 模拟器 CLI 子系统自动化冒烟测试
```bash
python3 app/phoenix_agent_app/test/run_auto_test.py --mode cli
```

### 3. 模拟器 320x240 GUI 全场景自驱动测试与 AI 视觉验收 (全自动截图+生成报告)
```bash
python3 app/phoenix_agent_app/test/run_auto_test.py --mode gui --screen 320x240
```
执行完成后，将在 `test/test_artifacts/` 自动生成：
- 各场景高清屏幕快照：`screenshots/01_clock.png` ~ `06_agent_dialog.png`；
- 科技感可视化交互式测试报告：`report.html`。

