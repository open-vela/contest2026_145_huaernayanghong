/**
 * @file ui_settings.c
 * @brief 顶部控制中心抽屉与设置面板实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "ui_settings.h"
#include "../hal/network_mgr.h"
#include "../core/config.h"
#include "../core/agent_core.h"
#include "../harness/llm_provider.h"
#include "../utils/log_utils.h"
#include "../utils/time_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "UISettings"
#define SETTINGS_DRAWER_HEIGHT 210

static void anim_y_cb(void *var, int32_t val)
{
    lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)val);
}

static void anim_mask_opa_cb(void *var, int32_t val)
{
    if (var) lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)val, 0);
}

static void anim_mask_close_ready_cb(lv_anim_t *a)
{
    lv_obj_t *mask = (lv_obj_t *)a->var;
    if (mask) {
        lv_obj_add_flag(mask, LV_OBJ_FLAG_HIDDEN);
    }
}

static void on_mask_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (s && s->is_open) {
        ui_settings_close(s);
    }
}

static void on_close_btn_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (s && s->is_open) {
        ui_settings_close(s);
    }
}

static void on_hotspot_btn_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    LOG_I(TAG, "用户在控制中心触发开启独立热点配网");
    net_mgr_reset_to_softap();
    ui_settings_refresh_data(s);
}

static void on_drawer_touch(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    static lv_point_t s_press_pt;
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();

    if (code == LV_EVENT_PRESSED) {
        if (indev) lv_indev_get_point(indev, &s_press_pt);
    } else if (code == LV_EVENT_RELEASED) {
        if (indev) {
            lv_point_t release_pt;
            lv_indev_get_point(indev, &release_pt);
            int16_t dx = release_pt.x - s_press_pt.x;
            int16_t dy = release_pt.y - s_press_pt.y;
            int16_t abs_dx = (dx < 0) ? -dx : dx;
            int16_t abs_dy = (dy < 0) ? -dy : dy;

            /* 向上滑动超过 30px 且垂直分量主导时收起抽屉，平滑顺畅 */
            if (dy < -30 && abs_dy > abs_dx && s->is_open) {
                ui_settings_close(s);
            }
        }
    }
}

ui_settings_t* ui_settings_create(lv_obj_t *parent, const lv_font_t *font)
{
    if (!parent) return NULL;

    ui_settings_t *s = (ui_settings_t *)malloc(sizeof(ui_settings_t));
    if (!s) return NULL;
    memset(s, 0, sizeof(ui_settings_t));

    s->font = font;
    s->drawer_h = SETTINGS_DRAWER_HEIGHT;
    s->is_open = false;

    /* 1. 半透明遮罩层 (深黑微透) */
    s->mask_bg = lv_obj_create(parent);
    lv_obj_set_size(s->mask_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s->mask_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s->mask_bg, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s->mask_bg, 0, 0);
    lv_obj_clear_flag(s->mask_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->mask_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s->mask_bg, on_mask_clicked, LV_EVENT_CLICKED, s);

    /* 2. 抽屉主容器 (高度 210px) */
    s->drawer = lv_obj_create(parent);
    lv_obj_set_size(s->drawer, LV_PCT(100), s->drawer_h);
    lv_obj_set_pos(s->drawer, 0, -s->drawer_h);
    lv_obj_set_style_bg_color(s->drawer, lv_color_hex(0x0A0F1D), 0);
    lv_obj_set_style_bg_opa(s->drawer, LV_OPA_90, 0);
    lv_obj_set_style_border_color(s->drawer, lv_color_hex(0x1E2B42), 0);
    lv_obj_set_style_border_width(s->drawer, 1, 0);
    lv_obj_set_style_border_side(s->drawer, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_radius(s->drawer, 12, 0);
    lv_obj_set_style_pad_all(s->drawer, 6, 0);
    lv_obj_clear_flag(s->drawer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->drawer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s->drawer, on_drawer_touch, LV_EVENT_PRESSED, s);
    lv_obj_add_event_cb(s->drawer, on_drawer_touch, LV_EVENT_RELEASED, s);

    /* 3. 极简顶栏 (控制中心与设置 + 小巧关闭) */
    lv_obj_t *header = lv_obj_create(s->drawer);
    lv_obj_set_size(header, LV_PCT(100), 22);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_title = lv_label_create(header);
    lv_obj_align(s->lbl_title, LV_ALIGN_LEFT_MID, 6, 0);
    if (s->font) lv_obj_set_style_text_font(s->lbl_title, s->font, 0);
    lv_label_set_text(s->lbl_title, "控制中心与设置");
    lv_obj_set_style_text_color(s->lbl_title, lv_color_hex(0x00E5FF), 0);

    s->btn_close = lv_btn_create(header);
    lv_obj_set_size(s->btn_close, 24, 20);
    lv_obj_align(s->btn_close, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(s->btn_close, lv_color_hex(0x182438), 0);
    lv_obj_set_style_radius(s->btn_close, 4, 0);
    lv_obj_set_style_pad_all(s->btn_close, 0, 0);
    lv_obj_add_event_cb(s->btn_close, on_close_btn_clicked, LV_EVENT_CLICKED, s);

    lv_obj_t *lbl_close = lv_label_create(s->btn_close);
    lv_obj_center(lbl_close);
    lv_label_set_text(lbl_close, "X");
    lv_obj_set_style_text_color(lbl_close, lv_color_hex(0x7E92AD), 0);

    /* 4. 外部配网与热点通道卡片 (y=25, h=52, 宽 304px) */
    s->card_hotspot = lv_obj_create(s->drawer);
    lv_obj_set_size(s->card_hotspot, 304, 52);
    lv_obj_align(s->card_hotspot, LV_ALIGN_TOP_MID, 0, 25);
    lv_obj_set_style_bg_color(s->card_hotspot, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_hotspot, lv_color_hex(0x1E2B42), 0);
    lv_obj_set_style_border_width(s->card_hotspot, 1, 0);
    lv_obj_set_style_radius(s->card_hotspot, 8, 0);
    lv_obj_set_style_pad_all(s->card_hotspot, 4, 0);
    lv_obj_clear_flag(s->card_hotspot, LV_OBJ_FLAG_SCROLLABLE);

    s->btn_hotspot = lv_btn_create(s->card_hotspot);
    lv_obj_set_size(s->btn_hotspot, 288, 24);
    lv_obj_align(s->btn_hotspot, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->btn_hotspot, lv_color_hex(0x142033), 0);
    lv_obj_set_style_border_color(s->btn_hotspot, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->btn_hotspot, 1, 0);
    lv_obj_set_style_radius(s->btn_hotspot, 5, 0);
    lv_obj_set_style_pad_all(s->btn_hotspot, 0, 0);
    lv_obj_add_event_cb(s->btn_hotspot, on_hotspot_btn_clicked, LV_EVENT_CLICKED, s);

    s->lbl_hotspot_btn = lv_label_create(s->btn_hotspot);
    lv_obj_center(s->lbl_hotspot_btn);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_btn, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_btn, "开启热点配网 (192.168.4.1:8080)");
    lv_obj_set_style_text_color(s->lbl_hotspot_btn, lv_color_hex(0x00E5FF), 0);

    s->lbl_hotspot_hint = lv_label_create(s->card_hotspot);
    lv_obj_align(s->lbl_hotspot_hint, LV_ALIGN_BOTTOM_MID, 0, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_hint, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_hint, "连入热点搜索周边Wi-Fi并配置联网");
    lv_obj_set_style_text_color(s->lbl_hotspot_hint, lv_color_hex(0x7E92AD), 0);

    /* 5. 系统当前运行状态卡片 (y=80, h=48, 宽 304px) */
    s->card_status = lv_obj_create(s->drawer);
    lv_obj_set_size(s->card_status, 304, 48);
    lv_obj_align(s->card_status, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_bg_color(s->card_status, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_status, lv_color_hex(0x1E2B42), 0);
    lv_obj_set_style_border_width(s->card_status, 1, 0);
    lv_obj_set_style_radius(s->card_status, 8, 0);
    lv_obj_set_style_pad_all(s->card_status, 4, 0);
    lv_obj_clear_flag(s->card_status, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_status_env = lv_label_create(s->card_status);
    lv_obj_align(s->lbl_status_env, LV_ALIGN_TOP_LEFT, 6, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_status_env, s->font, 0);
    lv_label_set_text(s->lbl_status_env, "● 环境: 26C 60%  |  电量: 85%  |  正常");
    lv_obj_set_style_text_color(s->lbl_status_env, lv_color_hex(0x00FF88), 0);

    s->lbl_status_health = lv_label_create(s->card_status);
    lv_obj_align(s->lbl_status_health, LV_ALIGN_BOTTOM_LEFT, 6, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_status_health, s->font, 0);
    lv_label_set_text(s->lbl_status_health, "● 负载: 优 (0.12)  |  运行时长: 00:08:20");
    lv_obj_set_style_text_color(s->lbl_status_health, lv_color_hex(0x8B9EB5), 0);

    /* 6. 历史交互数据统计卡片 (y=131, h=56, 宽 304px) */
    s->card_history = lv_obj_create(s->drawer);
    lv_obj_set_size(s->card_history, 304, 56);
    lv_obj_align(s->card_history, LV_ALIGN_TOP_MID, 0, 131);
    lv_obj_set_style_bg_color(s->card_history, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_history, lv_color_hex(0x1E2B42), 0);
    lv_obj_set_style_border_width(s->card_history, 1, 0);
    lv_obj_set_style_radius(s->card_history, 8, 0);
    lv_obj_set_style_pad_all(s->card_history, 4, 0);
    lv_obj_clear_flag(s->card_history, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_history_stats = lv_label_create(s->card_history);
    lv_obj_align(s->lbl_history_stats, LV_ALIGN_TOP_LEFT, 6, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_history_stats, s->font, 0);
    lv_label_set_text(s->lbl_history_stats, "交互: 12次 | Token: 1.8k | 专注: 25m");
    lv_obj_set_style_text_color(s->lbl_history_stats, lv_color_hex(0xFFB700), 0);

    s->lbl_history_last = lv_label_create(s->card_history);
    lv_obj_align(s->lbl_history_last, LV_ALIGN_BOTTOM_LEFT, 6, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_history_last, s->font, 0);
    lv_label_set_text(s->lbl_history_last, "最新: 帮我开启25分钟专注流");
    lv_obj_set_style_text_color(s->lbl_history_last, lv_color_hex(0x7E92AD), 0);

    /* 7. 实体极简拉手条 */
    s->handle_bar = lv_obj_create(s->drawer);
    lv_obj_set_size(s->handle_bar, 36, 3);
    lv_obj_align(s->handle_bar, LV_ALIGN_BOTTOM_MID, 0, -3);
    lv_obj_set_style_bg_color(s->handle_bar, lv_color_hex(0x3E4E68), 0);
    lv_obj_set_style_radius(s->handle_bar, 2, 0);
    lv_obj_set_style_border_width(s->handle_bar, 0, 0);
    lv_obj_clear_flag(s->handle_bar, LV_OBJ_FLAG_SCROLLABLE);

    ui_settings_refresh_data(s);
    return s;
}

void ui_settings_destroy(ui_settings_t *settings)
{
    if (!settings) return;

    if (settings->drawer) {
        lv_anim_del(settings->drawer, NULL);
        lv_obj_del(settings->drawer);
        settings->drawer = NULL;
    }
    if (settings->mask_bg) {
        lv_obj_del(settings->mask_bg);
        settings->mask_bg = NULL;
    }
    free(settings);
}

void ui_settings_open(ui_settings_t *settings)
{
    if (!settings || !settings->drawer || settings->is_open) return;

    settings->is_open = true;
    ui_settings_refresh_data(settings);

    /* 展现遮罩并启动渐变暗光动画 (0 -> 160) */
    if (settings->mask_bg) {
        lv_anim_del(settings->mask_bg, NULL);
        lv_obj_clear_flag(settings->mask_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(settings->mask_bg);

        lv_anim_t a_mask;
        lv_anim_init(&a_mask);
        lv_anim_set_var(&a_mask, settings->mask_bg);
        lv_anim_set_values(&a_mask, lv_obj_get_style_bg_opa(settings->mask_bg, 0), LV_OPA_60);
        lv_anim_set_time(&a_mask, 240);
        lv_anim_set_exec_cb(&a_mask, anim_mask_opa_cb);
        lv_anim_set_path_cb(&a_mask, lv_anim_path_ease_out);
        lv_anim_start(&a_mask);
    }
    lv_obj_move_foreground(settings->drawer);

    /* 停止原动画，启动滑入动效 */
    lv_anim_del(settings->drawer, NULL);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, settings->drawer);
    lv_anim_set_values(&a, lv_obj_get_y(settings->drawer), 0);
    lv_anim_set_time(&a, 240);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    LOG_I(TAG, "控制中心抽屉已滑入展开");
}

void ui_settings_close(ui_settings_t *settings)
{
    if (!settings || !settings->drawer || !settings->is_open) return;

    settings->is_open = false;

    /* 停止原动画，启动收起动效 */
    lv_anim_del(settings->drawer, NULL);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, settings->drawer);
    lv_anim_set_values(&a, lv_obj_get_y(settings->drawer), -settings->drawer_h);
    lv_anim_set_time(&a, 200);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);

    if (settings->mask_bg) {
        lv_anim_del(settings->mask_bg, NULL);
        lv_anim_t a_mask;
        lv_anim_init(&a_mask);
        lv_anim_set_var(&a_mask, settings->mask_bg);
        lv_anim_set_values(&a_mask, lv_obj_get_style_bg_opa(settings->mask_bg, 0), LV_OPA_0);
        lv_anim_set_time(&a_mask, 200);
        lv_anim_set_exec_cb(&a_mask, anim_mask_opa_cb);
        lv_anim_set_ready_cb(&a_mask, anim_mask_close_ready_cb);
        lv_anim_set_path_cb(&a_mask, lv_anim_path_ease_in);
        lv_anim_start(&a_mask);
    }

    LOG_I(TAG, "控制中心抽屉已收起");
}

void ui_settings_toggle(ui_settings_t *settings)
{
    if (!settings) return;
    if (settings->is_open) {
        ui_settings_close(settings);
    } else {
        ui_settings_open(settings);
    }
}

bool ui_settings_is_open(const ui_settings_t *settings)
{
    return settings ? settings->is_open : false;
}

void ui_settings_refresh_data(ui_settings_t *settings)
{
    if (!settings) return;

    char ip_buf[32] = {0};
    char ssid_buf[32] = {0};
    net_mode_t mode = net_mgr_get_mode();
    net_mgr_get_ip(ip_buf, sizeof(ip_buf));
    net_mgr_get_ssid(ssid_buf, sizeof(ssid_buf));

    /* 1. 刷新配网与热点卡片 */
    if (settings->lbl_hotspot_btn && settings->btn_hotspot) {
        char btn_str[64];
        if (mode == NET_MODE_SOFTAP_CONFIG) {
            snprintf(btn_str, sizeof(btn_str), "● 热点广播中: %s", ip_buf[0] ? ip_buf : "192.168.4.1:8080");
            lv_label_set_text(settings->lbl_hotspot_btn, btn_str);
            lv_obj_set_style_text_color(settings->lbl_hotspot_btn, lv_color_hex(0xFFB700), 0);
            lv_obj_set_style_border_color(settings->btn_hotspot, lv_color_hex(0xFFB700), 0);
            if (settings->lbl_hotspot_hint) {
                lv_label_set_text(settings->lbl_hotspot_hint, "已开启热点，外部连入可配网络/Agent参数/APIKey");
                lv_obj_set_style_text_color(settings->lbl_hotspot_hint, lv_color_hex(0x00FF88), 0);
            }
        } else {
            snprintf(btn_str, sizeof(btn_str), "开启热点配网 (192.168.4.1:8080)");
            lv_label_set_text(settings->lbl_hotspot_btn, btn_str);
            lv_obj_set_style_text_color(settings->lbl_hotspot_btn, lv_color_hex(0x00E5FF), 0);
            lv_obj_set_style_border_color(settings->btn_hotspot, lv_color_hex(0x00E5FF), 0);
            if (settings->lbl_hotspot_hint) {
                lv_label_set_text(settings->lbl_hotspot_hint, "连入热点可配置网络、Agent参数及APIKey");
                lv_obj_set_style_text_color(settings->lbl_hotspot_hint, lv_color_hex(0x7E92AD), 0);
            }
        }
    }

    /* 2. 刷新系统运行状态卡片 */
    phoenix_agent_ctx_t *agent = phoenix_agent_get_instance();
    uint32_t uptime_s = agent ? agent->stats.uptime_seconds : (uint32_t)(time_utils_get_ms() / 1000);
    uint32_t up_h = uptime_s / 3600;
    uint32_t up_m = (uptime_s % 3600) / 60;
    uint32_t up_s = uptime_s % 60;

    if (settings->lbl_status_env) {
        char env_buf[64];
        snprintf(env_buf, sizeof(env_buf), "● 环境: 26C 60%%  |  电量: 85%%  |  正常");
        lv_label_set_text(settings->lbl_status_env, env_buf);
    }
    if (settings->lbl_status_health) {
        char health_buf[64];
        snprintf(health_buf, sizeof(health_buf), "● 负载: 优 (0.12)  |  运行时长: %02u:%02u:%02u", up_h, up_m, up_s);
        lv_label_set_text(settings->lbl_status_health, health_buf);
    }

    /* 3. 刷新历史交互与遥测卡片 */
    uint32_t interactions = agent ? agent->stats.interaction_count : 12;
    uint32_t tokens = agent ? agent->stats.total_tokens_used : 1850;
    uint32_t focus_m = (agent && agent->stats.continuous_focus_s > 0) ? (agent->stats.continuous_focus_s / 60) : 25;

    if (settings->lbl_history_stats) {
        char stats_buf[64];
        snprintf(stats_buf, sizeof(stats_buf), "交互: %u次 | Token: %u | 专注: %um", interactions, tokens, focus_m);
        lv_label_set_text(settings->lbl_history_stats, stats_buf);
    }

    if (settings->lbl_history_last) {
        char last_buf[96] = "最新: 帮我开启25分钟专注流";
        if (agent && agent->history_count > 0) {
            /* 寻找最近一条用户指令 */
            for (int i = (int)agent->history_count - 1; i >= 0; i--) {
                if (agent->history[i].role == PHOENIX_ROLE_USER && agent->history[i].content) {
                    snprintf(last_buf, sizeof(last_buf), "最新: %s", agent->history[i].content);
                    break;
                }
            }
        }
        lv_label_set_text(settings->lbl_history_last, last_buf);
    }
}
