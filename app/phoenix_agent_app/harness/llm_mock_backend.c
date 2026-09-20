/**
 * @file llm_mock_backend.c
 * @brief Offline Mock & Embody Fallback Backend Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "llm_mock_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int mock_reasoning_eval(const char *user_prompt, phoenix_chat_resp_t *resp_out)
{
    if (!user_prompt || !resp_out) return -1;

    /* 1. 木鱼/功德意图 */
    if (strstr(user_prompt, "木鱼") || strstr(user_prompt, "功德") || strstr(user_prompt, "敲") || strstr(user_prompt, "pr")) {
        resp_out->is_tool_use = true;
        resp_out->tool_name = strdup("knock_wooden_fish");
        resp_out->tool_call_id = strdup("call_mock_fish_001");
        resp_out->tool_input = strdup("{\"count\":1}");
        resp_out->reasoning_content = strdup("【思维链】用户意图为敲击木鱼积攒功德，决定调用 knock_wooden_fish 具身工具并展示功德动效。");
        resp_out->content = strdup("主人，为你敲响赛博木鱼，愿代码零Bug，功德+1！");
        return 0;
    }

    /* 2. 番茄钟/专注工作意图 */
    if (strstr(user_prompt, "番茄") || strstr(user_prompt, "专注") || strstr(user_prompt, "工作")) {
        resp_out->is_tool_use = true;
        resp_out->tool_name = strdup("manage_pomodoro");
        resp_out->tool_call_id = strdup("call_mock_pomo_001");
        resp_out->tool_input = strdup("{\"action\":\"start\",\"duration_minutes\":25}");
        resp_out->reasoning_content = strdup("【思维链】识别到开发者沉浸专注意图，规划开启标准 25 分钟番茄钟流并触发提醒。");
        resp_out->content = strdup("沉浸专注流已启动，灵眸将伴你专注编码25分钟！");
        return 0;
    }

    /* 3. 待办事项管理意图 */
    if (strstr(user_prompt, "待办") || strstr(user_prompt, "todo") || strstr(user_prompt, "备忘")) {
        resp_out->is_tool_use = true;
        resp_out->tool_name = strdup("manage_todo");
        resp_out->tool_call_id = strdup("call_mock_todo_001");
        if (strstr(user_prompt, "添加") || strstr(user_prompt, "新增")) {
            resp_out->tool_input = strdup("{\"action\":\"add\",\"title\":\"智能体规划待办\",\"time_str\":\"今日\"}");
            resp_out->reasoning_content = strdup("【思维链】用户要求新增待办，规划调度 manage_todo 工具添加待办条目。");
            resp_out->content = strdup("正在为你添加到待办清单...");
        } else if (strstr(user_prompt, "清理") || strstr(user_prompt, "清空")) {
            resp_out->tool_input = strdup("{\"action\":\"clear_done\"}");
            resp_out->reasoning_content = strdup("【思维链】清理已完成待办，规划调度 manage_todo clear_done。");
            resp_out->content = strdup("正在为你清理已完成的待办条目...");
        } else {
            resp_out->tool_input = strdup("{\"action\":\"list\"}");
            resp_out->reasoning_content = strdup("【思维链】用户查询待办清单，规划调度 manage_todo list 获取最新待办列表。");
            resp_out->content = strdup("正在为你获取待办清单...");
        }
        return 0;
    }

    /* 4. 环境温湿度与传感器感知意图 */
    if (strstr(user_prompt, "温湿度") || strstr(user_prompt, "温度") || strstr(user_prompt, "湿度") ||
        strstr(user_prompt, "光照") || strstr(user_prompt, "环境")) {
        resp_out->is_tool_use = true;
        resp_out->tool_name = strdup("query_environment");
        resp_out->tool_call_id = strdup("call_mock_env_001");
        resp_out->tool_input = strdup("{}");
        resp_out->reasoning_content = strdup("【思维链】用户询问当前环境状态，调度 query_environment 读取板载传感器数据。");
        resp_out->content = strdup("正在为你采集板载温湿度及光照传感器数据...");
        return 0;
    }

    /* 5. 系统健康巡检意图 */
    if (strstr(user_prompt, "状态") || strstr(user_prompt, "健康") || strstr(user_prompt, "系统")) {
        resp_out->is_tool_use = true;
        resp_out->tool_name = strdup("query_system_health");
        resp_out->tool_call_id = strdup("call_mock_sys_001");
        resp_out->tool_input = strdup("{}");
        resp_out->reasoning_content = strdup("【思维链】诊断请求：调用 query_system_health 读取板载嵌入式健康遥测数据。");
        resp_out->content = strdup("正在为你巡检 OpenVela 嵌入式系统健康状态...");
        return 0;
    }

    /* 4. 灵眸眼球表情意图 */
    if (strstr(user_prompt, "开心") || strstr(user_prompt, "笑") || strstr(user_prompt, "表情")) {
        resp_out->is_tool_use = true;
        resp_out->tool_name = strdup("set_eye_emotion");
        resp_out->tool_call_id = strdup("call_mock_emo_001");
        resp_out->tool_input = strdup("{\"emotion\":\"happy\"}");
        resp_out->reasoning_content = strdup("【思维链】情感计算检测到正向共鸣，切换机械灵眸表情为 HAPPY 状态。");
        resp_out->content = strdup("灵眸现在非常开心！金色光环为你闪耀。");
        return 0;
    }

    /* 5. 原生系统应用启动调度 */
    if (strstr(user_prompt, "打开") || strstr(user_prompt, "启动") ||
        strstr(user_prompt, "日历") || strstr(user_prompt, "计算器") ||
        strstr(user_prompt, "白噪音") || strstr(user_prompt, "录音") ||
        strstr(user_prompt, "翻译") || strstr(user_prompt, "应用")) {
        const char *app = "日历";
        if (strstr(user_prompt, "计算器")) app = "计算器";
        else if (strstr(user_prompt, "白噪音")) app = "白噪音";
        else if (strstr(user_prompt, "录音") || strstr(user_prompt, "会议")) app = "会议录音";
        else if (strstr(user_prompt, "翻译")) app = "翻译";
        else if (strstr(user_prompt, "时钟") || strstr(user_prompt, "闹钟")) app = "时钟";
        else if (strstr(user_prompt, "设置")) app = "设置";

        char input_buf[128];
        snprintf(input_buf, sizeof(input_buf), "{\"app_name\":\"%s\"}", app);

        resp_out->is_tool_use = true;
        resp_out->tool_name = strdup("launch_system_app");
        resp_out->tool_call_id = strdup("call_mock_launch_001");
        resp_out->tool_input = strdup(input_buf);
        resp_out->reasoning_content = strdup("【思维链】识别到系统级应用调度意图，调用 launch_system_app 具身工具调起原生应用。");
        char content_buf[256];
        snprintf(content_buf, sizeof(content_buf), "正在为你启动系统原生应用【%s】...", app);
        resp_out->content = strdup(content_buf);
        return 0;
    }

    /* 6. 通用对话回复 */
    resp_out->is_tool_use = false;
    resp_out->reasoning_content = strdup("【思维链】通用交互语句，无需触发硬件外设工具，直接组织温暖具身的拟人化回复。");
    char reply_buf[256];
    snprintf(reply_buf, sizeof(reply_buf),
             "灵眸已收到指令: \"%s\"。我是你的桌面具身助手，随时听候调度！", user_prompt);
    resp_out->content = strdup(reply_buf);
    return 0;
}

static int mock_backend_init(phoenix_llm_backend_t *self, const phoenix_llm_config_t *config)
{
    (void)self;
    (void)config;
    return 0;
}

static int mock_backend_chat(phoenix_llm_backend_t *self,
                            const phoenix_chat_msg_t *messages,
                            size_t msg_count,
                            const char *tools_json,
                            phoenix_chat_resp_t *resp_out)
{
    (void)self;
    (void)tools_json;
    if (!resp_out) return -1;
    memset(resp_out, 0, sizeof(phoenix_chat_resp_t));

    /* 工具执行结果回调的收尾回复 */
    if (msg_count > 0 && messages[msg_count - 1].role == PHOENIX_ROLE_TOOL) {
        resp_out->is_tool_use = false;
        resp_out->reasoning_content = strdup("【思维链】具身工具已执行完毕，生成面向开发者的自然语言交互总结。");
        char summary_buf[256];
        snprintf(summary_buf, sizeof(summary_buf), "灵眸已执行完毕指令！(结果: %s)",
                 messages[msg_count - 1].content ? messages[msg_count - 1].content : "ok");
        resp_out->content = strdup(summary_buf);
        resp_out->prompt_tokens = (uint32_t)(msg_count * 28 + 15);
        resp_out->completion_tokens = (uint32_t)(strlen(resp_out->content) / 2 + 10);
        resp_out->total_tokens = resp_out->prompt_tokens + resp_out->completion_tokens;
        resp_out->latency_ms = 85 + (resp_out->total_tokens % 30);
        return 0;
    }

    /* 寻找最后一条来自用户的输入 */
    const char *last_user_text = "你好";
    for (int i = (int)msg_count - 1; i >= 0; i--) {
        if (messages[i].role == PHOENIX_ROLE_USER && messages[i].content) {
            last_user_text = messages[i].content;
            break;
        }
    }

    int ret = mock_reasoning_eval(last_user_text, resp_out);
    if (ret == 0 && resp_out) {
        resp_out->prompt_tokens = (uint32_t)(msg_count * 28 + 15);
        resp_out->completion_tokens = resp_out->content ? (uint32_t)(strlen(resp_out->content) / 2 + 10) : 25;
        resp_out->total_tokens = resp_out->prompt_tokens + resp_out->completion_tokens;
        resp_out->latency_ms = 120 + (resp_out->total_tokens % 50);
    }
    return ret;
}

static bool mock_backend_is_connected(phoenix_llm_backend_t *self)
{
    (void)self;
    return false; /* 本地离线驱动无云端/系统级 IPC 连接 */
}

static int mock_backend_ping(phoenix_llm_backend_t *self,
                            uint32_t *latency_ms,
                            int *http_status,
                            char *err_buf,
                            size_t err_sz)
{
    (void)self;
    if (latency_ms) *latency_ms = 0;
    if (http_status) *http_status = 200;
    if (err_buf && err_sz > 0) {
        snprintf(err_buf, err_sz, "Mock backend offline loopback OK");
    }
    return 0;
}

static void mock_backend_deinit(phoenix_llm_backend_t *self)
{
    (void)self;
}

static phoenix_llm_backend_t g_mock_backend_instance = {
    .name = "MockOfflineBackend",
    .init = mock_backend_init,
    .chat = mock_backend_chat,
    .is_connected = mock_backend_is_connected,
    .ping = mock_backend_ping,
    .deinit = mock_backend_deinit,
    .user_data = NULL
};

phoenix_llm_backend_t *phoenix_llm_mock_backend_create(void)
{
    return &g_mock_backend_instance;
}
