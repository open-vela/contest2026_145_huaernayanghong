---
name: openvela-phoenix-agent
description: "Phoenix HoloDesk-S1 桌面具身数字生命体业务卡带调度、DeepSeek 端云协同大模型、端侧工具链与 Web 伴侣看板开发指南。"
---

# Phoenix 具身卡带与 DeepSeek 端云协同开发指南

本指南专为 Phoenix HoloDesk-S1 桌面数字生命体与环境外脑工程定制，涵盖卡带（Cartridge）插件化生命周期规范、DeepSeek 大模型与端侧工具链调用（Tool Registry）、局域网 Web 孪生看板与 Host 仿真自动化测试验证。

---

## 1. 核心架构哲学与工程目录

```text
“硬件是躯壳（Hardware Vessel），卡带是心智（Agent Cartridge），网络是纽带（Companion Link）。”
```

整个工程采用模块化高内聚、低耦合架构，各子系统目录划分清晰：
* `cartridges/`：业务场景卡带（萌宠、灵感外脑、翻页时钟、赛博木鱼）。
* `core/`：卡带调度引擎（`cartridge_mgr`）、事件总线（`event_bus`）、意图分发器（`intent_router`）、微型 Web 服务（`web_portal`）。
* `harness/`：大模型接入后端（DeepSeek 接口、VelaClaw 总线适配、Mock 回环）。
* `tools/`：端侧能力工具集（木鱼功德、番茄钟、灵眸表情、健康监控）。
* `web/`：局域网 Web Companion 看板与 SoftAP 配网网页资产。
* `test/`：PC 宿主机免烧录快速验证套件（`run_host_test.sh`、`run_auto_test.py`）。

---

## 2. 卡带插件化规范 (`cartridge.h`)

所有卡带必须实现标准抽象接口 `cartridge_interface_t`，通过 `cartridge_mgr_register()` 动态挂载到调度器：

```c
typedef struct {
    const char *id;          /* 唯一标识符, 如 "cartridge.zen" */
    const char *name;        /* 友好显示名, 如 "赛博木鱼" */
    
    /* 生命周期回调 */
    int  (*init)(void);                                  /* 系统启动初始化 */
    int  (*enter)(lv_obj_t *stage_container);           /* 挂载到主舞台视窗 */
    void (*update)(uint32_t delta_ms);                   /* 帧循环周期刷新 */
    int  (*handle_event)(const event_t *event);          /* 响应敲击/按键/语音事件 */
    int  (*leave)(void);                                 /* 离开主舞台并休眠 */
    void (*deinit)(void);                                /* 卸载释放资源 */
} cartridge_interface_t;
```

### 2.1 四大核心业务卡带实现清单
1. **桌面萌宠卡带 (`cartridge_familiar.c`)**：
   - 伴读打字伴侣（键盘敲击微震触发搬出小电脑打字）、触摸抚摸眯眼摇尾巴。
2. **灵感胶囊外脑 (`cartridge_memo.c`)**：
   - 零手操实时录音示波器，本地循环 Flash 存储 200 条便签，自动划分 TODO / IDEA。
3. **拟物翻页钟与气象 (`cartridge_clock.c`)**：
   - 复古机械翻牌动效、整点打簧微震、天气雨滴粒子渲染、物理解压摇签。
4. **赛博木鱼与禅意 (`cartridge_zen.c`)**：
   - 真实木鱼音效、浮动“功德 +1”动效、心流番茄钟呼吸光环与本地热力图。

---

## 3. DeepSeek 端云协同与工具链注册 (Tool Registry)

### 3.1 DeepSeek 请求构建规范 (`llm_cloud_backend.c`)
在资源受限的嵌入式设备上，通过轻量级 HTTP Client 与 cJSON 进行请求打包：
* **模型名**：`deepseek-chat` 或端侧轻量小模型；
* **关键参数**：`max_tokens` 严格限制在 `1024 ~ 2048`，防止嵌入式堆内存耗尽；
* **Prompt 设计**：注入设备具身人设，并以标准 JSON Schema 形式声明端侧可用工具。

### 3.2 端侧 Tool 注册与自动触发
```c
#include "tools/tools.h"

/* 注册木鱼敲击工具 */
tool_register(&(tool_def_t){
    .name = "knock_wooden_fish",
    .description = "敲击赛博木鱼一次以积累功德并平复情绪",
    .execute = tool_wooden_fish_execute
});

/* 注册番茄钟启动工具 */
tool_register(&(tool_def_t){
    .name = "start_pomodoro",
    .description = "启动专注番茄钟计时(单位:分钟)",
    .execute = tool_pomodoro_execute
});
```
当大模型输出 Function Call 时，意图分发器（`intent_router`）自动解析并执行对应 C 函数，同时在顶部微胶囊弹出气泡提示。

---

## 4. Web Companion 孪生伴侣工作台

设备连入局域网（STA 模式）后，在同一局域网的电脑/手机浏览器中访问 `http://<设备IP>:8080` 即可打开伴侣工作台：

```text
┌─────────────────────────────────────────────────────────────┐
│  Phoenix HoloDesk-S1 Companion Dashboard                    │
├───────────────────────────┬─────────────────────────────────┤
│ [灵感速记与待办看板]       │ [具身萌宠人设与提示词微调]       │
│ • TODO: 完善 OpenVela 驱动 │ 语气风格: [幽默极客 ▼]           │
│ • IDEA: 3D 打印外壳导光柱  │ 自定义 Prompt: "你是一个可爱的..."│
│ [导出 Markdown]           │                                 │
├───────────────────────────┴─────────────────────────────────┤
│ [端侧卡带状态与遥控]                                         │
│ 当前卡带: [赛博木鱼]  功德总计: 1,024  番茄钟: 25:00        │
└─────────────────────────────────────────────────────────────┘
```

* **实现细节**：静态 HTML/CSS/JS 经由 `web/sync_assets.py` 压缩编译为 C 字节数组直接打包进固件，零额外文件系统依赖。
* **API 接口**：
  - `GET /api/v1/status`：获取当前卡带、IP、电量与传感器读数。
  - `GET /api/v1/notes`：拉取本地灵感速记列表。
  - `POST /api/v1/config`：保存萌宠人设 Prompt 与系统配置。

---

## 5. 宿主机 (Host) 极速测试与回归验证

无需频繁连接开发板或烧录固件，可在 macOS / Linux 宿主机上秒级运行单元测试与 UI 自动化回归：

```bash
# 进入测试目录
cd contest2026_145_huaernayanghong/app/phoenix_agent_app/test

# 1. 运行宿主机 C 单元测试 (含 Mock 驱动与卡带状态机验证)
./run_host_test.sh

# 2. 运行 Python 端到端自动化驱动测试与屏幕抓图生成
python3 run_auto_test.py
```

测试执行完毕后，自动化产物存放在 `test_artifacts/` 目录：
* `report.html`：测试覆盖率与执行报告；
* `screenshots/*.png`：各卡带全屏渲染截图，可直观核验 UI 布局是否符合小屏美学规范。
