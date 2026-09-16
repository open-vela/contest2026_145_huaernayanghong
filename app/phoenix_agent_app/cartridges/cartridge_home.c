/**
 * @file cartridge_home.c
 * @brief 主界面卡带实现：高信息密度时间、内嵌番茄钟与待办事项 (Home Dashboard)
 * @author OpenVela Contest 2026 Team 145
 */

#include "cartridge_home.h"
#include <lvgl/lvgl.h>
#include "ui/ui_font.h"

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#  include "core/cartridge_mgr.h"
#  include "core/event_bus.h"
#  include "core/store.h"
#  include "core/tool_registry.h"
#  include "utils/time_utils.h"
#  include "utils/log_utils.h"
#else
#  include "../core/cartridge.h"
#  include "../core/cartridge_mgr.h"
#  include "../core/event_bus.h"
#  include "../core/store.h"
#  include "../core/tool_registry.h"
#  include "../utils/time_utils.h"
#  include "../utils/log_utils.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TAG "CartridgeHome"

LV_FONT_DECLARE(lv_font_montserrat_32);

#define COLOR_CARD_BG        lv_color_hex(0x0a101d)
#define COLOR_CARD_BORDER    lv_color_hex(0x1a2638)
#define COLOR_TEXT_WHITE     lv_color_hex(0xffffff)
#define COLOR_TEXT_MUTED     lv_color_hex(0x6e87a8)
#define COLOR_TEXT_DIM       lv_color_hex(0x40536c)
#define COLOR_ACCENT_CYAN    lv_color_hex(0x00e5ff)
#define COLOR_HEALTH_GREEN   lv_color_hex(0x00e676)
#define COLOR_WARN_ORANGE    lv_color_hex(0xff9100)

typedef struct {
    lv_obj_t *container;

    /* 左侧：时间面板 */
    lv_obj_t *panel_left;
    lv_obj_t *lbl_time;
    lv_obj_t *lbl_date;

    /* 左侧下半部：内嵌番茄钟控制器 */
    lv_obj_t *card_pomo;
    lv_obj_t *lbl_pomo_title;
    lv_obj_t *lbl_pomo_time;
    lv_obj_t *bar_pomo;
    lv_obj_t *btn_pomo;
    lv_obj_t *lbl_pomo_btn;

    /* 番茄钟状态 */
    bool      is_pomodoro;
    uint16_t  pomo_remain_s;

    /* 右侧：今日待办卡片 */
    lv_obj_t *panel_right;
    lv_obj_t *lbl_todo_header;
    lv_obj_t *lbl_todo_progress;
    lv_obj_t *todo_rows[3];
    lv_obj_t *todo_checks[3];
    lv_obj_t *todo_titles[3];
    lv_obj_t *todo_times[3];

    bool colon_blink;
} home_ui_t;

static cartridge_t      s_cartridge_instance;
static home_ui_t        s_ui;
static bool             s_initialized = false;
static home_todo_item_t s_todos[HOME_MAX_TODOS] = {
    {"方案评审", "09:30", true},
    {"固件烧录", "16:00", false},
    {"项目周报", "19:00", false},
    {"温湿度联调", "20:30", false}
};
static size_t s_todo_count = 4;

static void update_pomodoro_display(home_ui_t *u)
{
    if (!u || !u->container || !u->card_pomo) return;

    if (u->is_pomodoro && u->pomo_remain_s > 0) {
        /* 运行态 */
        if (u->lbl_pomo_title) {
            lv_label_set_text(u->lbl_pomo_title, "● 专注进行中");
            lv_obj_set_style_text_color(u->lbl_pomo_title, COLOR_WARN_ORANGE, 0);
        }

        if (u->lbl_pomo_time) {
            char pbuf[16];
            int m = u->pomo_remain_s / 60;
            int s = u->pomo_remain_s % 60;
            snprintf(pbuf, sizeof(pbuf), "%02d:%02d", m, s);
            lv_label_set_text(u->lbl_pomo_time, pbuf);
            lv_obj_set_style_text_color(u->lbl_pomo_time, COLOR_WARN_ORANGE, 0);
        }

        if (u->bar_pomo) {
            lv_obj_clear_flag(u->bar_pomo, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(u->bar_pomo, u->pomo_remain_s, LV_ANIM_OFF);
        }

        if (u->btn_pomo) {
            lv_obj_set_style_bg_color(u->btn_pomo, lv_color_hex(0x2a1005), 0);
            lv_obj_set_style_border_color(u->btn_pomo, COLOR_WARN_ORANGE, 0);
        }
        if (u->lbl_pomo_btn) {
            lv_label_set_text(u->lbl_pomo_btn, "[ 结束专注 ]");
            lv_obj_set_style_text_color(u->lbl_pomo_btn, COLOR_WARN_ORANGE, 0);
        }
    } else {
        /* 空闲态 */
        if (u->lbl_pomo_title) {
            lv_label_set_text(u->lbl_pomo_title, "● 深度专注流");
            lv_obj_set_style_text_color(u->lbl_pomo_title, COLOR_TEXT_MUTED, 0);
        }

        if (u->lbl_pomo_time) {
            lv_label_set_text(u->lbl_pomo_time, "25:00");
            lv_obj_set_style_text_color(u->lbl_pomo_time, COLOR_TEXT_WHITE, 0);
        }

        if (u->bar_pomo) {
            lv_obj_add_flag(u->bar_pomo, LV_OBJ_FLAG_HIDDEN);
        }

        if (u->btn_pomo) {
            lv_obj_set_style_bg_color(u->btn_pomo, lv_color_hex(0x002c38), 0);
            lv_obj_set_style_border_color(u->btn_pomo, COLOR_ACCENT_CYAN, 0);
        }
        if (u->lbl_pomo_btn) {
            lv_label_set_text(u->lbl_pomo_btn, "[ 开启专注 ]");
            lv_obj_set_style_text_color(u->lbl_pomo_btn, COLOR_ACCENT_CYAN, 0);
        }
    }
}

static void update_todo_display(home_ui_t *u)
{
    if (!u || !u->container) return;

    size_t completed = 0;
    for (size_t i = 0; i < s_todo_count; i++) {
        if (s_todos[i].done) completed++;
    }

    if (u->lbl_todo_progress) {
        char pbuf[32];
        snprintf(pbuf, sizeof(pbuf), "%zu/%zu", completed, s_todo_count);
        lv_label_set_text(u->lbl_todo_progress, pbuf);
    }

    /* 渲染前 3 项到行组件 */
    for (int i = 0; i < 3; i++) {
        if (!u->todo_rows[i]) continue;

        if ((size_t)i < s_todo_count) {
            lv_obj_clear_flag(u->todo_rows[i], LV_OBJ_FLAG_HIDDEN);
            bool is_done = s_todos[i].done;

            if (u->todo_checks[i]) {
                lv_label_set_text(u->todo_checks[i], is_done ? "[v]" : "[ ]");
                lv_obj_set_style_text_color(u->todo_checks[i],
                    is_done ? COLOR_HEALTH_GREEN : COLOR_ACCENT_CYAN, 0);
            }

            if (u->todo_titles[i]) {
                lv_label_set_text(u->todo_titles[i], s_todos[i].title);
                lv_obj_set_style_text_color(u->todo_titles[i],
                    is_done ? COLOR_TEXT_DIM : COLOR_TEXT_WHITE, 0);
            }

            if (u->todo_times[i]) {
                lv_label_set_text(u->todo_times[i], s_todos[i].time_str);
                lv_obj_set_style_text_color(u->todo_times[i],
                    is_done ? COLOR_TEXT_DIM : COLOR_WARN_ORANGE, 0);
            }
        } else {
            lv_obj_add_flag(u->todo_rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void update_clock_and_calendar(home_ui_t *u)
{
    if (!u || !u->container) return;

    uint64_t now_ms = time_utils_get_ms();
    int64_t total_sec = (int64_t)(now_ms / 1000) + 8 * 3600; /* UTC+8 */
    int cur_hour = (int)((total_sec / 3600) % 24);
    int cur_min = (int)((total_sec / 60) % 60);

    /* 日期估算或系统当前时间 */
    time_t raw_time = time(NULL);
    struct tm ti;
    if (raw_time > 1600000000) {
        localtime_r(&raw_time, &ti);
    } else {
        /* 默认 2026-09-08 (周二) */
        memset(&ti, 0, sizeof(ti));
        ti.tm_year = 2026 - 1900;
        ti.tm_mon = 8; /* 9月 (0-11) */
        ti.tm_mday = 8;
        ti.tm_wday = 2; /* 2: 周二 */
    }

    /* 1. 更新大字时间 (支持冒号闪烁) */
    if (u->lbl_time) {
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "%02d%c%02d", cur_hour, u->colon_blink ? ':' : ' ', cur_min);
        lv_label_set_text(u->lbl_time, tbuf);
    }

    /* 2. 更新公历与星期 */
    if (u->lbl_date) {
        static const char *const week_names[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
        char dbuf[32];
        int w_idx = (ti.tm_wday >= 0 && ti.tm_wday < 7) ? ti.tm_wday : 2;
        snprintf(dbuf, sizeof(dbuf), "%d月%d日 %s", ti.tm_mon + 1, ti.tm_mday, week_names[w_idx]);
        lv_label_set_text(u->lbl_date, dbuf);
    }
}

static void on_todo_clicked(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    cartridge_home_toggle_todo(index);
}

static void on_pomo_clicked(lv_event_t *e)
{
    (void)e;
    cartridge_home_toggle_pomodoro();
}

static void home_on_pomodoro_event(const phoenix_event_data_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;
    if (event->type == PHOENIX_EVT_POMODORO_TICK) {
        s_ui.is_pomodoro = event->data.stats.is_active;
        s_ui.pomo_remain_s = event->data.stats.remaining_s;
        update_pomodoro_display(&s_ui);
    }
}

static int home_init(cartridge_t *cart, void *user_data)
{
    (void)user_data;
    if (!cart) return -1;
    s_initialized = true;
    phoenix_event_subscribe(PHOENIX_EVT_POMODORO_TICK, home_on_pomodoro_event, NULL);
    return 0;
}

static void home_enter(cartridge_t *cart, void *stage)
{
    if (!stage) return;
    home_ui_t *u = &s_ui;
    lv_obj_t *stage_obj = (lv_obj_t *)stage;
    memset(u, 0, sizeof(home_ui_t));
    u->colon_blink = true;

    const lv_font_t *font = phoenix_ui_get_font();

    /* 主舞台全屏容器 (舞台位于 46,24, 宽 274, 高 216) */
    u->container = lv_obj_create(stage_obj);
    lv_obj_set_size(u->container, 274, 214);
    lv_obj_align(u->container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(u->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(u->container, 0, 0);
    lv_obj_set_style_pad_all(u->container, 2, 0);
    lv_obj_clear_flag(u->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(u->container, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* ===================================================================== */
    /* 1. 左侧面板：时间与内嵌番茄钟 (宽 116px, 高 204px)                     */
    /* ===================================================================== */
    u->panel_left = lv_obj_create(u->container);
    lv_obj_set_size(u->panel_left, 116, 204);
    lv_obj_align(u->panel_left, LV_ALIGN_LEFT_MID, 1, 0);
    lv_obj_set_style_bg_color(u->panel_left, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(u->panel_left, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(u->panel_left, 1, 0);
    lv_obj_set_style_radius(u->panel_left, 10, 0);
    lv_obj_set_style_pad_all(u->panel_left, 5, 0);
    lv_obj_clear_flag(u->panel_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(u->panel_left, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 1.1 大字时间 (Montserrat 32px) */
    u->lbl_time = lv_label_create(u->panel_left);
    lv_obj_align(u->lbl_time, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_text_font(u->lbl_time, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(u->lbl_time, COLOR_TEXT_WHITE, 0);
    lv_label_set_text(u->lbl_time, "16:08");

    /* 1.2 分割细线 */
    lv_obj_t *line_div = lv_obj_create(u->panel_left);
    lv_obj_set_size(line_div, 102, 1);
    lv_obj_align(line_div, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_color(line_div, lv_color_hex(0x182638), 0);
    lv_obj_set_style_border_width(line_div, 0, 0);

    /* 1.3 日期与星期 (中文 16px) */
    u->lbl_date = lv_label_create(u->panel_left);
    lv_obj_align(u->lbl_date, LV_ALIGN_TOP_MID, 0, 48);
    if (font) lv_obj_set_style_text_font(u->lbl_date, font, 0);
    lv_obj_set_style_text_color(u->lbl_date, COLOR_ACCENT_CYAN, 0);
    lv_label_set_text(u->lbl_date, "9月8日 周二");

    /* 1.4 内嵌番茄钟控制卡片 (Y=72 ~ 188) */
    u->card_pomo = lv_obj_create(u->panel_left);
    lv_obj_set_size(u->card_pomo, 108, 116);
    lv_obj_align(u->card_pomo, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_bg_color(u->card_pomo, lv_color_hex(0x0e1726), 0);
    lv_obj_set_style_border_color(u->card_pomo, lv_color_hex(0x1c2b40), 0);
    lv_obj_set_style_border_width(u->card_pomo, 1, 0);
    lv_obj_set_style_radius(u->card_pomo, 8, 0);
    lv_obj_set_style_pad_all(u->card_pomo, 4, 0);
    lv_obj_clear_flag(u->card_pomo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(u->card_pomo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(u->card_pomo, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(u->card_pomo, on_pomo_clicked, LV_EVENT_CLICKED, NULL);

    /* 番茄钟状态微标 */
    u->lbl_pomo_title = lv_label_create(u->card_pomo);
    lv_obj_align(u->lbl_pomo_title, LV_ALIGN_TOP_MID, 0, 4);
    if (font) lv_obj_set_style_text_font(u->lbl_pomo_title, font, 0);
    lv_obj_set_style_text_color(u->lbl_pomo_title, COLOR_TEXT_MUTED, 0);
    lv_label_set_text(u->lbl_pomo_title, "● 深度专注流");

    /* 番茄钟倒计时 (Montserrat 32px) */
    u->lbl_pomo_time = lv_label_create(u->card_pomo);
    lv_obj_align(u->lbl_pomo_time, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_text_font(u->lbl_pomo_time, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(u->lbl_pomo_time, COLOR_TEXT_WHITE, 0);
    lv_label_set_text(u->lbl_pomo_time, "25:00");

    /* 线性微进度条 */
    u->bar_pomo = lv_bar_create(u->card_pomo);
    lv_obj_set_size(u->bar_pomo, 94, 3);
    lv_obj_align(u->bar_pomo, LV_ALIGN_TOP_MID, 0, 62);
    lv_obj_set_style_bg_color(u->bar_pomo, lv_color_hex(0x152233), LV_PART_MAIN);
    lv_obj_set_style_bg_color(u->bar_pomo, COLOR_WARN_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_radius(u->bar_pomo, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(u->bar_pomo, 2, LV_PART_INDICATOR);
    lv_bar_set_range(u->bar_pomo, 0, 1500);
    lv_bar_set_value(u->bar_pomo, 1500, LV_ANIM_OFF);
    lv_obj_add_flag(u->bar_pomo, LV_OBJ_FLAG_HIDDEN);

    /* 按钮胶囊容器 */
    u->btn_pomo = lv_obj_create(u->card_pomo);
    lv_obj_set_size(u->btn_pomo, 94, 24);
    lv_obj_align(u->btn_pomo, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_bg_color(u->btn_pomo, lv_color_hex(0x002c38), 0);
    lv_obj_set_style_border_color(u->btn_pomo, COLOR_ACCENT_CYAN, 0);
    lv_obj_set_style_border_width(u->btn_pomo, 1, 0);
    lv_obj_set_style_radius(u->btn_pomo, 12, 0);
    lv_obj_set_style_pad_all(u->btn_pomo, 0, 0);
    lv_obj_clear_flag(u->btn_pomo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(u->btn_pomo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(u->btn_pomo, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(u->btn_pomo, on_pomo_clicked, LV_EVENT_CLICKED, NULL);

    u->lbl_pomo_btn = lv_label_create(u->btn_pomo);
    lv_obj_center(u->lbl_pomo_btn);
    if (font) lv_obj_set_style_text_font(u->lbl_pomo_btn, font, 0);
    lv_obj_set_style_text_color(u->lbl_pomo_btn, COLOR_ACCENT_CYAN, 0);
    lv_label_set_text(u->lbl_pomo_btn, "[ 开启专注 ]");

    /* ===================================================================== */
    /* 2. 右侧面板：今日待办事项 (宽 156px, 高 204px)                        */
    /* ===================================================================== */
    u->panel_right = lv_obj_create(u->container);
    lv_obj_set_size(u->panel_right, 150, 204);
    lv_obj_align(u->panel_right, LV_ALIGN_RIGHT_MID, -1, 0);
    lv_obj_set_style_bg_color(u->panel_right, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(u->panel_right, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(u->panel_right, 1, 0);
    lv_obj_set_style_radius(u->panel_right, 10, 0);
    lv_obj_set_style_pad_all(u->panel_right, 5, 0);
    lv_obj_clear_flag(u->panel_right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(u->panel_right, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 2.1 待办标题栏 */
    u->lbl_todo_header = lv_label_create(u->panel_right);
    lv_obj_align(u->lbl_todo_header, LV_ALIGN_TOP_LEFT, 4, 4);
    if (font) lv_obj_set_style_text_font(u->lbl_todo_header, font, 0);
    lv_obj_set_style_text_color(u->lbl_todo_header, COLOR_ACCENT_CYAN, 0);
    lv_label_set_text(u->lbl_todo_header, "今日待办");

    /* 完成进度标签 (如 "1/4") */
    u->lbl_todo_progress = lv_label_create(u->panel_right);
    lv_obj_align(u->lbl_todo_progress, LV_ALIGN_TOP_RIGHT, -4, 4);
    if (font) lv_obj_set_style_text_font(u->lbl_todo_progress, font, 0);
    lv_obj_set_style_text_color(u->lbl_todo_progress, COLOR_HEALTH_GREEN, 0);
    lv_label_set_text(u->lbl_todo_progress, "1/4");

    /* 待办行分割线 */
    lv_obj_t *todo_div = lv_obj_create(u->panel_right);
    lv_obj_set_size(todo_div, 136, 1);
    lv_obj_align(todo_div, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_color(todo_div, lv_color_hex(0x182638), 0);
    lv_obj_set_style_border_width(todo_div, 0, 0);

    /* 2.2 紧凑 3 条待办行卡片 */
    for (int i = 0; i < 3; i++) {
        u->todo_rows[i] = lv_obj_create(u->panel_right);
        lv_obj_set_size(u->todo_rows[i], 138, 48);
        lv_obj_align(u->todo_rows[i], LV_ALIGN_TOP_MID, 0, 32 + i * 50);
        lv_obj_set_style_bg_color(u->todo_rows[i], lv_color_hex(0x0e1726), 0);
        lv_obj_set_style_border_color(u->todo_rows[i], lv_color_hex(0x1c2b40), 0);
        lv_obj_set_style_border_width(u->todo_rows[i], 1, 0);
        lv_obj_set_style_radius(u->todo_rows[i], 6, 0);
        lv_obj_set_style_pad_all(u->todo_rows[i], 3, 0);
        lv_obj_clear_flag(u->todo_rows[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(u->todo_rows[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        /* 绑定点击切换事件 */
        lv_obj_add_flag(u->todo_rows[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(u->todo_rows[i], on_todo_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        /* 勾选框图标 (☑ 或 ☐) */
        u->todo_checks[i] = lv_label_create(u->todo_rows[i]);
        lv_obj_align(u->todo_checks[i], LV_ALIGN_LEFT_MID, 4, 0);
        if (font) lv_obj_set_style_text_font(u->todo_checks[i], font, 0);

        /* 待办标题 */
        u->todo_titles[i] = lv_label_create(u->todo_rows[i]);
        lv_obj_set_width(u->todo_titles[i], 80);
        lv_obj_align(u->todo_titles[i], LV_ALIGN_LEFT_MID, 26, 0);
        if (font) lv_obj_set_style_text_font(u->todo_titles[i], font, 0);
        lv_label_set_long_mode(u->todo_titles[i], LV_LABEL_LONG_DOT);

        /* 待办时间 */
        u->todo_times[i] = lv_label_create(u->todo_rows[i]);
        lv_obj_align(u->todo_times[i], LV_ALIGN_RIGHT_MID, -4, 0);
        if (font) lv_obj_set_style_text_font(u->todo_times[i], font, 0);
    }

    update_clock_and_calendar(u);
    update_todo_display(u);
    update_pomodoro_display(u);

    cart->current_stage = stage;
}

static void home_exit(cartridge_t *cart)
{
    (void)cart;
    if (s_ui.container) {
        lv_obj_del(s_ui.container);
        s_ui.container = NULL;
    }
}

static void home_destroy(cartridge_t *cart)
{
    (void)cart;
    home_exit(cart);
    s_initialized = false;
}

static void home_tick_1s(cartridge_t *cart)
{
    (void)cart;
    s_ui.colon_blink = !s_ui.colon_blink;
    update_clock_and_calendar(&s_ui);
}

static void home_on_touch(cartridge_t *cart, int x, int y, cartridge_touch_type_t type)
{
    (void)cart;
    (void)x;
    (void)y;
    if (type == CARTRIDGE_TOUCH_CLICK) {
        /* 轻敲屏幕可轮换切换第一个未完成的待办 */
        for (size_t i = 0; i < s_todo_count; i++) {
            if (!s_todos[i].done) {
                cartridge_home_toggle_todo((int)i);
                break;
            }
        }
    }
}

static void home_on_knock(cartridge_t *cart, int intensity, int count)
{
    (void)cart;
    (void)intensity;
    /* 桌面敲击：切换下一项待办完成状态 */
    if (count == 1) {
        for (size_t i = 0; i < s_todo_count; i++) {
            if (!s_todos[i].done) {
                cartridge_home_toggle_todo((int)i);
                break;
            }
        }
    }
}

int cartridge_home_toggle_todo(int index)
{
    if (index < 0 || (size_t)index >= s_todo_count) return -1;

    s_todos[index].done = !s_todos[index].done;
    LOG_I(TAG, "待办 [%s] 状态变更为: %s", s_todos[index].title, s_todos[index].done ? "完成" : "未完成");

    /* 播放提示微音 */
    phoenix_event_data_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_PLAY_SOUND;
    evt.data.sound.sound_id = 4; /* Click */
    phoenix_event_publish(&evt);

    if (s_todos[index].done) {
        /* 飞字提示 */
        memset(&evt, 0, sizeof(evt));
        evt.type = PHOENIX_EVT_FLYING_TEXT;
        evt.data.flying_text.text = "待办达成 ✓";
        evt.data.flying_text.color_rgb = 0x00E676;
        phoenix_event_publish(&evt);
    }

    update_todo_display(&s_ui);
    return 0;
}

int cartridge_home_toggle_pomodoro(void)
{
    char tool_res[128];
    if (s_ui.is_pomodoro) {
        phoenix_tool_execute("manage_pomodoro", "{\"action\":\"stop\"}", tool_res, sizeof(tool_res));
        phoenix_event_data_t evt;
        memset(&evt, 0, sizeof(evt));
        evt.type = PHOENIX_EVT_FLYING_TEXT;
        evt.data.flying_text.text = "专注流已停止";
        evt.data.flying_text.color_rgb = 0x6E87A8;
        phoenix_event_publish(&evt);
    } else {
        phoenix_tool_execute("manage_pomodoro", "{\"action\":\"start\",\"duration_minutes\":25}", tool_res, sizeof(tool_res));
        phoenix_event_data_t evt;
        memset(&evt, 0, sizeof(evt));
        evt.type = PHOENIX_EVT_FLYING_TEXT;
        evt.data.flying_text.text = "专注流开启 25m";
        evt.data.flying_text.color_rgb = 0xFF9100;
        phoenix_event_publish(&evt);
    }
    return 0;
}

int cartridge_home_add_todo(const char *title, const char *time_str)
{
    if (!title || !title[0] || s_todo_count >= HOME_MAX_TODOS) return -1;

    strncpy(s_todos[s_todo_count].title, title, sizeof(s_todos[s_todo_count].title) - 1);
    strncpy(s_todos[s_todo_count].time_str, time_str ? time_str : "今日", sizeof(s_todos[s_todo_count].time_str) - 1);
    s_todos[s_todo_count].done = false;
    s_todo_count++;

    update_todo_display(&s_ui);
    return 0;
}

int cartridge_home_register(void)
{
    static const cartridge_ops_t ops = {
        .id       = "home",
        .name     = "主页",
        .icon     = "[HOME]",
        .init     = home_init,
        .enter    = home_enter,
        .exit     = home_exit,
        .destroy  = home_destroy,
        .tick_1s  = home_tick_1s,
        .on_touch = home_on_touch,
        .on_knock = home_on_knock,
    };

    memset(&s_cartridge_instance, 0, sizeof(s_cartridge_instance));
    memcpy(&s_cartridge_instance.ops, &ops, sizeof(ops));
    return cartridge_mgr_register(&ops, NULL, NULL);
}
