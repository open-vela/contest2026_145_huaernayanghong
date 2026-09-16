/**
 * @file cartridge_memo.c
 * @brief 灵感外脑速记胶囊卡带实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "cartridge_memo.h"
#include <lvgl/lvgl.h>
#include "ui/ui_font.h"

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#  include "core/cartridge_mgr.h"
#  include "core/event_bus.h"
#  include "utils/log_utils.h"
#else
#  include "../core/cartridge.h"
#  include "../core/cartridge_mgr.h"
#  include "../core/event_bus.h"
#  include "../utils/log_utils.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "CartridgeMemo"
#define MEMO_MAX_ENTRIES 8
#define MEMO_TEXT_LEN    128

typedef struct {
    char entries[MEMO_MAX_ENTRIES][MEMO_TEXT_LEN];
    size_t count;
    size_t cur_idx;
} memo_store_t;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *lbl_index;
    lv_obj_t *card_memo;
    lv_obj_t *lbl_content;
    lv_obj_t *lbl_hint;
} memo_ui_t;

static cartridge_t  s_cartridge_instance;
static memo_store_t s_store = {
    .entries = {
        "[闪念] 让 AI 成为桌面上具有实体质感的生命体伙伴",
        "[任务] 完成 OpenVela 极简卡带式插件框架架构",
        "[目标] 小屏 320x240 全功能无感流转，支持独立热点配网"
    },
    .count = 3,
    .cur_idx = 0
};
static memo_ui_t    s_ui;
static bool         s_initialized = false;

static void update_memo_view(memo_ui_t *u)
{
    if (!u || !u->container) return;

    if (u->lbl_index) {
        char ibuf[64];
        snprintf(ibuf, sizeof(ibuf), "- 灵感速记 %u / %u -",
                 (unsigned int)(s_store.cur_idx + 1), (unsigned int)s_store.count);
        lv_label_set_text(u->lbl_index, ibuf);
    }
    if (u->lbl_content) {
        if (s_store.count > 0 && s_store.cur_idx < s_store.count) {
            lv_label_set_text(u->lbl_content, s_store.entries[s_store.cur_idx]);
        } else {
            lv_label_set_text(u->lbl_content, "暂无速记内容");
        }
    }
}

int cartridge_memo_add_entry(const char *content)
{
    if (!content || !content[0]) return -1;

    if (s_store.count < MEMO_MAX_ENTRIES) {
        snprintf(s_store.entries[s_store.count], MEMO_TEXT_LEN, "%s", content);
        s_store.cur_idx = s_store.count;
        s_store.count++;
    } else {
        /* 环形覆盖最旧的一条 */
        for (size_t i = 1; i < MEMO_MAX_ENTRIES; i++) {
            strncpy(s_store.entries[i - 1], s_store.entries[i], MEMO_TEXT_LEN);
        }
        snprintf(s_store.entries[MEMO_MAX_ENTRIES - 1], MEMO_TEXT_LEN, "%s", content);
        s_store.cur_idx = MEMO_MAX_ENTRIES - 1;
    }

    LOG_I(TAG, "已存入新灵感备忘: [%s], 当前总计: %zu", content, s_store.count);
    update_memo_view(&s_ui);
    return 0;
}

static int memo_on_load(lv_obj_t *stage_parent)
{
    if (!stage_parent) return -1;

    memset(&s_ui, 0, sizeof(s_ui));

    s_ui.container = lv_obj_create(stage_parent);
    lv_obj_set_size(s_ui.container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_ui.container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.container, 0, 0);
    lv_obj_set_style_pad_all(s_ui.container, 0, 0);
    lv_obj_clear_flag(s_ui.container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.container, LV_OBJ_FLAG_EVENT_BUBBLE);

    const lv_font_t *font = phoenix_ui_get_font();

    /* 1. 卡片上方居中副标 (如 "— 灵感速记 1 / 3 —")，彻底消除右上角电量重叠 */
    s_ui.lbl_index = lv_label_create(s_ui.container);
    lv_obj_align(s_ui.lbl_index, LV_ALIGN_CENTER, 0, -68);
    lv_obj_set_style_text_color(s_ui.lbl_index, lv_color_hex(0x00E5FF), 0);
    if (font) lv_obj_set_style_text_font(s_ui.lbl_index, font, 0);

    /* 2. 核心便签卡片 (微晶深蓝底色、圆润微边框、呼吸留白、加宽触控热区) */
    s_ui.card_memo = lv_obj_create(s_ui.container);
    lv_obj_set_size(s_ui.card_memo, 290, 110);
    lv_obj_align(s_ui.card_memo, LV_ALIGN_CENTER, 0, 8);
    lv_obj_set_style_bg_color(s_ui.card_memo, lv_color_hex(0x0C1220), 0);
    lv_obj_set_style_bg_color(s_ui.card_memo, lv_color_hex(0x141F36), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(s_ui.card_memo, lv_color_hex(0x1F2C45), 0);
    lv_obj_set_style_border_color(s_ui.card_memo, lv_color_hex(0x00E5FF), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(s_ui.card_memo, 1, 0);
    lv_obj_set_style_radius(s_ui.card_memo, 10, 0);
    lv_obj_set_style_pad_all(s_ui.card_memo, 14, 0);
    lv_obj_clear_flag(s_ui.card_memo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.card_memo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.card_memo, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_ext_click_area(s_ui.card_memo, 12);

    s_ui.lbl_content = lv_label_create(s_ui.card_memo);
    lv_obj_set_width(s_ui.lbl_content, LV_PCT(100));
    lv_obj_align(s_ui.lbl_content, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(s_ui.lbl_content, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_ui.lbl_content, lv_color_hex(0xF0F4F8), 0);
    if (font) lv_obj_set_style_text_font(s_ui.lbl_content, font, 0);

    /* 3. 底部交互轻提示微文字 */
    s_ui.lbl_hint = lv_label_create(s_ui.container);
    lv_obj_align(s_ui.lbl_hint, LV_ALIGN_CENTER, 0, 78);
    lv_label_set_text(s_ui.lbl_hint, "轻敲屏幕翻阅下一条速记");
    lv_obj_set_style_text_color(s_ui.lbl_hint, lv_color_hex(0x5A6E85), 0);
    if (font) lv_obj_set_style_text_font(s_ui.lbl_hint, font, 0);

    update_memo_view(&s_ui);
    LOG_I(TAG, "灵感外脑速记卡带已载入舞台");
    return 0;
}

static int memo_on_unload(void)
{
    if (s_ui.container) {
        lv_obj_del(s_ui.container);
        s_ui.container = NULL;
    }
    LOG_I(TAG, "灵感外脑速记卡带已卸载");
    return 0;
}

static int memo_on_tap(uint8_t intensity)
{
    (void)intensity;
    /* 敲击翻阅下一条速记 */
    if (s_store.count > 0) {
        s_store.cur_idx = (s_store.cur_idx + 1) % s_store.count;
        update_memo_view(&s_ui);
        LOG_I(TAG, "翻阅速记卡片 -> 索引 [%zu]", s_store.cur_idx);
    }
    return 0;
}

static int memo_init(cartridge_t *self, void *user_data)
{
    (void)self;
    (void)user_data;
    return 0;
}

static void memo_enter(cartridge_t *self, void *stage_view)
{
    (void)self;
    memo_on_load((lv_obj_t *)stage_view);
}

static void memo_exit(cartridge_t *self)
{
    (void)self;
    memo_on_unload();
}

static void memo_destroy(cartridge_t *self)
{
    (void)self;
    memo_on_unload();
}

static void memo_tick_1s(cartridge_t *self)
{
    (void)self;
}

static void memo_on_knock(cartridge_t *self, int intensity, int count)
{
    (void)self;
    (void)count;
    memo_on_tap((uint8_t)intensity);
}

static int memo_get_web_status(cartridge_t *self, char *buf, size_t max_len)
{
    (void)self;
    if (!buf || max_len < 64) return -1;
    snprintf(buf, max_len, "{\"count\":%zu,\"cur_idx\":%zu}", s_store.count, s_store.cur_idx);
    return 0;
}

cartridge_t *cartridge_memo_create(void)
{
    if (s_initialized) return &s_cartridge_instance;

    memset(&s_cartridge_instance, 0, sizeof(s_cartridge_instance));
    snprintf(s_cartridge_instance.ops.id, sizeof(s_cartridge_instance.ops.id), "memo");
    snprintf(s_cartridge_instance.ops.name, sizeof(s_cartridge_instance.ops.name), "灵感外脑");
    snprintf(s_cartridge_instance.ops.icon, sizeof(s_cartridge_instance.ops.icon), "[MEMO]");
    s_cartridge_instance.ops.init = memo_init;
    s_cartridge_instance.ops.enter = memo_enter;
    s_cartridge_instance.ops.exit = memo_exit;
    s_cartridge_instance.ops.destroy = memo_destroy;
    s_cartridge_instance.ops.tick_1s = memo_tick_1s;
    s_cartridge_instance.ops.on_knock = memo_on_knock;
    s_cartridge_instance.ops.get_web_status = memo_get_web_status;

    s_initialized = true;
    return &s_cartridge_instance;
}

int cartridge_memo_register(void)
{
    cartridge_t *c = cartridge_memo_create();
    return cartridge_mgr_register(&c->ops, NULL, NULL);
}
