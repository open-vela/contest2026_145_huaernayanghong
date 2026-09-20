/**
 * @file cartridge_zen.c
 * @brief 极客禅意与赛博木鱼卡带实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "cartridge_zen.h"
#include <lvgl/lvgl.h>
#include "ui/ui_font.h"

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#  include "core/cartridge_mgr.h"
#  include "core/event_bus.h"
#  include "core/tool_registry.h"
#  include "core/store.h"
#  include "utils/log_utils.h"
#else
#  include "../core/cartridge.h"
#  include "../core/cartridge_mgr.h"
#  include "../core/event_bus.h"
#  include "../core/tool_registry.h"
#  include "../core/store.h"
#  include "../utils/log_utils.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "CartridgeZen"

typedef struct {
    lv_obj_t *container;
    lv_obj_t *lbl_merit_title;
    lv_obj_t *lbl_merit;
    lv_obj_t *halo_fish;
    lv_obj_t *btn_fish;
    lv_obj_t *lbl_fish;
    lv_obj_t *lbl_hint;

    uint32_t  total_merit;
} zen_ui_t;

static cartridge_t s_cartridge_instance;
static zen_ui_t    s_ui;
static bool        s_initialized = false;

static void update_merit_text(zen_ui_t *u)
{
    if (!u || !u->container) return;
    if (u->lbl_merit) {
        char mbuf[32];
        snprintf(mbuf, sizeof(mbuf), "%u", (unsigned int)u->total_merit);
        lv_label_set_text(u->lbl_merit, mbuf);
    }
}

static int zen_on_load(lv_obj_t *stage_parent)
{
    if (!stage_parent) return -1;

    memset(&s_ui, 0, sizeof(s_ui));
    phoenix_stats_t stats;
    phoenix_store_get_stats(&stats);
    s_ui.total_merit = stats.total_merit;

    s_ui.container = lv_obj_create(stage_parent);
    lv_obj_set_size(s_ui.container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_ui.container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.container, 0, 0);
    lv_obj_set_style_pad_all(s_ui.container, 0, 0);
    lv_obj_clear_flag(s_ui.container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.container, LV_OBJ_FLAG_EVENT_BUBBLE);

    const lv_font_t *font_cn = phoenix_ui_get_font();

    /* 1. 功德标题微文字 */
    s_ui.lbl_merit_title = lv_label_create(s_ui.container);
    lv_obj_align(s_ui.lbl_merit_title, LV_ALIGN_CENTER, 0, -68);
    lv_label_set_text(s_ui.lbl_merit_title, "— 赛博功德 —");
    lv_obj_set_style_text_color(s_ui.lbl_merit_title, lv_color_hex(0xA89360), 0);
    if (font_cn) lv_obj_set_style_text_font(s_ui.lbl_merit_title, font_cn, 0);

    /* 2. 核心大字号功德数值 (30px Montserrat 大数字) */
    s_ui.lbl_merit = lv_label_create(s_ui.container);
    lv_obj_align(s_ui.lbl_merit, LV_ALIGN_CENTER, 0, -44);
    lv_obj_set_style_text_color(s_ui.lbl_merit, lv_color_hex(0xFFD700), 0);
    const lv_font_t *font_large = phoenix_ui_get_font_large();
    if (font_large) lv_obj_set_style_text_font(s_ui.lbl_merit, font_large, 0);
    update_merit_text(&s_ui);

    /* 3. 外层光晕底座 (直径 98px) */
    s_ui.halo_fish = lv_obj_create(s_ui.container);
    lv_obj_set_size(s_ui.halo_fish, 98, 98);
    lv_obj_align(s_ui.halo_fish, LV_ALIGN_CENTER, 0, 16);
    lv_obj_set_style_bg_color(s_ui.halo_fish, lv_color_hex(0x18140C), 0);
    lv_obj_set_style_border_color(s_ui.halo_fish, lv_color_hex(0x423418), 0);
    lv_obj_set_style_border_width(s_ui.halo_fish, 1, 0);
    lv_obj_set_style_radius(s_ui.halo_fish, 49, 0);
    lv_obj_clear_flag(s_ui.halo_fish, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.halo_fish, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.halo_fish, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 4. 中央极简微拟态木鱼印台 (直径 82px，加宽点击热区与按下触控微反馈) */
    s_ui.btn_fish = lv_obj_create(s_ui.halo_fish);
    lv_obj_set_size(s_ui.btn_fish, 82, 82);
    lv_obj_center(s_ui.btn_fish);
    lv_obj_set_style_bg_color(s_ui.btn_fish, lv_color_hex(0x221C12), 0);
    lv_obj_set_style_bg_color(s_ui.btn_fish, lv_color_hex(0x382E1E), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(s_ui.btn_fish, lv_color_hex(0xC9A84E), 0);
    lv_obj_set_style_border_color(s_ui.btn_fish, lv_color_hex(0xFFD700), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(s_ui.btn_fish, 2, 0);
    lv_obj_set_style_radius(s_ui.btn_fish, 41, 0);
    lv_obj_clear_flag(s_ui.btn_fish, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.btn_fish, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.btn_fish, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_ext_click_area(s_ui.btn_fish, 16);

    s_ui.lbl_fish = lv_label_create(s_ui.btn_fish);
    lv_obj_center(s_ui.lbl_fish);
    lv_label_set_text(s_ui.lbl_fish, "ZEN");
    lv_obj_set_style_text_color(s_ui.lbl_fish, lv_color_hex(0xF0DE9E), 0);
    if (font_cn) lv_obj_set_style_text_font(s_ui.lbl_fish, font_cn, 0);

    /* 5. 底部温润微提示 */
    s_ui.lbl_hint = lv_label_create(s_ui.container);
    lv_obj_align(s_ui.lbl_hint, LV_ALIGN_CENTER, 0, 74);
    lv_label_set_text(s_ui.lbl_hint, "轻敲积攒功德 · 消除烦恼");
    lv_obj_set_style_text_color(s_ui.lbl_hint, lv_color_hex(0x6A7C91), 0);
    if (font_cn) lv_obj_set_style_text_font(s_ui.lbl_hint, font_cn, 0);

    LOG_I(TAG, "极客禅意木鱼卡带已载入舞台 (当前功德: %u)", s_ui.total_merit);
    return 0;
}

static int zen_on_unload(void)
{
    if (s_ui.container) {
        lv_obj_del(s_ui.container);
        s_ui.container = NULL;
    }
    LOG_I(TAG, "极客禅意木鱼卡带已卸载");
    return 0;
}

static int zen_on_tap(uint8_t intensity)
{
    (void)intensity;
    /* tool_wooden_fish 内部已规范发布功德飞字事件，此处无需重复发布，避免文字重影 */
    phoenix_tool_execute("knock_wooden_fish", "{\"count\":1}", NULL, 0);
    s_ui.total_merit++;
    update_merit_text(&s_ui);

    return 0;
}

static int zen_init(cartridge_t *self, void *user_data)
{
    (void)self;
    (void)user_data;
    phoenix_stats_t stats;
    phoenix_store_get_stats(&stats);
    s_ui.total_merit = stats.total_merit;
    return 0;
}

static void zen_enter(cartridge_t *self, void *stage_view)
{
    (void)self;
    zen_on_load((lv_obj_t *)stage_view);
}

static void zen_exit(cartridge_t *self)
{
    (void)self;
    zen_on_unload();
}

static void zen_destroy(cartridge_t *self)
{
    (void)self;
    zen_on_unload();
}

static void zen_tick_1s(cartridge_t *self)
{
    (void)self;
}

static void zen_on_knock(cartridge_t *self, int intensity, int count)
{
    (void)self;
    (void)count;
    zen_on_tap((uint8_t)intensity);
}

static int zen_get_web_status(cartridge_t *self, char *buf, size_t max_len)
{
    (void)self;
    if (!buf || max_len < 64) return -1;
    snprintf(buf, max_len, "{\"total_merit\":%u}", (unsigned int)s_ui.total_merit);
    return 0;
}

cartridge_t *cartridge_zen_create(void)
{
    if (s_initialized) return &s_cartridge_instance;

    memset(&s_cartridge_instance, 0, sizeof(s_cartridge_instance));
    snprintf(s_cartridge_instance.ops.id, sizeof(s_cartridge_instance.ops.id), "zen");
    snprintf(s_cartridge_instance.ops.name, sizeof(s_cartridge_instance.ops.name), "极客木鱼");
    snprintf(s_cartridge_instance.ops.icon, sizeof(s_cartridge_instance.ops.icon), "[ZEN]");
    s_cartridge_instance.ops.init = zen_init;
    s_cartridge_instance.ops.enter = zen_enter;
    s_cartridge_instance.ops.exit = zen_exit;
    s_cartridge_instance.ops.destroy = zen_destroy;
    s_cartridge_instance.ops.tick_1s = zen_tick_1s;
    s_cartridge_instance.ops.on_knock = zen_on_knock;
    s_cartridge_instance.ops.get_web_status = zen_get_web_status;

    s_initialized = true;
    return &s_cartridge_instance;
}

int cartridge_zen_register(void)
{
    cartridge_t *c = cartridge_zen_create();
    return cartridge_mgr_register(&c->ops, NULL, NULL);
}
