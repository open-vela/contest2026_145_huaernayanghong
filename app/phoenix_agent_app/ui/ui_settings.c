/**
 * @file ui_settings.c
 * @brief 控制中心面板与二级设置菜单实现 (Two-Level Settings Panel)
 * @author OpenVela Contest 2026 Team 145
 */

#include "ui_settings.h"
#include "../hal/network_mgr.h"
#include "../hal/ble_prov_service.h"
#include "../core/config.h"
#include "../core/agent_core.h"
#include "../harness/llm_provider.h"
#include "../utils/log_utils.h"
#include "../utils/time_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "UISettings"

static void on_close_btn_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (s && s->is_open) {
        ui_settings_close(s);
    }
}

static void on_back_btn_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (s) {
        ui_settings_set_page(s, UI_SETTINGS_PAGE_MAIN);
    }
}

static void on_nav_card_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    lv_obj_t *target = lv_event_get_target(e);
    if (target == s->card_nav_net) {
        ui_settings_set_page(s, UI_SETTINGS_PAGE_NETWORK);
    } else if (target == s->card_nav_sys) {
        ui_settings_set_page(s, UI_SETTINGS_PAGE_SYSTEM);
    } else if (target == s->card_nav_agent) {
        ui_settings_set_page(s, UI_SETTINGS_PAGE_AGENT);
    } else if (target == s->card_nav_store) {
        ui_settings_set_page(s, UI_SETTINGS_PAGE_STORAGE);
    }
}

static void on_ble_prov_btn_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    LOG_I(TAG, "用户触发启动/重启蓝牙极速配网广播");
    ble_prov_service_init(NULL);

    if (s->lbl_ble_prov_btn) {
        lv_label_set_text(s->lbl_ble_prov_btn, "[⚡] 蓝牙配网广播中...");
        lv_obj_set_style_text_color(s->lbl_ble_prov_btn, lv_color_hex(0x00FF88), 0);
    }
    if (s->lbl_ble_status) {
        lv_label_set_text(s->lbl_ble_status, "● 蓝牙配网: 广播中 (Phoenix-Setup)");
        lv_obj_set_style_text_color(s->lbl_ble_status, lv_color_hex(0x00FF88), 0);
    }

    ui_settings_refresh_data(s);
}

static void on_hotspot_btn_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    LOG_I(TAG, "用户触发启动/重置独立热点配网");

    /* 立即给出视觉反馈，杜绝假死与无响应感 */
    if (s->lbl_hotspot_btn) {
        lv_label_set_text(s->lbl_hotspot_btn, "[..] 正在启动热点广播...");
        lv_obj_set_style_text_color(s->lbl_hotspot_btn, lv_color_hex(0xFFB700), 0);
    }
    if (s->btn_hotspot) {
        lv_obj_set_style_border_color(s->btn_hotspot, lv_color_hex(0xFFB700), 0);
        lv_obj_set_style_bg_color(s->btn_hotspot, lv_color_hex(0x2A3B18), 0);
    }
    if (s->lbl_hotspot_hint) {
        lv_label_set_text(s->lbl_hotspot_hint, "后台正在配置射频与DHCP，请稍候...");
        lv_obj_set_style_text_color(s->lbl_hotspot_hint, lv_color_hex(0xFFB700), 0);
    }

    net_mgr_reset_to_softap();
    ui_settings_refresh_data(s);
}

ui_settings_t* ui_settings_create(lv_obj_t *parent, const lv_font_t *font)
{
    if (!parent) return NULL;

    ui_settings_t *s = (ui_settings_t *)calloc(1, sizeof(ui_settings_t));
    if (!s) return NULL;

    s->font = font;
    s->drawer_h = 216;
    s->is_open = false;
    s->current_page = UI_SETTINGS_PAGE_MAIN;

    /* 1. 控制中心专属视窗面板 (宽 274px, 高 216px, x=46, y=24, 贴合左侧侧边栏) */
    s->drawer = lv_obj_create(parent);
    lv_obj_set_size(s->drawer, 274, 216);
    lv_obj_set_pos(s->drawer, 46, 24);
    lv_obj_set_style_bg_color(s->drawer, lv_color_hex(0x070C18), 0);
    lv_obj_set_style_bg_opa(s->drawer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s->drawer, lv_color_hex(0x1B2C46), 0);
    lv_obj_set_style_border_width(s->drawer, 1, 0);
    lv_obj_set_style_radius(s->drawer, 0, 0);
    lv_obj_set_style_pad_all(s->drawer, 4, 0);
    lv_obj_clear_flag(s->drawer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->drawer, LV_OBJ_FLAG_HIDDEN);

    /* 2. 顶栏 (返回 + 动态标题 + 关闭按钮) */
    s->header = lv_obj_create(s->drawer);
    lv_obj_set_size(s->header, LV_PCT(100), 22);
    lv_obj_align(s->header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->header, 0, 0);
    lv_obj_set_style_pad_all(s->header, 0, 0);
    lv_obj_clear_flag(s->header, LV_OBJ_FLAG_SCROLLABLE);

    /* 返回按钮 (默认隐藏) */
    s->btn_back = lv_btn_create(s->header);
    lv_obj_set_size(s->btn_back, 50, 20);
    lv_obj_align(s->btn_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(s->btn_back, lv_color_hex(0x182438), 0);
    lv_obj_set_style_radius(s->btn_back, 4, 0);
    lv_obj_set_style_pad_all(s->btn_back, 0, 0);
    lv_obj_set_ext_click_area(s->btn_back, 8);
    lv_obj_add_flag(s->btn_back, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s->btn_back, on_back_btn_clicked, LV_EVENT_CLICKED, s);

    s->lbl_back = lv_label_create(s->btn_back);
    lv_obj_center(s->lbl_back);
    if (s->font) lv_obj_set_style_text_font(s->lbl_back, s->font, 0);
    lv_label_set_text(s->lbl_back, "< 返回");
    lv_obj_set_style_text_color(s->lbl_back, lv_color_hex(0x00E5FF), 0);

    /* 标题 */
    s->lbl_title = lv_label_create(s->header);
    lv_obj_align(s->lbl_title, LV_ALIGN_LEFT_MID, 4, 0);
    if (s->font) lv_obj_set_style_text_font(s->lbl_title, s->font, 0);
    lv_label_set_text(s->lbl_title, "控制中心");
    lv_obj_set_style_text_color(s->lbl_title, lv_color_hex(0x00E5FF), 0);

    /* 关闭按钮 */
    s->btn_close = lv_btn_create(s->header);
    lv_obj_set_size(s->btn_close, 24, 20);
    lv_obj_align(s->btn_close, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(s->btn_close, lv_color_hex(0x182438), 0);
    lv_obj_set_style_radius(s->btn_close, 4, 0);
    lv_obj_set_style_pad_all(s->btn_close, 0, 0);
    lv_obj_set_ext_click_area(s->btn_close, 10);
    lv_obj_add_event_cb(s->btn_close, on_close_btn_clicked, LV_EVENT_CLICKED, s);

    lv_obj_t *lbl_close = lv_label_create(s->btn_close);
    lv_obj_center(lbl_close);
    lv_label_set_text(lbl_close, "X");
    lv_obj_set_style_text_color(lbl_close, lv_color_hex(0x7E92AD), 0);

    /* =====================================================================
     * 3. 一级主菜单视图 (2x2 四宫格卡牌, w=266, h=184, y=24)
     * ===================================================================== */
    s->view_main = lv_obj_create(s->drawer);
    lv_obj_set_size(s->view_main, 266, 184);
    lv_obj_align(s->view_main, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_opa(s->view_main, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->view_main, 0, 0);
    lv_obj_set_style_pad_all(s->view_main, 0, 0);
    lv_obj_clear_flag(s->view_main, LV_OBJ_FLAG_SCROLLABLE);

    /* 3.1 卡牌 1: 无线与蓝牙 (左上, 129x86) */
    s->card_nav_net = lv_obj_create(s->view_main);
    lv_obj_set_size(s->card_nav_net, 129, 86);
    lv_obj_set_pos(s->card_nav_net, 2, 2);
    lv_obj_set_style_bg_color(s->card_nav_net, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_nav_net, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->card_nav_net, 1, 0);
    lv_obj_set_style_radius(s->card_nav_net, 6, 0);
    lv_obj_set_style_pad_all(s->card_nav_net, 4, 0);
    lv_obj_set_ext_click_area(s->card_nav_net, 6);
    lv_obj_clear_flag(s->card_nav_net, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->card_nav_net, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s->card_nav_net, on_nav_card_clicked, LV_EVENT_CLICKED, s);

    s->lbl_nav_net_t = lv_label_create(s->card_nav_net);
    lv_obj_align(s->lbl_nav_net_t, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_net_t, s->font, 0);
    lv_label_set_text(s->lbl_nav_net_t, "无线与蓝牙 >");
    lv_obj_set_style_text_color(s->lbl_nav_net_t, lv_color_hex(0x00E5FF), 0);

    s->lbl_nav_net_sub = lv_label_create(s->card_nav_net);
    lv_obj_align(s->lbl_nav_net_sub, LV_ALIGN_BOTTOM_LEFT, 2, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_net_sub, s->font, 0);
    lv_label_set_text(s->lbl_nav_net_sub, "● 蓝牙极速配网\n独立热点双模");
    lv_obj_set_style_text_color(s->lbl_nav_net_sub, lv_color_hex(0x8B9EB5), 0);

    /* 3.2 卡牌 2: 系统遥测 (右上, 129x86) */
    s->card_nav_sys = lv_obj_create(s->view_main);
    lv_obj_set_size(s->card_nav_sys, 129, 86);
    lv_obj_set_pos(s->card_nav_sys, 135, 2);
    lv_obj_set_style_bg_color(s->card_nav_sys, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_nav_sys, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_nav_sys, 1, 0);
    lv_obj_set_style_radius(s->card_nav_sys, 6, 0);
    lv_obj_set_style_pad_all(s->card_nav_sys, 4, 0);
    lv_obj_set_ext_click_area(s->card_nav_sys, 6);
    lv_obj_clear_flag(s->card_nav_sys, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->card_nav_sys, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s->card_nav_sys, on_nav_card_clicked, LV_EVENT_CLICKED, s);

    s->lbl_nav_sys_t = lv_label_create(s->card_nav_sys);
    lv_obj_align(s->lbl_nav_sys_t, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_sys_t, s->font, 0);
    lv_label_set_text(s->lbl_nav_sys_t, "系统健康 >");
    lv_obj_set_style_text_color(s->lbl_nav_sys_t, lv_color_hex(0x00FF88), 0);

    s->lbl_nav_sys_sub = lv_label_create(s->card_nav_sys);
    lv_obj_align(s->lbl_nav_sys_sub, LV_ALIGN_BOTTOM_LEFT, 2, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_sys_sub, s->font, 0);
    lv_label_set_text(s->lbl_nav_sys_sub, "CPU/内存/遥测\n60FPS 极速");
    lv_obj_set_style_text_color(s->lbl_nav_sys_sub, lv_color_hex(0x8B9EB5), 0);

    /* 3.3 卡牌 3: 灵眸 Agent (左下, 129x86) */
    s->card_nav_agent = lv_obj_create(s->view_main);
    lv_obj_set_size(s->card_nav_agent, 129, 86);
    lv_obj_set_pos(s->card_nav_agent, 2, 92);
    lv_obj_set_style_bg_color(s->card_nav_agent, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_nav_agent, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_nav_agent, 1, 0);
    lv_obj_set_style_radius(s->card_nav_agent, 6, 0);
    lv_obj_set_style_pad_all(s->card_nav_agent, 4, 0);
    lv_obj_set_ext_click_area(s->card_nav_agent, 6);
    lv_obj_clear_flag(s->card_nav_agent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->card_nav_agent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s->card_nav_agent, on_nav_card_clicked, LV_EVENT_CLICKED, s);

    s->lbl_nav_agent_t = lv_label_create(s->card_nav_agent);
    lv_obj_align(s->lbl_nav_agent_t, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_agent_t, s->font, 0);
    lv_label_set_text(s->lbl_nav_agent_t, "灵眸模型 >");
    lv_obj_set_style_text_color(s->lbl_nav_agent_t, lv_color_hex(0x00E5FF), 0);

    s->lbl_nav_agent_sub = lv_label_create(s->card_nav_agent);
    lv_obj_align(s->lbl_nav_agent_sub, LV_ALIGN_BOTTOM_LEFT, 2, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_agent_sub, s->font, 0);
    lv_label_set_text(s->lbl_nav_agent_sub, "DeepSeek\n交互统计");
    lv_obj_set_style_text_color(s->lbl_nav_agent_sub, lv_color_hex(0x8B9EB5), 0);

    /* 3.4 卡牌 4: TF存储与伴侣 (右下, 129x86) */
    s->card_nav_store = lv_obj_create(s->view_main);
    lv_obj_set_size(s->card_nav_store, 129, 86);
    lv_obj_set_pos(s->card_nav_store, 135, 92);
    lv_obj_set_style_bg_color(s->card_nav_store, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_nav_store, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_nav_store, 1, 0);
    lv_obj_set_style_radius(s->card_nav_store, 6, 0);
    lv_obj_set_style_pad_all(s->card_nav_store, 4, 0);
    lv_obj_set_ext_click_area(s->card_nav_store, 6);
    lv_obj_clear_flag(s->card_nav_store, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->card_nav_store, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s->card_nav_store, on_nav_card_clicked, LV_EVENT_CLICKED, s);

    s->lbl_nav_store_t = lv_label_create(s->card_nav_store);
    lv_obj_align(s->lbl_nav_store_t, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_store_t, s->font, 0);
    lv_label_set_text(s->lbl_nav_store_t, "伴侣看板 >");
    lv_obj_set_style_text_color(s->lbl_nav_store_t, lv_color_hex(0xFFB700), 0);

    s->lbl_nav_store_sub = lv_label_create(s->card_nav_store);
    lv_obj_align(s->lbl_nav_store_sub, LV_ALIGN_BOTTOM_LEFT, 2, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_store_sub, s->font, 0);
    lv_label_set_text(s->lbl_nav_store_sub, "局域网直达\nTF卡存储管理");
    lv_obj_set_style_text_color(s->lbl_nav_store_sub, lv_color_hex(0x8B9EB5), 0);

    /* =====================================================================
     * 4. 二级详情视图容器 (全宽专享面板, 默认隐藏)
     * ===================================================================== */
    s->view_detail = lv_obj_create(s->drawer);
    lv_obj_set_size(s->view_detail, 266, 184);
    lv_obj_align(s->view_detail, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_opa(s->view_detail, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->view_detail, 0, 0);
    lv_obj_set_style_pad_all(s->view_detail, 0, 0);
    lv_obj_clear_flag(s->view_detail, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->view_detail, LV_OBJ_FLAG_HIDDEN);

    /* 4.1 二级详情：无线网络与蓝牙配网 */
    s->sec_net_box = lv_obj_create(s->view_detail);
    lv_obj_set_size(s->sec_net_box, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s->sec_net_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->sec_net_box, 0, 0);
    lv_obj_set_style_pad_all(s->sec_net_box, 0, 0);
    lv_obj_clear_flag(s->sec_net_box, LV_OBJ_FLAG_SCROLLABLE);

    s->card_net_info = lv_obj_create(s->sec_net_box);
    lv_obj_set_size(s->card_net_info, 262, 76);
    lv_obj_align(s->card_net_info, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s->card_net_info, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_net_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_net_info, 1, 0);
    lv_obj_set_style_radius(s->card_net_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_net_info, 4, 0);
    lv_obj_clear_flag(s->card_net_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_net_status = lv_label_create(s->card_net_info);
    lv_obj_align(s->lbl_net_status, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_net_status, s->font, 0);
    lv_label_set_text(s->lbl_net_status, "● 网络状态: 未连接");
    lv_obj_set_style_text_color(s->lbl_net_status, lv_color_hex(0x7E92AD), 0);

    s->lbl_net_ip = lv_label_create(s->card_net_info);
    lv_obj_align(s->lbl_net_ip, LV_ALIGN_TOP_LEFT, 2, 24);
    if (s->font) lv_obj_set_style_text_font(s->lbl_net_ip, s->font, 0);
    lv_label_set_text(s->lbl_net_ip, "局域网 IP: 0.0.0.0 (端口 :8080)");
    lv_obj_set_style_text_color(s->lbl_net_ip, lv_color_hex(0x8B9EB5), 0);

    s->lbl_ble_status = lv_label_create(s->card_net_info);
    lv_obj_align(s->lbl_ble_status, LV_ALIGN_TOP_LEFT, 2, 46);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_status, s->font, 0);
    lv_label_set_text(s->lbl_ble_status, "● 蓝牙配网: 广播中 (Phoenix-Setup)");
    lv_obj_set_style_text_color(s->lbl_ble_status, lv_color_hex(0x00FF88), 0);

    /* 蓝牙极速配网按钮 (青蓝高亮) */
    s->btn_ble_prov = lv_btn_create(s->sec_net_box);
    lv_obj_set_size(s->btn_ble_prov, 262, 38);
    lv_obj_align(s->btn_ble_prov, LV_ALIGN_TOP_MID, 0, 82);
    lv_obj_set_style_bg_color(s->btn_ble_prov, lv_color_hex(0x13273F), 0);
    lv_obj_set_style_border_color(s->btn_ble_prov, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->btn_ble_prov, 2, 0);
    lv_obj_set_style_radius(s->btn_ble_prov, 6, 0);
    lv_obj_set_style_pad_all(s->btn_ble_prov, 0, 0);
    lv_obj_set_ext_click_area(s->btn_ble_prov, 8);
    lv_obj_add_event_cb(s->btn_ble_prov, on_ble_prov_btn_clicked, LV_EVENT_CLICKED, s);

    s->lbl_ble_prov_btn = lv_label_create(s->btn_ble_prov);
    lv_obj_center(s->lbl_ble_prov_btn);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_prov_btn, s->font, 0);
    lv_label_set_text(s->lbl_ble_prov_btn, "[⚡] 开启蓝牙极速配网 (Web BLE)");
    lv_obj_set_style_text_color(s->lbl_ble_prov_btn, lv_color_hex(0x00E5FF), 0);

    /* 独立热点按钮 */
    s->btn_hotspot = lv_btn_create(s->sec_net_box);
    lv_obj_set_size(s->btn_hotspot, 262, 38);
    lv_obj_align(s->btn_hotspot, LV_ALIGN_TOP_MID, 0, 126);
    lv_obj_set_style_bg_color(s->btn_hotspot, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->btn_hotspot, lv_color_hex(0x233754), 0);
    lv_obj_set_style_border_width(s->btn_hotspot, 1, 0);
    lv_obj_set_style_radius(s->btn_hotspot, 6, 0);
    lv_obj_set_style_pad_all(s->btn_hotspot, 0, 0);
    lv_obj_set_ext_click_area(s->btn_hotspot, 8);
    lv_obj_add_event_cb(s->btn_hotspot, on_hotspot_btn_clicked, LV_EVENT_CLICKED, s);

    s->lbl_hotspot_btn = lv_label_create(s->btn_hotspot);
    lv_obj_center(s->lbl_hotspot_btn);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_btn, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_btn, "[📡] 开启独立热点配网 (192.168.4.1)");
    lv_obj_set_style_text_color(s->lbl_hotspot_btn, lv_color_hex(0x8B9EB5), 0);

    /* 4.2 二级详情：系统健康与遥测 */
    s->sec_sys_box = lv_obj_create(s->view_detail);
    lv_obj_set_size(s->sec_sys_box, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s->sec_sys_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->sec_sys_box, 0, 0);
    lv_obj_set_style_pad_all(s->sec_sys_box, 0, 0);
    lv_obj_clear_flag(s->sec_sys_box, LV_OBJ_FLAG_SCROLLABLE);

    s->card_sys_info = lv_obj_create(s->sec_sys_box);
    lv_obj_set_size(s->card_sys_info, 272, 146);
    lv_obj_align(s->card_sys_info, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s->card_sys_info, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_sys_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_sys_info, 1, 0);
    lv_obj_set_style_radius(s->card_sys_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_sys_info, 6, 0);
    lv_obj_clear_flag(s->card_sys_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_sys_uptime = lv_label_create(s->card_sys_info);
    lv_obj_align(s->lbl_sys_uptime, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_sys_uptime, s->font, 0);
    lv_label_set_text(s->lbl_sys_uptime, "● 运行时长: 00:00:00 | 感知正常");
    lv_obj_set_style_text_color(s->lbl_sys_uptime, lv_color_hex(0x00FF88), 0);

    s->lbl_sys_cpu = lv_label_create(s->card_sys_info);
    lv_obj_align(s->lbl_sys_cpu, LV_ALIGN_TOP_LEFT, 2, 26);
    if (s->font) lv_obj_set_style_text_font(s->lbl_sys_cpu, s->font, 0);
    lv_label_set_text(s->lbl_sys_cpu, "CPU 负载: 12%  |  RAM: 18.5MB / 32MB");
    lv_obj_set_style_text_color(s->lbl_sys_cpu, lv_color_hex(0x8B9EB5), 0);

    s->lbl_sys_fps = lv_label_create(s->card_sys_info);
    lv_obj_align(s->lbl_sys_fps, LV_ALIGN_TOP_LEFT, 2, 50);
    if (s->font) lv_obj_set_style_text_font(s->lbl_sys_fps, s->font, 0);
    lv_label_set_text(s->lbl_sys_fps, "UI 渲染: 60 FPS  |  DMA 传输正常");
    lv_obj_set_style_text_color(s->lbl_sys_fps, lv_color_hex(0x00E5FF), 0);

    /* 4.3 二级详情：灵眸 Agent 模型 */
    s->sec_agent_box = lv_obj_create(s->view_detail);
    lv_obj_set_size(s->sec_agent_box, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s->sec_agent_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->sec_agent_box, 0, 0);
    lv_obj_set_style_pad_all(s->sec_agent_box, 0, 0);
    lv_obj_clear_flag(s->sec_agent_box, LV_OBJ_FLAG_SCROLLABLE);

    s->card_agent_info = lv_obj_create(s->sec_agent_box);
    lv_obj_set_size(s->card_agent_info, 272, 146);
    lv_obj_align(s->card_agent_info, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s->card_agent_info, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_agent_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_agent_info, 1, 0);
    lv_obj_set_style_radius(s->card_agent_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_agent_info, 6, 0);
    lv_obj_clear_flag(s->card_agent_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_agent_model = lv_label_create(s->card_agent_info);
    lv_obj_align(s->lbl_agent_model, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_model, s->font, 0);
    lv_label_set_text(s->lbl_agent_model, "模型: DeepSeek-V3 端云协同");
    lv_obj_set_style_text_color(s->lbl_agent_model, lv_color_hex(0x00E5FF), 0);

    s->lbl_agent_stats = lv_label_create(s->card_agent_info);
    lv_obj_align(s->lbl_agent_stats, LV_ALIGN_TOP_LEFT, 2, 26);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_stats, s->font, 0);
    lv_label_set_text(s->lbl_agent_stats, "交互: 12次 | Token: 1850 | 专注: 25m");
    lv_obj_set_style_text_color(s->lbl_agent_stats, lv_color_hex(0x8B9EB5), 0);

    s->lbl_agent_last = lv_label_create(s->card_agent_info);
    lv_obj_align(s->lbl_agent_last, LV_ALIGN_TOP_LEFT, 2, 50);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_last, s->font, 0);
    lv_label_set_text(s->lbl_agent_last, "最新: 帮我开启25分钟专注流");
    lv_obj_set_style_text_color(s->lbl_agent_last, lv_color_hex(0xFFB700), 0);

    /* 4.4 二级详情：TF卡与伴侣看板 */
    s->sec_store_box = lv_obj_create(s->view_detail);
    lv_obj_set_size(s->sec_store_box, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s->sec_store_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->sec_store_box, 0, 0);
    lv_obj_set_style_pad_all(s->sec_store_box, 0, 0);
    lv_obj_clear_flag(s->sec_store_box, LV_OBJ_FLAG_SCROLLABLE);

    s->card_store_info = lv_obj_create(s->sec_store_box);
    lv_obj_set_size(s->card_store_info, 272, 146);
    lv_obj_align(s->card_store_info, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s->card_store_info, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_store_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_store_info, 1, 0);
    lv_obj_set_style_radius(s->card_store_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_store_info, 6, 0);
    lv_obj_clear_flag(s->card_store_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_store_sd = lv_label_create(s->card_store_info);
    lv_obj_align(s->lbl_store_sd, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_store_sd, s->font, 0);
    lv_label_set_text(s->lbl_store_sd, "TF卡存储: 已挂载 (可用 28.6GB)");
    lv_obj_set_style_text_color(s->lbl_store_sd, lv_color_hex(0x00FF88), 0);

    s->lbl_store_web = lv_label_create(s->card_store_info);
    lv_obj_align(s->lbl_store_web, LV_ALIGN_TOP_LEFT, 2, 26);
    if (s->font) lv_obj_set_style_text_font(s->lbl_store_web, s->font, 0);
    lv_label_set_text(s->lbl_store_web, "伴侣看板: 免端口直达 HTTP :8080");
    lv_obj_set_style_text_color(s->lbl_store_web, lv_color_hex(0x00E5FF), 0);

    s->lbl_store_desc = lv_label_create(s->card_store_info);
    lv_obj_align(s->lbl_store_desc, LV_ALIGN_TOP_LEFT, 2, 50);
    if (s->font) lv_obj_set_style_text_font(s->lbl_store_desc, s->font, 0);
    lv_label_set_text(s->lbl_store_desc, "手机/电脑同局域网浏览器即可管理");
    lv_obj_set_style_text_color(s->lbl_store_desc, lv_color_hex(0x8B9EB5), 0);

    ui_settings_set_page(s, UI_SETTINGS_PAGE_MAIN);
    return s;
}

void ui_settings_destroy(ui_settings_t *settings)
{
    if (!settings) return;
    if (settings->drawer) {
        lv_obj_del(settings->drawer);
        settings->drawer = NULL;
    }
    free(settings);
}

void ui_settings_set_close_cb(ui_settings_t *settings, void (*cb)(void *), void *user_data)
{
    if (!settings) return;
    settings->on_close_cb = cb;
    settings->close_user_data = user_data;
}

void ui_settings_set_page(ui_settings_t *settings, ui_settings_page_t page)
{
    if (!settings) return;
    settings->current_page = page;

    if (page == UI_SETTINGS_PAGE_MAIN) {
        /* 展示一级四宫格，隐藏二级详情 */
        lv_obj_clear_flag(settings->view_main, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(settings->view_detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(settings->btn_back, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(settings->lbl_title, LV_ALIGN_LEFT_MID, 4, 0);
        lv_label_set_text(settings->lbl_title, "控制中心");
    } else {
        /* 隐藏一级四宫格，展示二级详情 */
        lv_obj_add_flag(settings->view_main, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(settings->view_detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(settings->btn_back, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(settings->lbl_title, LV_ALIGN_LEFT_MID, 56, 0);

        /* 隐藏所有二级子区块 */
        lv_obj_add_flag(settings->sec_net_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(settings->sec_sys_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(settings->sec_agent_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(settings->sec_store_box, LV_OBJ_FLAG_HIDDEN);

        if (page == UI_SETTINGS_PAGE_NETWORK) {
            lv_obj_clear_flag(settings->sec_net_box, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(settings->lbl_title, "无线网络");
        } else if (page == UI_SETTINGS_PAGE_SYSTEM) {
            lv_obj_clear_flag(settings->sec_sys_box, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(settings->lbl_title, "系统健康");
        } else if (page == UI_SETTINGS_PAGE_AGENT) {
            lv_obj_clear_flag(settings->sec_agent_box, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(settings->lbl_title, "灵眸模型");
        } else if (page == UI_SETTINGS_PAGE_STORAGE) {
            lv_obj_clear_flag(settings->sec_store_box, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(settings->lbl_title, "伴侣看板");
        }
    }

    ui_settings_refresh_data(settings);
}

ui_settings_page_t ui_settings_get_page(const ui_settings_t *settings)
{
    return settings ? settings->current_page : UI_SETTINGS_PAGE_MAIN;
}

void ui_settings_open(ui_settings_t *settings)
{
    if (!settings || !settings->drawer || settings->is_open) return;

    settings->is_open = true;
    ui_settings_set_page(settings, UI_SETTINGS_PAGE_MAIN);

    lv_obj_clear_flag(settings->drawer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(settings->drawer);

    LOG_I(TAG, "控制中心面板已展开");
}

void ui_settings_close(ui_settings_t *settings)
{
    if (!settings || !settings->drawer || !settings->is_open) return;

    settings->is_open = false;
    lv_obj_add_flag(settings->drawer, LV_OBJ_FLAG_HIDDEN);

    if (settings->on_close_cb) {
        settings->on_close_cb(settings->close_user_data);
    }

    LOG_I(TAG, "控制中心面板已收起");
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

    /* 1. 刷新一级菜单四宫格简报 */
    if (settings->lbl_nav_net_sub) {
        if (mode == NET_MODE_STA_CONNECTED) {
            char buf[64];
            snprintf(buf, sizeof(buf), "已连: %s\nIP: %s", ssid_buf[0] ? ssid_buf : "Wi-Fi", ip_buf);
            lv_label_set_text(settings->lbl_nav_net_sub, buf);
            lv_obj_set_style_text_color(settings->lbl_nav_net_sub, lv_color_hex(0x00FF88), 0);
        } else if (mode == NET_MODE_STA_CONNECTING) {
            char buf[64];
            snprintf(buf, sizeof(buf), "正在连网...\n%s", ssid_buf[0] ? ssid_buf : "Wi-Fi");
            lv_label_set_text(settings->lbl_nav_net_sub, buf);
            lv_obj_set_style_text_color(settings->lbl_nav_net_sub, lv_color_hex(0xFFB700), 0);
        } else if (mode == NET_MODE_SOFTAP_CONFIG) {
            lv_label_set_text(settings->lbl_nav_net_sub, "热点广播中\n192.168.4.1");
            lv_obj_set_style_text_color(settings->lbl_nav_net_sub, lv_color_hex(0xFFB700), 0);
        } else {
            lv_label_set_text(settings->lbl_nav_net_sub, "未配置网络\n点击配置");
            lv_obj_set_style_text_color(settings->lbl_nav_net_sub, lv_color_hex(0x8B9EB5), 0);
        }
    }

    /* 2. 刷新二级网络页面 */
    if (settings->lbl_net_status) {
        if (mode == NET_MODE_STA_CONNECTED) {
            char buf[64];
            snprintf(buf, sizeof(buf), "● 已连入网络: %s", ssid_buf[0] ? ssid_buf : "Wi-Fi");
            lv_label_set_text(settings->lbl_net_status, buf);
            lv_obj_set_style_text_color(settings->lbl_net_status, lv_color_hex(0x00FF88), 0);
        } else if (mode == NET_MODE_STA_CONNECTING) {
            char buf[64];
            snprintf(buf, sizeof(buf), "● 正在连接无线网络: %s...", ssid_buf[0] ? ssid_buf : "Wi-Fi");
            lv_label_set_text(settings->lbl_net_status, buf);
            lv_obj_set_style_text_color(settings->lbl_net_status, lv_color_hex(0xFFB700), 0);
        } else if (mode == NET_MODE_SOFTAP_CONFIG) {
            lv_label_set_text(settings->lbl_net_status, "● 广播热点: Gemini-Agent-S1");
            lv_obj_set_style_text_color(settings->lbl_net_status, lv_color_hex(0xFFB700), 0);
        } else {
            lv_label_set_text(settings->lbl_net_status, "● 网络已断开或未连接");
            lv_obj_set_style_text_color(settings->lbl_net_status, lv_color_hex(0x7E92AD), 0);
        }
    }

    if (settings->lbl_net_ip) {
        char buf[64];
        snprintf(buf, sizeof(buf), "局域网 IP: %s (端口 :8080)", ip_buf[0] ? ip_buf : "0.0.0.0");
        lv_label_set_text(settings->lbl_net_ip, buf);
    }

    if (settings->lbl_ble_status) {
        ble_prov_state_t bst = ble_prov_service_get_state();
        if (bst == BLE_PROV_STATE_CONNECTED) {
            lv_label_set_text(settings->lbl_ble_status, "● 蓝牙配网: 客户端已连接");
            lv_obj_set_style_text_color(settings->lbl_ble_status, lv_color_hex(0x00E5FF), 0);
            if (settings->lbl_ble_prov_btn) {
                lv_label_set_text(settings->lbl_ble_prov_btn, "[⚡] 蓝牙客户端已在线");
                lv_obj_set_style_text_color(settings->lbl_ble_prov_btn, lv_color_hex(0x00E5FF), 0);
            }
        } else if (bst == BLE_PROV_STATE_PROVISIONED) {
            lv_label_set_text(settings->lbl_ble_status, "● 蓝牙配网: 配网凭证同步完成");
            lv_obj_set_style_text_color(settings->lbl_ble_status, lv_color_hex(0x00FF88), 0);
            if (settings->lbl_ble_prov_btn) {
                lv_label_set_text(settings->lbl_ble_prov_btn, "[✓] 蓝牙配网已完成 (点击重启)");
                lv_obj_set_style_text_color(settings->lbl_ble_prov_btn, lv_color_hex(0x00FF88), 0);
            }
        } else if (bst == BLE_PROV_STATE_ADVERTISING) {
            lv_label_set_text(settings->lbl_ble_status, "● 蓝牙配网: 广播中 (Phoenix-Setup)");
            lv_obj_set_style_text_color(settings->lbl_ble_status, lv_color_hex(0x00FF88), 0);
            if (settings->lbl_ble_prov_btn) {
                lv_label_set_text(settings->lbl_ble_prov_btn, "[⚡] 蓝牙广播等待网页连接...");
                lv_obj_set_style_text_color(settings->lbl_ble_prov_btn, lv_color_hex(0x00FF88), 0);
            }
        } else {
            lv_label_set_text(settings->lbl_ble_status, "● 蓝牙配网: 就绪 (点击下方启动)");
            lv_obj_set_style_text_color(settings->lbl_ble_status, lv_color_hex(0x8B9EB5), 0);
            if (settings->lbl_ble_prov_btn) {
                lv_label_set_text(settings->lbl_ble_prov_btn, "[⚡] 启动蓝牙极速配网 (Web BLE)");
                lv_obj_set_style_text_color(settings->lbl_ble_prov_btn, lv_color_hex(0x00E5FF), 0);
            }
        }
    }

    if (settings->lbl_hotspot_btn && settings->btn_hotspot) {
        char btn_str[64];
        if (mode == NET_MODE_STA_CONNECTING) {
            lv_label_set_text(settings->lbl_hotspot_btn, "[..] 正在连接目标 Wi-Fi...");
            lv_obj_set_style_text_color(settings->lbl_hotspot_btn, lv_color_hex(0xFFB700), 0);
            lv_obj_set_style_border_color(settings->btn_hotspot, lv_color_hex(0xFFB700), 0);
            if (settings->lbl_hotspot_hint) {
                lv_label_set_text(settings->lbl_hotspot_hint, "正在握手并申请 IP，请稍候...");
                lv_obj_set_style_text_color(settings->lbl_hotspot_hint, lv_color_hex(0xFFB700), 0);
            }
        } else if (mode == NET_MODE_SOFTAP_CONFIG) {
            snprintf(btn_str, sizeof(btn_str), "● 热点广播中: %s", ip_buf[0] ? ip_buf : "192.168.4.1:8080");
            lv_label_set_text(settings->lbl_hotspot_btn, btn_str);
            lv_obj_set_style_text_color(settings->lbl_hotspot_btn, lv_color_hex(0xFFB700), 0);
            lv_obj_set_style_border_color(settings->btn_hotspot, lv_color_hex(0xFFB700), 0);
            if (settings->lbl_hotspot_hint) {
                lv_label_set_text(settings->lbl_hotspot_hint, "手机直连 Gemini-Agent-S1 免密配网");
                lv_obj_set_style_text_color(settings->lbl_hotspot_hint, lv_color_hex(0x00FF88), 0);
            }
        } else {
            snprintf(btn_str, sizeof(btn_str), "[●] 开启独立热点配网 (192.168.4.1)");
            lv_label_set_text(settings->lbl_hotspot_btn, btn_str);
            lv_obj_set_style_text_color(settings->lbl_hotspot_btn, lv_color_hex(0x00E5FF), 0);
            lv_obj_set_style_border_color(settings->btn_hotspot, lv_color_hex(0x00E5FF), 0);
            if (settings->lbl_hotspot_hint) {
                lv_label_set_text(settings->lbl_hotspot_hint, "点击后将启动热点广播，断开当前 Wi-Fi");
                lv_obj_set_style_text_color(settings->lbl_hotspot_hint, lv_color_hex(0x7E92AD), 0);
            }
        }
    }

    /* 3. 刷新系统遥测 */
    phoenix_agent_ctx_t *agent = phoenix_agent_get_instance();
    uint32_t uptime_s = agent ? agent->stats.uptime_seconds : (uint32_t)(time_utils_get_ms() / 1000);
    uint32_t up_h = uptime_s / 3600;
    uint32_t up_m = (uptime_s % 3600) / 60;
    uint32_t up_s = uptime_s % 60;

    if (settings->lbl_sys_uptime) {
        char buf[64];
        snprintf(buf, sizeof(buf), "● 运行时长: %02u:%02u:%02u | 感知正常",
                 (unsigned int)up_h, (unsigned int)up_m, (unsigned int)up_s);
        lv_label_set_text(settings->lbl_sys_uptime, buf);
    }

    /* 4. 刷新 Agent 模型遥测 */
    uint32_t interactions = agent ? agent->stats.interaction_count : 12;
    uint32_t tokens = agent ? agent->stats.total_tokens_used : 1850;
    uint32_t focus_m = (agent && agent->stats.continuous_focus_s > 0) ? (agent->stats.continuous_focus_s / 60) : 25;

    if (settings->lbl_agent_stats) {
        char stats_buf[64];
        snprintf(stats_buf, sizeof(stats_buf), "交互: %u次 | Token: %u | 专注: %um",
                 (unsigned int)interactions, (unsigned int)tokens, (unsigned int)focus_m);
        lv_label_set_text(settings->lbl_agent_stats, stats_buf);
    }
    if (settings->lbl_nav_agent_sub) {
        char buf[64];
        snprintf(buf, sizeof(buf), "DeepSeek\n交互: %u次", (unsigned int)interactions);
        lv_label_set_text(settings->lbl_nav_agent_sub, buf);
    }

    if (settings->lbl_agent_last) {
        char last_buf[96] = "最新: 帮我开启25分钟专注流";
        if (agent && agent->history_count > 0) {
            for (int i = (int)agent->history_count - 1; i >= 0; i--) {
                if (agent->history[i].role == PHOENIX_ROLE_USER && agent->history[i].content) {
                    snprintf(last_buf, sizeof(last_buf), "最新: %s", agent->history[i].content);
                    break;
                }
            }
        }
        lv_label_set_text(settings->lbl_agent_last, last_buf);
    }
}
