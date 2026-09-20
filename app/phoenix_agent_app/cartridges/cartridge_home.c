/**
 * @file cartridge_home.c
 * @brief 主界面卡带实现：高信息密度时间、极客拟物天气与今日待办 (Home Dashboard)
 * @author OpenVela Contest 2026 Team 145
 */

#include "cartridge_home.h"
#include <lvgl/lvgl.h>
#include "ui/ui_font.h"

#if defined(__has_include) && __has_include("core/cartridge.h")
#  include "core/cartridge.h"
#  include "core/cartridge_mgr.h"
#  include "core/event_bus.h"
#  include "core/weather_service.h"
#  include "core/todo_mgr.h"
#  include "core/tool_registry.h"
#  include "tools/tools.h"
#  include "utils/time_utils.h"
#  include "utils/log_utils.h"
#else
#  include "../core/cartridge.h"
#  include "../core/cartridge_mgr.h"
#  include "../core/event_bus.h"
#  include "../core/weather_service.h"
#  include "../core/todo_mgr.h"
#  include "../core/tool_registry.h"
#  include "../tools/tools.h"
#  include "../utils/time_utils.h"
#  include "../utils/log_utils.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TAG "CartridgeHome"

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

    /* 左侧：时间与天气卡片容器 */
    lv_obj_t *panel_left;
    lv_obj_t *lbl_time;
    lv_obj_t *lbl_date;

    /* 左侧下半部：拟物天气仪表卡片 */
    lv_obj_t *card_weather;
    lv_obj_t *lbl_weather_city_cond;
    lv_obj_t *lbl_weather_temp;
    lv_obj_t *lbl_weather_range;
    lv_obj_t *lbl_weather_extra;

    /* 右侧：今日待办卡片 */
    lv_obj_t *panel_right;
    lv_obj_t *lbl_todo_header;
    lv_obj_t *lbl_todo_progress;
    lv_obj_t *lbl_todo_page;
    lv_obj_t *todo_empty_label;
    lv_obj_t *todo_rows[3];
    lv_obj_t *todo_checks[3];
    lv_obj_t *todo_titles[3];
    lv_obj_t *todo_times[3];

    int      todo_current_page;
    int      displayed_todo_ids[3];
    bool     colon_blink;
} home_ui_t;

static cartridge_t s_cartridge_instance;
static home_ui_t   s_ui;
static bool        s_initialized = false;

/* ========================================================================= */
/*                              UI 刷新逻辑                                  */
/* ========================================================================= */

static void update_weather_display(home_ui_t *u)
{
    if (!u || !u->container || !u->card_weather) return;

    weather_info_t info;
    memset(&info, 0, sizeof(info));
    weather_service_get_info(&info);

    /* 1. 城市与天气状况标签 (如 "上海 · 晴") */
    if (u->lbl_weather_city_cond) {
        char cbuf[64];
        if (info.is_fetching) {
            snprintf(cbuf, sizeof(cbuf), "%s · 同步中", info.city[0] ? info.city : "上海");
            lv_obj_set_style_text_color(u->lbl_weather_city_cond, COLOR_WARN_ORANGE, 0);
        } else {
            snprintf(cbuf, sizeof(cbuf), "%s · %s",
                     info.city[0] ? info.city : "上海",
                     info.condition[0] ? info.condition : "晴");
            lv_obj_set_style_text_color(u->lbl_weather_city_cond, COLOR_ACCENT_CYAN, 0);
        }
        lv_label_set_text(u->lbl_weather_city_cond, cbuf);
    }

    /* 2. 实时大字气温 (如 "24°" 或 "--°") */
    if (u->lbl_weather_temp) {
        char tbuf[16];
        if (info.is_valid) {
            snprintf(tbuf, sizeof(tbuf), "%d°", info.temp_c);
            lv_obj_set_style_text_color(u->lbl_weather_temp, COLOR_TEXT_WHITE, 0);
        } else {
            snprintf(tbuf, sizeof(tbuf), "--°");
            lv_obj_set_style_text_color(u->lbl_weather_temp, COLOR_TEXT_MUTED, 0);
        }
        lv_label_set_text(u->lbl_weather_temp, tbuf);
    }

    /* 3. 预报温差范围 (如 "18°~27°") */
    if (u->lbl_weather_range) {
        char rbuf[32];
        if (info.is_valid) {
            snprintf(rbuf, sizeof(rbuf), "%d° ~ %d°", info.temp_min, info.temp_max);
        } else {
            snprintf(rbuf, sizeof(rbuf), "联网自动更新");
        }
        lv_label_set_text(u->lbl_weather_range, rbuf);
    }

    /* 4. 湿度/状态附加信息 (如 "💧 55%") */
    if (u->lbl_weather_extra) {
        char ebuf[32];
        if (info.is_valid) {
            snprintf(ebuf, sizeof(ebuf), "湿度 %d%%", info.humidity);
            lv_obj_set_style_text_color(u->lbl_weather_extra, COLOR_HEALTH_GREEN, 0);
        } else {
            snprintf(ebuf, sizeof(ebuf), "轻触立即同步");
            lv_obj_set_style_text_color(u->lbl_weather_extra, COLOR_TEXT_DIM, 0);
        }
        lv_label_set_text(u->lbl_weather_extra, ebuf);
    }
}

static void update_todo_display(home_ui_t *u)
{
    if (!u || !u->container) return;

    todo_item_t all_items[TODO_MAX_ITEMS];
    size_t total = todo_mgr_get_all(all_items, TODO_MAX_ITEMS);
    size_t done_cnt = 0;
    todo_mgr_get_counts(&done_cnt);

    /* 1. 更新完成统计 (如 "2/4") */
    if (u->lbl_todo_progress) {
        char pbuf[32];
        snprintf(pbuf, sizeof(pbuf), "%zu/%zu", done_cnt, total);
        lv_label_set_text(u->lbl_todo_progress, pbuf);
    }

    /* 2. 计算分页 */
    int total_pages = (total > 0) ? (int)((total + 2) / 3) : 1;
    if (u->todo_current_page >= total_pages) {
        u->todo_current_page = total_pages - 1;
    }
    if (u->todo_current_page < 0) u->todo_current_page = 0;

    if (u->lbl_todo_page) {
        if (total > 3) {
            char page_buf[16];
            snprintf(page_buf, sizeof(page_buf), "%d/%d", u->todo_current_page + 1, total_pages);
            lv_label_set_text(u->lbl_todo_page, page_buf);
            lv_obj_clear_flag(u->lbl_todo_page, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(u->lbl_todo_page, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 3. 空状态展示处理 */
    if (total == 0) {
        if (u->todo_empty_label) {
            lv_obj_clear_flag(u->todo_empty_label, LV_OBJ_FLAG_HIDDEN);
        }
        for (int i = 0; i < 3; i++) {
            if (u->todo_rows[i]) lv_obj_add_flag(u->todo_rows[i], LV_OBJ_FLAG_HIDDEN);
            u->displayed_todo_ids[i] = -1;
        }
        return;
    } else {
        if (u->todo_empty_label) {
            lv_obj_add_flag(u->todo_empty_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 4. 渲染当前页的 3 项 */
    size_t start_idx = (size_t)u->todo_current_page * 3;
    for (int i = 0; i < 3; i++) {
        if (!u->todo_rows[i]) continue;

        size_t cur_idx = start_idx + i;
        if (cur_idx < total) {
            lv_obj_clear_flag(u->todo_rows[i], LV_OBJ_FLAG_HIDDEN);
            todo_item_t *it = &all_items[cur_idx];
            u->displayed_todo_ids[i] = it->id;

            bool is_done = it->done;

            /* 首项未完成施加高光专注焦点 (Focus Single Task) */
            if (cur_idx == 0 && !is_done) {
                lv_obj_set_style_border_color(u->todo_rows[i], COLOR_ACCENT_CYAN, 0);
                lv_obj_set_style_bg_color(u->todo_rows[i], lv_color_hex(0x132236), 0);
            } else {
                lv_obj_set_style_border_color(u->todo_rows[i], lv_color_hex(0x1c2b40), 0);
                lv_obj_set_style_bg_color(u->todo_rows[i], lv_color_hex(0x0e1726), 0);
            }

            if (u->todo_checks[i]) {
                lv_label_set_text(u->todo_checks[i], is_done ? "[v]" : "[ ]");
                lv_obj_set_style_text_color(u->todo_checks[i],
                    is_done ? COLOR_HEALTH_GREEN : COLOR_ACCENT_CYAN, 0);
            }

            if (u->todo_titles[i]) {
                lv_label_set_text(u->todo_titles[i], it->title);
                lv_obj_set_style_text_color(u->todo_titles[i],
                    is_done ? COLOR_TEXT_DIM : COLOR_TEXT_WHITE, 0);
            }

            if (u->todo_times[i]) {
                lv_label_set_text(u->todo_times[i], it->time_str);
                lv_obj_set_style_text_color(u->todo_times[i],
                    is_done ? COLOR_TEXT_DIM : COLOR_WARN_ORANGE, 0);
            }
        } else {
            lv_obj_add_flag(u->todo_rows[i], LV_OBJ_FLAG_HIDDEN);
            u->displayed_todo_ids[i] = -1;
        }
    }
}

static void update_clock_and_calendar(home_ui_t *u)
{
    if (!u || !u->container) return;

    struct tm ti;
    time_utils_get_local_time(&ti);

    /* 1. 更新大字时间 (支持冒号闪烁) */
    if (u->lbl_time) {
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "%02d%c%02d", ti.tm_hour, u->colon_blink ? ':' : ' ', ti.tm_min);
        lv_label_set_text(u->lbl_time, tbuf);
    }

    /* 2. 更新公历与星期 */
    if (u->lbl_date) {
        static const char *const week_names[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
        char dbuf[32];
        int w_idx = (ti.tm_wday >= 0 && ti.tm_wday < 7) ? ti.tm_wday : 0;
        snprintf(dbuf, sizeof(dbuf), "%d月%d日 %s", ti.tm_mon + 1, ti.tm_mday, week_names[w_idx]);
        lv_label_set_text(u->lbl_date, dbuf);
    }
}

/* ========================================================================= */
/*                              事件回调接口                                 */
/* ========================================================================= */

static void on_todo_row_clicked(lv_event_t *e)
{
    int row_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (row_idx < 0 || row_idx >= 3) return;

    int target_id = s_ui.displayed_todo_ids[row_idx];
    if (target_id <= 0) return;

    todo_mgr_toggle(target_id);

    /* 播放点击微音 */
    phoenix_event_data_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_PLAY_SOUND;
    evt.data.sound.sound_id = 4;
    phoenix_event_publish(&evt);

    /* 顶部飞字提示 */
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_FLYING_TEXT;
    evt.data.flying_text.text = "待办已更新 ✓";
    evt.data.flying_text.color_rgb = 0x00E676;
    phoenix_event_publish(&evt);

    update_todo_display(&s_ui);
}

static void on_todo_page_clicked(lv_event_t *e)
{
    (void)e;
    size_t total = todo_mgr_get_counts(NULL);
    int total_pages = (total > 0) ? (int)((total + 2) / 3) : 1;
    if (total_pages <= 1) return;

    s_ui.todo_current_page = (s_ui.todo_current_page + 1) % total_pages;
    update_todo_display(&s_ui);

    phoenix_event_data_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_PLAY_SOUND;
    evt.data.sound.sound_id = 4;
    phoenix_event_publish(&evt);
}

static void on_weather_card_clicked(lv_event_t *e)
{
    (void)e;
    LOG_I(TAG, "用户触控天气卡片，触发即时网络同步");

    /* 触发后台异步刷新 */
    weather_service_fetch_async();

    /* 界面飞字动效反馈 */
    phoenix_event_data_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_FLYING_TEXT;
    evt.data.flying_text.text = "正在同步天气...";
    evt.data.flying_text.color_rgb = 0x00E5FF;
    phoenix_event_publish(&evt);

    evt.type = PHOENIX_EVT_PLAY_SOUND;
    evt.data.sound.sound_id = 4;
    phoenix_event_publish(&evt);

    update_weather_display(&s_ui);
}

static void home_on_bus_event(const phoenix_event_data_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;

    if (event->type == PHOENIX_EVT_WEATHER_UPDATED) {
        LOG_D(TAG, "主界面收到天气更新广播，刷新显示");
        update_weather_display(&s_ui);
    } else if (event->type == PHOENIX_EVT_TODO_CHANGED) {
        LOG_D(TAG, "主界面收到待办事项变动广播，刷新列表");
        update_todo_display(&s_ui);
    }
}

/* ========================================================================= */
/*                              卡带生命周期                                 */
/* ========================================================================= */

static int home_init(cartridge_t *cart, void *user_data)
{
    (void)user_data;
    if (!cart) return -1;
    s_initialized = true;

    /* 初始化天气服务与待办管理器 */
    weather_service_init();
    todo_mgr_init("/data/phoenix");

    phoenix_event_subscribe(PHOENIX_EVT_WEATHER_UPDATED, home_on_bus_event, NULL);
    phoenix_event_subscribe(PHOENIX_EVT_TODO_CHANGED, home_on_bus_event, NULL);
    return 0;
}

static void home_enter(cartridge_t *cart, void *stage)
{
    if (!stage) return;
    home_ui_t *u = &s_ui;
    lv_obj_t *stage_obj = (lv_obj_t *)stage;
    memset(u, 0, sizeof(home_ui_t));
    u->colon_blink = true;
    u->todo_current_page = 0;
    for (int i = 0; i < 3; i++) u->displayed_todo_ids[i] = -1;

    const lv_font_t *font = phoenix_ui_get_font();

    /* 主舞台全屏容器 (舞台位于 46,24, 宽 274, 高 214) */
    u->container = lv_obj_create(stage_obj);
    lv_obj_set_size(u->container, 274, 214);
    lv_obj_align(u->container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(u->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(u->container, 0, 0);
    lv_obj_set_style_pad_all(u->container, 2, 0);
    lv_obj_clear_flag(u->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(u->container, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* ===================================================================== */
    /* 1. 左侧面板：时间与极客天气仪表盘 (宽 116px, 高 204px)                 */
    /* ===================================================================== */
    u->panel_left = lv_obj_create(u->container);
    lv_obj_set_size(u->panel_left, 116, 204);
    lv_obj_align(u->panel_left, LV_ALIGN_LEFT_MID, 1, 0);
    lv_obj_set_style_bg_color(u->panel_left, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(u->panel_left, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(u->panel_left, 1, 0);
    lv_obj_set_style_radius(u->panel_left, 10, 0);
    lv_obj_set_style_pad_all(u->panel_left, 4, 0);
    lv_obj_clear_flag(u->panel_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(u->panel_left, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 1.1 大字时间 (Montserrat 30px) */
    u->lbl_time = lv_label_create(u->panel_left);
    lv_obj_align(u->lbl_time, LV_ALIGN_TOP_MID, 0, 4);
    const lv_font_t *font_large = phoenix_ui_get_font_large();
    if (font_large) lv_obj_set_style_text_font(u->lbl_time, font_large, 0);
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
    lv_label_set_text(u->lbl_date, "9月20日 周日");

    /* 1.4 拟物天气仪表卡片 (Y=72 ~ 192) - 彻底替代原番茄钟位置 */
    u->card_weather = lv_obj_create(u->panel_left);
    lv_obj_set_size(u->card_weather, 108, 118);
    lv_obj_align(u->card_weather, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_bg_color(u->card_weather, lv_color_hex(0x0c1524), 0);
    lv_obj_set_style_border_color(u->card_weather, lv_color_hex(0x1e304d), 0);
    lv_obj_set_style_border_width(u->card_weather, 1, 0);
    lv_obj_set_style_radius(u->card_weather, 8, 0);
    lv_obj_set_style_pad_all(u->card_weather, 4, 0);
    lv_obj_clear_flag(u->card_weather, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(u->card_weather, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(u->card_weather, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(u->card_weather, on_weather_card_clicked, LV_EVENT_CLICKED, NULL);

    /* 天气城市与状况标签 */
    u->lbl_weather_city_cond = lv_label_create(u->card_weather);
    lv_obj_align(u->lbl_weather_city_cond, LV_ALIGN_TOP_MID, 0, 4);
    if (font) lv_obj_set_style_text_font(u->lbl_weather_city_cond, font, 0);
    lv_obj_set_style_text_color(u->lbl_weather_city_cond, COLOR_ACCENT_CYAN, 0);
    lv_label_set_text(u->lbl_weather_city_cond, "上海 · 晴");

    /* 醒目大号温度 */
    u->lbl_weather_temp = lv_label_create(u->card_weather);
    lv_obj_align(u->lbl_weather_temp, LV_ALIGN_TOP_MID, 0, 24);
    if (font_large) lv_obj_set_style_text_font(u->lbl_weather_temp, font_large, 0);
    lv_obj_set_style_text_color(u->lbl_weather_temp, COLOR_TEXT_WHITE, 0);
    lv_label_set_text(u->lbl_weather_temp, "24°");

    /* 气温预报温差范围 */
    u->lbl_weather_range = lv_label_create(u->card_weather);
    lv_obj_align(u->lbl_weather_range, LV_ALIGN_TOP_MID, 0, 68);
    if (font) lv_obj_set_style_text_font(u->lbl_weather_range, font, 0);
    lv_obj_set_style_text_color(u->lbl_weather_range, COLOR_TEXT_MUTED, 0);
    lv_label_set_text(u->lbl_weather_range, "18° ~ 27°");

    /* 湿度与空气附加提示 */
    u->lbl_weather_extra = lv_label_create(u->card_weather);
    lv_obj_align(u->lbl_weather_extra, LV_ALIGN_TOP_MID, 0, 90);
    if (font) lv_obj_set_style_text_font(u->lbl_weather_extra, font, 0);
    lv_obj_set_style_text_color(u->lbl_weather_extra, COLOR_HEALTH_GREEN, 0);
    lv_label_set_text(u->lbl_weather_extra, "湿度 55%");

    /* ===================================================================== */
    /* 2. 右侧面板：今日待办事项 (宽 150px, 高 204px)                        */
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

    /* 完成进度统计微标 (如 "1/4") */
    u->lbl_todo_progress = lv_label_create(u->panel_right);
    lv_obj_align(u->lbl_todo_progress, LV_ALIGN_TOP_RIGHT, -4, 4);
    if (font) lv_obj_set_style_text_font(u->lbl_todo_progress, font, 0);
    lv_obj_set_style_text_color(u->lbl_todo_progress, COLOR_HEALTH_GREEN, 0);
    lv_label_set_text(u->lbl_todo_progress, "0/0");

    /* 分页提示微标 (如 "1/2", 点击可切换翻页) */
    u->lbl_todo_page = lv_label_create(u->panel_right);
    lv_obj_align(u->lbl_todo_page, LV_ALIGN_TOP_RIGHT, -40, 4);
    if (font) lv_obj_set_style_text_font(u->lbl_todo_page, font, 0);
    lv_obj_set_style_text_color(u->lbl_todo_page, COLOR_TEXT_MUTED, 0);
    lv_obj_add_flag(u->lbl_todo_page, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(u->lbl_todo_page, on_todo_page_clicked, LV_EVENT_CLICKED, NULL);

    /* 待办行分割线 */
    lv_obj_t *todo_div = lv_obj_create(u->panel_right);
    lv_obj_set_size(todo_div, 136, 1);
    lv_obj_align(todo_div, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_color(todo_div, lv_color_hex(0x182638), 0);
    lv_obj_set_style_border_width(todo_div, 0, 0);

    /* 空状态提示 */
    u->todo_empty_label = lv_label_create(u->panel_right);
    lv_obj_align(u->todo_empty_label, LV_ALIGN_CENTER, 0, 10);
    if (font) lv_obj_set_style_text_font(u->todo_empty_label, font, 0);
    lv_obj_set_style_text_color(u->todo_empty_label, COLOR_TEXT_MUTED, 0);
    lv_label_set_text(u->todo_empty_label, "暂无待办事项\n在 Web 伴侣新增");
    lv_obj_add_flag(u->todo_empty_label, LV_OBJ_FLAG_HIDDEN);

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
        lv_obj_add_event_cb(u->todo_rows[i], on_todo_row_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);

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
    update_weather_display(u);
    update_todo_display(u);

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
    phoenix_event_unsubscribe(PHOENIX_EVT_WEATHER_UPDATED, home_on_bus_event, NULL);
    phoenix_event_unsubscribe(PHOENIX_EVT_TODO_CHANGED, home_on_bus_event, NULL);
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
        /* 轻敲空白区域可轮转切换待办事项分页 */
        size_t total = todo_mgr_get_counts(NULL);
        int total_pages = (total > 0) ? (int)((total + 2) / 3) : 1;
        if (total_pages > 1) {
            s_ui.todo_current_page = (s_ui.todo_current_page + 1) % total_pages;
            update_todo_display(&s_ui);
        }
    }
}

static void home_on_knock(cartridge_t *cart, int intensity, int count)
{
    (void)cart;
    (void)intensity;
    /* 桌面敲击：快速切换下一个未完成待办为已达成 */
    if (count == 1) {
        todo_item_t items[TODO_MAX_ITEMS];
        size_t total = todo_mgr_get_all(items, TODO_MAX_ITEMS);
        for (size_t i = 0; i < total; i++) {
            if (!items[i].done) {
                todo_mgr_toggle(items[i].id);
                break;
            }
        }
    }
}

int cartridge_home_toggle_todo(int index)
{
    todo_item_t items[TODO_MAX_ITEMS];
    size_t total = todo_mgr_get_all(items, TODO_MAX_ITEMS);
    if (index < 0 || (size_t)index >= total) return -1;

    int ret = todo_mgr_toggle(items[index].id);
    update_todo_display(&s_ui);
    return ret;
}

int cartridge_home_add_todo(const char *title, const char *time_str)
{
    int ret = todo_mgr_add(title, time_str);
    update_todo_display(&s_ui);
    return (ret > 0) ? 0 : -1;
}

int cartridge_home_refresh_weather(void)
{
    return weather_service_fetch_async();
}

int cartridge_home_toggle_pomodoro(void)
{
    /* 兼容接口：调用系统级番茄钟服务 */
    if (pomodoro_service_is_active()) {
        pomodoro_service_stop();
    } else {
        pomodoro_service_start(25);
    }
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
