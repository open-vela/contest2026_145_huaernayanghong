/**
 * @file cartridge_familiar.c
 * @brief 桌面萌宠使魔卡带实现 (Desktop Familiar Cyber-Pet Cartridge)
 * @author OpenVela Contest 2026 Team 145
 */

#include "cartridge_familiar.h"
#include <lvgl/lvgl.h>
#include "ui/ui_font.h"
#include "ui/eye_anim.h"
#include "core/cartridge_mgr.h"
#include "core/event_bus.h"
#include "core/config.h"
#include "utils/log_utils.h"
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
    lv_obj_t       *lbl_whisper;     /**< 使魔心语/气泡 */
    lv_obj_t       *lbl_affinity;    /**< 亲密度心形徽章 */
    
    pet_mood_t      mood;
    uint32_t        affinity;        /**< 亲密度 (0 ~ 9999) */
    uint32_t        last_interact_ms;/**< 上次互动时刻 */
    uint32_t        next_blink_ms;   /**< 下次眨眼时刻 */
} familiar_ui_t;

static cartridge_t   s_cartridge_instance;
static familiar_ui_t s_ui;
static bool          s_initialized = false;

static const char *const s_whispers_normal[] = {
    "静静陪伴着你",
    "今天也要加油哦",
    "桌面暖暖的",
    "守护在你的身边"
};

static const char *const s_whispers_happy[] = {
    "蹭蹭你的手指~ (亲密度+1)",
    "呼噜呼噜...最喜欢你了",
    "心情大好，好舒服~",
    "摸摸头，元气满满！"
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
        snprintf(abuf, sizeof(abuf), "● 亲密度 %u", (unsigned int)u->affinity);
        lv_label_set_text(u->lbl_affinity, abuf);
    }
}

static int familiar_on_load(lv_obj_t *stage_parent)
{
    if (!stage_parent) return -1;

    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.affinity = (uint32_t)phoenix_config_get_int("familiar_affinity", 88);
    s_ui.mood = PET_MOOD_NORMAL;
    s_ui.last_interact_ms = 0;
    s_ui.next_blink_ms = 3000;

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

    /* 2. 核心具身动态灵眸 (尺寸 92px，位于舞台偏上) */
    s_ui.eye = phoenix_eye_create(s_ui.container, 92);
    if (s_ui.eye && s_ui.eye->container) {
        lv_obj_align(s_ui.eye->container, LV_ALIGN_CENTER, 0, -24);
    }

    /* 3. 亲密度徽章 (微型半透明胶囊，粉红温润) */
    s_ui.lbl_affinity = lv_label_create(s_ui.container);
    lv_obj_align(s_ui.lbl_affinity, LV_ALIGN_CENTER, 0, 44);
    lv_obj_set_style_text_color(s_ui.lbl_affinity, lv_color_hex(0xFF6EA7), 0);
    if (font) lv_obj_set_style_text_font(s_ui.lbl_affinity, font, 0);

    /* 4. 极简克制的心语 (冷灰高雅微文字) */
    s_ui.lbl_whisper = lv_label_create(s_ui.container);
    lv_obj_align(s_ui.lbl_whisper, LV_ALIGN_CENTER, 0, 68);
    lv_obj_set_style_text_color(s_ui.lbl_whisper, lv_color_hex(0x7E92AD), 0);
    if (font) lv_obj_set_style_text_font(s_ui.lbl_whisper, font, 0);

    update_mood_display(&s_ui);
    LOG_I(TAG, "使魔卡带已载入舞台 (亲密度: %u)", s_ui.affinity);
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
    familiar_on_unload();
}

static void familiar_tick_1s(cartridge_t *self)
{
    (void)self;
    familiar_on_periodic_tick(1000);
}

static void familiar_on_knock(cartridge_t *self, int intensity, int count)
{
    (void)self;
    (void)count;
    familiar_on_tap((uint8_t)intensity);
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
