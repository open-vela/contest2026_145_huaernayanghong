/**
 * @file cartridge_agent.c
 * @brief 主动式 Agent 语音交互卡带实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "cartridge_agent.h"
#include <lvgl/lvgl.h>
#include "ui/ui_font.h"

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#  include "core/cartridge_mgr.h"
#  include "core/event_bus.h"
#  include "core/agent_core.h"
#  include "core/tool_registry.h"
#  include "utils/log_utils.h"
#  include "hal/hal_types.h"
#else
#  include "../core/cartridge.h"
#  include "../core/cartridge_mgr.h"
#  include "../core/event_bus.h"
#  include "../core/agent_core.h"
#  include "../core/tool_registry.h"
#  include "../utils/log_utils.h"
#  include "../hal/hal_types.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "CartridgeAgent"

typedef enum {
    VOICE_UI_IDLE = 0,
    VOICE_UI_LISTENING,
    VOICE_UI_THINKING
} voice_ui_state_t;

typedef struct {
    lv_obj_t *container;

    /* 1. 顶部栏 */
    lv_obj_t *lbl_title;
    lv_obj_t *pill_status;
    lv_obj_t *lbl_status;

    /* 2. 对话与思维链展示卡片 */
    lv_obj_t *card_dialog;
    lv_obj_t *lbl_user_tag;
    lv_obj_t *lbl_user_msg;
    lv_obj_t *lbl_agent_tag;
    lv_obj_t *lbl_agent_msg;
    lv_obj_t *pill_tool;
    lv_obj_t *lbl_tool;

    /* 3. 主动语音交互按钮区 */
    lv_obj_t *btn_mic;
    lv_obj_t *lbl_mic;
    lv_obj_t *lbl_hint;

    voice_ui_state_t ui_state;
    uint32_t prompt_idx;
} agent_ui_t;

static cartridge_t s_cartridge_instance;
static agent_ui_t  s_ui;
static bool        s_initialized = false;

/* 轮换演示的典型语音指令集 */
static const char *s_voice_prompts[] = {
    "帮我开启25分钟专注流",
    "查询当前环境温湿度与电量",
    "写一首关于赛博极客的短诗",
    "执行系统体检与自愈巡检"
};
#define PROMPTS_COUNT 4

static void update_ui_state(voice_ui_state_t state)
{
    s_ui.ui_state = state;
    if (!s_ui.container) return;

    if (state == VOICE_UI_IDLE) {
        if (s_ui.lbl_status) {
            lv_label_set_text(s_ui.lbl_status, "● 待命中");
            lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0x00FF88), 0);
        }
        if (s_ui.btn_mic) {
            lv_obj_set_style_bg_color(s_ui.btn_mic, lv_color_hex(0x122438), 0);
            lv_obj_set_style_border_color(s_ui.btn_mic, lv_color_hex(0x00E5FF), 0);
        }
        if (s_ui.lbl_mic) {
            lv_label_set_text(s_ui.lbl_mic, "[ 点击发起语音交互 ]");
            lv_obj_set_style_text_color(s_ui.lbl_mic, lv_color_hex(0x00E5FF), 0);
        }
    } else if (state == VOICE_UI_LISTENING) {
        if (s_ui.lbl_status) {
            lv_label_set_text(s_ui.lbl_status, "● 聆听中");
            lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0xFF9100), 0);
        }
        if (s_ui.btn_mic) {
            lv_obj_set_style_bg_color(s_ui.btn_mic, lv_color_hex(0x331000), 0);
            lv_obj_set_style_border_color(s_ui.btn_mic, lv_color_hex(0xFF3D00), 0);
        }
        if (s_ui.lbl_mic) {
            lv_label_set_text(s_ui.lbl_mic, "● 正在聆听... 再次点击发送");
            lv_obj_set_style_text_color(s_ui.lbl_mic, lv_color_hex(0xFFFFFF), 0);
        }
    } else if (state == VOICE_UI_THINKING) {
        if (s_ui.lbl_status) {
            lv_label_set_text(s_ui.lbl_status, "● 推理中");
            lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0xB388FF), 0);
        }
        if (s_ui.btn_mic) {
            lv_obj_set_style_bg_color(s_ui.btn_mic, lv_color_hex(0x1D1438), 0);
            lv_obj_set_style_border_color(s_ui.btn_mic, lv_color_hex(0x7C4DFF), 0);
        }
        if (s_ui.lbl_mic) {
            lv_label_set_text(s_ui.lbl_mic, "● 灵眸正在推理执行...");
            lv_obj_set_style_text_color(s_ui.lbl_mic, lv_color_hex(0xD1C4E9), 0);
        }
    }
}

void cartridge_agent_trigger_voice_chat(const char *prompt_override)
{
    phoenix_agent_ctx_t *agent = phoenix_agent_get_instance();
    if (!agent) {
        LOG_W(TAG, "Agent core not initialized!");
        return;
    }

    const char *prompt = prompt_override;
    if (!prompt) {
        prompt = s_voice_prompts[s_ui.prompt_idx % PROMPTS_COUNT];
        s_ui.prompt_idx++;
    }

    /* 1. 更新为推理状态并更新界面用户提问 */
    update_ui_state(VOICE_UI_THINKING);

    if (s_ui.lbl_user_msg) {
        char ubuf[128];
        snprintf(ubuf, sizeof(ubuf), "\"%s\"", prompt);
        lv_label_set_text(s_ui.lbl_user_msg, ubuf);
    }
    if (s_ui.lbl_agent_msg) {
        lv_label_set_text(s_ui.lbl_agent_msg, "正在规划执行链并思考...");
    }

    /* 2. 异步调用 Agent 核心进行 ReAct 推理 (后台线程执行，主界面保持 60fps) */
    phoenix_agent_chat_async(agent, prompt);
}

static void on_mic_button_clicked(lv_event_t *e)
{
    (void)e;
    if (s_ui.ui_state == VOICE_UI_IDLE) {
        /* 点击开始聆听语音 */
        update_ui_state(VOICE_UI_LISTENING);

        /* 触发唤醒提示音与状态事件 */
        phoenix_event_data_t snd_evt;
        memset(&snd_evt, 0, sizeof(snd_evt));
        snd_evt.type = PHOENIX_EVT_PLAY_SOUND;
        snd_evt.data.sound.sound_id = HAL_SOUND_WAKEUP;
        phoenix_event_publish(&snd_evt);

        phoenix_event_data_t fly_evt;
        memset(&fly_evt, 0, sizeof(fly_evt));
        fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
        fly_evt.data.flying_text.text = "🎙️ 语音唤醒中";
        fly_evt.data.flying_text.color_rgb = 0x00E5FF;
        phoenix_event_publish(&fly_evt);

    } else if (s_ui.ui_state == VOICE_UI_LISTENING) {
        /* 再次点击完成语音输入并发送执行 */
        cartridge_agent_trigger_voice_chat(NULL);

        /* 触发成功按键音 */
        phoenix_event_data_t snd_evt;
        memset(&snd_evt, 0, sizeof(snd_evt));
        snd_evt.type = PHOENIX_EVT_PLAY_SOUND;
        snd_evt.data.sound.sound_id = HAL_SOUND_CLICK;
        phoenix_event_publish(&snd_evt);
    }
}

/* 事件总线监听回调 */
static void on_agent_event(const phoenix_event_data_t *evt, void *user_data)
{
    (void)user_data;
    if (!evt) return;

    if (evt->type == PHOENIX_EVT_LLM_FINISHED && evt->data.llm_text.text) {
        if (s_ui.lbl_agent_msg) {
            lv_label_set_text(s_ui.lbl_agent_msg, evt->data.llm_text.text);
        }
        update_ui_state(VOICE_UI_IDLE);
    } else if (evt->type == PHOENIX_EVT_TOOL_TRIGGERED) {
        if (s_ui.lbl_tool && evt->data.tool.tool_name) {
            char tbuf[64];
            snprintf(tbuf, sizeof(tbuf), "[✓ %s 已执行]", evt->data.tool.tool_name);
            lv_label_set_text(s_ui.lbl_tool, tbuf);
            if (s_ui.pill_tool) {
                lv_obj_clear_flag(s_ui.pill_tool, LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else if (evt->type == PHOENIX_EVT_STATE_CHANGED) {
        if (s_ui.lbl_status) {
            int st = evt->data.state.new_state;
            if (st == AGENT_STATE_THINKING) {
                lv_label_set_text(s_ui.lbl_status, "● 思考中");
                lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0xB388FF), 0);
            } else if (st == AGENT_STATE_EXECUTING) {
                lv_label_set_text(s_ui.lbl_status, "● 执行中");
                lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0x00E5FF), 0);
            } else if (st == AGENT_STATE_IDLE && s_ui.ui_state == VOICE_UI_IDLE) {
                lv_label_set_text(s_ui.lbl_status, "● 待命中");
                lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0x00FF88), 0);
            }
        }
    }
}

static int agent_init(cartridge_t *self, void *user_data)
{
    (void)self;
    (void)user_data;
    s_ui.ui_state = VOICE_UI_IDLE;
    s_ui.prompt_idx = 0;

    phoenix_event_subscribe(PHOENIX_EVT_LLM_FINISHED, on_agent_event, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_TOOL_TRIGGERED, on_agent_event, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_STATE_CHANGED, on_agent_event, NULL);

    s_initialized = true;
    LOG_I(TAG, "主动式 Agent 语音卡带初始化成功");
    return 0;
}

static void agent_enter(cartridge_t *self, void *stage_view)
{
    (void)self;
    lv_obj_t *stage = (lv_obj_t *)stage_view;
    if (!stage) return;

    const lv_font_t *font_zh = phoenix_ui_get_font();

    /* 主容器 (320 × 216) */
    lv_obj_t *cont = lv_obj_create(stage);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x070B14), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 6, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    s_ui.container = cont;

    /* 1. 顶部标题与状态栏 (y=2, h=22, 宽 272) */
    lv_obj_t *header = lv_obj_create(cont);
    lv_obj_set_size(header, 272, 22);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_title = lv_label_create(header);
    lv_obj_align(s_ui.lbl_title, LV_ALIGN_LEFT_MID, 2, 0);
    if (font_zh) lv_obj_set_style_text_font(s_ui.lbl_title, font_zh, 0);
    lv_label_set_text(s_ui.lbl_title, "灵眸具身智能体");
    lv_obj_set_style_text_color(s_ui.lbl_title, lv_color_hex(0x00E5FF), 0);

    s_ui.pill_status = lv_obj_create(header);
    lv_obj_set_size(s_ui.pill_status, 64, 20);
    lv_obj_align(s_ui.pill_status, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(s_ui.pill_status, lv_color_hex(0x101C2E), 0);
    lv_obj_set_style_border_color(s_ui.pill_status, lv_color_hex(0x1E3352), 0);
    lv_obj_set_style_border_width(s_ui.pill_status, 1, 0);
    lv_obj_set_style_radius(s_ui.pill_status, 10, 0);
    lv_obj_set_style_pad_all(s_ui.pill_status, 0, 0);
    lv_obj_clear_flag(s_ui.pill_status, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_status = lv_label_create(s_ui.pill_status);
    lv_obj_center(s_ui.lbl_status);
    if (font_zh) lv_obj_set_style_text_font(s_ui.lbl_status, font_zh, 0);
    lv_label_set_text(s_ui.lbl_status, "● 待命");
    lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0x00FF88), 0);

    /* 2. 对话流与思维执行展示卡片 (y=26, h=106, w=272, 居中) */
    s_ui.card_dialog = lv_obj_create(cont);
    lv_obj_set_size(s_ui.card_dialog, 272, 106);
    lv_obj_align(s_ui.card_dialog, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_color(s_ui.card_dialog, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_border_color(s_ui.card_dialog, lv_color_hex(0x1B2A42), 0);
    lv_obj_set_style_border_width(s_ui.card_dialog, 1, 0);
    lv_obj_set_style_radius(s_ui.card_dialog, 10, 0);
    lv_obj_set_style_pad_all(s_ui.card_dialog, 5, 0);
    lv_obj_clear_flag(s_ui.card_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 用户行 */
    s_ui.lbl_user_tag = lv_label_create(s_ui.card_dialog);
    lv_obj_align(s_ui.lbl_user_tag, LV_ALIGN_TOP_LEFT, 4, 2);
    if (font_zh) lv_obj_set_style_text_font(s_ui.lbl_user_tag, font_zh, 0);
    lv_label_set_text(s_ui.lbl_user_tag, "用户:");
    lv_obj_set_style_text_color(s_ui.lbl_user_tag, lv_color_hex(0x8B9EB5), 0);

    s_ui.lbl_user_msg = lv_label_create(s_ui.card_dialog);
    lv_obj_set_width(s_ui.lbl_user_msg, 206);
    lv_obj_align(s_ui.lbl_user_msg, LV_ALIGN_TOP_LEFT, 48, 2);
    if (font_zh) lv_obj_set_style_text_font(s_ui.lbl_user_msg, font_zh, 0);
    lv_label_set_long_mode(s_ui.lbl_user_msg, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_ui.lbl_user_msg, "\"帮我开启25分钟专注流\"");
    lv_obj_set_style_text_color(s_ui.lbl_user_msg, lv_color_hex(0x00E5FF), 0);

    /* 灵眸行 */
    s_ui.lbl_agent_tag = lv_label_create(s_ui.card_dialog);
    lv_obj_align(s_ui.lbl_agent_tag, LV_ALIGN_TOP_LEFT, 4, 28);
    if (font_zh) lv_obj_set_style_text_font(s_ui.lbl_agent_tag, font_zh, 0);
    lv_label_set_text(s_ui.lbl_agent_tag, "灵眸:");
    lv_obj_set_style_text_color(s_ui.lbl_agent_tag, lv_color_hex(0xFFD54F), 0);

    s_ui.lbl_agent_msg = lv_label_create(s_ui.card_dialog);
    lv_obj_set_width(s_ui.lbl_agent_msg, 206);
    lv_obj_align(s_ui.lbl_agent_msg, LV_ALIGN_TOP_LEFT, 48, 28);
    if (font_zh) lv_obj_set_style_text_font(s_ui.lbl_agent_msg, font_zh, 0);
    lv_label_set_long_mode(s_ui.lbl_agent_msg, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_ui.lbl_agent_msg, "收到！已为您设定 25 分钟极客专注流，现在开始倒计时！");
    lv_obj_set_style_text_color(s_ui.lbl_agent_msg, lv_color_hex(0xE6EDF3), 0);

    /* 工具调用徽章 */
    s_ui.pill_tool = lv_obj_create(s_ui.card_dialog);
    lv_obj_set_size(s_ui.pill_tool, 172, 20);
    lv_obj_align(s_ui.pill_tool, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    lv_obj_set_style_bg_color(s_ui.pill_tool, lv_color_hex(0x0E261A), 0);
    lv_obj_set_style_border_color(s_ui.pill_tool, lv_color_hex(0x1B4D34), 0);
    lv_obj_set_style_border_width(s_ui.pill_tool, 1, 0);
    lv_obj_set_style_radius(s_ui.pill_tool, 4, 0);
    lv_obj_set_style_pad_all(s_ui.pill_tool, 0, 0);
    lv_obj_clear_flag(s_ui.pill_tool, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_tool = lv_label_create(s_ui.pill_tool);
    lv_obj_center(s_ui.lbl_tool);
    if (font_zh) lv_obj_set_style_text_font(s_ui.lbl_tool, font_zh, 0);
    lv_label_set_text(s_ui.lbl_tool, "[✓ manage_pomodoro 已执行]");
    lv_obj_set_style_text_color(s_ui.lbl_tool, lv_color_hex(0x00E676), 0);

    /* 3. 主动语音交互核心按钮 (y=138, 宽 220px, 高 38px) */
    s_ui.btn_mic = lv_btn_create(cont);
    lv_obj_set_size(s_ui.btn_mic, 220, 38);
    lv_obj_align(s_ui.btn_mic, LV_ALIGN_TOP_MID, 0, 138);
    lv_obj_set_style_bg_color(s_ui.btn_mic, lv_color_hex(0x122438), 0);
    lv_obj_set_style_border_color(s_ui.btn_mic, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s_ui.btn_mic, 1, 0);
    lv_obj_set_style_radius(s_ui.btn_mic, 19, 0);
    lv_obj_add_event_cb(s_ui.btn_mic, on_mic_button_clicked, LV_EVENT_CLICKED, NULL);

    s_ui.lbl_mic = lv_label_create(s_ui.btn_mic);
    lv_obj_center(s_ui.lbl_mic);
    if (font_zh) lv_obj_set_style_text_font(s_ui.lbl_mic, font_zh, 0);
    lv_label_set_text(s_ui.lbl_mic, "[ 点击发起语音交互 ]");
    lv_obj_set_style_text_color(s_ui.lbl_mic, lv_color_hex(0x00E5FF), 0);

    /* 底部提示语 */
    s_ui.lbl_hint = lv_label_create(cont);
    lv_obj_align(s_ui.lbl_hint, LV_ALIGN_BOTTOM_MID, 0, -2);
    if (font_zh) lv_obj_set_style_text_font(s_ui.lbl_hint, font_zh, 0);
    lv_label_set_text(s_ui.lbl_hint, "支持唤醒词、任务调度、番茄钟与系统设置");
    lv_obj_set_style_text_color(s_ui.lbl_hint, lv_color_hex(0x5A6E85), 0);

    update_ui_state(VOICE_UI_IDLE);
}

static void agent_exit(cartridge_t *self)
{
    (void)self;
    if (s_ui.container) {
        lv_obj_del(s_ui.container);
        s_ui.container = NULL;
    }
}

static void agent_destroy(cartridge_t *self)
{
    (void)self;
    phoenix_event_unsubscribe(PHOENIX_EVT_LLM_FINISHED, on_agent_event, NULL);
    phoenix_event_unsubscribe(PHOENIX_EVT_TOOL_TRIGGERED, on_agent_event, NULL);
    phoenix_event_unsubscribe(PHOENIX_EVT_STATE_CHANGED, on_agent_event, NULL);
    agent_exit(self);
    s_initialized = false;
}

static void agent_on_knock(cartridge_t *self, int intensity, int count)
{
    (void)self;
    (void)intensity;
    (void)count;
    LOG_I(TAG, "敲击触发语音交互");
    cartridge_agent_trigger_voice_chat(NULL);
}

static void agent_tick_1s(cartridge_t *self)
{
    (void)self;
}

static int agent_get_web_status(cartridge_t *self, char *buf, size_t max_len)
{
    (void)self;
    if (!buf || max_len < 32) return -1;
    snprintf(buf, max_len,
             "{\"state\":\"%s\",\"mode\":\"voice_agent\"}",
             (s_ui.ui_state == VOICE_UI_LISTENING) ? "listening" :
             (s_ui.ui_state == VOICE_UI_THINKING) ? "thinking" : "idle");
    return 0;
}

cartridge_t *cartridge_agent_create(void)
{
    if (s_initialized) return &s_cartridge_instance;

    memset(&s_cartridge_instance, 0, sizeof(s_cartridge_instance));
    snprintf(s_cartridge_instance.ops.id, sizeof(s_cartridge_instance.ops.id), "agent");
    snprintf(s_cartridge_instance.ops.name, sizeof(s_cartridge_instance.ops.name), "灵眸AI");
    snprintf(s_cartridge_instance.ops.icon, sizeof(s_cartridge_instance.ops.icon), "[AI]");
    s_cartridge_instance.ops.init = agent_init;
    s_cartridge_instance.ops.enter = agent_enter;
    s_cartridge_instance.ops.exit = agent_exit;
    s_cartridge_instance.ops.destroy = agent_destroy;
    s_cartridge_instance.ops.tick_1s = agent_tick_1s;
    s_cartridge_instance.ops.on_knock = agent_on_knock;
    s_cartridge_instance.ops.get_web_status = agent_get_web_status;

    s_initialized = true;
    return &s_cartridge_instance;
}

int cartridge_agent_register(void)
{
    cartridge_t *c = cartridge_agent_create();
    return cartridge_mgr_register(&c->ops, NULL, NULL);
}
