/**
 * @file cartridge_clock.c
 * @brief 番茄专注时钟卡带实现 (Pomodoro Focus Cartridge)
 * @author OpenVela Contest 2026 Team 145
 */

#include "cartridge_clock.h"
#include <lvgl/lvgl.h>
#include "ui/ui_font.h"

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#  include "core/cartridge_mgr.h"
#  include "core/event_bus.h"
#  include "core/store.h"
#  include "core/tool_registry.h"
#  include "tools/tools.h"
#  include "utils/time_utils.h"
#  include "utils/log_utils.h"
#else
#  include "../core/cartridge.h"
#  include "../core/cartridge_mgr.h"
#  include "../core/event_bus.h"
#  include "../core/store.h"
#  include "../core/tool_registry.h"
#  include "../tools/tools.h"
#  include "../utils/time_utils.h"
#  include "../utils/log_utils.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "CartridgePomodoro"

LV_FONT_DECLARE(lv_font_montserrat_32);

#define COLOR_BG_DARK        lv_color_hex(0x070B14)
#define COLOR_PANEL_BG       lv_color_hex(0x0E1726)
#define COLOR_BORDER_DARK    lv_color_hex(0x1B2A40)
#define COLOR_FOCUS_ACCENT   lv_color_hex(0xFF5722) /* 番茄活力红 */
#define COLOR_FOCUS_BORDER   lv_color_hex(0x802509)
#define COLOR_SHORT_ACCENT   lv_color_hex(0x00E676) /* 薄荷清凉绿 */
#define COLOR_SHORT_BORDER   lv_color_hex(0x005C2F)
#define COLOR_LONG_ACCENT    lv_color_hex(0x2979FF) /* 宁静海青蓝 */
#define COLOR_LONG_BORDER    lv_color_hex(0x103673)
#define COLOR_PAUSE_ACCENT   lv_color_hex(0xFFA000) /* 暂停琥珀金 */
#define COLOR_TEXT_WHITE     lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_MUTED     lv_color_hex(0x758CA8)

typedef struct {
    lv_obj_t *container;

    /* 顶部模式切换胶囊按钮 */
    lv_obj_t *seg_container;
    lv_obj_t *btn_mode_focus;
    lv_obj_t *lbl_mode_focus;
    lv_obj_t *btn_mode_short;
    lv_obj_t *lbl_mode_short;
    lv_obj_t *btn_mode_long;
    lv_obj_t *lbl_mode_long;

    /* 中部环形表盘与倒计时 */
    lv_obj_t *dial_container;
    lv_obj_t *arc_progress;
    lv_obj_t *lbl_status_hint;
    lv_obj_t *lbl_time;
    lv_obj_t *lbl_stats;

    /* 底部触控控制台 */
    lv_obj_t *btn_reset;
    lv_obj_t *lbl_reset;
    lv_obj_t *btn_toggle;
    lv_obj_t *lbl_toggle;
    lv_obj_t *btn_skip;
    lv_obj_t *lbl_skip;

    pomodoro_mode_t current_mode;
    bool            is_active;
    bool            is_paused;
    uint16_t        remaining_s;
    uint16_t        total_s;
} pomodoro_ui_t;

static cartridge_t   s_cartridge_instance;
static pomodoro_ui_t s_ui;
static bool          s_initialized = false;

/* 声明前向回调 */
static void update_pomodoro_ui(pomodoro_ui_t *u);
static void on_mode_btn_clicked(lv_event_t *e);
static void on_toggle_btn_clicked(lv_event_t *e);
static void on_reset_btn_clicked(lv_event_t *e);
static void on_skip_btn_clicked(lv_event_t *e);
static void on_dial_clicked(lv_event_t *e);

/* ========================================================================= */
/*                              UI 刷新与动效                                */
/* ========================================================================= */

static void update_pomodoro_ui(pomodoro_ui_t *u)
{
    if (!u || !u->container) return;

    /* 同步服务底层状态 */
    u->is_active = pomodoro_service_is_active();
    u->is_paused = pomodoro_service_is_paused();
    u->remaining_s = pomodoro_service_get_remaining();
    u->total_s = pomodoro_service_get_total_duration();
    u->current_mode = pomodoro_service_get_mode();

    if (u->total_s == 0) u->total_s = 25 * 60;
    if (!u->is_active && u->remaining_s == 0) {
        u->remaining_s = u->total_s;
    }

    /* 1. 颜色与主题确定 */
    lv_color_t accent_color = COLOR_FOCUS_ACCENT;
    lv_color_t border_color = COLOR_FOCUS_BORDER;
    const char *status_str = "● 深度专注中";

    if (u->current_mode == POMODORO_MODE_SHORT_BREAK) {
        accent_color = COLOR_SHORT_ACCENT;
        border_color = COLOR_SHORT_BORDER;
        status_str = "☕ 短时小憩中";
    } else if (u->current_mode == POMODORO_MODE_LONG_BREAK) {
        accent_color = COLOR_LONG_ACCENT;
        border_color = COLOR_LONG_BORDER;
        status_str = "🌿 长时深休整";
    }

    if (!u->is_active) {
        status_str = "● 准备就绪";
    } else if (u->is_paused) {
        accent_color = COLOR_PAUSE_ACCENT;
        status_str = "⏸ 专注已暂停";
    }

    /* 2. 更新模式胶囊高亮态 */
    if (u->btn_mode_focus) {
        bool sel = (u->current_mode == POMODORO_MODE_FOCUS);
        lv_obj_set_style_bg_color(u->btn_mode_focus, sel ? COLOR_FOCUS_ACCENT : COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_color(u->btn_mode_focus, sel ? COLOR_FOCUS_BORDER : COLOR_BORDER_DARK, 0);
        if (u->lbl_mode_focus) {
            lv_obj_set_style_text_color(u->lbl_mode_focus, sel ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED, 0);
        }
    }
    if (u->btn_mode_short) {
        bool sel = (u->current_mode == POMODORO_MODE_SHORT_BREAK);
        lv_obj_set_style_bg_color(u->btn_mode_short, sel ? COLOR_SHORT_ACCENT : COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_color(u->btn_mode_short, sel ? COLOR_SHORT_BORDER : COLOR_BORDER_DARK, 0);
        if (u->lbl_mode_short) {
            lv_obj_set_style_text_color(u->lbl_mode_short, sel ? lv_color_hex(0x002B16) : COLOR_TEXT_MUTED, 0);
        }
    }
    if (u->btn_mode_long) {
        bool sel = (u->current_mode == POMODORO_MODE_LONG_BREAK);
        lv_obj_set_style_bg_color(u->btn_mode_long, sel ? COLOR_LONG_ACCENT : COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_color(u->btn_mode_long, sel ? COLOR_LONG_BORDER : COLOR_BORDER_DARK, 0);
        if (u->lbl_mode_long) {
            lv_obj_set_style_text_color(u->lbl_mode_long, sel ? COLOR_TEXT_WHITE : COLOR_TEXT_MUTED, 0);
        }
    }

    /* 3. 更新环形进度表盘 */
    if (u->arc_progress) {
        int32_t arc_val = 0;
        if (u->total_s > 0) {
            arc_val = (int32_t)((uint32_t)u->remaining_s * 100 / u->total_s);
        }
        lv_arc_set_value(u->arc_progress, arc_val);
        lv_obj_set_style_arc_color(u->arc_progress, accent_color, LV_PART_INDICATOR);
    }

    /* 4. 更新倒计时大字与状态微标签 */
    if (u->lbl_time) {
        char time_buf[16];
        uint16_t m = u->remaining_s / 60;
        uint16_t s = u->remaining_s % 60;
        snprintf(time_buf, sizeof(time_buf), "%02u:%02u", m, s);
        lv_label_set_text(u->lbl_time, time_buf);
    }

    if (u->lbl_status_hint) {
        lv_label_set_text(u->lbl_status_hint, status_str);
        lv_obj_set_style_text_color(u->lbl_status_hint, accent_color, 0);
    }

    /* 5. 更新今日番茄战绩 */
    if (u->lbl_stats) {
        phoenix_stats_t stats;
        memset(&stats, 0, sizeof(stats));
        phoenix_store_get_stats(&stats);

        char stats_buf[64];
        uint32_t focus_mins = stats.total_focus_seconds / 60;
        snprintf(stats_buf, sizeof(stats_buf), "🍅 %u 轮达成 · %u min",
                 (unsigned int)stats.pomodoro_count, (unsigned int)focus_mins);
        lv_label_set_text(u->lbl_stats, stats_buf);
    }

    /* 6. 更新底部控制台按钮文案与颜色 */
    if (u->btn_toggle && u->lbl_toggle) {
        if (!u->is_active) {
            lv_label_set_text(u->lbl_toggle, "▶ 开始专注");
            lv_obj_set_style_bg_color(u->btn_toggle, accent_color, 0);
            lv_obj_set_style_border_color(u->btn_toggle, border_color, 0);
            lv_obj_set_style_text_color(u->lbl_toggle, COLOR_TEXT_WHITE, 0);
        } else if (u->is_paused) {
            lv_label_set_text(u->lbl_toggle, "▶ 继续");
            lv_obj_set_style_bg_color(u->btn_toggle, COLOR_PAUSE_ACCENT, 0);
            lv_obj_set_style_border_color(u->btn_toggle, lv_color_hex(0x8F5900), 0);
            lv_obj_set_style_text_color(u->lbl_toggle, lv_color_hex(0x1B1200), 0);
        } else {
            lv_label_set_text(u->lbl_toggle, "⏸ 暂停");
            lv_obj_set_style_bg_color(u->btn_toggle, lv_color_hex(0x1F2E45), 0);
            lv_obj_set_style_border_color(u->btn_toggle, lv_color_hex(0x324A6D), 0);
            lv_obj_set_style_text_color(u->lbl_toggle, COLOR_TEXT_WHITE, 0);
        }
    }
}

/* ========================================================================= */
/*                              交互事件回调                                 */
/* ========================================================================= */

static void on_mode_btn_clicked(lv_event_t *e)
{
    uintptr_t target_mode = (uintptr_t)lv_event_get_user_data(e);
    pomodoro_mode_t mode = (pomodoro_mode_t)target_mode;

    LOG_I(TAG, "用户切换番茄钟模式: %d", (int)mode);
    pomodoro_service_set_mode(mode);
    update_pomodoro_ui(&s_ui);
}

static void on_toggle_btn_clicked(lv_event_t *e)
{
    (void)e;
    cartridge_clock_toggle();
}

static void on_reset_btn_clicked(lv_event_t *e)
{
    (void)e;
    LOG_I(TAG, "用户点击重置番茄钟");
    pomodoro_service_reset();
    update_pomodoro_ui(&s_ui);
}

static void on_skip_btn_clicked(lv_event_t *e)
{
    (void)e;
    pomodoro_mode_t cur = pomodoro_service_get_mode();
    pomodoro_mode_t next_mode = (cur == POMODORO_MODE_FOCUS) ?
                                POMODORO_MODE_SHORT_BREAK : POMODORO_MODE_FOCUS;
    LOG_I(TAG, "用户跳过当前阶段，进入模式: %d", (int)next_mode);
    pomodoro_service_stop();
    pomodoro_service_set_mode(next_mode);
    update_pomodoro_ui(&s_ui);
}

static void on_dial_clicked(lv_event_t *e)
{
    (void)e;
    /* 点击中间表盘直接切换 开始/暂停 */
    cartridge_clock_toggle();
}

int cartridge_clock_toggle(void)
{
    if (!pomodoro_service_is_active()) {
        /* 启动当前模式 */
        uint16_t mins = 25;
        pomodoro_mode_t m = pomodoro_service_get_mode();
        if (m == POMODORO_MODE_SHORT_BREAK) mins = 5;
        else if (m == POMODORO_MODE_LONG_BREAK) mins = 15;

        pomodoro_service_start(mins);
        LOG_I(TAG, "启动番茄专注: 模式=%d, 时长=%u分", (int)m, mins);
    } else if (pomodoro_service_is_paused()) {
        pomodoro_service_resume();
        LOG_I(TAG, "恢复番茄专注");
    } else {
        pomodoro_service_pause();
        LOG_I(TAG, "暂停番茄专注");
    }
    update_pomodoro_ui(&s_ui);
    return 0;
}

int cartridge_clock_set_mode(int mode)
{
    if (mode < 0 || mode > 2) return -1;
    pomodoro_service_set_mode((pomodoro_mode_t)mode);
    update_pomodoro_ui(&s_ui);
    return 0;
}

/* ========================================================================= */
/*                              卡带生命周期                                 */
/* ========================================================================= */

static int clock_on_load(lv_obj_t *stage_parent)
{
    if (!stage_parent) return -1;

    memset(&s_ui, 0, sizeof(s_ui));

    const lv_font_t *font_cjk = phoenix_ui_get_font();

    /* 1. 主容器 (全舞台自适应) */
    s_ui.container = lv_obj_create(stage_parent);
    lv_obj_set_size(s_ui.container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_ui.container, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(s_ui.container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.container, 0, 0);
    lv_obj_set_style_pad_all(s_ui.container, 0, 0);
    lv_obj_clear_flag(s_ui.container, LV_OBJ_FLAG_SCROLLABLE);

    /* 2. 顶部模式切换 Segmented Pill (宽 240px, 高 26px, 居中靠上 y=6) */
    s_ui.seg_container = lv_obj_create(s_ui.container);
    lv_obj_set_size(s_ui.seg_container, 252, 26);
    lv_obj_align(s_ui.seg_container, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_style_bg_color(s_ui.seg_container, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_color(s_ui.seg_container, COLOR_BORDER_DARK, 0);
    lv_obj_set_style_border_width(s_ui.seg_container, 1, 0);
    lv_obj_set_style_radius(s_ui.seg_container, 13, 0);
    lv_obj_set_style_pad_all(s_ui.seg_container, 2, 0);
    lv_obj_clear_flag(s_ui.seg_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 2.1 模式按钮 1: 专注 25m */
    s_ui.btn_mode_focus = lv_btn_create(s_ui.seg_container);
    lv_obj_set_size(s_ui.btn_mode_focus, 80, 20);
    lv_obj_align(s_ui.btn_mode_focus, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(s_ui.btn_mode_focus, 10, 0);
    lv_obj_set_style_pad_all(s_ui.btn_mode_focus, 0, 0);
    lv_obj_add_event_cb(s_ui.btn_mode_focus, on_mode_btn_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)POMODORO_MODE_FOCUS);

    s_ui.lbl_mode_focus = lv_label_create(s_ui.btn_mode_focus);
    lv_obj_center(s_ui.lbl_mode_focus);
    lv_label_set_text(s_ui.lbl_mode_focus, "专注 25m");
    if (font_cjk) lv_obj_set_style_text_font(s_ui.lbl_mode_focus, font_cjk, 0);

    /* 2.2 模式按钮 2: 短休 5m */
    s_ui.btn_mode_short = lv_btn_create(s_ui.seg_container);
    lv_obj_set_size(s_ui.btn_mode_short, 80, 20);
    lv_obj_align(s_ui.btn_mode_short, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(s_ui.btn_mode_short, 10, 0);
    lv_obj_set_style_pad_all(s_ui.btn_mode_short, 0, 0);
    lv_obj_add_event_cb(s_ui.btn_mode_short, on_mode_btn_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)POMODORO_MODE_SHORT_BREAK);

    s_ui.lbl_mode_short = lv_label_create(s_ui.btn_mode_short);
    lv_obj_center(s_ui.lbl_mode_short);
    lv_label_set_text(s_ui.lbl_mode_short, "短休 5m");
    if (font_cjk) lv_obj_set_style_text_font(s_ui.lbl_mode_short, font_cjk, 0);

    /* 2.3 模式按钮 3: 长休 15m */
    s_ui.btn_mode_long = lv_btn_create(s_ui.seg_container);
    lv_obj_set_size(s_ui.btn_mode_long, 80, 20);
    lv_obj_align(s_ui.btn_mode_long, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_radius(s_ui.btn_mode_long, 10, 0);
    lv_obj_set_style_pad_all(s_ui.btn_mode_long, 0, 0);
    lv_obj_add_event_cb(s_ui.btn_mode_long, on_mode_btn_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)POMODORO_MODE_LONG_BREAK);

    s_ui.lbl_mode_long = lv_label_create(s_ui.btn_mode_long);
    lv_obj_center(s_ui.lbl_mode_long);
    lv_label_set_text(s_ui.lbl_mode_long, "长休 15m");
    if (font_cjk) lv_obj_set_style_text_font(s_ui.lbl_mode_long, font_cjk, 0);

    /* 3. 中部核心表盘容器 (触控热区 180x130, y=36) */
    s_ui.dial_container = lv_obj_create(s_ui.container);
    lv_obj_set_size(s_ui.dial_container, 200, 126);
    lv_obj_align(s_ui.dial_container, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_bg_opa(s_ui.dial_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.dial_container, 0, 0);
    lv_obj_set_style_pad_all(s_ui.dial_container, 0, 0);
    lv_obj_clear_flag(s_ui.dial_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.dial_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_ui.dial_container, on_dial_clicked, LV_EVENT_CLICKED, NULL);

    /* 3.1 环形动态进度条 */
    s_ui.arc_progress = lv_arc_create(s_ui.dial_container);
    lv_obj_set_size(s_ui.arc_progress, 120, 120);
    lv_obj_center(s_ui.arc_progress);
    lv_arc_set_rotation(s_ui.arc_progress, 270);
    lv_arc_set_bg_angles(s_ui.arc_progress, 0, 360);
    lv_arc_set_range(s_ui.arc_progress, 0, 100);
    lv_arc_set_value(s_ui.arc_progress, 100);
    lv_obj_set_style_arc_width(s_ui.arc_progress, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_ui.arc_progress, lv_color_hex(0x152238), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_ui.arc_progress, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_ui.arc_progress, true, LV_PART_INDICATOR);
    /* 隐藏旋钮 */
    lv_obj_set_style_opa(s_ui.arc_progress, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(s_ui.arc_progress, LV_OBJ_FLAG_CLICKABLE);

    /* 3.2 环内上方微标签 (状态提示) */
    s_ui.lbl_status_hint = lv_label_create(s_ui.dial_container);
    lv_obj_align(s_ui.lbl_status_hint, LV_ALIGN_CENTER, 0, -28);
    if (font_cjk) lv_obj_set_style_text_font(s_ui.lbl_status_hint, font_cjk, 0);

    /* 3.3 环内核心倒计时大字 */
    s_ui.lbl_time = lv_label_create(s_ui.dial_container);
    lv_obj_align(s_ui.lbl_time, LV_ALIGN_CENTER, 0, -2);
    lv_obj_set_style_text_font(s_ui.lbl_time, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_ui.lbl_time, COLOR_TEXT_WHITE, 0);

    /* 3.4 环内下方番茄战绩 */
    s_ui.lbl_stats = lv_label_create(s_ui.dial_container);
    lv_obj_align(s_ui.lbl_stats, LV_ALIGN_CENTER, 0, 26);
    if (font_cjk) lv_obj_set_style_text_font(s_ui.lbl_stats, font_cjk, 0);
    lv_obj_set_style_text_color(s_ui.lbl_stats, COLOR_TEXT_MUTED, 0);

    /* 4. 底部触控控制台 (y = 168, 高 38px) */
    /* 4.1 [重置] 按钮 (次要操作) */
    s_ui.btn_reset = lv_btn_create(s_ui.container);
    lv_obj_set_size(s_ui.btn_reset, 56, 32);
    lv_obj_align(s_ui.btn_reset, LV_ALIGN_BOTTOM_LEFT, 16, -10);
    lv_obj_set_style_bg_color(s_ui.btn_reset, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_color(s_ui.btn_reset, COLOR_BORDER_DARK, 0);
    lv_obj_set_style_border_width(s_ui.btn_reset, 1, 0);
    lv_obj_set_style_radius(s_ui.btn_reset, 16, 0);
    lv_obj_add_event_cb(s_ui.btn_reset, on_reset_btn_clicked, LV_EVENT_CLICKED, NULL);

    s_ui.lbl_reset = lv_label_create(s_ui.btn_reset);
    lv_obj_center(s_ui.lbl_reset);
    lv_label_set_text(s_ui.lbl_reset, "重置");
    if (font_cjk) lv_obj_set_style_text_font(s_ui.lbl_reset, font_cjk, 0);
    lv_obj_set_style_text_color(s_ui.lbl_reset, COLOR_TEXT_MUTED, 0);

    /* 4.2 [开始/暂停/继续] 核心按钮 (主视觉) */
    s_ui.btn_toggle = lv_btn_create(s_ui.container);
    lv_obj_set_size(s_ui.btn_toggle, 108, 34);
    lv_obj_align(s_ui.btn_toggle, LV_ALIGN_BOTTOM_MID, 0, -9);
    lv_obj_set_style_radius(s_ui.btn_toggle, 17, 0);
    lv_obj_set_style_border_width(s_ui.btn_toggle, 1, 0);
    lv_obj_add_event_cb(s_ui.btn_toggle, on_toggle_btn_clicked, LV_EVENT_CLICKED, NULL);

    s_ui.lbl_toggle = lv_label_create(s_ui.btn_toggle);
    lv_obj_center(s_ui.lbl_toggle);
    lv_label_set_text(s_ui.lbl_toggle, "▶ 开始专注");
    if (font_cjk) lv_obj_set_style_text_font(s_ui.lbl_toggle, font_cjk, 0);

    /* 4.3 [跳过] 按钮 (次要操作) */
    s_ui.btn_skip = lv_btn_create(s_ui.container);
    lv_obj_set_size(s_ui.btn_skip, 56, 32);
    lv_obj_align(s_ui.btn_skip, LV_ALIGN_BOTTOM_RIGHT, -16, -10);
    lv_obj_set_style_bg_color(s_ui.btn_skip, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_color(s_ui.btn_skip, COLOR_BORDER_DARK, 0);
    lv_obj_set_style_border_width(s_ui.btn_skip, 1, 0);
    lv_obj_set_style_radius(s_ui.btn_skip, 16, 0);
    lv_obj_add_event_cb(s_ui.btn_skip, on_skip_btn_clicked, LV_EVENT_CLICKED, NULL);

    s_ui.lbl_skip = lv_label_create(s_ui.btn_skip);
    lv_obj_center(s_ui.lbl_skip);
    lv_label_set_text(s_ui.lbl_skip, "跳过");
    if (font_cjk) lv_obj_set_style_text_font(s_ui.lbl_skip, font_cjk, 0);
    lv_obj_set_style_text_color(s_ui.lbl_skip, COLOR_TEXT_MUTED, 0);

    update_pomodoro_ui(&s_ui);
    LOG_I(TAG, "番茄专注时钟卡带已载入舞台");
    return 0;
}

static int clock_on_unload(void)
{
    if (s_ui.container) {
        lv_obj_del(s_ui.container);
        s_ui.container = NULL;
    }
    LOG_I(TAG, "番茄时钟卡带已卸载");
    return 0;
}

static void clock_on_event(const phoenix_event_data_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;
    if (event->type == PHOENIX_EVT_POMODORO_TICK) {
        update_pomodoro_ui(&s_ui);
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
    update_pomodoro_ui(&s_ui);
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
    /* 每秒触发一次 UI 状态同步 */
    update_pomodoro_ui(&s_ui);
}

static void clock_on_knock(cartridge_t *self, int intensity, int count)
{
    (void)self;
    (void)intensity;
    LOG_I(TAG, "用户敲击桌面感知: count=%d, intensity=%d", count, intensity);
    if (count >= 2) {
        /* 双击快速切换模式 */
        pomodoro_mode_t cur = pomodoro_service_get_mode();
        pomodoro_mode_t next = (pomodoro_mode_t)(((int)cur + 1) % 3);
        pomodoro_service_stop();
        pomodoro_service_set_mode(next);
        update_pomodoro_ui(&s_ui);
    } else {
        /* 单击触发 开始 / 暂停 */
        cartridge_clock_toggle();
    }
}

static int clock_get_web_status(cartridge_t *self, char *buf, size_t max_len)
{
    (void)self;
    if (!buf || max_len < 64) return -1;
    bool is_act = pomodoro_service_is_active();
    bool is_pau = pomodoro_service_is_paused();
    uint16_t rem_s = pomodoro_service_get_remaining();
    uint16_t tot_s = pomodoro_service_get_total_duration();
    pomodoro_mode_t m = pomodoro_service_get_mode();

    phoenix_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    phoenix_store_get_stats(&stats);

    snprintf(buf, max_len,
             "{\"is_pomodoro\":%s,\"is_active\":%s,\"is_paused\":%s,\"mode\":%d,\"remain_s\":%u,\"total_s\":%u,\"cycles\":%u}",
             is_act ? "true" : "false",
             is_act ? "true" : "false",
             is_pau ? "true" : "false",
             (int)m,
             (unsigned int)rem_s,
             (unsigned int)tot_s,
             (unsigned int)stats.pomodoro_count);
    return 0;
}

cartridge_t *cartridge_clock_create(void)
{
    if (s_initialized) return &s_cartridge_instance;

    memset(&s_cartridge_instance, 0, sizeof(s_cartridge_instance));
    snprintf(s_cartridge_instance.ops.id, sizeof(s_cartridge_instance.ops.id), "clock");
    snprintf(s_cartridge_instance.ops.name, sizeof(s_cartridge_instance.ops.name), "番茄时钟");
    snprintf(s_cartridge_instance.ops.icon, sizeof(s_cartridge_instance.ops.icon), "[POMO]");
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
