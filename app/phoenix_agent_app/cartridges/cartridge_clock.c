/**
 * @file cartridge_clock.c
 * @brief 拟物机械翻页钟与专注时钟卡带实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "cartridge_clock.h"
#include <lvgl/lvgl.h>
#include "ui/ui_font.h"

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#  include "core/cartridge_mgr.h"
#  include "core/event_bus.h"
#  include "core/tool_registry.h"
#  include "tools/tools.h"
#  include "utils/time_utils.h"
#  include "utils/log_utils.h"
#else
#  include "../core/cartridge.h"
#  include "../core/cartridge_mgr.h"
#  include "../core/event_bus.h"
#  include "../core/tool_registry.h"
#  include "../tools/tools.h"
#  include "../utils/time_utils.h"
#  include "../utils/log_utils.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "CartridgeClock"

LV_FONT_DECLARE(lv_font_montserrat_32);

typedef struct {
    lv_obj_t *container;
    lv_obj_t *card_hour;
    lv_obj_t *lbl_hour;
    lv_obj_t *lbl_colon;
    lv_obj_t *card_min;
    lv_obj_t *lbl_min;
    lv_obj_t *pill_status;
    lv_obj_t *lbl_status;
    lv_obj_t *bar_pomo;

    bool      colon_visible;
    bool      is_pomodoro;
    uint16_t  pomo_remain_s;
} clock_ui_t;

static cartridge_t s_cartridge_instance;
static clock_ui_t  s_ui;
static bool        s_initialized = false;

static void update_clock_display(clock_ui_t *u)
{
    if (!u || !u->container) return;

    char h_str[8] = "08";
    char m_str[8] = "00";

    if (u->is_pomodoro) {
        uint16_t min = u->pomo_remain_s / 60;
        uint16_t sec = u->pomo_remain_s % 60;
        snprintf(h_str, sizeof(h_str), "%02u", min);
        snprintf(m_str, sizeof(m_str), "%02u", sec);
        if (u->lbl_status) {
            lv_label_set_text(u->lbl_status, "● 专注流 · 深度沉浸");
            lv_obj_set_style_text_color(u->lbl_status, lv_color_hex(0xFF7043), 0);
        }
        if (u->card_hour) lv_obj_set_style_border_color(u->card_hour, lv_color_hex(0x663319), 0);
        if (u->card_min)  lv_obj_set_style_border_color(u->card_min, lv_color_hex(0x663319), 0);
        if (u->bar_pomo) {
            lv_obj_clear_flag(u->bar_pomo, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(u->bar_pomo, u->pomo_remain_s, LV_ANIM_OFF);
        }
    } else {
        struct tm ti;
        time_utils_get_local_time(&ti);
        snprintf(h_str, sizeof(h_str), "%02d", ti.tm_hour);
        snprintf(m_str, sizeof(m_str), "%02d", ti.tm_min);
        if (u->lbl_status) {
            lv_label_set_text(u->lbl_status, "机械翻页 · 桌面质感");
            lv_obj_set_style_text_color(u->lbl_status, lv_color_hex(0x6E87A8), 0);
        }
        if (u->card_hour) lv_obj_set_style_border_color(u->card_hour, lv_color_hex(0x1F2E45), 0);
        if (u->card_min)  lv_obj_set_style_border_color(u->card_min, lv_color_hex(0x1F2E45), 0);
        if (u->bar_pomo) {
            lv_obj_add_flag(u->bar_pomo, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (u->lbl_hour) lv_label_set_text(u->lbl_hour, h_str);
    if (u->lbl_min)  lv_label_set_text(u->lbl_min, m_str);
    if (u->lbl_colon) {
        lv_obj_set_style_text_opa(u->lbl_colon, u->colon_visible ? LV_OPA_COVER : LV_OPA_30, 0);
    }
}

static int clock_on_load(lv_obj_t *stage_parent)
{
    if (!stage_parent) return -1;

    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.colon_visible = true;

    s_ui.container = lv_obj_create(stage_parent);
    lv_obj_set_size(s_ui.container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_ui.container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.container, 0, 0);
    lv_obj_set_style_pad_all(s_ui.container, 0, 0);
    lv_obj_clear_flag(s_ui.container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.container, LV_OBJ_FLAG_EVENT_BUBBLE);

    const lv_font_t *font_cjk = phoenix_ui_get_font();

    /* 翻页钟物理面板容器 (260x100) */
    lv_obj_t *row_flip = lv_obj_create(s_ui.container);
    lv_obj_set_size(row_flip, 260, 100);
    lv_obj_align(row_flip, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_opa(row_flip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_flip, 0, 0);
    lv_obj_set_style_pad_all(row_flip, 0, 0);
    lv_obj_clear_flag(row_flip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row_flip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(row_flip, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 1. 小时黑晶翻页卡牌 */
    s_ui.card_hour = lv_obj_create(row_flip);
    lv_obj_set_size(s_ui.card_hour, 112, 90);
    lv_obj_align(s_ui.card_hour, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(s_ui.card_hour, lv_color_hex(0x0C1322), 0);
    lv_obj_set_style_border_color(s_ui.card_hour, lv_color_hex(0x1F2E45), 0);
    lv_obj_set_style_border_width(s_ui.card_hour, 1, 0);
    lv_obj_set_style_radius(s_ui.card_hour, 8, 0);
    lv_obj_set_style_pad_all(s_ui.card_hour, 0, 0);
    lv_obj_clear_flag(s_ui.card_hour, LV_OBJ_FLAG_SCROLLABLE);

    /* 小时卡牌中间物理折痕 (立体阴影 + 高光反光) */
    lv_obj_t *crease_h_shadow = lv_obj_create(s_ui.card_hour);
    lv_obj_set_size(crease_h_shadow, LV_PCT(100), 1);
    lv_obj_align(crease_h_shadow, LV_ALIGN_CENTER, 0, -1);
    lv_obj_set_style_bg_color(crease_h_shadow, lv_color_hex(0x020408), 0);
    lv_obj_set_style_border_width(crease_h_shadow, 0, 0);
    lv_obj_clear_flag(crease_h_shadow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *crease_h_highlight = lv_obj_create(s_ui.card_hour);
    lv_obj_set_size(crease_h_highlight, LV_PCT(100), 1);
    lv_obj_align(crease_h_highlight, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(crease_h_highlight, lv_color_hex(0x283B54), 0);
    lv_obj_set_style_border_width(crease_h_highlight, 0, 0);
    lv_obj_clear_flag(crease_h_highlight, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_hour = lv_label_create(s_ui.card_hour);
    lv_obj_center(s_ui.lbl_hour);
    lv_obj_set_style_text_font(s_ui.lbl_hour, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_ui.lbl_hour, lv_color_hex(0xFFFFFF), 0);
    lv_obj_move_foreground(s_ui.lbl_hour);

    /* 2. 赛博青蓝发光冒号 */
    s_ui.lbl_colon = lv_label_create(row_flip);
    lv_obj_center(s_ui.lbl_colon);
    lv_label_set_text(s_ui.lbl_colon, ":");
    lv_obj_set_style_text_font(s_ui.lbl_colon, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_ui.lbl_colon, lv_color_hex(0x00E5FF), 0);

    /* 3. 分钟黑晶翻页卡牌 */
    s_ui.card_min = lv_obj_create(row_flip);
    lv_obj_set_size(s_ui.card_min, 112, 90);
    lv_obj_align(s_ui.card_min, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(s_ui.card_min, lv_color_hex(0x0C1322), 0);
    lv_obj_set_style_border_color(s_ui.card_min, lv_color_hex(0x1F2E45), 0);
    lv_obj_set_style_border_width(s_ui.card_min, 1, 0);
    lv_obj_set_style_radius(s_ui.card_min, 8, 0);
    lv_obj_set_style_pad_all(s_ui.card_min, 0, 0);
    lv_obj_clear_flag(s_ui.card_min, LV_OBJ_FLAG_SCROLLABLE);

    /* 分钟卡牌中间物理折痕 (立体阴影 + 高光反光) */
    lv_obj_t *crease_m_shadow = lv_obj_create(s_ui.card_min);
    lv_obj_set_size(crease_m_shadow, LV_PCT(100), 1);
    lv_obj_align(crease_m_shadow, LV_ALIGN_CENTER, 0, -1);
    lv_obj_set_style_bg_color(crease_m_shadow, lv_color_hex(0x020408), 0);
    lv_obj_set_style_border_width(crease_m_shadow, 0, 0);
    lv_obj_clear_flag(crease_m_shadow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *crease_m_highlight = lv_obj_create(s_ui.card_min);
    lv_obj_set_size(crease_m_highlight, LV_PCT(100), 1);
    lv_obj_align(crease_m_highlight, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(crease_m_highlight, lv_color_hex(0x283B54), 0);
    lv_obj_set_style_border_width(crease_m_highlight, 0, 0);
    lv_obj_clear_flag(crease_m_highlight, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_min = lv_label_create(s_ui.card_min);
    lv_obj_center(s_ui.lbl_min);
    lv_obj_set_style_text_font(s_ui.lbl_min, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_ui.lbl_min, lv_color_hex(0xFFFFFF), 0);
    lv_obj_move_foreground(s_ui.lbl_min);

    /* 4. 下方单行微胶囊状态提示 (统一居中单行显示，绝不堆叠重叠) */
    s_ui.pill_status = lv_obj_create(s_ui.container);
    lv_obj_set_size(s_ui.pill_status, LV_SIZE_CONTENT, 22);
    lv_obj_align(s_ui.pill_status, LV_ALIGN_CENTER, 0, 56);
    lv_obj_set_style_bg_color(s_ui.pill_status, lv_color_hex(0x0C1424), 0);
    lv_obj_set_style_border_color(s_ui.pill_status, lv_color_hex(0x1F2E45), 0);
    lv_obj_set_style_border_width(s_ui.pill_status, 1, 0);
    lv_obj_set_style_radius(s_ui.pill_status, 11, 0);
    lv_obj_set_style_pad_hor(s_ui.pill_status, 12, 0);
    lv_obj_set_style_pad_ver(s_ui.pill_status, 0, 0);
    lv_obj_clear_flag(s_ui.pill_status, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.lbl_status = lv_label_create(s_ui.pill_status);
    lv_obj_center(s_ui.lbl_status);
    if (font_cjk) lv_obj_set_style_text_font(s_ui.lbl_status, font_cjk, 0);

    /* 5. 专注流线性进度光条 (番茄钟专属，常态隐藏) */
    s_ui.bar_pomo = lv_bar_create(s_ui.container);
    lv_obj_set_size(s_ui.bar_pomo, 180, 4);
    lv_obj_align(s_ui.bar_pomo, LV_ALIGN_CENTER, 0, 74);
    lv_obj_set_style_bg_color(s_ui.bar_pomo, lv_color_hex(0x131D2D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.bar_pomo, lv_color_hex(0xFF6B35), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_ui.bar_pomo, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ui.bar_pomo, 2, LV_PART_INDICATOR);
    lv_bar_set_range(s_ui.bar_pomo, 0, 1500);
    lv_bar_set_value(s_ui.bar_pomo, 1499, LV_ANIM_OFF);
    lv_obj_add_flag(s_ui.bar_pomo, LV_OBJ_FLAG_HIDDEN);

    update_clock_display(&s_ui);
    LOG_I(TAG, "拟物翻页钟卡带已载入舞台");
    return 0;
}

static int clock_on_unload(void)
{
    if (s_ui.container) {
        lv_obj_del(s_ui.container);
        s_ui.container = NULL;
    }
    LOG_I(TAG, "翻页钟卡带已卸载");
    return 0;
}

static int clock_on_tap(uint8_t intensity)
{
    (void)intensity;
    /* 敲击翻转专注状态 (启动/停止系统级番茄钟服务) */
    if (pomodoro_service_is_active()) {
        pomodoro_service_stop();
        s_ui.is_pomodoro = false;
        s_ui.pomo_remain_s = 0;
        LOG_I(TAG, "用户敲击翻页钟，停止全局番茄钟");
    } else {
        pomodoro_service_start(25);
        s_ui.is_pomodoro = true;
        s_ui.pomo_remain_s = 25 * 60;
        LOG_I(TAG, "用户敲击翻页钟，开启25分钟系统级番茄专注");
    }
    update_clock_display(&s_ui);
    return 0;
}

static void clock_on_event(const phoenix_event_data_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;
    if (event->type == PHOENIX_EVT_POMODORO_TICK) {
        s_ui.is_pomodoro = event->data.stats.is_active;
        s_ui.pomo_remain_s = event->data.stats.remaining_s;
        update_clock_display(&s_ui);
    }
}

static int clock_init(cartridge_t *self, void *user_data)
{
    (void)self;
    (void)user_data;
    phoenix_event_subscribe(PHOENIX_EVT_POMODORO_TICK, clock_on_event, NULL);
    return 0;
}

static void clock_enter(cartridge_t *self, void *stage_view)
{
    (void)self;
    clock_on_load((lv_obj_t *)stage_view);
    /* 载入舞台时无缝同步全局后台番茄钟 */
    s_ui.is_pomodoro = pomodoro_service_is_active();
    s_ui.pomo_remain_s = pomodoro_service_get_remaining();
    update_clock_display(&s_ui);
}

static void clock_exit(cartridge_t *self)
{
    (void)self;
    clock_on_unload();
}

static void clock_destroy(cartridge_t *self)
{
    (void)self;
    clock_on_unload();
}

static void clock_tick_1s(cartridge_t *self)
{
    (void)self;
    s_ui.colon_visible = !s_ui.colon_visible;
    update_clock_display(&s_ui);
}

static void clock_on_knock(cartridge_t *self, int intensity, int count)
{
    (void)self;
    (void)count;
    clock_on_tap((uint8_t)intensity);
}

static int clock_get_web_status(cartridge_t *self, char *buf, size_t max_len)
{
    (void)self;
    if (!buf || max_len < 64) return -1;
    snprintf(buf, max_len, "{\"is_pomodoro\":%s,\"remain_s\":%u}",
             s_ui.is_pomodoro ? "true" : "false", (unsigned int)s_ui.pomo_remain_s);
    return 0;
}

cartridge_t *cartridge_clock_create(void)
{
    if (s_initialized) return &s_cartridge_instance;

    memset(&s_cartridge_instance, 0, sizeof(s_cartridge_instance));
    snprintf(s_cartridge_instance.ops.id, sizeof(s_cartridge_instance.ops.id), "clock");
    snprintf(s_cartridge_instance.ops.name, sizeof(s_cartridge_instance.ops.name), "翻页时钟");
    snprintf(s_cartridge_instance.ops.icon, sizeof(s_cartridge_instance.ops.icon), "[TIME]");
    s_cartridge_instance.ops.init = clock_init;
    s_cartridge_instance.ops.enter = clock_enter;
    s_cartridge_instance.ops.exit = clock_exit;
    s_cartridge_instance.ops.destroy = clock_destroy;
    s_cartridge_instance.ops.tick_1s = clock_tick_1s;
    s_cartridge_instance.ops.on_knock = clock_on_knock;
    s_cartridge_instance.ops.get_web_status = clock_get_web_status;

    s_initialized = true;
    return &s_cartridge_instance;
}

int cartridge_clock_register(void)
{
    cartridge_t *c = cartridge_clock_create();
    return cartridge_mgr_register(&c->ops, NULL, NULL);
}
