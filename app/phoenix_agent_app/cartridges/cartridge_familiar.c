/**
 * @file cartridge_familiar.c
 * @brief 桌面萌宠使魔卡带实现 (Desktop Familiar Cyber-Pet Cartridge)
 * @author OpenVela Contest 2026 Team 145
 */

#include "cartridge_familiar.h"
#include <lvgl/lvgl.h>
#include "ui/ui_font.h"
#include "ui/eye_anim.h"

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#  include "core/cartridge_mgr.h"
#  include "core/event_bus.h"
#  include "core/store.h"
#  include "core/config.h"
#  include "core/agent_core.h"
#  include "utils/time_utils.h"
#  include "utils/log_utils.h"
#else
#  include "../core/cartridge.h"
#  include "../core/cartridge_mgr.h"
#  include "../core/event_bus.h"
#  include "../core/store.h"
#  include "../core/config.h"
#  include "../core/agent_core.h"
#  include "../utils/time_utils.h"
#  include "../utils/log_utils.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "CartridgeFamiliar"

typedef enum {
    PET_MOOD_NORMAL = 0,    /**< 悠闲日常 */
    PET_MOOD_HAPPY,         /**< 被抚摸/敲击撒娇 */
    PET_MOOD_SURPRISED,     /**< 重敲被惊吓 */
    PET_MOOD_SLEEPY         /**< 久未交互打瞌睡 */
} pet_mood_t;

typedef struct {
    lv_obj_t       *container;       /**< 专属舞台根节点 */
    phoenix_eye_t  *eye;             /**< 核心具身动态灵眸 */
    lv_obj_t       *lbl_time;        /**< 顶部极简数字时钟挂件 */
    lv_obj_t       *lbl_whisper;     /**< 使魔心语/气泡 */
    lv_obj_t       *lbl_affinity;    /**< 亲密度徽章 */
    lv_obj_t       *lbl_merit;       /**< 赛博功德徽章 */
    lv_obj_t       *btn_voice;       /**< 灵眸具身对话胶囊 */
    lv_obj_t       *lbl_voice;       /**< 对话胶囊文字 */
    
    pet_mood_t      mood;
    uint32_t        affinity;        /**< 亲密度 (0 ~ 9999) */
    uint32_t        total_merit;     /**< 累计赛博功德 */
    uint32_t        last_interact_ms;/**< 上次互动时刻 */
    uint32_t        next_blink_ms;   /**< 下次眨眼时刻 */
    bool            colon_blink;
} familiar_ui_t;

static cartridge_t   s_cartridge_instance;
static familiar_ui_t s_ui;
static bool          s_initialized = false;

static const char *const s_whispers_normal[] = {
    "静静陪伴着你",
    "今天也要元气满满哦",
    "桌面暖暖的，守护你",
    "灵眸伴侣 · 待命中"
};

static const char *const s_whispers_happy[] = {
    "蹭蹭你的手指~ (亲密+1 功德+1)",
    "呼噜呼噜...最喜欢你了",
    "心情大好，积攒赛博功德~",
    "摸摸头，功德圆满！"
};

static const char *const s_whispers_surprised[] = {
    "哇呀！轻一点嘛",
    "吓得瞳孔放大了！",
    "好大力气，轻抚我嘛"
};

static const char *const s_whispers_sleepy[] = {
    "呼... 眯一会儿",
    "困困... 打个哈欠",
    "夜深了，早点休息"
};

static void update_time_display(familiar_ui_t *u)
{
    if (!u || !u->container || !u->lbl_time) return;
    uint64_t now_ms = time_utils_get_ms();
    int64_t total_sec = (int64_t)(now_ms / 1000) + 8 * 3600; /* UTC+8 */
    int cur_hour = (int)((total_sec / 3600) % 24);
    int cur_min = (int)((total_sec / 60) % 60);

    char tbuf[16];
    u->colon_blink = !u->colon_blink;
    snprintf(tbuf, sizeof(tbuf), "%02d%c%02d", cur_hour, u->colon_blink ? ':' : ' ', cur_min);
    lv_label_set_text(u->lbl_time, tbuf);
}

static const char *const s_agent_prompts[] = {
    "告诉我一则充满禅意的极简人生建议",
    "今天我该如何保持专注高效？",
    "给我讲一句幽默的赛博工程师短句",
    "使魔，汇报一下当前桌面能量"
};
static size_t s_prompt_index = 0;

static void familiar_trigger_voice_chat(void)
{
    phoenix_agent_ctx_t *agent = phoenix_agent_get_instance();
    if (!agent) {
        LOG_W(TAG, "Agent 未初始化");
        return;
    }

    const char *prompt = s_agent_prompts[s_prompt_index % 4];
    s_prompt_index++;

    if (s_ui.eye) {
        phoenix_eye_set_emotion(s_ui.eye, PHOENIX_EYE_LISTENING);
    }
    if (s_ui.lbl_whisper) {
        lv_label_set_text(s_ui.lbl_whisper, "灵眸正在倾听思考...");
    }
    if (s_ui.lbl_voice) {
        lv_label_set_text(s_ui.lbl_voice, "● 灵眸思考中...");
    }

    phoenix_event_data_t fly_evt;
    memset(&fly_evt, 0, sizeof(fly_evt));
    fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
    fly_evt.data.flying_text.text = "灵眸唤醒";
    fly_evt.data.flying_text.color_rgb = 0x00E5FF;
    phoenix_event_publish(&fly_evt);

    phoenix_agent_chat_async(agent, prompt);
    LOG_I(TAG, "已触发使魔具身对话: %s", prompt);
}

static void on_voice_btn_clicked(lv_event_t *e)
{
    (void)e;
    familiar_trigger_voice_chat();
}

static void on_familiar_agent_event(const phoenix_event_data_t *evt, void *user_data)
{
    (void)user_data;
    if (!evt) return;

    if (evt->type == PHOENIX_EVT_LLM_FINISHED && evt->data.llm_text.text) {
        if (s_ui.eye) {
            phoenix_eye_set_emotion(s_ui.eye, PHOENIX_EYE_HAPPY);
        }
        if (s_ui.lbl_whisper) {
            char snippet[64];
            snprintf(snippet, sizeof(snippet), "%.48s", evt->data.llm_text.text);
            lv_label_set_text(s_ui.lbl_whisper, snippet);
        }
        if (s_ui.lbl_voice) {
            lv_label_set_text(s_ui.lbl_voice, "● 双敲或轻点唤醒灵眸");
        }
    } else if (evt->type == PHOENIX_EVT_TOOL_TRIGGERED && evt->data.tool.tool_name) {
        if (s_ui.eye) {
            phoenix_eye_set_emotion(s_ui.eye, PHOENIX_EYE_ALERT);
        }
        if (s_ui.lbl_whisper) {
            char tbuf[48];
            snprintf(tbuf, sizeof(tbuf), "调用工具: %s", evt->data.tool.tool_name);
            lv_label_set_text(s_ui.lbl_whisper, tbuf);
        }
    }
}

static void update_mood_display(familiar_ui_t *u)
{
    if (!u || !u->container) return;

    const char *whisper = s_whispers_normal[rand() % 4];

    switch (u->mood) {
        case PET_MOOD_HAPPY:
            whisper = s_whispers_happy[rand() % 4];
            if (u->eye) phoenix_eye_set_emotion(u->eye, PHOENIX_EYE_HAPPY);
            break;
        case PET_MOOD_SURPRISED:
            whisper = s_whispers_surprised[rand() % 3];
            if (u->eye) phoenix_eye_set_emotion(u->eye, PHOENIX_EYE_ALERT);
            break;
        case PET_MOOD_SLEEPY:
            whisper = s_whispers_sleepy[rand() % 3];
            if (u->eye) phoenix_eye_set_emotion(u->eye, PHOENIX_EYE_SLEEPY);
            break;
        case PET_MOOD_NORMAL:
        default:
            whisper = s_whispers_normal[rand() % 4];
            if (u->eye) phoenix_eye_set_emotion(u->eye, PHOENIX_EYE_IDLE);
            break;
    }

    if (u->lbl_whisper) {
        lv_label_set_text(u->lbl_whisper, whisper);
    }
    if (u->lbl_affinity) {
        char abuf[32];
        snprintf(abuf, sizeof(abuf), "● 亲密 %u", (unsigned int)u->affinity);
        lv_label_set_text(u->lbl_affinity, abuf);
    }
    if (u->lbl_merit) {
        char mbuf[32];
        snprintf(mbuf, sizeof(mbuf), "★ 功德 %u", (unsigned int)u->total_merit);
        lv_label_set_text(u->lbl_merit, mbuf);
    }
}

static int familiar_on_load(lv_obj_t *stage_parent)
{
    if (!stage_parent) return -1;

    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.affinity = (uint32_t)phoenix_config_get_int("familiar_affinity", 88);
    phoenix_stats_t stats;
    phoenix_store_get_stats(&stats);
    s_ui.total_merit = stats.total_merit;
    s_ui.mood = PET_MOOD_NORMAL;
    s_ui.last_interact_ms = 0;
    s_ui.next_blink_ms = 3000;
    s_ui.colon_blink = true;

    /* 1. 舞台卡带容器 (居中填满舞台视窗) */
    s_ui.container = lv_obj_create(stage_parent);
    lv_obj_set_size(s_ui.container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_ui.container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.container, 0, 0);
    lv_obj_set_style_pad_all(s_ui.container, 0, 0);
    lv_obj_clear_flag(s_ui.container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.container, LV_OBJ_FLAG_EVENT_BUBBLE);

    const lv_font_t *font = phoenix_ui_get_font();

    /* 2. 顶部极简时间挂件 (半透明青白，低调科技感) */
    s_ui.lbl_time = lv_label_create(s_ui.container);
    lv_obj_align(s_ui.lbl_time, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_text_color(s_ui.lbl_time, lv_color_hex(0x6E87A8), 0);
    if (font) lv_obj_set_style_text_font(s_ui.lbl_time, font, 0);
    update_time_display(&s_ui);

    /* 3. 核心具身动态灵眸 (尺寸 88px，位于舞台正中偏上) */
    s_ui.eye = phoenix_eye_create(s_ui.container, 88);
    if (s_ui.eye && s_ui.eye->container) {
        lv_obj_align(s_ui.eye->container, LV_ALIGN_CENTER, 0, -22);
    }

    /* 4. 亲密度与功德双徽章 (并排展示) */
    s_ui.lbl_affinity = lv_label_create(s_ui.container);
    lv_obj_align(s_ui.lbl_affinity, LV_ALIGN_CENTER, -44, 36);
    lv_obj_set_style_text_color(s_ui.lbl_affinity, lv_color_hex(0xFF6EA7), 0);
    if (font) lv_obj_set_style_text_font(s_ui.lbl_affinity, font, 0);

    s_ui.lbl_merit = lv_label_create(s_ui.container);
    lv_obj_align(s_ui.lbl_merit, LV_ALIGN_CENTER, 44, 36);
    lv_obj_set_style_text_color(s_ui.lbl_merit, lv_color_hex(0xFFD700), 0);
    if (font) lv_obj_set_style_text_font(s_ui.lbl_merit, font, 0);

    /* 5. 极简克制的心语 (冷灰微文字) */
    s_ui.lbl_whisper = lv_label_create(s_ui.container);
    lv_obj_align(s_ui.lbl_whisper, LV_ALIGN_CENTER, 0, 58);
    lv_obj_set_style_text_color(s_ui.lbl_whisper, lv_color_hex(0x7E92AD), 0);
    if (font) lv_obj_set_style_text_font(s_ui.lbl_whisper, font, 0);

    /* 6. 灵眸具身对话胶囊 (轻点或双敲桌面唤醒) */
    s_ui.btn_voice = lv_btn_create(s_ui.container);
    lv_obj_set_size(s_ui.btn_voice, 150, 24);
    lv_obj_align(s_ui.btn_voice, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_radius(s_ui.btn_voice, 12, 0);
    lv_obj_set_style_bg_color(s_ui.btn_voice, lv_color_hex(0x101C2E), 0);
    lv_obj_set_style_bg_opa(s_ui.btn_voice, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_ui.btn_voice, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s_ui.btn_voice, 1, 0);
    lv_obj_set_style_pad_all(s_ui.btn_voice, 0, 0);
    lv_obj_add_event_cb(s_ui.btn_voice, on_voice_btn_clicked, LV_EVENT_CLICKED, NULL);

    s_ui.lbl_voice = lv_label_create(s_ui.btn_voice);
    lv_obj_center(s_ui.lbl_voice);
    lv_obj_set_style_text_color(s_ui.lbl_voice, lv_color_hex(0x80D8FF), 0);
    if (font) lv_obj_set_style_text_font(s_ui.lbl_voice, font, 0);
    lv_label_set_text(s_ui.lbl_voice, "● 双敲或轻点唤醒灵眸");

    update_mood_display(&s_ui);
    LOG_I(TAG, "灵眸主屏已载入舞台 (亲密度: %u, 功德: %u)", s_ui.affinity, s_ui.total_merit);
    return 0;
}

static int familiar_on_unload(void)
{
    if (s_ui.eye) {
        s_ui.eye->container = NULL; /* container 作为子对象交由 s_ui.container 统一递归释放 */
        phoenix_eye_destroy(s_ui.eye);
        s_ui.eye = NULL;
    }
    if (s_ui.container) {
        lv_obj_del(s_ui.container);
        s_ui.container = NULL;
    }
    LOG_I(TAG, "使魔卡带已卸载");
    return 0;
}

static uint32_t s_last_saved_affinity = 0;

static int familiar_on_tap(uint8_t intensity)
{
    s_ui.last_interact_ms = 0; /* 重置互动防瞌睡 */

    /* 1. 亲密度与使魔情绪 */
    if (intensity >= 3) {
        s_ui.mood = PET_MOOD_SURPRISED;
        LOG_I(TAG, "使魔受惊吓 (敲击强度: %u)", intensity);
    } else {
        s_ui.mood = PET_MOOD_HAPPY;
        s_ui.affinity++;
        if (s_ui.affinity >= s_last_saved_affinity + 5) {
            phoenix_config_set_int("familiar_affinity", (int)s_ui.affinity);
            s_last_saved_affinity = s_ui.affinity;
        }
        LOG_I(TAG, "使魔被轻抚撒娇 (亲密度 -> %u)", s_ui.affinity);
    }

    /* 2. 深度融合赛博木鱼功德：功德 +1，触发清脆音效与金色飞字 */
    s_ui.total_merit = phoenix_store_add_merit(1);

    /* 触发清脆木鱼音效 (Sound ID 1 = Wooden Fish) */
    phoenix_event_data_t snd_evt;
    memset(&snd_evt, 0, sizeof(snd_evt));
    snd_evt.type = PHOENIX_EVT_PLAY_SOUND;
    snd_evt.data.sound.sound_id = 1;
    phoenix_event_publish(&snd_evt);

    /* 触发金色飞字动画 */
    phoenix_event_data_t fly_evt;
    memset(&fly_evt, 0, sizeof(fly_evt));
    fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
    fly_evt.data.flying_text.text = (s_ui.affinity % 3 == 0) ? "亲密度 +1" : "功德 +1";
    fly_evt.data.flying_text.color_rgb = 0xFFD700;
    phoenix_event_publish(&fly_evt);

    /* 广播功德更新事件至总线 (供顶部状态胶囊同步) */
    phoenix_event_data_t merit_evt;
    memset(&merit_evt, 0, sizeof(merit_evt));
    merit_evt.type = PHOENIX_EVT_MERIT_UPDATED;
    merit_evt.data.stats.total_merit = s_ui.total_merit;
    phoenix_event_publish(&merit_evt);

    update_mood_display(&s_ui);
    return 0;
}

static void familiar_on_periodic_tick(uint32_t now_ms)
{
    (void)now_ms;
    /* 简单计时状态机：若处于撒娇或惊吓，3秒后恢复普通日常态 */
    if (s_ui.mood == PET_MOOD_HAPPY || s_ui.mood == PET_MOOD_SURPRISED) {
        s_ui.mood = PET_MOOD_NORMAL;
        update_mood_display(&s_ui);
    }
}

static int familiar_init(cartridge_t *self, void *user_data)
{
    (void)self;
    (void)user_data;
    s_ui.affinity = (uint32_t)phoenix_config_get_int("familiar_affinity", 88);
    s_last_saved_affinity = s_ui.affinity;
    s_ui.mood = PET_MOOD_NORMAL;

    /* 订阅大模型推理与工具触发事件，实现灵眸与心语自动响应 */
    phoenix_event_subscribe(PHOENIX_EVT_LLM_FINISHED, on_familiar_agent_event, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_TOOL_TRIGGERED, on_familiar_agent_event, NULL);
    return 0;
}

static void familiar_enter(cartridge_t *self, void *stage_view)
{
    (void)self;
    familiar_on_load((lv_obj_t *)stage_view);
}

static void familiar_exit(cartridge_t *self)
{
    (void)self;
    if (s_ui.affinity != s_last_saved_affinity) {
        phoenix_config_set_int("familiar_affinity", (int)s_ui.affinity);
        s_last_saved_affinity = s_ui.affinity;
    }
    familiar_on_unload();
}

static void familiar_destroy(cartridge_t *self)
{
    (void)self;
    if (s_ui.affinity != s_last_saved_affinity) {
        phoenix_config_set_int("familiar_affinity", (int)s_ui.affinity);
        s_last_saved_affinity = s_ui.affinity;
    }
    phoenix_event_unsubscribe(PHOENIX_EVT_LLM_FINISHED, on_familiar_agent_event, NULL);
    phoenix_event_unsubscribe(PHOENIX_EVT_TOOL_TRIGGERED, on_familiar_agent_event, NULL);
    familiar_on_unload();
}

static void familiar_tick_1s(cartridge_t *self)
{
    (void)self;
    update_time_display(&s_ui);
    familiar_on_periodic_tick(1000);
}

static void familiar_on_knock(cartridge_t *self, int intensity, int count)
{
    (void)self;
    if (count == 2) {
        familiar_trigger_voice_chat();
    } else {
        familiar_on_tap((uint8_t)intensity);
    }
}

static int familiar_get_web_status(cartridge_t *self, char *buf, size_t max_len)
{
    (void)self;
    if (!buf || max_len < 64) return -1;
    snprintf(buf, max_len, "{\"affinity\":%u,\"mood\":%d}", s_ui.affinity, (int)s_ui.mood);
    return 0;
}

cartridge_t *cartridge_familiar_create(void)
{
    if (s_initialized) return &s_cartridge_instance;

    memset(&s_cartridge_instance, 0, sizeof(s_cartridge_instance));
    snprintf(s_cartridge_instance.ops.id, sizeof(s_cartridge_instance.ops.id), "familiar");
    snprintf(s_cartridge_instance.ops.name, sizeof(s_cartridge_instance.ops.name), "使魔萌宠");
    snprintf(s_cartridge_instance.ops.icon, sizeof(s_cartridge_instance.ops.icon), "[PET]");
    s_cartridge_instance.ops.init = familiar_init;
    s_cartridge_instance.ops.enter = familiar_enter;
    s_cartridge_instance.ops.exit = familiar_exit;
    s_cartridge_instance.ops.destroy = familiar_destroy;
    s_cartridge_instance.ops.tick_1s = familiar_tick_1s;
    s_cartridge_instance.ops.on_knock = familiar_on_knock;
    s_cartridge_instance.ops.get_web_status = familiar_get_web_status;

    s_initialized = true;
    return &s_cartridge_instance;
}

int cartridge_familiar_register(void)
{
    cartridge_t *c = cartridge_familiar_create();
    return cartridge_mgr_register(&c->ops, NULL, NULL);
}
