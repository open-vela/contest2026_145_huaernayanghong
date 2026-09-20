/**
 * @file cartridge_agent.c
 * @brief 灵眸具身智能体 (Phoenix Cyber-Eye Proactive Agent)
 * @author OpenVela Contest 2026 Team 145
 *
 * @details
 * 采用“单焦点极简流式交互”设计：
 * 1. 待命守护态：全屏居中展示灵眸眼球，自然呼吸/眨眼，无任何多余静态卡片堆叠；
 * 2. 交互锚点：直接点击/轻戳灵眸眼球即可唤醒语音聆听（超大盲操靶心）；
 * 3. 伴随神态：
 *    - 聆听时：金色光环聚焦，底部浮现极简拾音指示；
 *    - 思考时：紫色脉冲转动，底部显示思考呼吸词；
 *    - 播报时：微表情共鸣，弹出半透明答复字幕气泡，播报完毕 4 秒后自动收起淡出；
 * 4. 触控抚摸：敲击或摸眼球触发撒娇微表情，亲密度与赛博功德 +1。
 */

#include "cartridge_agent.h"
#if defined(__has_include) && __has_include(<lvgl/lvgl.h>)
#  include <lvgl/lvgl.h>
#elif defined(__has_include) && __has_include(<lvgl.h>)
#  include <lvgl.h>
#else
#  include <lvgl/lvgl.h>
#endif

#if defined(__has_include) && __has_include("ui/ui_font.h")
#  include "ui/ui_font.h"
#  include "ui/eye_anim.h"
#else
#  include "../ui/ui_font.h"
#  include "../ui/eye_anim.h"
#endif

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#  include "core/cartridge_mgr.h"
#  include "core/event_bus.h"
#  include "core/agent_core.h"
#  include "core/tool_registry.h"
#  include "core/store.h"
#  include "utils/log_utils.h"
#  include "utils/time_utils.h"
#  include "hal/hal_types.h"
#  include "harness/llm_provider.h"
#  include "hal/network_mgr.h"
#else
#  include "../core/cartridge.h"
#  include "../core/cartridge_mgr.h"
#  include "../core/event_bus.h"
#  include "../core/agent_core.h"
#  include "../core/tool_registry.h"
#  include "../core/store.h"
#  include "../utils/log_utils.h"
#  include "../utils/time_utils.h"
#  include "../hal/hal_types.h"
#  include "../harness/llm_provider.h"
#  include "../hal/network_mgr.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "CartridgeAgent"

typedef enum {
    VOICE_UI_IDLE = 0,
    VOICE_UI_LISTENING,
    VOICE_UI_THINKING,
    VOICE_UI_SPEAKING
} voice_ui_state_t;

typedef struct {
    lv_obj_t       *container;        /**< 专属舞台根容器 */
    lv_obj_t       *eye_box;          /**< 灵眸触控区域 (居中，超大触控靶心) */
    phoenix_eye_t  *eye;              /**< 核心具身动态灵眸 */

    /* 底部动态单焦点指示区 */
    lv_obj_t       *box_indicator;    /**< 底部单焦点容器 */
    lv_obj_t       *lbl_status;       /**< 动态状态/提示文字 */
    lv_obj_t       *card_bubble;      /**< 临时答复字幕气泡 */
    lv_obj_t       *lbl_bubble;       /**< 字幕内容 */

    lv_timer_t     *subtitle_timer;   /**< 字幕自动淡出定时器 */
    lv_timer_t     *emotion_timer;    /**< 情绪表情复位定时器 */

    voice_ui_state_t ui_state;
    uint32_t        prompt_idx;
    uint32_t        affinity;         /**< 情感亲密度 */
    uint32_t        total_merit;      /**< 累计赛博功德 */
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

static bool is_cloud_agent_active(void)
{
    phoenix_llm_backend_t *backend = phoenix_llm_provider_get_backend();
    if (backend && backend->name && strstr(backend->name, "Cloud")) {
        return (net_mgr_get_mode() == NET_MODE_STA_CONNECTED);
    }
    return false;
}

static void subtitle_auto_hide_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_ui.container) return;

    if (s_ui.card_bubble) {
        lv_obj_add_flag(s_ui.card_bubble, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.lbl_status) {
        lv_obj_clear_flag(s_ui.lbl_status, LV_OBJ_FLAG_HIDDEN);
    }
    s_ui.ui_state = VOICE_UI_IDLE;
    if (s_ui.eye) {
        phoenix_eye_set_emotion(s_ui.eye, PHOENIX_EYE_IDLE);
    }
    if (s_ui.subtitle_timer) {
        lv_timer_pause(s_ui.subtitle_timer);
    }
}

static void emotion_reset_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_ui.eye && s_ui.ui_state == VOICE_UI_IDLE) {
        phoenix_eye_set_emotion(s_ui.eye, PHOENIX_EYE_IDLE);
    }
    if (s_ui.emotion_timer) {
        lv_timer_pause(s_ui.emotion_timer);
    }
}

static void update_ui_state(voice_ui_state_t state)
{
    s_ui.ui_state = state;
    if (!s_ui.container) return;

    if (state == VOICE_UI_IDLE) {
        if (s_ui.eye) {
            phoenix_eye_set_emotion(s_ui.eye, PHOENIX_EYE_IDLE);
        }
        if (s_ui.card_bubble) {
            lv_obj_add_flag(s_ui.card_bubble, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_ui.lbl_status) {
            lv_obj_clear_flag(s_ui.lbl_status, LV_OBJ_FLAG_HIDDEN);
            bool cloud_on = is_cloud_agent_active();
            if (cloud_on) {
                lv_label_set_text(s_ui.lbl_status, "✨ 灵眸守护中 · 轻戳眼球对话");
                lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0x00E5FF), 0);
            } else {
                lv_label_set_text(s_ui.lbl_status, "✨ 灵眸就绪 · 轻戳眼球或唤醒");
                lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0x7E92AD), 0);
            }
        }
    } else if (state == VOICE_UI_LISTENING) {
        if (s_ui.eye) {
            phoenix_eye_set_emotion(s_ui.eye, PHOENIX_EYE_LISTENING);
        }
        if (s_ui.card_bubble) {
            lv_obj_add_flag(s_ui.card_bubble, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_ui.lbl_status) {
            lv_obj_clear_flag(s_ui.lbl_status, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_ui.lbl_status, "🎙️ 正在聆听... (再次轻戳执行)");
            lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0xFF9100), 0);
        }
    } else if (state == VOICE_UI_THINKING) {
        if (s_ui.eye) {
            phoenix_eye_set_emotion(s_ui.eye, PHOENIX_EYE_THINKING);
        }
        if (s_ui.card_bubble) {
            lv_obj_add_flag(s_ui.card_bubble, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_ui.lbl_status) {
            lv_obj_clear_flag(s_ui.lbl_status, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_ui.lbl_status, "💭 正在决策思考...");
            lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0xB388FF), 0);
        }
    } else if (state == VOICE_UI_SPEAKING) {
        if (s_ui.eye) {
            phoenix_eye_set_emotion(s_ui.eye, PHOENIX_EYE_HAPPY);
        }
        if (s_ui.lbl_status) {
            lv_obj_add_flag(s_ui.lbl_status, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_ui.card_bubble) {
            lv_obj_clear_flag(s_ui.card_bubble, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_ui.subtitle_timer) {
            lv_timer_reset(s_ui.subtitle_timer);
            lv_timer_resume(s_ui.subtitle_timer);
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

    /* 1. 更新为推理状态 */
    update_ui_state(VOICE_UI_THINKING);

    /* 2. 异步调用 Agent 核心进行 ReAct 推理 */
    phoenix_agent_chat_async(agent, prompt);
}

static void on_eye_clicked(lv_event_t *e)
{
    (void)e;
    if (s_ui.ui_state == VOICE_UI_IDLE) {
        /* 轻戳眼球：开始聆听语音 */
        update_ui_state(VOICE_UI_LISTENING);

        /* 触发唤醒提示音与飞字 */
        phoenix_event_data_t snd_evt;
        memset(&snd_evt, 0, sizeof(snd_evt));
        snd_evt.type = PHOENIX_EVT_PLAY_SOUND;
        snd_evt.data.sound.sound_id = HAL_SOUND_WAKEUP;
        phoenix_event_publish(&snd_evt);

        phoenix_event_data_t fly_evt;
        memset(&fly_evt, 0, sizeof(fly_evt));
        fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
        fly_evt.data.flying_text.text = "🎙️ 灵眸倾听中...";
        fly_evt.data.flying_text.color_rgb = 0x00E5FF;
        phoenix_event_publish(&fly_evt);

    } else if (s_ui.ui_state == VOICE_UI_LISTENING) {
        /* 再次轻戳眼球：完成输入并触发执行 */
        cartridge_agent_trigger_voice_chat(NULL);

        phoenix_event_data_t snd_evt;
        memset(&snd_evt, 0, sizeof(snd_evt));
        snd_evt.type = PHOENIX_EVT_PLAY_SOUND;
        snd_evt.data.sound.sound_id = HAL_SOUND_CLICK;
        phoenix_event_publish(&snd_evt);

    } else if (s_ui.ui_state == VOICE_UI_SPEAKING) {
        /* 播报时点击眼球：提前收起字幕 */
        if (s_ui.subtitle_timer) {
            subtitle_auto_hide_cb(s_ui.subtitle_timer);
        }
    }
}

/* 事件总线监听回调 */
static void on_agent_event(const phoenix_event_data_t *evt, void *user_data)
{
    (void)user_data;
    if (!evt) return;

    if (evt->type == PHOENIX_EVT_LLM_FINISHED && evt->data.llm_text.text) {
        if (s_ui.lbl_bubble) {
            lv_label_set_text(s_ui.lbl_bubble, evt->data.llm_text.text);
        }
        update_ui_state(VOICE_UI_SPEAKING);
    } else if (evt->type == PHOENIX_EVT_TOOL_TRIGGERED) {
        if (evt->data.tool.tool_name) {
            char tbuf[64];
            snprintf(tbuf, sizeof(tbuf), "✓ 触发: %s", evt->data.tool.tool_name);
            phoenix_event_data_t fly;
            memset(&fly, 0, sizeof(fly));
            fly.type = PHOENIX_EVT_FLYING_TEXT;
            fly.data.flying_text.text = tbuf;
            fly.data.flying_text.color_rgb = 0x00E676;
            phoenix_event_publish(&fly);
        }
    } else if (evt->type == PHOENIX_EVT_LLM_THINKING) {
        if (s_ui.ui_state == VOICE_UI_THINKING && s_ui.lbl_status && evt->data.thinking.reasoning_snippet[0]) {
            char tbuf[64];
            snprintf(tbuf, sizeof(tbuf), "💭 %.36s...", evt->data.thinking.reasoning_snippet);
            lv_label_set_text(s_ui.lbl_status, tbuf);
        }
    } else if (evt->type == PHOENIX_EVT_STATE_CHANGED) {
        int st = evt->data.state.new_state;
        if (st == AGENT_STATE_THINKING) {
            update_ui_state(VOICE_UI_THINKING);
        } else if (st == AGENT_STATE_IDLE && s_ui.ui_state != VOICE_UI_SPEAKING) {
            update_ui_state(VOICE_UI_IDLE);
        }
    }
}

static int agent_init(cartridge_t *self, void *user_data)
{
    (void)self;
    (void)user_data;
    s_ui.ui_state = VOICE_UI_IDLE;
    s_ui.prompt_idx = 0;

    phoenix_stats_t st;
    phoenix_store_get_stats(&st);
    s_ui.affinity = 100;
    s_ui.total_merit = st.total_merit;

    phoenix_event_subscribe(PHOENIX_EVT_LLM_FINISHED, on_agent_event, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_TOOL_TRIGGERED, on_agent_event, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_LLM_THINKING, on_agent_event, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_STATE_CHANGED, on_agent_event, NULL);

    s_initialized = true;
    LOG_I(TAG, "灵眸具身智能体初始化成功");
    return 0;
}

static void agent_enter(cartridge_t *self, void *stage_view)
{
    (void)self;
    lv_obj_t *stage = (lv_obj_t *)stage_view;
    if (!stage) return;

    const lv_font_t *font_zh = phoenix_ui_get_font();

    /* 主舞台容器 (284 × 216) */
    lv_obj_t *cont = lv_obj_create(stage);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x06080E), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    s_ui.container = cont;

    /* 1. 灵眸眼球触控靶心容器 (居中偏上，120×120，整球响应点击) */
    s_ui.eye_box = lv_obj_create(cont);
    lv_obj_set_size(s_ui.eye_box, 120, 120);
    lv_obj_align(s_ui.eye_box, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_opa(s_ui.eye_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.eye_box, 0, 0);
    lv_obj_set_style_pad_all(s_ui.eye_box, 0, 0);
    lv_obj_add_flag(s_ui.eye_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_ui.eye_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_ui.eye_box, on_eye_clicked, LV_EVENT_CLICKED, NULL);

    /* 创建 100px 核心灵眸并居中 */
    s_ui.eye = phoenix_eye_create(s_ui.eye_box, 100);
    if (s_ui.eye && s_ui.eye->container) {
        lv_obj_center(s_ui.eye->container);
        /* 点击眼球本体同样冒泡或直达 */
        lv_obj_add_flag(s_ui.eye->container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_ui.eye->container, on_eye_clicked, LV_EVENT_CLICKED, NULL);
    }

    /* 2. 底部单焦点动态指示区 (y=136, h=72, 宽 270) */
    s_ui.box_indicator = lv_obj_create(cont);
    lv_obj_set_size(s_ui.box_indicator, 270, 72);
    lv_obj_align(s_ui.box_indicator, LV_ALIGN_TOP_MID, 0, 136);
    lv_obj_set_style_bg_opa(s_ui.box_indicator, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.box_indicator, 0, 0);
    lv_obj_set_style_pad_all(s_ui.box_indicator, 0, 0);
    lv_obj_clear_flag(s_ui.box_indicator, LV_OBJ_FLAG_SCROLLABLE);

    /* 2.1 动态状态/心语提示 (待命 / 正在聆听 / 思考中) */
    s_ui.lbl_status = lv_label_create(s_ui.box_indicator);
    lv_obj_align(s_ui.lbl_status, LV_ALIGN_CENTER, 0, 0);
    if (font_zh) lv_obj_set_style_text_font(s_ui.lbl_status, font_zh, 0);
    lv_label_set_text(s_ui.lbl_status, "✨ 灵眸守护中 · 轻戳眼球对话");
    lv_obj_set_style_text_color(s_ui.lbl_status, lv_color_hex(0x00E5FF), 0);

    /* 2.2 浮动答复字幕气泡 (播报时展开，大字号呈现) */
    s_ui.card_bubble = lv_obj_create(s_ui.box_indicator);
    lv_obj_set_size(s_ui.card_bubble, 266, 68);
    lv_obj_align(s_ui.card_bubble, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_ui.card_bubble, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_bg_opa(s_ui.card_bubble, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_ui.card_bubble, lv_color_hex(0x1B2A42), 0);
    lv_obj_set_style_border_width(s_ui.card_bubble, 1, 0);
    lv_obj_set_style_radius(s_ui.card_bubble, 8, 0);
    lv_obj_set_style_pad_all(s_ui.card_bubble, 6, 0);
    lv_obj_clear_flag(s_ui.card_bubble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.card_bubble, LV_OBJ_FLAG_HIDDEN);

    s_ui.lbl_bubble = lv_label_create(s_ui.card_bubble);
    lv_obj_set_size(s_ui.lbl_bubble, 252, 54);
    lv_obj_align(s_ui.lbl_bubble, LV_ALIGN_TOP_LEFT, 0, 0);
    if (font_zh) lv_obj_set_style_text_font(s_ui.lbl_bubble, font_zh, 0);
    lv_label_set_long_mode(s_ui.lbl_bubble, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_ui.lbl_bubble, "");
    lv_obj_set_style_text_color(s_ui.lbl_bubble, lv_color_hex(0xE6EDF3), 0);

    /* 3. 定时器初始化 */
    s_ui.subtitle_timer = lv_timer_create(subtitle_auto_hide_cb, 4000, NULL);
    lv_timer_pause(s_ui.subtitle_timer);

    s_ui.emotion_timer = lv_timer_create(emotion_reset_cb, 1800, NULL);
    lv_timer_pause(s_ui.emotion_timer);

    update_ui_state(VOICE_UI_IDLE);
}

static void agent_exit(cartridge_t *self)
{
    (void)self;
    if (s_ui.subtitle_timer) {
        lv_timer_delete(s_ui.subtitle_timer);
        s_ui.subtitle_timer = NULL;
    }
    if (s_ui.emotion_timer) {
        lv_timer_delete(s_ui.emotion_timer);
        s_ui.emotion_timer = NULL;
    }
    if (s_ui.eye) {
        s_ui.eye->container = NULL; /* container 作为子对象交由 s_ui.container 统一递归释放 */
        phoenix_eye_destroy(s_ui.eye);
        s_ui.eye = NULL;
    }
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
    phoenix_event_unsubscribe(PHOENIX_EVT_LLM_THINKING, on_agent_event, NULL);
    phoenix_event_unsubscribe(PHOENIX_EVT_STATE_CHANGED, on_agent_event, NULL);
    agent_exit(self);
    s_initialized = false;
}

static void agent_on_knock(cartridge_t *self, int intensity, int count)
{
    (void)self;
    (void)intensity;
    (void)count;

    s_ui.affinity++;
    s_ui.total_merit = phoenix_store_add_merit(1);

    if (s_ui.eye) {
        phoenix_eye_set_emotion(s_ui.eye, PHOENIX_EYE_HAPPY);
        if (s_ui.emotion_timer) {
            lv_timer_reset(s_ui.emotion_timer);
            lv_timer_resume(s_ui.emotion_timer);
        }
    }

    /* 播放功德飞字与音效 */
    phoenix_event_data_t fly_evt;
    memset(&fly_evt, 0, sizeof(fly_evt));
    fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
    fly_evt.data.flying_text.text = "+1 功德 (亲密+1)";
    fly_evt.data.flying_text.color_rgb = 0xFFD54F;
    phoenix_event_publish(&fly_evt);

    phoenix_event_data_t snd_evt;
    memset(&snd_evt, 0, sizeof(snd_evt));
    snd_evt.type = PHOENIX_EVT_PLAY_SOUND;
    snd_evt.data.sound.sound_id = HAL_SOUND_CLICK;
    phoenix_event_publish(&snd_evt);

    phoenix_event_data_t merit_evt;
    memset(&merit_evt, 0, sizeof(merit_evt));
    merit_evt.type = PHOENIX_EVT_MERIT_UPDATED;
    merit_evt.data.stats.total_merit = s_ui.total_merit;
    phoenix_event_publish(&merit_evt);

    LOG_I(TAG, "轻敲灵眸: 亲密度=%u, 功德=%u", s_ui.affinity, s_ui.total_merit);
}

static void agent_tick_1s(cartridge_t *self)
{
    (void)self;
}

static int agent_get_web_status(cartridge_t *self, char *buf, size_t max_len)
{
    (void)self;
    if (!buf || max_len < 64) return -1;
    snprintf(buf, max_len,
             "{\"state\":\"%s\",\"mode\":\"voice_agent\",\"affinity\":%u,\"total_merit\":%u,\"mood\":\"%s\"}",
             (s_ui.ui_state == VOICE_UI_LISTENING) ? "listening" :
             (s_ui.ui_state == VOICE_UI_THINKING) ? "thinking" :
             (s_ui.ui_state == VOICE_UI_SPEAKING) ? "speaking" : "idle",
             s_ui.affinity, s_ui.total_merit,
             (s_ui.ui_state == VOICE_UI_SPEAKING) ? "happy" : "normal");
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
