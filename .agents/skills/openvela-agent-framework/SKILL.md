---
name: openvela-agent-framework
description: "OpenVela / Phoenix 具身智能体 (Agent Core) 架构规范、DeepSeek 端云协同大模型接入、ReAct 决策闭环、端侧工具链 (Tool Registry) 扩展开发及 ADB 命令行测试全流程实战指南。"
---

# Phoenix 具身智能体 (Agent Framework) 开发与 ADB 测试指南

本指南专为 Phoenix HoloDesk-S1 具身灵眸数字生命体定制，涵盖 Master Agent 状态机调度、两级长程记忆模型、DeepSeek/大模型端云协同传输层（libcurl/TLS）、端侧工具链注册扩展（Tool Registry）以及基于 ADB 命令行的无头测试与自动化巡检全流程。

---

## 1. 核心心智架构与模块目录

智能体心智架构严格遵循 **ReAct (Reasoning + Acting)** 范式，通过事件总线连接认知模型与具身外设：

```text
[语音/按键/ADB 输入]
        │
        ▼
[Fast-path 意图分发器 (intent_router)] ──(极速槽位命中)──> [0ms 本地直达执行]
        │ (非直达指令)
        ▼
[Master Agent 状态调度器 (agent_core)]
        │ 状态流转: IDLE -> THINKING -> EXECUTING -> CELEBRATING
        │ 记忆管理: 两级压缩机制 (当前窗口 + 滚动历史摘要卡片)
        ▼
[大模型接入网关 (llm_provider / llm_cloud_backend)]
        │ 传输驱动: libcurl (HTTPS/TLS) + 宽松证书校验 + 局域网反向代理
        ▼
[云端推理引擎 (DeepSeek / MiMo)] ──(Thinking 思维链 + Function Call)
        │
        ▼
[具身工具执行中心 (tool_registry)]
        │ 调度端侧 C 函数: 木鱼敲击 / 番茄钟 / 微表情 / 健康查询 / 原生应用
        ▼
[统一事件总线 (event_bus)] ──> 触发 LCD 浮字动效 / 音效 / 物理震动反馈
```

### 1.1 关键源码目录一览
* `core/agent_core.c` / `.h`：智能体主调度器、ReAct 5 轮循环控制、两级记忆压缩引擎。
* `core/tool_registry.c` / `.h`：端侧具身工具注册表与 JSON Schema 序列化器。
* `core/config.c` / `.h`：Flash KV 配置管理器（存储 API Key、Base URL、Model 等）。
* `harness/llm_cloud_backend.c`：云端通信适配器（libcurl HTTPS、握手超时控制、HTTP 错误解析）。
* `harness/llm_provider.c`：模型后端 Facade 外观（支持 Cloud 与 Mock 动态平滑升级）。
* `tools/`：内置具身工具集（木鱼功德、番茄钟、灵眸眼球表情、硬件遥测健康、应用拉起）。
* `main.c`：ADB / CLI 诊断与自动化测试命令入口。

---

## 2. 大模型端云接入与配置规范 (LLM Harness)

### 2.1 嵌入式通信传输层规范 (`llm_cloud_backend.c`)
嵌入式设备网络通信存在内存受限与本地 CA 根证书缺失的特殊性，必须严格遵循以下通信准则：
1. **HTTPS / TLS 传输支持**：底层基于 OpenVela 系统已内建的 `CONFIG_LIB_CURL=y` 与 `mbedtls`，确保与现代大模型 API 的 TLS 1.2/1.3 握手顺畅。
2. **证书校验模式**：显式设置 `CURLOPT_SSL_VERIFYPEER = 0L` 与 `CURLOPT_SSL_VERIFYHOST = 0L`，规避嵌入式文件系统缺少 `/etc/ssl/certs` 根证书导致的握手失败。
3. **超时保护控制**：连接超时严格限定为 `10s`，整体会话传输超时限定为 `30s`，防止网络阻塞导致主线程看门狗超时。
4. **Token 与堆水位保护**：`max_tokens` 严格控制在 `1024 ~ 2048`，响应 JSON 缓冲区设置 `64KB` 堆上限保护。
5. **支持局域网中转调试**：支持动态修改 `llm_base_url` 为 PC 宿主机代理（如 `http://192.168.x.x:8000/v1/chat/completions`）。

### 2.2 两级长程记忆压缩机制
为防止多轮对话耗尽嵌入式 RAM，`agent_core.c` 实现了滑动窗口与语义提炼两级记忆：
* **最近上下文**：保留最近 12 条对话消息（强制保持 Tool Call 与 Tool Result 成对保留）；
* **历史前情摘要**：即将丢弃的老对话提炼为增量关键词卡片，写入 `context_summary`，作为 System Prompt 注入后续会话。

---

## 3. 端侧具身工具开发指南 (Tool Registry)

所有端侧外设或业务插件均需通过标准接口注册到智能体工具池，以便大模型自主决定调用。

### 3.1 编写新工具标准流程
1. **定义参数结构与执行回调**：
```c
#include "core/tool_registry.h"
#include <cJSON.h>

static int tool_my_sensor_execute(const char *json_args, char *result_buf, size_t buf_sz)
{
    /* 1. 解析大模型传入的 JSON 参数 */
    cJSON *root = cJSON_Parse(json_args ? json_args : "{}");
    const char *sensor_type = "temp";
    if (root) {
        cJSON *t = cJSON_GetObjectItem(root, "type");
        if (t && t->valuestring) sensor_type = t->valuestring;
    }

    /* 2. 调用硬件 HAL 或底层驱动读取数据 */
    float val = 25.6f;

    /* 3. 构造输出给大模型的结构化 Observation 文本 */
    snprintf(result_buf, buf_sz, "{\"sensor\":\"%s\",\"value\":%.1f,\"unit\":\"C\"}",
             sensor_type, val);

    if (root) cJSON_Delete(root);
    return 0; /* 0 表示执行成功 */
}
```

2. **声明 JSON Schema 并完成注册**：
```c
void my_tools_register(void)
{
    phoenix_tool_def_t def;
    memset(&def, 0, sizeof(def));
    def.name = "read_environment_sensor";
    def.description = "读取开发板当前环境传感器数据(支持温度temp、湿度humi、光照light)";
    def.parameters_json = "{"
        "\"type\":\"object\","
        "\"properties\":{"
            "\"type\":{\"type\":\"string\",\"enum\":[\"temp\",\"humi\",\"light\"],\"description\":\"传感器指标类型\"}"
        "},"
        "\"required\":[\"type\"]"
    "}";
    def.execute = tool_my_sensor_execute;

    phoenix_tool_register(&def);
}
```

3. **联动事件总线反馈（可选）**：
在工具执行函数内，可通过 `phoenix_event_publish()` 发送飘字动效（`PHOENIX_EVT_FLYING_TEXT`）或音效（`PHOENIX_EVT_PLAY_SOUND`），实现软硬件声光同步。

---

## 4. ADB 终端调试与测试实战手册

在无需启动 LVGL 屏幕 GUI（无头模式）的情况下，所有智能体功能均可通过 ADB 命令行完成单步验证与自动化回归。

### 4.1 命令总览表

| 分类 | ADB 调试命令 | 说明 |
| :--- | :--- | :--- |
| **配置** | `adb shell phoenix_agent_app config get [key]` | 查看系统配置（API Key 脱敏显示） |
| | `adb shell phoenix_agent_app config set api_key <key>` | 录入并持久化大模型 API Key |
| | `adb shell phoenix_agent_app config set base_url <url>` | 修改模型请求 Endpoint |
| | `adb shell phoenix_agent_app config set model <model>` | 设置模型名（默认 deepseek-chat） |
| | `adb shell phoenix_agent_app config reset` | 恢复出厂默认配置 |
| **连通** | `adb shell phoenix_agent_app llm ping` | 快速测试网络握手、TLS 与 Key 鉴权（1秒返回） |
| **问答** | `adb shell phoenix_agent_app llm chat "<prompt>"` | 直接与大模型单轮问答（跳过工具调用） |
| **工具** | `adb shell phoenix_agent_app tools` | 列出当前已注册全部工具及 Schema |
| | `adb shell phoenix_agent_app tool <name> '<args>'` | 独立执行指定具身工具并返回结果 |
| **智能体**| `adb shell phoenix_agent_app agent ask "<prompt>"` | 启动完整 ReAct 决策闭环与端侧工具调度 |
| **自检** | `adb shell phoenix_agent_app agent selftest` | 一键运行端到端自动化 4 阶段全流程巡检 |

---

### 4.2 典型测试用例操作序列

#### 用例 1：首次使用配置与网络连通性探测
```bash
# 1. 录入你的 DeepSeek API Key (存盘至 /data/phoenix/phoenix_config.txt)
adb shell phoenix_agent_app config set api_key sk-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

# 2. 检查配置是否生效
adb shell phoenix_agent_app config get

# 3. 执行轻量心跳探测 (核验 DNS、TLS 及鉴权状态)
adb shell phoenix_agent_app llm ping
```
* **期望输出**：`[CLI:LLM] ✅ 探测成功! HTTP 200 (OK), 往返耗时: 320 ms`。

#### 用例 2：直接验证大模型思考模式与回答
```bash
adb shell phoenix_agent_app llm chat "用简短的一句话介绍你自己"
```
* **期望输出**：打印大模型的 `[CLI:LLM:Thinking]` 思维链内容、`[CLI:LLM:Answer]` 回复正文以及消耗的 Token 统计。

#### 用例 3：独立单步测试具身工具
```bash
# 测试敲击木鱼累加功德
adb shell phoenix_agent_app tool knock_wooden_fish '{"count":3}'

# 测试查询设备健康遥测
adb shell phoenix_agent_app tool system_health '{}'

# 测试番茄钟服务状态
adb shell phoenix_agent_app tool manage_pomodoro '{"action":"status"}'
```

#### 用例 4：验证 Agent 意图决策与工具调用闭环 (Tool Calling)
```bash
adb shell phoenix_agent_app agent ask "我感觉工作有点累，帮我敲3下赛博木鱼积攒功德"
```
* **终端实时 Trace 闭环核验点**：
  1. `[Turn 1 Thinking]`：大模型识别用户放松需求，决策调用 `knock_wooden_fish`；
  2. `[Turn 1 Tool Call]`：触发 `knock_wooden_fish(args: {"count":3})`；
  3. `[Turn 1 Tool Result]`：端侧木鱼功德服务返回 `{"success":true,"added_merit":3,"total_merit":3}`；
  4. `[Turn 2 Final Answer]`：大模型综合工具执行结果，生成拟人化祝贺回复并结束轮次。

#### 用例 5：一键全量自动化自检 (CI / 产测模式)
```bash
adb shell phoenix_agent_app agent selftest
```
* **自动化覆盖范围**：
  - `[Test 1/4]` 配置项合法性与 Key 持久化回读；
  - `[Test 2/4]` 云端模型心跳握手与延迟测试；
  - `[Test 3/4]` 5 大内置具身工具独立单元执行；
  - `[Test 4/4]` 端到端 ReAct 真实工具调用回环；
  - 自动输出 `[PASS / FAIL]` 汇总比率。

---

## 5. 宿主机 (Host) 极速免烧录仿真测试

在未连接板端硬件时，可在 macOS / Linux 宿主机上直接秒级编译并执行包含 Mock 驱动的全量单元测试：

```bash
# 进入测试目录
cd contest2026_145_huaernayanghong/app/phoenix_agent_app/test

# 运行全量 29 项单元测试与 ReAct 状态机验证
./run_host_test.sh

# 进入交互式命令行交互会话模式
./run_host_test.sh -i
```
* **回归产物**：全部 29 个用例均应返回 `PASSED`，验证内存紧缩、事件总线、卡带生命周期与大模型 Harness 状态流转无内存泄漏或死锁。
