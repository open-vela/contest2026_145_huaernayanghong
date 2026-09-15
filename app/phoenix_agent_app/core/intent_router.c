/**
 * @file intent_router.c
 * @brief Fast-path Intent Router & Workflow Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "intent_router.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

static bool contains_str(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return false;
    return strstr(haystack, needle) != NULL;
}

phoenix_intent_result_t phoenix_intent_route(const char *input_text)
{
    phoenix_intent_result_t res;
    memset(&res, 0, sizeof(res));

    if (!input_text || strlen(input_text) == 0) {
        res.category = INTENT_TYPE_NONE;
        return res;
    }

    /* Fast-path Direct Command Detection:
     * Fast-path is triggered when input has prefix 'fast:', 'cmd:', '直达:', '极速:', 
     * or contains explicit fast directives like '快敲', '极客伴工', '快速巡检'.
     */
    bool is_fast_directive = false;
    const char *cmd_ptr = input_text;

    if (strncmp(input_text, "fast:", 5) == 0) {
        is_fast_directive = true;
        cmd_ptr = input_text + 5;
    } else if (strncmp(input_text, "cmd:", 4) == 0) {
        is_fast_directive = true;
        cmd_ptr = input_text + 4;
    } else if (strncmp(input_text, "直达:", 7) == 0) {
        is_fast_directive = true;
        cmd_ptr = input_text + 7;
    } else if (strncmp(input_text, "极速:", 7) == 0) {
        is_fast_directive = true;
        cmd_ptr = input_text + 7;
    } else if (contains_str(input_text, "快敲") || contains_str(input_text, "快速巡检") ||
               contains_str(input_text, "极客伴工") || contains_str(input_text, "直达")) {
        is_fast_directive = true;
    }

    if (!is_fast_directive) {
        /* Natural language dialogue -> Full LLM ReAct & CoT Reasoning */
        res.category = INTENT_TYPE_LLM_REASONING;
        return res;
    }

    /* 1. Fast-path: Cyber Wooden Fish & Merit */
    if (contains_str(cmd_ptr, "木鱼") || contains_str(cmd_ptr, "功德") ||
        contains_str(cmd_ptr, "merit") || contains_str(cmd_ptr, "knock")) {
        res.category = INTENT_TYPE_FASTPATH;
        res.tool_name = "knock_wooden_fish";
        res.tool_args_json = "{\"count\":1}";
        res.fast_reply = "【极速直达】灵眸为你敲响赛博木鱼，功德+1！";
        return res;
    }

    /* 2. Fast-path: Pomodoro Focus Session */
    if (contains_str(cmd_ptr, "番茄") || contains_str(cmd_ptr, "专注") ||
        contains_str(cmd_ptr, "pomodoro") || contains_str(cmd_ptr, "focus")) {
        res.category = INTENT_TYPE_FASTPATH;
        res.tool_name = "manage_pomodoro";
        res.tool_args_json = "{\"action\":\"start\",\"minutes\":25}";
        res.fast_reply = "【极速直达】已为你启动25分钟极客番茄专注流！";
        return res;
    }

    /* 3. Fast-path: Embedded Telemetry & Health Inspection */
    if (contains_str(cmd_ptr, "巡检") || contains_str(cmd_ptr, "健康") ||
        contains_str(cmd_ptr, "电量") || contains_str(cmd_ptr, "内存") ||
        contains_str(cmd_ptr, "telemetry") || contains_str(cmd_ptr, "health")) {
        res.category = INTENT_TYPE_FASTPATH;
        res.tool_name = "query_system_health";
        res.tool_args_json = "{}";
        res.fast_reply = "【极速直达】已完成嵌入式底层硬件遥测自检！";
        return res;
    }

    /* 4. Fast-path: Emotion Control */
    if (contains_str(cmd_ptr, "开心") || contains_str(cmd_ptr, "高兴") || contains_str(cmd_ptr, "happy")) {
        res.category = INTENT_TYPE_FASTPATH;
        res.tool_name = "set_eye_emotion";
        res.tool_args_json = "{\"emotion\":\"happy\"}";
        res.fast_reply = "【极速直达】灵眸已切换至愉悦情绪状态！";
        return res;
    }
    if (contains_str(cmd_ptr, "警戒") || contains_str(cmd_ptr, "红光") || contains_str(cmd_ptr, "alert")) {
        res.category = INTENT_TYPE_FASTPATH;
        res.tool_name = "set_eye_emotion";
        res.tool_args_json = "{\"emotion\":\"alert\"}";
        res.fast_reply = "【极速直达】灵眸已进入安全警戒状态！";
        return res;
    }

    /* 5. Fast-path: Launch System Apps */
    if (contains_str(cmd_ptr, "日历") || contains_str(cmd_ptr, "calendar")) {
        res.category = INTENT_TYPE_FASTPATH;
        res.tool_name = "launch_system_app";
        res.tool_args_json = "{\"app_id\":\"calendar\"}";
        res.fast_reply = "【极速直达】已调度启动系统原生工作日历！";
        return res;
    }
    if (contains_str(cmd_ptr, "白噪音") || contains_str(cmd_ptr, "助眠") || contains_str(cmd_ptr, "whitenoise")) {
        res.category = INTENT_TYPE_FASTPATH;
        res.tool_name = "launch_system_app";
        res.tool_args_json = "{\"app_id\":\"whitenoise\"}";
        res.fast_reply = "【极速直达】已调起专注助眠白噪音！";
        return res;
    }

    /* 6. Composite Macro Workflow: Deep Work Workflow */
    if (contains_str(cmd_ptr, "开启专注流") || contains_str(cmd_ptr, "极客伴工")) {
        res.category = INTENT_TYPE_WORKFLOW;
        res.workflow_name = "deep_focus_flow";
        res.fast_reply = "【宏工作流】已激活极客伴工流：启动番茄钟 + 专注青光 + 白噪音环境！";
        return res;
    }

    /* 7. Default: Complex Reasoning / Natural Dialog -> LLM ReAct Loop */
    res.category = INTENT_TYPE_LLM_REASONING;
    return res;
}
