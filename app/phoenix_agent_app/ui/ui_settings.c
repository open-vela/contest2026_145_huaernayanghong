/**
 * @file ui_settings.c
 * @brief 设置与控制中心同级视图组件实现 (Peer Stage Settings & Sub-Sidebar Navigation)
 * @author OpenVela Contest 2026 Team 145
 */

#include "ui_settings.h"
#include "ui_font.h"
#include "../core/config.h"
#include "../core/audio_test_service.h"
#include "../hal/network_mgr.h"
#include "../hal/ble_prov_service.h"
#include "../hal/hal_system.h"
#include "../hal/hal_sdcard.h"
#include "../hal/hal_manager.h"
#include "../hal/hal_actuator.h"
#include "../utils/log_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TAG "UI:Settings"

/* 私有事件回调声明 */
static void on_top_close_clicked(lv_event_t *e);
static void on_menu_item_clicked(lv_event_t *e);
static void on_top_tab_btn_clicked(lv_event_t *e);
static void on_sub_tab_btn_clicked(lv_event_t *e);
static void on_hotspot_action_clicked(lv_event_t *e);
static void on_hotspot_reset_clicked(lv_event_t *e);
static void on_ble_toggle_clicked(lv_event_t *e);
static void on_wifi_refresh_clicked(lv_event_t *e);
static void on_wifi_forget_clicked(lv_event_t *e);
static void on_wifi_item_clicked(lv_event_t *e);
static void on_wifi_ap_mode_clicked(lv_event_t *e);
static void on_pwd_close_clicked(lv_event_t *e);
static void on_pwd_connect_clicked(lv_event_t *e);
static void on_pwd_eye_clicked(lv_event_t *e);
static void on_pwd_kb_event(lv_event_t *e);

/* 音频调试事件回调声明 */
static void on_audio_rec_clicked(lv_event_t *e);
static void on_audio_play_rec_clicked(lv_event_t *e);
static void on_audio_play_tone_clicked(lv_event_t *e);
static void on_audio_loopback_changed(lv_event_t *e);
static void on_audio_vol_slider_changed(lv_event_t *e);

/* ========================================================================= */
/*                              生命周期接口                                 */
/* ========================================================================= */

ui_settings_t* ui_settings_create(lv_obj_t *parent, const lv_font_t *font)
{
    if (!parent) return NULL;

    ui_settings_t *s = (ui_settings_t *)calloc(1, sizeof(ui_settings_t));
    if (!s) return NULL;

    s->font = font;
    s->is_open = false;
    s->is_in_detail = false;
    s->current_tab = UI_SETTINGS_TAB_NET;

    /* 1. 设置主舞台容器 (全宽对等主舞台: W=274, H=216, X=46, Y=24) */
    s->container = lv_obj_create(parent);
    s->drawer = s->container; /* 兼容字段 */
    lv_obj_set_size(s->container, 274, 216);
    lv_obj_set_pos(s->container, 46, 24);
    lv_obj_set_style_bg_color(s->container, lv_color_hex(0x060A12), 0);
    lv_obj_set_style_bg_opa(s->container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s->container, lv_color_hex(0x152238), 0);
    lv_obj_set_style_border_width(s->container, 1, 0);
    lv_obj_set_style_radius(s->container, 0, 0);
    lv_obj_set_style_pad_all(s->container, 0, 0);
    lv_obj_clear_flag(s->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->container, LV_OBJ_FLAG_HIDDEN);

    /* 2. 顶栏导航条 (高 30px, 宽 274px, x=0, y=0) */
    s->top_tab_bar = lv_obj_create(s->container);
    s->sub_sidebar = s->top_tab_bar; /* 兼容字段 */
    s->header_bar = s->top_tab_bar;  /* 兼容字段 */
    lv_obj_set_size(s->top_tab_bar, 274, 30);
    lv_obj_set_pos(s->top_tab_bar, 0, 0);
    lv_obj_set_style_bg_color(s->top_tab_bar, lv_color_hex(0x09101C), 0);
    lv_obj_set_style_bg_opa(s->top_tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s->top_tab_bar, lv_color_hex(0x16263D), 0);
    lv_obj_set_style_border_width(s->top_tab_bar, 1, 0);
    lv_obj_set_style_border_side(s->top_tab_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_radius(s->top_tab_bar, 0, 0);
    lv_obj_set_style_pad_all(s->top_tab_bar, 2, 0);
    lv_obj_clear_flag(s->top_tab_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* 2.1 顶栏左侧动态按钮 (主菜单显示 ✕ 退出，二级详情显示 < 返回) */
    s->btn_top_close = lv_btn_create(s->top_tab_bar);
    lv_obj_set_size(s->btn_top_close, 32, 24);
    lv_obj_set_pos(s->btn_top_close, 2, 1);
    lv_obj_set_style_bg_color(s->btn_top_close, lv_color_hex(0x182436), 0);
    lv_obj_set_style_border_color(s->btn_top_close, lv_color_hex(0x2B4263), 0);
    lv_obj_set_style_border_width(s->btn_top_close, 1, 0);
    lv_obj_set_style_radius(s->btn_top_close, 6, 0);
    lv_obj_set_style_pad_all(s->btn_top_close, 0, 0);
    lv_obj_add_event_cb(s->btn_top_close, on_top_close_clicked, LV_EVENT_CLICKED, s);

    s->lbl_top_close = lv_label_create(s->btn_top_close);
    lv_obj_center(s->lbl_top_close);
    if (s->font) lv_obj_set_style_text_font(s->lbl_top_close, s->font, 0);
    lv_label_set_text(s->lbl_top_close, "X");
    lv_obj_set_style_text_color(s->lbl_top_close, lv_color_hex(0x00E5FF), 0);

    /* 2.2 顶栏标题 (主菜单显示 系统设置，详情页显示对应名称) */
    s->lbl_top_title = lv_label_create(s->top_tab_bar);
    lv_obj_align(s->lbl_top_title, LV_ALIGN_CENTER, 0, 0);
    if (s->font) lv_obj_set_style_text_font(s->lbl_top_title, s->font, 0);
    lv_label_set_text(s->lbl_top_title, "系统设置");
    lv_obj_set_style_text_color(s->lbl_top_title, lv_color_hex(0x00E5FF), 0);
    s->lbl_header_title = s->lbl_top_title; /* 兼容字段 */

    /* 3. 主体内容区 (彻底释放为 274px 全宽, 高 186px, x=0, y=30) */
    s->content_area = lv_obj_create(s->container);
    lv_obj_set_size(s->content_area, 274, 186);
    lv_obj_set_pos(s->content_area, 0, 30);
    lv_obj_set_style_bg_opa(s->content_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->content_area, 0, 0);
    lv_obj_set_style_pad_all(s->content_area, 0, 0);
    lv_obj_clear_flag(s->content_area, LV_OBJ_FLAG_SCROLLABLE);

    /* =====================================================================
     * 3.1 视图 1: 单列垂直卡片主菜单列表 (无限纵向扩展, 宽 274px, 高 186px)
     * ===================================================================== */
    s->view_menu_list = lv_obj_create(s->content_area);
    lv_obj_set_size(s->view_menu_list, 274, 186);
    lv_obj_set_pos(s->view_menu_list, 0, 0);
    lv_obj_set_style_bg_opa(s->view_menu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->view_menu_list, 0, 0);
    lv_obj_set_style_pad_all(s->view_menu_list, 6, 0);
    lv_obj_add_flag(s->view_menu_list, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建 7 个菜单项卡片 (高度 34px, 宽度 260px，使用字库安全符号) */
    const char *menu_names[7] = {
        "● Wi-Fi网络",
        "● 蓝牙配网",
        "● 灵眸模型",
        "● 硬件状态",
        "● 存储日志",
        "● 关于设备",
        "● 音频调试"
    };
    const char *menu_defaults[7] = {
        "已连接 >",
        "未开启 >",
        "DeepSeek >",
        "60FPS / 正常 >",
        "28.6GB >",
        "R528-S3 >",
        "录放音测试 >"
    };
    lv_obj_t **menu_btns[7] = {
        &s->btn_menu_net,
        &s->btn_menu_ble,
        &s->btn_menu_agent,
        &s->btn_menu_system,
        &s->btn_menu_storage,
        &s->btn_menu_about,
        &s->btn_menu_audio
    };
    lv_obj_t **menu_subs[7] = {
        &s->lbl_menu_net_sub,
        &s->lbl_menu_ble_sub,
        &s->lbl_menu_agent_sub,
        &s->lbl_menu_system_sub,
        &s->lbl_menu_storage_sub,
        &s->lbl_menu_about_sub,
        &s->lbl_menu_audio_sub
    };

    for (int i = 0; i < 7; i++) {
        lv_obj_t *b = lv_btn_create(s->view_menu_list);
        lv_obj_set_size(b, 260, 34);
        lv_obj_set_pos(b, 1, (lv_coord_t)(i * 38));
        lv_obj_set_style_bg_color(b, lv_color_hex(0x0E1726), 0);
        lv_obj_set_style_border_color(b, lv_color_hex(0x1C2F4D), 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_set_style_pad_all(b, 4, 0);
        lv_obj_set_ext_click_area(b, 4);
        lv_obj_add_event_cb(b, on_menu_item_clicked, LV_EVENT_CLICKED, s);

        /* 左侧标题 */
        lv_obj_t *lbl_title = lv_label_create(b);
        lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 6, 0);
        if (s->font) lv_obj_set_style_text_font(lbl_title, s->font, 0);
        lv_label_set_text(lbl_title, menu_names[i]);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xE6EDF3), 0);

        /* 右侧状态摘要 */
        lv_obj_t *lbl_sub = lv_label_create(b);
        lv_obj_align(lbl_sub, LV_ALIGN_RIGHT_MID, -6, 0);
        if (s->font) lv_obj_set_style_text_font(lbl_sub, s->font, 0);
        lv_label_set_text(lbl_sub, menu_defaults[i]);
        lv_obj_set_style_text_color(lbl_sub, (i == 0 || i == 1 || i == 3 || i == 6) ? lv_color_hex(0x00FF88) : lv_color_hex(0x00E5FF), 0);

        *menu_btns[i] = b;
        *menu_subs[i] = lbl_sub;
    }

    /* 兼容历史字段指针绑定 */
    s->btn_tab_net = s->btn_menu_net;
    s->lbl_tab_net = s->lbl_menu_net_sub;
    s->btn_tab_ble = s->btn_menu_ble;
    s->lbl_tab_ble = s->lbl_menu_ble_sub;
    s->btn_tab_agent = s->btn_menu_agent;
    s->lbl_tab_agent = s->lbl_menu_agent_sub;
    s->btn_tab_system = s->btn_menu_system;
    s->lbl_tab_system = s->lbl_menu_system_sub;
    s->btn_tab_storage = s->btn_menu_storage;
    s->lbl_tab_storage = s->lbl_menu_storage_sub;
    s->btn_tab_hotspot = s->btn_menu_net;
    s->lbl_tab_hotspot = s->lbl_menu_net_sub;

    /* =====================================================================
     * 3.2 视图 2: 二级下钻详情区域 (宽 274px, 高 186px, 默认隐藏)
     * ===================================================================== */
    s->view_detail_area = lv_obj_create(s->content_area);
    s->body_area = s->view_detail_area; /* 兼容字段 */
    lv_obj_set_size(s->view_detail_area, 274, 186);
    lv_obj_set_pos(s->view_detail_area, 0, 0);
    lv_obj_set_style_bg_opa(s->view_detail_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->view_detail_area, 0, 0);
    lv_obj_set_style_pad_all(s->view_detail_area, 4, 0);
    lv_obj_clear_flag(s->view_detail_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->view_detail_area, LV_OBJ_FLAG_HIDDEN);

    /* =====================================================================
     * 4. 统一 Wi-Fi 网络直连与搜索面板 (Native UI Search & Connect)
     * ===================================================================== */
    s->panel_wifi = lv_obj_create(s->body_area);
    s->panel_hotspot = s->panel_wifi; /* 兼容历史别名 */
    lv_obj_set_size(s->panel_wifi, 266, 176);
    lv_obj_align(s->panel_wifi, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_wifi, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_wifi, 0, 0);
    lv_obj_set_style_pad_all(s->panel_wifi, 0, 0);
    lv_obj_clear_flag(s->panel_wifi, LV_OBJ_FLAG_SCROLLABLE);

    /* 4.1 顶部状态与刷新操作栏 (高 26px) */
    s->box_wifi_header = lv_obj_create(s->panel_wifi);
    lv_obj_set_size(s->box_wifi_header, 260, 26);
    lv_obj_align(s->box_wifi_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->box_wifi_header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->box_wifi_header, 0, 0);
    lv_obj_set_style_pad_all(s->box_wifi_header, 0, 0);
    lv_obj_clear_flag(s->box_wifi_header, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_wifi_status = lv_label_create(s->box_wifi_header);
    lv_obj_align(s->lbl_wifi_status, LV_ALIGN_LEFT_MID, 2, 0);
    if (s->font) lv_obj_set_style_text_font(s->lbl_wifi_status, s->font, 0);
    lv_label_set_text(s->lbl_wifi_status, "📶 未连接网络");
    lv_obj_set_style_text_color(s->lbl_wifi_status, lv_color_hex(0x7E92AD), 0);

    s->btn_wifi_refresh = lv_btn_create(s->box_wifi_header);
    lv_obj_set_size(s->btn_wifi_refresh, 58, 24);
    lv_obj_align(s->btn_wifi_refresh, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(s->btn_wifi_refresh, lv_color_hex(0x13273F), 0);
    lv_obj_set_style_border_color(s->btn_wifi_refresh, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->btn_wifi_refresh, 1, 0);
    lv_obj_set_style_radius(s->btn_wifi_refresh, 4, 0);
    lv_obj_add_event_cb(s->btn_wifi_refresh, on_wifi_refresh_clicked, LV_EVENT_CLICKED, s);

    s->lbl_wifi_refresh = lv_label_create(s->btn_wifi_refresh);
    lv_obj_center(s->lbl_wifi_refresh);
    if (s->font) lv_obj_set_style_text_font(s->lbl_wifi_refresh, s->font, 0);
    lv_label_set_text(s->lbl_wifi_refresh, "刷新");
    lv_obj_set_style_text_color(s->lbl_wifi_refresh, lv_color_hex(0x00E5FF), 0);

    /* 4.2 Wi-Fi 列表滚动视窗 (宽 260px, 高 116px, 纵向平滑滚动) */
    s->list_wifi = lv_obj_create(s->panel_wifi);
    lv_obj_set_size(s->list_wifi, 260, 116);
    lv_obj_align(s->list_wifi, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_bg_color(s->list_wifi, lv_color_hex(0x080E18), 0);
    lv_obj_set_style_border_color(s->list_wifi, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->list_wifi, 1, 0);
    lv_obj_set_style_radius(s->list_wifi, 6, 0);
    lv_obj_set_style_pad_all(s->list_wifi, 3, 0);
    lv_obj_set_scroll_dir(s->list_wifi, LV_DIR_VER);
    lv_obj_set_flex_flow(s->list_wifi, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s->list_wifi, 4, 0);

    s->lbl_wifi_empty = lv_label_create(s->list_wifi);
    lv_obj_align(s->lbl_wifi_empty, LV_ALIGN_CENTER, 0, 0);
    if (s->font) lv_obj_set_style_text_font(s->lbl_wifi_empty, s->font, 0);
    lv_label_set_text(s->lbl_wifi_empty, "📡 正在搜索周边 Wi-Fi...");
    lv_obj_set_style_text_color(s->lbl_wifi_empty, lv_color_hex(0x7E92AD), 0);

    /* 4.3 底部辅助操作栏 (高 26px) */
    s->box_wifi_footer = lv_obj_create(s->panel_wifi);
    lv_obj_set_size(s->box_wifi_footer, 260, 26);
    lv_obj_align(s->box_wifi_footer, LV_ALIGN_TOP_MID, 0, 146);
    lv_obj_set_style_bg_opa(s->box_wifi_footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->box_wifi_footer, 0, 0);
    lv_obj_set_style_pad_all(s->box_wifi_footer, 0, 0);
    lv_obj_clear_flag(s->box_wifi_footer, LV_OBJ_FLAG_SCROLLABLE);

    s->btn_wifi_forget = lv_btn_create(s->box_wifi_footer);
    lv_obj_set_size(s->btn_wifi_forget, 126, 24);
    lv_obj_align(s->btn_wifi_forget, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(s->btn_wifi_forget, lv_color_hex(0x151B27), 0);
    lv_obj_set_style_border_color(s->btn_wifi_forget, lv_color_hex(0x28384E), 0);
    lv_obj_set_style_border_width(s->btn_wifi_forget, 1, 0);
    lv_obj_set_style_radius(s->btn_wifi_forget, 4, 0);
    lv_obj_add_event_cb(s->btn_wifi_forget, on_wifi_forget_clicked, LV_EVENT_CLICKED, s);

    s->lbl_wifi_forget = lv_label_create(s->btn_wifi_forget);
    lv_obj_center(s->lbl_wifi_forget);
    if (s->font) lv_obj_set_style_text_font(s->lbl_wifi_forget, s->font, 0);
    lv_label_set_text(s->lbl_wifi_forget, "清空配置");
    lv_obj_set_style_text_color(s->lbl_wifi_forget, lv_color_hex(0x7E92AD), 0);

    s->btn_wifi_ap_mode = lv_btn_create(s->box_wifi_footer);
    lv_obj_set_size(s->btn_wifi_ap_mode, 126, 24);
    lv_obj_align(s->btn_wifi_ap_mode, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(s->btn_wifi_ap_mode, lv_color_hex(0x151B27), 0);
    lv_obj_set_style_border_color(s->btn_wifi_ap_mode, lv_color_hex(0x28384E), 0);
    lv_obj_set_style_border_width(s->btn_wifi_ap_mode, 1, 0);
    lv_obj_set_style_radius(s->btn_wifi_ap_mode, 4, 0);
    lv_obj_add_event_cb(s->btn_wifi_ap_mode, on_wifi_ap_mode_clicked, LV_EVENT_CLICKED, s);

    s->lbl_wifi_ap_mode = lv_label_create(s->btn_wifi_ap_mode);
    lv_obj_center(s->lbl_wifi_ap_mode);
    if (s->font) lv_obj_set_style_text_font(s->lbl_wifi_ap_mode, s->font, 0);
    lv_label_set_text(s->lbl_wifi_ap_mode, "热点模式");
    lv_obj_set_style_text_color(s->lbl_wifi_ap_mode, lv_color_hex(0x7E92AD), 0);

    /* =====================================================================
     * 4.4 全屏密码输入模态对话框与 LVGL 软键盘 (320x240, 顶层悬浮)
     * ===================================================================== */
    s->dlg_pwd_modal = lv_obj_create(parent);
    lv_obj_set_size(s->dlg_pwd_modal, 320, 240);
    lv_obj_set_pos(s->dlg_pwd_modal, 0, 0);
    lv_obj_set_style_bg_color(s->dlg_pwd_modal, lv_color_hex(0x060A12), 0);
    lv_obj_set_style_bg_opa(s->dlg_pwd_modal, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s->dlg_pwd_modal, 0, 0);
    lv_obj_set_style_pad_all(s->dlg_pwd_modal, 0, 0);
    lv_obj_clear_flag(s->dlg_pwd_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->dlg_pwd_modal, LV_OBJ_FLAG_HIDDEN);

    /* 顶部标题与关闭 (0~28px) */
    s->lbl_pwd_target = lv_label_create(s->dlg_pwd_modal);
    lv_obj_align(s->lbl_pwd_target, LV_ALIGN_TOP_LEFT, 10, 6);
    if (s->font) lv_obj_set_style_text_font(s->lbl_pwd_target, s->font, 0);
    lv_label_set_text(s->lbl_pwd_target, "📶 连接到 Wi-Fi");
    lv_obj_set_style_text_color(s->lbl_pwd_target, lv_color_hex(0x00E5FF), 0);

    s->btn_pwd_close = lv_btn_create(s->dlg_pwd_modal);
    lv_obj_set_size(s->btn_pwd_close, 36, 24);
    lv_obj_align(s->btn_pwd_close, LV_ALIGN_TOP_RIGHT, -6, 4);
    lv_obj_set_style_bg_color(s->btn_pwd_close, lv_color_hex(0x2A1515), 0);
    lv_obj_set_style_border_color(s->btn_pwd_close, lv_color_hex(0xFF5555), 0);
    lv_obj_set_style_border_width(s->btn_pwd_close, 1, 0);
    lv_obj_set_style_radius(s->btn_pwd_close, 4, 0);
    lv_obj_add_event_cb(s->btn_pwd_close, on_pwd_close_clicked, LV_EVENT_CLICKED, s);

    s->lbl_pwd_close = lv_label_create(s->btn_pwd_close);
    lv_obj_center(s->lbl_pwd_close);
    lv_label_set_text(s->lbl_pwd_close, "X");
    lv_obj_set_style_text_color(s->lbl_pwd_close, lv_color_hex(0xFF8888), 0);

    /* 输入栏: textarea (宽 190) + 眼睛 (宽 36) + 连接 (宽 66) */
    s->ta_pwd_input = lv_textarea_create(s->dlg_pwd_modal);
    lv_obj_set_size(s->ta_pwd_input, 190, 32);
    lv_obj_set_pos(s->ta_pwd_input, 8, 32);
    lv_textarea_set_password_mode(s->ta_pwd_input, false);
    lv_textarea_set_one_line(s->ta_pwd_input, true);
    lv_textarea_set_max_length(s->ta_pwd_input, 63);
    lv_textarea_set_placeholder_text(s->ta_pwd_input, "输入密码");
    lv_obj_set_style_bg_color(s->ta_pwd_input, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->ta_pwd_input, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->ta_pwd_input, 1, 0);
    lv_obj_set_style_radius(s->ta_pwd_input, 4, 0);
    lv_obj_set_style_text_color(s->ta_pwd_input, lv_color_hex(0xFFFFFF), 0);
    s->is_pwd_obscure = false;

    s->btn_pwd_eye = lv_btn_create(s->dlg_pwd_modal);
    lv_obj_set_size(s->btn_pwd_eye, 36, 32);
    lv_obj_set_pos(s->btn_pwd_eye, 204, 32);
    lv_obj_set_style_bg_color(s->btn_pwd_eye, lv_color_hex(0x13273F), 0);
    lv_obj_set_style_border_color(s->btn_pwd_eye, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->btn_pwd_eye, 1, 0);
    lv_obj_set_style_radius(s->btn_pwd_eye, 4, 0);
    lv_obj_add_event_cb(s->btn_pwd_eye, on_pwd_eye_clicked, LV_EVENT_CLICKED, s);

    s->lbl_pwd_eye = lv_label_create(s->btn_pwd_eye);
    lv_obj_center(s->lbl_pwd_eye);
    lv_label_set_text(s->lbl_pwd_eye, "明");
    if (s->font) lv_obj_set_style_text_font(s->lbl_pwd_eye, s->font, 0);
    lv_obj_set_style_text_color(s->lbl_pwd_eye, lv_color_hex(0x00E5FF), 0);

    s->btn_pwd_connect = lv_btn_create(s->dlg_pwd_modal);
    lv_obj_set_size(s->btn_pwd_connect, 66, 32);
    lv_obj_set_pos(s->btn_pwd_connect, 246, 32);
    lv_obj_set_style_bg_color(s->btn_pwd_connect, lv_color_hex(0x007ACC), 0);
    lv_obj_set_style_border_color(s->btn_pwd_connect, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->btn_pwd_connect, 1, 0);
    lv_obj_set_style_radius(s->btn_pwd_connect, 4, 0);
    lv_obj_add_event_cb(s->btn_pwd_connect, on_pwd_connect_clicked, LV_EVENT_CLICKED, s);

    s->lbl_pwd_connect = lv_label_create(s->btn_pwd_connect);
    lv_obj_center(s->lbl_pwd_connect);
    if (s->font) lv_obj_set_style_text_font(s->lbl_pwd_connect, s->font, 0);
    lv_label_set_text(s->lbl_pwd_connect, "连接");
    lv_obj_set_style_text_color(s->lbl_pwd_connect, lv_color_hex(0xFFFFFF), 0);

    /* 提示信息标签 (高 18px) */
    s->lbl_pwd_hint = lv_label_create(s->dlg_pwd_modal);
    lv_obj_align(s->lbl_pwd_hint, LV_ALIGN_TOP_LEFT, 10, 68);
    if (s->font) lv_obj_set_style_text_font(s->lbl_pwd_hint, s->font, 0);
    lv_label_set_text(s->lbl_pwd_hint, "请输入 Wi-Fi 密码 (不少于8位)");
    lv_obj_set_style_text_color(s->lbl_pwd_hint, lv_color_hex(0x8B9EB5), 0);

    /* 底部软键盘 lv_keyboard (宽 320, 高 144, 贴底) */
    s->kb_pwd = lv_keyboard_create(s->dlg_pwd_modal);
    lv_obj_set_size(s->kb_pwd, 320, 144);
    lv_obj_align(s->kb_pwd, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(s->kb_pwd, s->ta_pwd_input);
    lv_obj_add_event_cb(s->kb_pwd, on_pwd_kb_event, LV_EVENT_ALL, s);

    /* =====================================================================
     * 5. 独立标签页 2: 蓝牙配网 (兼容保留)
     * ===================================================================== */
    s->panel_ble = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_ble, 266, 176);
    lv_obj_align(s->panel_ble, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_ble, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_ble, 0, 0);
    lv_obj_set_style_pad_all(s->panel_ble, 0, 0);
    lv_obj_clear_flag(s->panel_ble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->panel_ble, LV_OBJ_FLAG_HIDDEN);

    s->card_ble_info = lv_obj_create(s->panel_ble);
    lv_obj_set_size(s->card_ble_info, 260, 80);
    lv_obj_align(s->card_ble_info, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->card_ble_info, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->card_ble_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_ble_info, 1, 0);
    lv_obj_set_style_radius(s->card_ble_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_ble_info, 4, 0);
    lv_obj_clear_flag(s->card_ble_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_ble_status = lv_label_create(s->card_ble_info);
    lv_obj_align(s->lbl_ble_status, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_status, s->font, 0);
    lv_label_set_text(s->lbl_ble_status, "● 蓝牙状态: 未开启 (点击启动)");
    lv_obj_set_style_text_color(s->lbl_ble_status, lv_color_hex(0x7E92AD), 0);

    s->lbl_ble_dev_name = lv_label_create(s->card_ble_info);
    lv_obj_align(s->lbl_ble_dev_name, LV_ALIGN_TOP_LEFT, 4, 24);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_dev_name, s->font, 0);
    lv_label_set_text(s->lbl_ble_dev_name, "广播设备名: Phoenix-Setup");
    lv_obj_set_style_text_color(s->lbl_ble_dev_name, lv_color_hex(0x00E5FF), 0);

    s->lbl_ble_uuid = lv_label_create(s->card_ble_info);
    lv_obj_align(s->lbl_ble_uuid, LV_ALIGN_TOP_LEFT, 4, 46);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_uuid, s->font, 0);
    lv_label_set_text(s->lbl_ble_uuid, "GATT 配网服务 UUID: 0xFFE0");
    lv_obj_set_style_text_color(s->lbl_ble_uuid, lv_color_hex(0x8B9EB5), 0);

    /* 启动/停止蓝牙广播按钮 */
    s->btn_ble_toggle = lv_btn_create(s->panel_ble);
    lv_obj_set_size(s->btn_ble_toggle, 260, 36);
    lv_obj_align(s->btn_ble_toggle, LV_ALIGN_TOP_MID, 0, 88);
    lv_obj_set_style_bg_color(s->btn_ble_toggle, lv_color_hex(0x13273F), 0);
    lv_obj_set_style_border_color(s->btn_ble_toggle, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->btn_ble_toggle, 1, 0);
    lv_obj_set_style_radius(s->btn_ble_toggle, 6, 0);
    lv_obj_add_event_cb(s->btn_ble_toggle, on_ble_toggle_clicked, LV_EVENT_CLICKED, s);

    s->lbl_ble_toggle = lv_label_create(s->btn_ble_toggle);
    lv_obj_center(s->lbl_ble_toggle);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_toggle, s->font, 0);
    lv_label_set_text(s->lbl_ble_toggle, "[⚡ 启动蓝牙极速配网]");
    lv_obj_set_style_text_color(s->lbl_ble_toggle, lv_color_hex(0x00E5FF), 0);

    /* 蓝牙操作指引卡片 */
    s->card_ble_guide = lv_obj_create(s->panel_ble);
    lv_obj_set_size(s->card_ble_guide, 260, 48);
    lv_obj_align(s->card_ble_guide, LV_ALIGN_TOP_MID, 0, 126);
    lv_obj_set_style_bg_color(s->card_ble_guide, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_border_color(s->card_ble_guide, lv_color_hex(0x1A283D), 0);
    lv_obj_set_style_border_width(s->card_ble_guide, 1, 0);
    lv_obj_set_style_radius(s->card_ble_guide, 6, 0);
    lv_obj_set_style_pad_all(s->card_ble_guide, 4, 0);
    lv_obj_clear_flag(s->card_ble_guide, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_ble_guide = lv_label_create(s->card_ble_guide);
    lv_obj_align(s->lbl_ble_guide, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_guide, s->font, 0);
    lv_label_set_text(s->lbl_ble_guide, "1. 浏览器打开 /ble_setup 极速直连\n2. 手机蓝牙靠近 Phoenix-Setup 自动识别");
    lv_obj_set_style_text_color(s->lbl_ble_guide, lv_color_hex(0x7E92AD), 0);

    /* =====================================================================
     * 6. 独立标签页 2 (UI_SETTINGS_TAB_AGENT): AI 大模型
     * ===================================================================== */
    s->panel_agent = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_agent, 266, 176);
    lv_obj_align(s->panel_agent, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_agent, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_agent, 0, 0);
    lv_obj_set_style_pad_all(s->panel_agent, 0, 0);
    lv_obj_clear_flag(s->panel_agent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->panel_agent, LV_OBJ_FLAG_HIDDEN);

    s->card_agent_info = lv_obj_create(s->panel_agent);
    lv_obj_set_size(s->card_agent_info, 260, 172);
    lv_obj_align(s->card_agent_info, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->card_agent_info, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->card_agent_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_agent_info, 1, 0);
    lv_obj_set_style_radius(s->card_agent_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_agent_info, 6, 0);
    lv_obj_clear_flag(s->card_agent_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_agent_model = lv_label_create(s->card_agent_info);
    lv_obj_align(s->lbl_agent_model, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_model, s->font, 0);
    lv_label_set_text(s->lbl_agent_model, "端云大模型: DeepSeek-V3/R1");
    lv_obj_set_style_text_color(s->lbl_agent_model, lv_color_hex(0x00FF88), 0);

    s->lbl_agent_key_st = lv_label_create(s->card_agent_info);
    lv_obj_align(s->lbl_agent_key_st, LV_ALIGN_TOP_LEFT, 4, 26);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_key_st, s->font, 0);
    lv_label_set_text(s->lbl_agent_key_st, "API Key: [已加载持久化密钥]");
    lv_obj_set_style_text_color(s->lbl_agent_key_st, lv_color_hex(0x00E5FF), 0);

    s->lbl_agent_prompt = lv_label_create(s->card_agent_info);
    lv_obj_align(s->lbl_agent_prompt, LV_ALIGN_TOP_LEFT, 4, 50);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_prompt, s->font, 0);
    lv_label_set_text(s->lbl_agent_prompt, "人设: 贴心极客使魔 (Cyber-Familiar)\n支持敲击感应/双敲唤醒大模型对话");
    lv_obj_set_style_text_color(s->lbl_agent_prompt, lv_color_hex(0x8B9EB5), 0);

    s->lbl_agent_hint = lv_label_create(s->panel_agent);
    lv_obj_set_pos(s->lbl_agent_hint, 6, 142);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_hint, s->font, 0);
    lv_label_set_text(s->lbl_agent_hint, "● 浏览器访问 :8080/ 修改密钥与Prompt");
    lv_obj_set_style_text_color(s->lbl_agent_hint, lv_color_hex(0xFFB700), 0);

    /* =====================================================================
     * 7. 独立标签页 3 (UI_SETTINGS_TAB_SYSTEM): 系统健康与遥测
     * ===================================================================== */
    s->panel_system = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_system, 266, 176);
    lv_obj_align(s->panel_system, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_system, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_system, 0, 0);
    lv_obj_set_style_pad_all(s->panel_system, 0, 0);
    lv_obj_clear_flag(s->panel_system, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->panel_system, LV_OBJ_FLAG_HIDDEN);

    s->card_system_info = lv_obj_create(s->panel_system);
    lv_obj_set_size(s->card_system_info, 260, 172);
    lv_obj_align(s->card_system_info, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->card_system_info, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->card_system_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_system_info, 1, 0);
    lv_obj_set_style_radius(s->card_system_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_system_info, 6, 0);
    lv_obj_clear_flag(s->card_system_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_system_uptime = lv_label_create(s->card_system_info);
    lv_obj_align(s->lbl_system_uptime, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_system_uptime, s->font, 0);
    lv_label_set_text(s->lbl_system_uptime, "运行时长: 00:00:00");
    lv_obj_set_style_text_color(s->lbl_system_uptime, lv_color_hex(0x00FF88), 0);

    s->lbl_system_cpu = lv_label_create(s->card_system_info);
    lv_obj_align(s->lbl_system_cpu, LV_ALIGN_TOP_LEFT, 4, 26);
    if (s->font) lv_obj_set_style_text_font(s->lbl_system_cpu, s->font, 0);
    lv_label_set_text(s->lbl_system_cpu, "CPU 负载: 15% | 核心 0/1");
    lv_obj_set_style_text_color(s->lbl_system_cpu, lv_color_hex(0x00E5FF), 0);

    s->lbl_system_ram = lv_label_create(s->card_system_info);
    lv_obj_align(s->lbl_system_ram, LV_ALIGN_TOP_LEFT, 4, 50);
    if (s->font) lv_obj_set_style_text_font(s->lbl_system_ram, s->font, 0);
    lv_label_set_text(s->lbl_system_ram, "RAM 内存: 18.2MB / 32.0MB");
    lv_obj_set_style_text_color(s->lbl_system_ram, lv_color_hex(0x8B9EB5), 0);

    s->lbl_system_fps = lv_label_create(s->card_system_info);
    lv_obj_align(s->lbl_system_fps, LV_ALIGN_TOP_LEFT, 4, 74);
    if (s->font) lv_obj_set_style_text_font(s->lbl_system_fps, s->font, 0);
    lv_label_set_text(s->lbl_system_fps, "LVGL: 60 FPS | 触控手势就绪");
    lv_obj_set_style_text_color(s->lbl_system_fps, lv_color_hex(0x8B9EB5), 0);

    s->lbl_system_ver = lv_label_create(s->card_system_info);
    lv_obj_align(s->lbl_system_ver, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_system_ver, s->font, 0);
    lv_label_set_text(s->lbl_system_ver, "OS: OpenVela R528-S3 (Gemini-S1)");
    lv_obj_set_style_text_color(s->lbl_system_ver, lv_color_hex(0x7E92AD), 0);

    /* =====================================================================
     * 8. 独立标签页 4 (UI_SETTINGS_TAB_STORAGE): 存储卡与外脑日志
     * ===================================================================== */
    s->panel_storage = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_storage, 266, 176);
    lv_obj_align(s->panel_storage, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_storage, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_storage, 0, 0);
    lv_obj_set_style_pad_all(s->panel_storage, 0, 0);
    lv_obj_clear_flag(s->panel_storage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->panel_storage, LV_OBJ_FLAG_HIDDEN);

    s->card_storage_info = lv_obj_create(s->panel_storage);
    lv_obj_set_size(s->card_storage_info, 260, 172);
    lv_obj_align(s->card_storage_info, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->card_storage_info, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->card_storage_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_storage_info, 1, 0);
    lv_obj_set_style_radius(s->card_storage_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_storage_info, 6, 0);
    lv_obj_clear_flag(s->card_storage_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_storage_sd_st = lv_label_create(s->card_storage_info);
    lv_obj_align(s->lbl_storage_sd_st, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_storage_sd_st, s->font, 0);
    lv_label_set_text(s->lbl_storage_sd_st, "TF 卡: 已检测挂载 (/data)");
    lv_obj_set_style_text_color(s->lbl_storage_sd_st, lv_color_hex(0x00FF88), 0);

    s->lbl_storage_cap = lv_label_create(s->card_storage_info);
    lv_obj_align(s->lbl_storage_cap, LV_ALIGN_TOP_LEFT, 4, 26);
    if (s->font) lv_obj_set_style_text_font(s->lbl_storage_cap, s->font, 0);
    lv_label_set_text(s->lbl_storage_cap, "可用容量: 28.6 GB / 32.0 GB");
    lv_obj_set_style_text_color(s->lbl_storage_cap, lv_color_hex(0x00E5FF), 0);

    s->lbl_storage_log = lv_label_create(s->card_storage_info);
    lv_obj_align(s->lbl_storage_log, LV_ALIGN_TOP_LEFT, 4, 50);
    if (s->font) lv_obj_set_style_text_font(s->lbl_storage_log, s->font, 0);
    lv_label_set_text(s->lbl_storage_log, "外脑速记: 自动持久化开启\n黑匣日志: 16KB环形缓冲 + 实时流API");
    lv_obj_set_style_text_color(s->lbl_storage_log, lv_color_hex(0x8B9EB5), 0);

    /* =====================================================================
     * 9. 独立标签页 5: 关于设备
     * ===================================================================== */
    s->panel_about = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_about, 266, 176);
    lv_obj_align(s->panel_about, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_about, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_about, 0, 0);
    lv_obj_set_style_pad_all(s->panel_about, 0, 0);
    lv_obj_clear_flag(s->panel_about, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->panel_about, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *card_about = lv_obj_create(s->panel_about);
    lv_obj_set_size(card_about, 260, 172);
    lv_obj_align(card_about, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(card_about, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(card_about, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(card_about, 1, 0);
    lv_obj_set_style_radius(card_about, 6, 0);
    lv_obj_set_style_pad_all(card_about, 6, 0);
    lv_obj_clear_flag(card_about, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_about_app = lv_label_create(card_about);
    lv_obj_align(lbl_about_app, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(lbl_about_app, s->font, 0);
    lv_label_set_text(lbl_about_app, "Phoenix HoloDesk-S1");
    lv_obj_set_style_text_color(lbl_about_app, lv_color_hex(0x00E5FF), 0);

    lv_obj_t *lbl_about_desc = lv_label_create(card_about);
    lv_obj_align(lbl_about_desc, LV_ALIGN_TOP_LEFT, 4, 26);
    if (s->font) lv_obj_set_style_text_font(lbl_about_desc, s->font, 0);
    lv_label_set_text(lbl_about_desc, "OpenVela 2026 Contest Team 145\n全志 R528-S3 双核ARM Cortex-A7\n固件版本: v1.0.0-Release\n微内核: OpenVela / RT-Thread Smart\n外设: Wi-Fi/BLE + 敲击感应 + TF存储");
    lv_obj_set_style_text_color(lbl_about_desc, lv_color_hex(0x8B9EB5), 0);

    /* =====================================================================
     * 10. 独立标签页 6: 音频调试与声学实验室 (panel_audio)
     * ===================================================================== */
    s->panel_audio = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_audio, 266, 176);
    lv_obj_align(s->panel_audio, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_audio, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_audio, 0, 0);
    lv_obj_set_style_pad_all(s->panel_audio, 0, 0);
    lv_obj_clear_flag(s->panel_audio, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->panel_audio, LV_OBJ_FLAG_HIDDEN);

    /* 卡片 1: 麦克风录音测试 (高度 78px) */
    lv_obj_t *card_rec = lv_obj_create(s->panel_audio);
    lv_obj_set_size(card_rec, 260, 78);
    lv_obj_align(card_rec, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(card_rec, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(card_rec, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(card_rec, 1, 0);
    lv_obj_set_style_radius(card_rec, 6, 0);
    lv_obj_set_style_pad_all(card_rec, 4, 0);
    lv_obj_clear_flag(card_rec, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_rec_title = lv_label_create(card_rec);
    lv_obj_align(lbl_rec_title, LV_ALIGN_TOP_LEFT, 4, 1);
    if (s->font) lv_obj_set_style_text_font(lbl_rec_title, s->font, 0);
    lv_label_set_text(lbl_rec_title, "麦克风录音 (/dev/audio/pcm0c)");
    lv_obj_set_style_text_color(lbl_rec_title, lv_color_hex(0x00E5FF), 0);

    s->lbl_audio_rec_status = lv_label_create(card_rec);
    lv_obj_align(s->lbl_audio_rec_status, LV_ALIGN_TOP_LEFT, 4, 20);
    if (s->font) lv_obj_set_style_text_font(s->lbl_audio_rec_status, s->font, 0);
    lv_label_set_text(s->lbl_audio_rec_status, "待命 (16kHz 16bit 单声道)");
    lv_obj_set_style_text_color(s->lbl_audio_rec_status, lv_color_hex(0x8B9EB5), 0);

    /* 音频能量条 */
    s->bar_audio_energy = lv_bar_create(card_rec);
    lv_obj_set_size(s->bar_audio_energy, 140, 8);
    lv_obj_align(s->bar_audio_energy, LV_ALIGN_BOTTOM_LEFT, 4, -8);
    lv_bar_set_range(s->bar_audio_energy, 0, 100);
    lv_bar_set_value(s->bar_audio_energy, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s->bar_audio_energy, lv_color_hex(0x182436), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s->bar_audio_energy, lv_color_hex(0x00FF88), LV_PART_INDICATOR);

    /* 录音按钮 */
    s->btn_audio_rec = lv_btn_create(card_rec);
    lv_obj_set_size(s->btn_audio_rec, 90, 26);
    lv_obj_align(s->btn_audio_rec, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    lv_obj_set_style_bg_color(s->btn_audio_rec, lv_color_hex(0x1F3554), 0);
    lv_obj_set_style_border_color(s->btn_audio_rec, lv_color_hex(0x3B6B9E), 0);
    lv_obj_set_style_border_width(s->btn_audio_rec, 1, 0);
    lv_obj_set_style_radius(s->btn_audio_rec, 4, 0);
    lv_obj_add_event_cb(s->btn_audio_rec, on_audio_rec_clicked, LV_EVENT_CLICKED, s);

    s->lbl_audio_rec_btn = lv_label_create(s->btn_audio_rec);
    lv_obj_center(s->lbl_audio_rec_btn);
    if (s->font) lv_obj_set_style_text_font(s->lbl_audio_rec_btn, s->font, 0);
    lv_label_set_text(s->lbl_audio_rec_btn, "🎤 录音");
    lv_obj_set_style_text_color(s->lbl_audio_rec_btn, lv_color_hex(0xFFFFFF), 0);

    /* 卡片 2: 扬声器放音与耳返 (高度 88px) */
    lv_obj_t *card_play = lv_obj_create(s->panel_audio);
    lv_obj_set_size(card_play, 260, 88);
    lv_obj_align(card_play, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(card_play, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(card_play, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(card_play, 1, 0);
    lv_obj_set_style_radius(card_play, 6, 0);
    lv_obj_set_style_pad_all(card_play, 4, 0);
    lv_obj_clear_flag(card_play, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_audio_play_status = lv_label_create(card_play);
    lv_obj_align(s->lbl_audio_play_status, LV_ALIGN_TOP_LEFT, 4, 1);
    if (s->font) lv_obj_set_style_text_font(s->lbl_audio_play_status, s->font, 0);
    lv_label_set_text(s->lbl_audio_play_status, "扬声器放音 (/dev/audio/pcm0p)");
    lv_obj_set_style_text_color(s->lbl_audio_play_status, lv_color_hex(0x00E5FF), 0);

    /* 回放按钮 */
    s->btn_audio_play_rec = lv_btn_create(card_play);
    lv_obj_set_size(s->btn_audio_play_rec, 76, 24);
    lv_obj_align(s->btn_audio_play_rec, LV_ALIGN_TOP_LEFT, 4, 20);
    lv_obj_set_style_bg_color(s->btn_audio_play_rec, lv_color_hex(0x182D42), 0);
    lv_obj_set_style_border_color(s->btn_audio_play_rec, lv_color_hex(0x2B4B6E), 0);
    lv_obj_set_style_border_width(s->btn_audio_play_rec, 1, 0);
    lv_obj_set_style_radius(s->btn_audio_play_rec, 4, 0);
    lv_obj_add_event_cb(s->btn_audio_play_rec, on_audio_play_rec_clicked, LV_EVENT_CLICKED, s);

    s->lbl_audio_play_rec = lv_label_create(s->btn_audio_play_rec);
    lv_obj_center(s->lbl_audio_play_rec);
    if (s->font) lv_obj_set_style_text_font(s->lbl_audio_play_rec, s->font, 0);
    lv_label_set_text(s->lbl_audio_play_rec, "▶ 回放");
    lv_obj_set_style_text_color(s->lbl_audio_play_rec, lv_color_hex(0x00FF88), 0);

    /* 1kHz 纯音按钮 */
    s->btn_audio_play_tone = lv_btn_create(card_play);
    lv_obj_set_size(s->btn_audio_play_tone, 82, 24);
    lv_obj_align(s->btn_audio_play_tone, LV_ALIGN_TOP_LEFT, 86, 20);
    lv_obj_set_style_bg_color(s->btn_audio_play_tone, lv_color_hex(0x182D42), 0);
    lv_obj_set_style_border_color(s->btn_audio_play_tone, lv_color_hex(0x2B4B6E), 0);
    lv_obj_set_style_border_width(s->btn_audio_play_tone, 1, 0);
    lv_obj_set_style_radius(s->btn_audio_play_tone, 4, 0);
    lv_obj_add_event_cb(s->btn_audio_play_tone, on_audio_play_tone_clicked, LV_EVENT_CLICKED, s);

    s->lbl_audio_play_tone = lv_label_create(s->btn_audio_play_tone);
    lv_obj_center(s->lbl_audio_play_tone);
    if (s->font) lv_obj_set_style_text_font(s->lbl_audio_play_tone, s->font, 0);
    lv_label_set_text(s->lbl_audio_play_tone, "🔔 1kHz");
    lv_obj_set_style_text_color(s->lbl_audio_play_tone, lv_color_hex(0xFFD700), 0);

    /* 耳返回环开关 */
    s->sw_audio_loopback = lv_switch_create(card_play);
    lv_obj_set_size(s->sw_audio_loopback, 36, 18);
    lv_obj_align(s->sw_audio_loopback, LV_ALIGN_TOP_RIGHT, -6, 23);
    lv_obj_add_event_cb(s->sw_audio_loopback, on_audio_loopback_changed, LV_EVENT_VALUE_CHANGED, s);

    s->lbl_audio_loopback = lv_label_create(card_play);
    lv_obj_align(s->lbl_audio_loopback, LV_ALIGN_TOP_RIGHT, -46, 24);
    if (s->font) lv_obj_set_style_text_font(s->lbl_audio_loopback, s->font, 0);
    lv_label_set_text(s->lbl_audio_loopback, "耳返");
    lv_obj_set_style_text_color(s->lbl_audio_loopback, lv_color_hex(0x8B9EB5), 0);

    /* 音量滑块 */
    lv_obj_t *lbl_vol_title = lv_label_create(card_play);
    lv_obj_align(lbl_vol_title, LV_ALIGN_BOTTOM_LEFT, 4, -4);
    if (s->font) lv_obj_set_style_text_font(lbl_vol_title, s->font, 0);
    lv_label_set_text(lbl_vol_title, "音量");
    lv_obj_set_style_text_color(lbl_vol_title, lv_color_hex(0x8B9EB5), 0);

    s->slider_audio_vol = lv_slider_create(card_play);
    lv_obj_set_size(s->slider_audio_vol, 150, 10);
    lv_obj_align(s->slider_audio_vol, LV_ALIGN_BOTTOM_LEFT, 40, -8);
    lv_slider_set_range(s->slider_audio_vol, 0, 100);
    int cur_vol = phoenix_config_get_int(PHOENIX_CFG_VOLUME, 80);
    lv_slider_set_value(s->slider_audio_vol, cur_vol, LV_ANIM_OFF);
    lv_obj_add_event_cb(s->slider_audio_vol, on_audio_vol_slider_changed, LV_EVENT_VALUE_CHANGED, s);

    s->lbl_audio_vol_val = lv_label_create(card_play);
    lv_obj_align(s->lbl_audio_vol_val, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
    if (s->font) lv_obj_set_style_text_font(s->lbl_audio_vol_val, s->font, 0);
    char vol_str[16];
    snprintf(vol_str, sizeof(vol_str), "%d%%", cur_vol);
    lv_label_set_text(s->lbl_audio_vol_val, vol_str);
    lv_obj_set_style_text_color(s->lbl_audio_vol_val, lv_color_hex(0x00E5FF), 0);

    /* 兼容性绑定 */
    s->lbl_net_status = s->lbl_hotspot_ssid;
    s->lbl_net_ip = s->lbl_hotspot_ip;
    s->btn_ble_prov = s->btn_ble_toggle;
    s->lbl_ble_prov_btn = s->lbl_ble_toggle;
    s->btn_hotspot = s->btn_hotspot_action;
    s->lbl_hotspot_btn = s->lbl_hotspot_action;
    s->lbl_sys_uptime = s->lbl_system_uptime;
    s->lbl_sys_cpu = s->lbl_system_cpu;

    /* 默认初始化在单列垂直卡片菜单列表 */
    ui_settings_back_to_menu(s);

    return s;
}

void ui_settings_destroy(ui_settings_t *settings)
{
    if (!settings) return;
    if (settings->container) {
        lv_obj_del(settings->container);
        settings->container = NULL;
    }
    free(settings);
}

void ui_settings_open(ui_settings_t *settings)
{
    if (!settings || !settings->container) return;

    settings->is_open = true;
    lv_obj_clear_flag(settings->container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(settings->container);

    /* 每次进入设置时，重置并呈现主菜单列表 */
    ui_settings_back_to_menu(settings);
    LOG_I(TAG, "设置同级视图已激活展示(主菜单列表)");
}

void ui_settings_close(ui_settings_t *settings)
{
    if (!settings || !settings->container) return;

    ui_settings_close_password_dialog(settings);
    settings->is_open = false;
    lv_obj_add_flag(settings->container, LV_OBJ_FLAG_HIDDEN);

    if (settings->on_close_cb) {
        settings->on_close_cb(settings->close_user_data);
    }
    LOG_I(TAG, "设置同级视图已退出");
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

void ui_settings_set_page(ui_settings_t *settings, ui_settings_page_t page)
{
    ui_settings_enter_detail(settings, page);
}

ui_settings_page_t ui_settings_get_page(const ui_settings_t *settings)
{
    return settings ? settings->current_tab : UI_SETTINGS_TAB_HOTSPOT;
}

/* ========================================================================= */
/*                   单列菜单列表与二级下钻详情交互引擎                      */
/* ========================================================================= */

void ui_settings_enter_detail(ui_settings_t *settings, ui_settings_tab_t tab)
{
    if (!settings) return;
    if ((int)tab > 5) tab = UI_SETTINGS_TAB_NET;

    settings->is_in_detail = true;
    settings->current_tab = tab;

    /* 1. 顶栏切换为返回模式: [ < ] 与 模块标题 */
    if (settings->lbl_top_close) {
        lv_label_set_text(settings->lbl_top_close, "<");
    }

    const char *tab_titles[7] = {
        "Wi-Fi网络连接",
        "蓝牙极速配网",
        "灵眸大模型",
        "硬件状态与遥测",
        "存储卡与外脑日志",
        "关于设备",
        "音频调试与声学"
    };
    if (settings->lbl_top_title) {
        lv_label_set_text(settings->lbl_top_title, tab_titles[(int)tab]);
    }

    /* 2. 隐藏菜单列表，激活二级详情区域 */
    if (settings->view_menu_list) {
        lv_obj_add_flag(settings->view_menu_list, LV_OBJ_FLAG_HIDDEN);
    }
    if (settings->view_detail_area) {
        lv_obj_clear_flag(settings->view_detail_area, LV_OBJ_FLAG_HIDDEN);
    }

    /* 3. 切换详情面板可见性 */
    lv_obj_t *panels[7] = {
        settings->panel_hotspot,
        settings->panel_ble,
        settings->panel_agent,
        settings->panel_system,
        settings->panel_storage,
        settings->panel_about,
        settings->panel_audio
    };
    for (int i = 0; i < 7; i++) {
        if (panels[i]) {
            if (i == (int)tab) lv_obj_clear_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    ui_settings_refresh_data(settings);
    if (tab == UI_SETTINGS_TAB_NET) {
        ui_settings_refresh_wifi_list(settings);
    }
    LOG_I(TAG, "已下钻进入二级设置详情: [%s]", tab_titles[(int)tab]);
}

void ui_settings_back_to_menu(ui_settings_t *settings)
{
    if (!settings) return;

    ui_settings_close_password_dialog(settings);
    settings->is_in_detail = false;

    /* 1. 顶栏恢复为主菜单模式: [ X ] 与 系统设置 */
    if (settings->lbl_top_close) {
        lv_label_set_text(settings->lbl_top_close, "X");
    }
    if (settings->lbl_top_title) {
        lv_label_set_text(settings->lbl_top_title, "系统设置");
    }

    /* 2. 隐藏二级详情区域，恢复菜单列表 */
    if (settings->view_detail_area) {
        lv_obj_add_flag(settings->view_detail_area, LV_OBJ_FLAG_HIDDEN);
    }
    if (settings->view_menu_list) {
        lv_obj_clear_flag(settings->view_menu_list, LV_OBJ_FLAG_HIDDEN);
    }

    ui_settings_refresh_data(settings);
    LOG_I(TAG, "已平滑返回设置主菜单列表");
}

void ui_settings_switch_tab(ui_settings_t *settings, ui_settings_tab_t tab)
{
    ui_settings_enter_detail(settings, tab);
}

/* ========================================================================= */
/*                      步进配网状态机与数据同步                             */
/* ========================================================================= */

void ui_settings_update_net_progress(ui_settings_t *settings, int mode, const char *ssid, const char *ip, const char *msg)
{
    if (!settings) return;

    if (mode == NET_MODE_STA_CONNECTING) {
        /* 立即切换显示步进状态机面板 */
        if (settings->box_hotspot_idle) lv_obj_add_flag(settings->box_hotspot_idle, LV_OBJ_FLAG_HIDDEN);
        if (settings->box_hotspot_progress) lv_obj_clear_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN);

        char tbuf[64];
        snprintf(tbuf, sizeof(tbuf), "● 正在加入网络: %s", (ssid && ssid[0]) ? ssid : "目标Wi-Fi");
        if (settings->lbl_prog_title) lv_label_set_text(settings->lbl_prog_title, tbuf);

        bool is_dhcp_stage = (msg && (strstr(msg, "DHCP") || strstr(msg, "租约") || strstr(msg, "申请")));
        if (!msg && settings->lbl_prog_step2) {
            const char *cur_s2 = lv_label_get_text(settings->lbl_prog_step2);
            if (cur_s2 && (strstr(cur_s2, "✓") || strstr(cur_s2, "完成"))) {
                is_dhcp_stage = true;
            }
        }

        if (settings->lbl_prog_step1) {
            lv_label_set_text(settings->lbl_prog_step1, "[✓] 1. 收到配网指令，关闭热点");
            lv_obj_set_style_text_color(settings->lbl_prog_step1, lv_color_hex(0x00FF88), 0);
        }

        if (is_dhcp_stage) {
            if (settings->lbl_prog_step2) {
                lv_label_set_text(settings->lbl_prog_step2, "[✓] 2. 目标 Wi-Fi 关联完成");
                lv_obj_set_style_text_color(settings->lbl_prog_step2, lv_color_hex(0x00FF88), 0);
            }
            if (settings->lbl_prog_step3) {
                lv_label_set_text(settings->lbl_prog_step3, (msg && msg[0]) ? msg : "[..] 3. 正在申请 DHCP 局域网 IP...");
                lv_obj_set_style_text_color(settings->lbl_prog_step3, lv_color_hex(0xFFB700), 0);
            }
            if (settings->lbl_prog_result_ip) {
                lv_label_set_text(settings->lbl_prog_result_ip, (msg && msg[0]) ? msg : "Wi-Fi 关联成功，正在协商 IP...");
                lv_obj_set_style_text_color(settings->lbl_prog_result_ip, lv_color_hex(0xFFB700), 0);
            }
        } else {
            if (settings->lbl_prog_step2) {
                lv_label_set_text(settings->lbl_prog_step2, (msg && msg[0]) ? msg : "[..] 2. 正在关联目标 Wi-Fi 路由...");
                lv_obj_set_style_text_color(settings->lbl_prog_step2, lv_color_hex(0xFFB700), 0);
            }
            if (settings->lbl_prog_step3) {
                lv_label_set_text(settings->lbl_prog_step3, "[..] 3. 申请 DHCP 局域网 IP 租约...");
                lv_obj_set_style_text_color(settings->lbl_prog_step3, lv_color_hex(0x7E92AD), 0);
            }
            if (settings->lbl_prog_result_ip) {
                lv_label_set_text(settings->lbl_prog_result_ip, (msg && msg[0]) ? msg : "已向射频下发凭证，正在握手...");
                lv_obj_set_style_text_color(settings->lbl_prog_result_ip, lv_color_hex(0xFFB700), 0);
            }
        }
        if (settings->lbl_prog_result_url) {
            lv_label_set_text(settings->lbl_prog_result_url, "设备正面微胶囊将同步呈现连网");
            lv_obj_set_style_text_color(settings->lbl_prog_result_url, lv_color_hex(0x8B9EB5), 0);
        }
        if (settings->lbl_prog_done) {
            char btn_buf[64];
            snprintf(btn_buf, sizeof(btn_buf), "[.. %s]", (msg && msg[0]) ? msg : "连网进行中...");
            lv_label_set_text(settings->lbl_prog_done, btn_buf);
            lv_obj_set_style_text_color(settings->lbl_prog_done, lv_color_hex(0xFFB700), 0);
        }
    } else if (mode == NET_MODE_STA_CONNECTED) {
        if (settings->box_hotspot_idle) lv_obj_add_flag(settings->box_hotspot_idle, LV_OBJ_FLAG_HIDDEN);
        if (settings->box_hotspot_progress) lv_obj_clear_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN);

        if (settings->lbl_prog_title) {
            lv_label_set_text(settings->lbl_prog_title, "[✓] Wi-Fi 连接成功!");
            lv_obj_set_style_text_color(settings->lbl_prog_title, lv_color_hex(0x00FF88), 0);
        }
        if (settings->lbl_prog_step2) {
            lv_label_set_text(settings->lbl_prog_step2, "[✓] 2. 目标 Wi-Fi 关联完成");
            lv_obj_set_style_text_color(settings->lbl_prog_step2, lv_color_hex(0x00FF88), 0);
        }
        if (settings->lbl_prog_step3) {
            lv_label_set_text(settings->lbl_prog_step3, "[✓] 3. DHCP 局域网 IP 分配成功");
            lv_obj_set_style_text_color(settings->lbl_prog_step3, lv_color_hex(0x00FF88), 0);
        }

        char ip_buf[64];
        snprintf(ip_buf, sizeof(ip_buf), "物理 IP: %s (端口 :8080)", (ip && ip[0]) ? ip : "获取中");
        if (settings->lbl_prog_result_ip) {
            lv_label_set_text(settings->lbl_prog_result_ip, ip_buf);
            lv_obj_set_style_text_color(settings->lbl_prog_result_ip, lv_color_hex(0x00FF88), 0);
        }

        char url_buf[64];
        snprintf(url_buf, sizeof(url_buf), "看板: http://%s:8080/", (ip && ip[0]) ? ip : "127.0.0.1");
        if (settings->lbl_prog_result_url) {
            lv_label_set_text(settings->lbl_prog_result_url, url_buf);
            lv_obj_set_style_text_color(settings->lbl_prog_result_url, lv_color_hex(0x00E5FF), 0);
        }

        if (settings->lbl_prog_done) {
            lv_label_set_text(settings->lbl_prog_done, "[✓ 完成并返回主页]");
            lv_obj_set_style_text_color(settings->lbl_prog_done, lv_color_hex(0x00FF88), 0);
        }
    } else if (mode == NET_MODE_DISCONNECTED) {
        if (settings->box_hotspot_progress && !lv_obj_has_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN)) {
            if (settings->lbl_prog_title) {
                lv_label_set_text(settings->lbl_prog_title, "[!] 连网超时或密码错误");
                lv_obj_set_style_text_color(settings->lbl_prog_title, lv_color_hex(0xFF5252), 0);
            }
            if (settings->lbl_prog_step2) {
                const char *cur_s2 = lv_label_get_text(settings->lbl_prog_step2);
                if (!cur_s2 || !strstr(cur_s2, "✓")) {
                    lv_label_set_text(settings->lbl_prog_step2, "[x] 2. 路由关联握手失败");
                    lv_obj_set_style_text_color(settings->lbl_prog_step2, lv_color_hex(0xFF5252), 0);
                }
            }
            if (settings->lbl_prog_step3) {
                lv_label_set_text(settings->lbl_prog_step3, "[x] 3. DHCP 局域网 IP 获取超时");
                lv_obj_set_style_text_color(settings->lbl_prog_step3, lv_color_hex(0xFF5252), 0);
            }
            if (settings->lbl_prog_result_ip) {
                lv_label_set_text(settings->lbl_prog_result_ip, (msg && msg[0]) ? msg : "已自动切回独立热点");
                lv_obj_set_style_text_color(settings->lbl_prog_result_ip, lv_color_hex(0xFFB700), 0);
            }
            if (settings->lbl_prog_done) {
                lv_label_set_text(settings->lbl_prog_done, "[返回热点重试]");
                lv_obj_set_style_text_color(settings->lbl_prog_done, lv_color_hex(0xFF5252), 0);
            }
        }
    } else if (mode == NET_MODE_SOFTAP_CONFIG) {
        /* 当系统切换为 SoftAP 独立热点时：
         * 若此时进度卡片处于显示中，更新提示为热点就绪并允许用户点击一键切回待机；
         * 若用户直接在常规视图，则正常隐藏进度框展示待机卡片 */
        if (settings->box_hotspot_progress && !lv_obj_has_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN)) {
            if (settings->lbl_prog_result_ip) {
                lv_label_set_text(settings->lbl_prog_result_ip, "📡 独立热点已就绪: 192.168.4.1");
                lv_obj_set_style_text_color(settings->lbl_prog_result_ip, lv_color_hex(0x00FF88), 0);
            }
            if (settings->lbl_prog_done) {
                lv_label_set_text(settings->lbl_prog_done, "[返回热点待机卡片]");
                lv_obj_set_style_text_color(settings->lbl_prog_done, lv_color_hex(0x00E5FF), 0);
            }
        } else {
            if (settings->box_hotspot_progress) lv_obj_add_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN);
            if (settings->box_hotspot_idle) lv_obj_clear_flag(settings->box_hotspot_idle, LV_OBJ_FLAG_HIDDEN);
        }

        if (settings->lbl_hotspot_ssid) {
            char buf[64];
            snprintf(buf, sizeof(buf), "热点: %s", (ssid && ssid[0]) ? ssid : "Gemini-Agent-Setup");
            lv_label_set_text(settings->lbl_hotspot_ssid, buf);
        }
        if (settings->lbl_hotspot_ip) {
            lv_label_set_text(settings->lbl_hotspot_ip, "IP: 192.168.4.1 (Web 免端口直达)");
        }
        if (settings->lbl_hotspot_action) {
            lv_label_set_text(settings->lbl_hotspot_action, "[● 重启热点广播]");
            lv_obj_set_style_text_color(settings->lbl_hotspot_action, lv_color_hex(0x00E5FF), 0);
        }
    }
}

void ui_settings_refresh_data(ui_settings_t *settings)
{
    /* 关键性能防护：仅在设置面板真正展开于前台时才执行数据获取与重绘，杜绝后台无谓更新与锁争抢 */
    if (!settings || !settings->is_open) return;

    char ip_buf[32] = {0};
    char ssid_buf[32] = {0};
    net_mode_t mode = net_mgr_get_mode();
    net_mgr_get_ip(ip_buf, sizeof(ip_buf));
    net_mgr_get_ssid(ssid_buf, sizeof(ssid_buf));

    /* 1. 刷新 Wi-Fi 网络页面 */
    if (settings->lbl_wifi_status) {
        if (mode == NET_MODE_STA_CONNECTING) {
            char buf[64];
            snprintf(buf, sizeof(buf), "⏳ 正在连接: %s", ssid_buf[0] ? ssid_buf : "目标路由");
            lv_label_set_text(settings->lbl_wifi_status, buf);
            lv_obj_set_style_text_color(settings->lbl_wifi_status, lv_color_hex(0xFFB700), 0);
        } else if (mode == NET_MODE_STA_CONNECTED) {
            char buf[64];
            snprintf(buf, sizeof(buf), "📶 已连: %s", ssid_buf[0] ? ssid_buf : "已连网");
            lv_label_set_text(settings->lbl_wifi_status, buf);
            lv_obj_set_style_text_color(settings->lbl_wifi_status, lv_color_hex(0x00FF88), 0);
        } else if (mode == NET_MODE_SOFTAP_CONFIG) {
            char buf[64];
            snprintf(buf, sizeof(buf), "📡 热点中: %s", ssid_buf[0] ? ssid_buf : "Gemini-Setup");
            lv_label_set_text(settings->lbl_wifi_status, buf);
            lv_obj_set_style_text_color(settings->lbl_wifi_status, lv_color_hex(0x00E5FF), 0);
        } else {
            if (net_mgr_is_scanning()) {
                lv_label_set_text(settings->lbl_wifi_status, "📡 正在搜索周边 Wi-Fi...");
                lv_obj_set_style_text_color(settings->lbl_wifi_status, lv_color_hex(0xFFB700), 0);
            } else {
                lv_label_set_text(settings->lbl_wifi_status, "📶 未连接网络 (请选Wi-Fi)");
                lv_obj_set_style_text_color(settings->lbl_wifi_status, lv_color_hex(0x7E92AD), 0);
            }
        }
    }

    if (settings->lbl_wifi_ap_mode) {
        if (mode == NET_MODE_SOFTAP_CONFIG) {
            lv_label_set_text(settings->lbl_wifi_ap_mode, "关闭热点");
            lv_obj_set_style_text_color(settings->lbl_wifi_ap_mode, lv_color_hex(0xFF7043), 0);
        } else {
            lv_label_set_text(settings->lbl_wifi_ap_mode, "热点模式");
            lv_obj_set_style_text_color(settings->lbl_wifi_ap_mode, lv_color_hex(0x7E92AD), 0);
        }
    }

    /* 当处于 Wi-Fi 网络详情页时，后台扫描完成或有结果更新自动刷新条目列表 */
    if (settings->is_in_detail && settings->current_tab == UI_SETTINGS_TAB_NET && settings->list_wifi) {
        uint32_t child_cnt = lv_obj_get_child_cnt(settings->list_wifi);
        if (child_cnt <= 1 && !net_mgr_is_scanning()) {
            net_wifi_ap_info_t check_aps[1];
            if (net_mgr_get_cached_scan_results(check_aps, 1) > 0) {
                ui_settings_refresh_wifi_list(settings);
            }
        }
    }

    /* 兼容保留历史步进状态机提示 */
    if (mode == NET_MODE_STA_CONNECTING || mode == NET_MODE_STA_CONNECTED) {
        ui_settings_update_net_progress(settings, mode, ssid_buf, ip_buf, NULL);
    }

    /* 2. 刷新蓝牙配网页面 */
    ble_prov_state_t bst = ble_prov_service_get_state();
    char dev_name[32] = {0};
    ble_prov_service_get_dev_name(dev_name, sizeof(dev_name));

    if (settings->lbl_ble_dev_name) {
        char buf[64];
        snprintf(buf, sizeof(buf), "广播设备名: %s", dev_name[0] ? dev_name : "Phoenix-Setup");
        lv_label_set_text(settings->lbl_ble_dev_name, buf);
    }

    if (bst == BLE_PROV_STATE_ADVERTISING) {
        if (settings->lbl_ble_status) {
            lv_label_set_text(settings->lbl_ble_status, "● 蓝牙状态: 广播中 (等待网页连接)");
            lv_obj_set_style_text_color(settings->lbl_ble_status, lv_color_hex(0x00FF88), 0);
        }
        if (settings->lbl_ble_toggle) {
            lv_label_set_text(settings->lbl_ble_toggle, "[🛑 停止蓝牙配网广播]");
            lv_obj_set_style_text_color(settings->lbl_ble_toggle, lv_color_hex(0xFF7043), 0);
        }
        if (settings->btn_ble_toggle) {
            lv_obj_set_style_border_color(settings->btn_ble_toggle, lv_color_hex(0xFF7043), 0);
        }
    } else if (bst == BLE_PROV_STATE_CONNECTED) {
        if (settings->lbl_ble_status) {
            lv_label_set_text(settings->lbl_ble_status, "● 蓝牙状态: Web 客户端已在线");
            lv_obj_set_style_text_color(settings->lbl_ble_status, lv_color_hex(0x00E5FF), 0);
        }
        if (settings->lbl_ble_toggle) {
            lv_label_set_text(settings->lbl_ble_toggle, "[🛑 断开并停止广播]");
            lv_obj_set_style_text_color(settings->lbl_ble_toggle, lv_color_hex(0xFF7043), 0);
        }
        if (settings->btn_ble_toggle) {
            lv_obj_set_style_border_color(settings->btn_ble_toggle, lv_color_hex(0xFF7043), 0);
        }
    } else if (bst == BLE_PROV_STATE_PROVISIONING) {
        if (settings->lbl_ble_status) {
            lv_label_set_text(settings->lbl_ble_status, "● 蓝牙状态: 正在接收 Wi-Fi 凭证...");
            lv_obj_set_style_text_color(settings->lbl_ble_status, lv_color_hex(0xFFB700), 0);
        }
    } else if (bst == BLE_PROV_STATE_STARTING) {
        if (settings->lbl_ble_status) {
            lv_label_set_text(settings->lbl_ble_status, "● 蓝牙状态: 正在使能与初始化...");
            lv_obj_set_style_text_color(settings->lbl_ble_status, lv_color_hex(0x00E5FF), 0);
        }
        if (settings->lbl_ble_toggle) {
            lv_label_set_text(settings->lbl_ble_toggle, "[⏳ 蓝牙初始化中...]");
            lv_obj_set_style_text_color(settings->lbl_ble_toggle, lv_color_hex(0x7E92AD), 0);
        }
        if (settings->btn_ble_toggle) {
            lv_obj_set_style_border_color(settings->btn_ble_toggle, lv_color_hex(0x7E92AD), 0);
        }
    } else if (bst == BLE_PROV_STATE_PROVISIONED) {
        if (settings->lbl_ble_status) {
            lv_label_set_text(settings->lbl_ble_status, "● 蓝牙状态: 配网完成");
            lv_obj_set_style_text_color(settings->lbl_ble_status, lv_color_hex(0x00FF88), 0);
        }
        if (settings->lbl_ble_toggle) {
            lv_label_set_text(settings->lbl_ble_toggle, "[⚡ 重新开启配网广播]");
            lv_obj_set_style_text_color(settings->lbl_ble_toggle, lv_color_hex(0x00E5FF), 0);
        }
        if (settings->btn_ble_toggle) {
            lv_obj_set_style_border_color(settings->btn_ble_toggle, lv_color_hex(0x00E5FF), 0);
        }
    } else {
        if (settings->lbl_ble_status) {
            lv_label_set_text(settings->lbl_ble_status, "● 蓝牙状态: 未开启 (点击启动)");
            lv_obj_set_style_text_color(settings->lbl_ble_status, lv_color_hex(0x7E92AD), 0);
        }
        if (settings->lbl_ble_toggle) {
            lv_label_set_text(settings->lbl_ble_toggle, "[⚡ 启动蓝牙极速配网]");
            lv_obj_set_style_text_color(settings->lbl_ble_toggle, lv_color_hex(0x00E5FF), 0);
        }
        if (settings->btn_ble_toggle) {
            lv_obj_set_style_border_color(settings->btn_ble_toggle, lv_color_hex(0x00E5FF), 0);
        }
    }

    /* 
     * 关键性能保护：仅在设置抽屉实际展开处于前台时，才深度读取系统 CPU、温度、主频及 TF 卡 IO 遥测；
     * 彻底规避抽屉隐藏在后台时因网络事件高频触发而导致的 sysfs/statvfs 硬件级 IO 阻塞。
     */
    if (!settings->is_open) {
        return;
    }

    /* 3. 刷新系统遥测与硬件状态页面 */
    hal_system_telemetry_t sys;
    memset(&sys, 0, sizeof(sys));
    if (hal_system_get_telemetry(&sys) == 0) {
        if (settings->lbl_system_uptime) {
            uint64_t s = sys.uptime_seconds;
            char ubuf[64];
            snprintf(ubuf, sizeof(ubuf), "运行时长: %02u:%02u:%02u",
                     (unsigned int)(s / 3600),
                     (unsigned int)((s % 3600) / 60),
                     (unsigned int)(s % 60));
            lv_label_set_text(settings->lbl_system_uptime, ubuf);
        }
        if (settings->lbl_system_cpu) {
            char cbuf[64];
            if (sys.cpu_temperature_c > 0) {
                snprintf(cbuf, sizeof(cbuf), "CPU: %uMHz (%u%%) | %d°C",
                         (unsigned int)sys.cpu_freq_mhz,
                         (unsigned int)sys.cpu_load_pct,
                         (int)sys.cpu_temperature_c);
            } else {
                snprintf(cbuf, sizeof(cbuf), "CPU: %uMHz | 负载: %u%%",
                         (unsigned int)sys.cpu_freq_mhz,
                         (unsigned int)sys.cpu_load_pct);
            }
            lv_label_set_text(settings->lbl_system_cpu, cbuf);
        }
        if (settings->lbl_system_ram) {
            char rbuf[64];
            float used_mb = (float)(sys.mem_total_kb - sys.mem_free_kb) / 1024.0f;
            float total_mb = (float)sys.mem_total_kb / 1024.0f;
            snprintf(rbuf, sizeof(rbuf), "RAM 内存: %.1fMB / %.1fMB", used_mb, total_mb);
            lv_label_set_text(settings->lbl_system_ram, rbuf);
        }
        if (settings->lbl_system_fps) {
            char fbuf[64];
            snprintf(fbuf, sizeof(fbuf), "LVGL: %u FPS | 调度就绪", (unsigned int)sys.fps);
            lv_label_set_text(settings->lbl_system_fps, fbuf);
        }
        if (settings->lbl_system_ver && sys.os_version[0]) {
            char vbuf[64];
            snprintf(vbuf, sizeof(vbuf), "OS: %s", sys.os_version);
            lv_label_set_text(settings->lbl_system_ver, vbuf);
        }
    }

    /* 4. 刷新外置 TF 卡与持久化存储页面 */
    hal_sdcard_info_t sd;
    memset(&sd, 0, sizeof(sd));
    hal_sdcard_get_info(&sd);

    if (settings->lbl_storage_sd_st) {
        char sdbuf[64];
        if (sd.is_mounted) {
            snprintf(sdbuf, sizeof(sdbuf), "TF 卡: 已就绪 (%s)", sd.mount_point[0] ? sd.mount_point : "/mnt/sdcard");
            lv_obj_set_style_text_color(settings->lbl_storage_sd_st, lv_color_hex(0x00FF88), 0);
        } else {
            snprintf(sdbuf, sizeof(sdbuf), "TF 卡: 未检测到外置卡");
            lv_obj_set_style_text_color(settings->lbl_storage_sd_st, lv_color_hex(0xFFB700), 0);
        }
        lv_label_set_text(settings->lbl_storage_sd_st, sdbuf);
    }

    if (settings->lbl_storage_cap) {
        char capbuf[64];
        if (sd.total_mb >= 1024) {
            float total_gb = (float)sd.total_mb / 1024.0f;
            float free_gb = (float)sd.free_mb / 1024.0f;
            snprintf(capbuf, sizeof(capbuf), "可用容量: %.1f GB / %.1f GB", free_gb, total_gb);
        } else if (sd.total_mb > 0) {
            snprintf(capbuf, sizeof(capbuf), "可用容量: %u MB / %u MB", (unsigned int)sd.free_mb, (unsigned int)sd.total_mb);
        } else {
            snprintf(capbuf, sizeof(capbuf), "容量: 物理存储未挂载");
        }
        lv_label_set_text(settings->lbl_storage_cap, capbuf);
    }

    /* 5. 刷新大模型配置面板与一键访问路径 */
    char cur_api_key[128] = {0};
    phoenix_config_get_str(PHOENIX_CFG_API_KEY, "", cur_api_key, sizeof(cur_api_key));
    bool has_api_key = (cur_api_key[0] != '\0');

    if (settings->lbl_agent_key_st) {
        char kbuf[64];
        if (has_api_key) {
            size_t klen = strlen(cur_api_key);
            if (klen > 8) {
                snprintf(kbuf, sizeof(kbuf), "API Key: [%.4s****%.4s]", cur_api_key, cur_api_key + klen - 4);
            } else {
                snprintf(kbuf, sizeof(kbuf), "API Key: [已配置密钥]");
            }
            lv_obj_set_style_text_color(settings->lbl_agent_key_st, lv_color_hex(0x00FF88), 0);
        } else {
            snprintf(kbuf, sizeof(kbuf), "API Key: [未配置 - 离线兜底]");
            lv_obj_set_style_text_color(settings->lbl_agent_key_st, lv_color_hex(0xFFB700), 0);
        }
        lv_label_set_text(settings->lbl_agent_key_st, kbuf);
    }

    if (settings->lbl_agent_hint) {
        char hbuf[128];
        if (mode == NET_MODE_STA_CONNECTED && ip_buf[0]) {
            snprintf(hbuf, sizeof(hbuf), "● 访问 http://%s/ 免端口录入 Key", ip_buf);
            lv_obj_set_style_text_color(settings->lbl_agent_hint, lv_color_hex(0x00E5FF), 0);
        } else if (mode == NET_MODE_SOFTAP_CONFIG) {
            snprintf(hbuf, sizeof(hbuf), "● 手机连热点访问 http://192.168.4.1/ 录入 Key");
            lv_obj_set_style_text_color(settings->lbl_agent_hint, lv_color_hex(0xFFB700), 0);
        } else {
            snprintf(hbuf, sizeof(hbuf), "● 请先在 [网络配网] 中连入 Wi-Fi");
            lv_obj_set_style_text_color(settings->lbl_agent_hint, lv_color_hex(0x7E92AD), 0);
        }
        lv_label_set_text(settings->lbl_agent_hint, hbuf);
    }

    /* 6. 同步刷新主菜单列表卡片的右侧状态摘要 (Glanceable Summary) */
    if (settings->lbl_menu_net_sub) {
        if (mode == NET_MODE_STA_CONNECTED) {
            char nbuf[32];
            snprintf(nbuf, sizeof(nbuf), "%s >", ssid_buf[0] ? ssid_buf : "已连网");
            lv_label_set_text(settings->lbl_menu_net_sub, nbuf);
            lv_obj_set_style_text_color(settings->lbl_menu_net_sub, lv_color_hex(0x00FF88), 0);
        } else if (mode == NET_MODE_STA_CONNECTING) {
            lv_label_set_text(settings->lbl_menu_net_sub, "连网中 >");
            lv_obj_set_style_text_color(settings->lbl_menu_net_sub, lv_color_hex(0xFFB700), 0);
        } else if (mode == NET_MODE_SOFTAP_CONFIG) {
            lv_label_set_text(settings->lbl_menu_net_sub, "热点广播 >");
            lv_obj_set_style_text_color(settings->lbl_menu_net_sub, lv_color_hex(0x00E5FF), 0);
        } else {
            lv_label_set_text(settings->lbl_menu_net_sub, "未配置 >");
            lv_obj_set_style_text_color(settings->lbl_menu_net_sub, lv_color_hex(0x7E92AD), 0);
        }
    }

    if (settings->lbl_menu_ble_sub) {
        if (ble_prov_service_is_active()) {
            bst = ble_prov_service_get_state();
            if (bst == BLE_PROV_STATE_CONNECTED) {
                lv_label_set_text(settings->lbl_menu_ble_sub, "Web已在线 >");
                lv_obj_set_style_text_color(settings->lbl_menu_ble_sub, lv_color_hex(0x00E5FF), 0);
            } else if (bst == BLE_PROV_STATE_PROVISIONED) {
                lv_label_set_text(settings->lbl_menu_ble_sub, "配网完成 >");
                lv_obj_set_style_text_color(settings->lbl_menu_ble_sub, lv_color_hex(0x00FF88), 0);
            } else {
                lv_label_set_text(settings->lbl_menu_ble_sub, "广播中 >");
                lv_obj_set_style_text_color(settings->lbl_menu_ble_sub, lv_color_hex(0x00FF88), 0);
            }
        } else {
            lv_label_set_text(settings->lbl_menu_ble_sub, "已关闭 >");
            lv_obj_set_style_text_color(settings->lbl_menu_ble_sub, lv_color_hex(0x7E92AD), 0);
        }
    }

    if (settings->lbl_menu_agent_sub) {
        if (has_api_key) {
            lv_label_set_text(settings->lbl_menu_agent_sub, "DeepSeek >");
            lv_obj_set_style_text_color(settings->lbl_menu_agent_sub, lv_color_hex(0x00FF88), 0);
        } else {
            lv_label_set_text(settings->lbl_menu_agent_sub, "待配Key >");
            lv_obj_set_style_text_color(settings->lbl_menu_agent_sub, lv_color_hex(0xFFB700), 0);
        }
    }

    if (settings->lbl_menu_system_sub) {
        char sbuf[32];
        if (sys.fps > 0) {
            snprintf(sbuf, sizeof(sbuf), "%uFPS/%u%% >", (unsigned int)sys.fps, (unsigned int)sys.cpu_load_pct);
        } else {
            snprintf(sbuf, sizeof(sbuf), "%u%% 负载 >", (unsigned int)sys.cpu_load_pct);
        }
        lv_label_set_text(settings->lbl_menu_system_sub, sbuf);
        lv_obj_set_style_text_color(settings->lbl_menu_system_sub, lv_color_hex(0x00FF88), 0);
    }

    if (settings->lbl_menu_storage_sub) {
        char stbuf[32];
        if (sd.total_mb >= 1024) {
            float free_gb = (float)sd.free_mb / 1024.0f;
            snprintf(stbuf, sizeof(stbuf), "%.1fGB >", free_gb);
        } else if (sd.total_mb > 0) {
            snprintf(stbuf, sizeof(stbuf), "%uMB >", (unsigned int)sd.free_mb);
        } else {
            snprintf(stbuf, sizeof(stbuf), "未挂载 >");
        }
        lv_label_set_text(settings->lbl_menu_storage_sub, stbuf);
        lv_obj_set_style_text_color(settings->lbl_menu_storage_sub, sd.is_mounted ? lv_color_hex(0x00E5FF) : lv_color_hex(0x7E92AD), 0);
    }

    if (settings->lbl_menu_about_sub) {
        lv_label_set_text(settings->lbl_menu_about_sub, "v1.0.0 >");
        lv_obj_set_style_text_color(settings->lbl_menu_about_sub, lv_color_hex(0x00E5FF), 0);
    }

    /* 7. 刷新音频调试与声学实验室面板数据 */
    audio_test_status_t ast;
    audio_test_get_status(&ast);

    if (settings->lbl_menu_audio_sub) {
        if (ast.state == AUDIO_TEST_STATE_RECORDING) {
            lv_label_set_text(settings->lbl_menu_audio_sub, "录音中 >");
            lv_obj_set_style_text_color(settings->lbl_menu_audio_sub, lv_color_hex(0xFFB700), 0);
        } else if (ast.state == AUDIO_TEST_STATE_PLAYING_REC || ast.state == AUDIO_TEST_STATE_PLAYING_TONE) {
            lv_label_set_text(settings->lbl_menu_audio_sub, "放音中 >");
            lv_obj_set_style_text_color(settings->lbl_menu_audio_sub, lv_color_hex(0x00E5FF), 0);
        } else {
            lv_label_set_text(settings->lbl_menu_audio_sub, "就绪 >");
            lv_obj_set_style_text_color(settings->lbl_menu_audio_sub, lv_color_hex(0x00FF88), 0);
        }
    }

    if (settings->is_in_detail && settings->current_tab == UI_SETTINGS_TAB_AUDIO && settings->panel_audio) {
        if (settings->bar_audio_energy) {
            lv_bar_set_value(settings->bar_audio_energy, ast.current_energy, LV_ANIM_OFF);
        }
        if (settings->lbl_audio_rec_status) {
            char rec_buf[64];
            if (ast.state == AUDIO_TEST_STATE_RECORDING) {
                snprintf(rec_buf, sizeof(rec_buf), "录音中 %02u:%02u (能量: %d%%)",
                         (unsigned int)((ast.record_duration_ms / 1000) / 60),
                         (unsigned int)((ast.record_duration_ms / 1000) % 60),
                         ast.current_energy);
                lv_obj_set_style_text_color(settings->lbl_audio_rec_status, lv_color_hex(0xFFB700), 0);
            } else if (ast.recorded_bytes > 0) {
                snprintf(rec_buf, sizeof(rec_buf), "就绪 (已录制 %zu 字节, %ums)",
                         ast.recorded_bytes, (unsigned int)ast.record_duration_ms);
                lv_obj_set_style_text_color(settings->lbl_audio_rec_status, lv_color_hex(0x00FF88), 0);
            } else {
                snprintf(rec_buf, sizeof(rec_buf), "待命 (16kHz 16bit 单声道)");
                lv_obj_set_style_text_color(settings->lbl_audio_rec_status, lv_color_hex(0x8B9EB5), 0);
            }
            lv_label_set_text(settings->lbl_audio_rec_status, rec_buf);
        }
        if (settings->lbl_audio_play_status) {
            if (ast.state == AUDIO_TEST_STATE_PLAYING_REC) {
                lv_label_set_text(settings->lbl_audio_play_status, "正在回放录音...");
                lv_obj_set_style_text_color(settings->lbl_audio_play_status, lv_color_hex(0x00FF88), 0);
            } else if (ast.state == AUDIO_TEST_STATE_PLAYING_TONE) {
                lv_label_set_text(settings->lbl_audio_play_status, "正在播放 1kHz 纯音...");
                lv_obj_set_style_text_color(settings->lbl_audio_play_status, lv_color_hex(0xFFD700), 0);
            } else {
                lv_label_set_text(settings->lbl_audio_play_status, "扬声器放音 (/dev/audio/pcm0p)");
                lv_obj_set_style_text_color(settings->lbl_audio_play_status, lv_color_hex(0x00E5FF), 0);
            }
        }
        if (settings->lbl_audio_rec_btn) {
            if (ast.state == AUDIO_TEST_STATE_RECORDING) {
                lv_label_set_text(settings->lbl_audio_rec_btn, "⏹ 停止");
            } else {
                lv_label_set_text(settings->lbl_audio_rec_btn, "🎤 录音");
            }
        }
    }
}

/* ========================================================================= */
/*                              按钮事件处理                                 */
/* ========================================================================= */

static void on_top_close_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;
    if (s->is_in_detail) {
        LOG_I(TAG, "用户在详情页点击顶栏 [<] 返回主菜单列表");
        ui_settings_back_to_menu(s);
    } else {
        LOG_I(TAG, "用户在主菜单点击顶栏 [✕] 退出设置同级视图");
        ui_settings_close(s);
    }
}

static void on_menu_item_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    lv_obj_t *target = lv_event_get_target(e);
    if (target == s->btn_menu_net) {
        ui_settings_enter_detail(s, UI_SETTINGS_TAB_NET);
    } else if (target == s->btn_menu_ble) {
        ui_settings_enter_detail(s, UI_SETTINGS_TAB_BLE);
    } else if (target == s->btn_menu_agent) {
        ui_settings_enter_detail(s, UI_SETTINGS_TAB_AGENT);
    } else if (target == s->btn_menu_system) {
        ui_settings_enter_detail(s, UI_SETTINGS_TAB_SYSTEM);
    } else if (target == s->btn_menu_storage) {
        ui_settings_enter_detail(s, UI_SETTINGS_TAB_STORAGE);
    } else if (target == s->btn_menu_about) {
        ui_settings_enter_detail(s, UI_SETTINGS_TAB_ABOUT);
    } else if (target == s->btn_menu_audio) {
        ui_settings_enter_detail(s, UI_SETTINGS_TAB_AUDIO);
    }
}

static void on_audio_rec_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;
    audio_test_status_t st;
    audio_test_get_status(&st);
    if (st.state == AUDIO_TEST_STATE_RECORDING) {
        audio_test_record_stop();
        if (s->lbl_audio_rec_btn) lv_label_set_text(s->lbl_audio_rec_btn, "🎤 录音");
    } else {
        audio_test_record_start();
        if (s->lbl_audio_rec_btn) lv_label_set_text(s->lbl_audio_rec_btn, "⏹ 停止");
    }
}

static void on_audio_play_rec_clicked(lv_event_t *e)
{
    (void)e;
    audio_test_play_record();
}

static void on_audio_play_tone_clicked(lv_event_t *e)
{
    (void)e;
    audio_test_play_tone(1000, 1500);
}

static void on_audio_loopback_changed(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    if (!sw) return;
    bool en = lv_obj_has_state(sw, LV_STATE_CHECKED);
    audio_test_set_loopback(en);
}

static void on_audio_vol_slider_changed(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    lv_obj_t *slider = lv_event_get_target(e);
    if (!slider) return;
    int32_t val = lv_slider_get_value(slider);
    hal_actuator_set_volume((uint8_t)val);
    phoenix_config_set_int(PHOENIX_CFG_VOLUME, (int)val);
    if (s && s->lbl_audio_vol_val) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)val);
        lv_label_set_text(s->lbl_audio_vol_val, buf);
    }
}

static void on_top_tab_btn_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    lv_obj_t *btn = lv_event_get_target(e);
    if (btn == s->btn_tab_net) {
        ui_settings_switch_tab(s, UI_SETTINGS_TAB_NET);
    } else if (btn == s->btn_tab_ble) {
        ui_settings_switch_tab(s, UI_SETTINGS_TAB_BLE);
    } else if (btn == s->btn_tab_agent) {
        ui_settings_switch_tab(s, UI_SETTINGS_TAB_AGENT);
    } else if (btn == s->btn_tab_system) {
        ui_settings_switch_tab(s, UI_SETTINGS_TAB_SYSTEM);
    } else if (btn == s->btn_tab_storage) {
        ui_settings_switch_tab(s, UI_SETTINGS_TAB_STORAGE);
    }
}

static void on_sub_tab_btn_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    lv_obj_t *btn = lv_event_get_target(e);
    if (btn == s->btn_tab_hotspot) {
        ui_settings_switch_tab(s, UI_SETTINGS_TAB_HOTSPOT);
    } else if (btn == s->btn_tab_ble) {
        ui_settings_switch_tab(s, UI_SETTINGS_TAB_BLE);
    } else if (btn == s->btn_tab_agent) {
        ui_settings_switch_tab(s, UI_SETTINGS_TAB_AGENT);
    } else if (btn == s->btn_tab_system) {
        ui_settings_switch_tab(s, UI_SETTINGS_TAB_SYSTEM);
    } else if (btn == s->btn_tab_storage) {
        ui_settings_switch_tab(s, UI_SETTINGS_TAB_STORAGE);
    }
}

static void on_hotspot_action_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    net_mode_t mode = net_mgr_get_mode();
    if (mode == NET_MODE_SOFTAP_CONFIG) {
        LOG_I(TAG, "用户在设置面板手动关闭 SoftAP 独立热点");
        net_mgr_stop_softap();
        if (s->lbl_hotspot_action) {
            lv_label_set_text(s->lbl_hotspot_action, "[✓ 热点已关闭]");
            lv_obj_set_style_text_color(s->lbl_hotspot_action, lv_color_hex(0x7E92AD), 0);
        }
    } else {
        LOG_I(TAG, "用户在设置面板手动开启 SoftAP 独立热点");
        net_mgr_start_softap("Gemini-Agent-Setup");
        if (s->lbl_hotspot_action) {
            lv_label_set_text(s->lbl_hotspot_action, "[✓ 热点启动中...]");
            lv_obj_set_style_text_color(s->lbl_hotspot_action, lv_color_hex(0x00FF88), 0);
        }
    }
    ui_settings_refresh_data(s);
}

static void on_hotspot_reset_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    LOG_I(TAG, "用户在设置面板点击清空网络配置");
    net_mgr_clear_config();

    if (s->lbl_hotspot_reset) {
        lv_label_set_text(s->lbl_hotspot_reset, "[✓ 网络配置已清空]");
        lv_obj_set_style_text_color(s->lbl_hotspot_reset, lv_color_hex(0xFFB700), 0);
    }
    ui_settings_refresh_data(s);
}

static void on_ble_toggle_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    if (ble_prov_service_get_state() == BLE_PROV_STATE_STARTING) {
        LOG_I(TAG, "蓝牙正在使能初始化中，请稍候...");
        return;
    }

    if (ble_prov_service_is_active()) {
        LOG_I(TAG, "用户点击停止蓝牙广播");
        ble_prov_service_deinit();
    } else {
        LOG_I(TAG, "用户点击启动蓝牙配网广播 (异步非阻塞)");
        int ret = ble_prov_service_start_async(NULL);
        if (ret != 0) {
            LOG_E(TAG, "启动蓝牙配网广播失败, ret: %d", ret);
        }
    }

    ui_settings_refresh_data(s);
}

static void on_prog_done_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    net_mode_t mode = net_mgr_get_mode();
    if (mode == NET_MODE_STA_CONNECTED) {
        /* 配网完成，如果注册了返回主页回调则直接返回主页 */
        if (s->on_switch_home_cb) {
            s->on_switch_home_cb(s->switch_home_user_data);
        } else {
            ui_settings_close(s);
        }
    } else {
        /* 处于断网、失败或 SoftAP 状态，点击切回待机卡片 */
        if (s->box_hotspot_idle) lv_obj_clear_flag(s->box_hotspot_idle, LV_OBJ_FLAG_HIDDEN);
        if (s->box_hotspot_progress) lv_obj_add_flag(s->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN);
        ui_settings_refresh_data(s);
    }
}

void ui_settings_set_close_cb(ui_settings_t *settings, void (*cb)(void *), void *user_data)
{
    if (!settings) return;
    settings->on_close_cb = cb;
    settings->close_user_data = user_data;
}

void ui_settings_set_switch_home_cb(ui_settings_t *settings, void (*cb)(void *), void *user_data)
{
    if (!settings) return;
    settings->on_switch_home_cb = cb;
    settings->switch_home_user_data = user_data;
}

/* ========================================================================= */
/*                   Wi-Fi 扫描列表与软键盘密码弹窗实现                       */
/* ========================================================================= */

void ui_settings_show_password_dialog(ui_settings_t *settings, const char *ssid)
{
    if (!settings || !settings->dlg_pwd_modal || !ssid) return;

    strncpy(settings->selected_ssid, ssid, sizeof(settings->selected_ssid) - 1);
    settings->selected_ssid[sizeof(settings->selected_ssid) - 1] = '\0';

    if (settings->lbl_pwd_target) {
        char buf[64];
        snprintf(buf, sizeof(buf), "📶 连接: %s", settings->selected_ssid);
        lv_label_set_text(settings->lbl_pwd_target, buf);
    }

    if (settings->ta_pwd_input) {
        lv_textarea_set_text(settings->ta_pwd_input, "");
        lv_textarea_set_password_mode(settings->ta_pwd_input, false);
        settings->is_pwd_obscure = false;
    }

    if (settings->lbl_pwd_eye) {
        lv_label_set_text(settings->lbl_pwd_eye, "明");
        lv_obj_set_style_text_color(settings->lbl_pwd_eye, lv_color_hex(0x00E5FF), 0);
    }

    if (settings->lbl_pwd_hint) {
        lv_label_set_text(settings->lbl_pwd_hint, "请输入 Wi-Fi 密码 (不少于8位)");
        lv_obj_set_style_text_color(settings->lbl_pwd_hint, lv_color_hex(0x8B9EB5), 0);
    }

    lv_obj_clear_flag(settings->dlg_pwd_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(settings->dlg_pwd_modal);
}

void ui_settings_close_password_dialog(ui_settings_t *settings)
{
    if (!settings || !settings->dlg_pwd_modal) return;
    lv_obj_add_flag(settings->dlg_pwd_modal, LV_OBJ_FLAG_HIDDEN);
}

void ui_settings_refresh_wifi_list(ui_settings_t *settings)
{
    if (!settings || !settings->list_wifi) return;

    /* 1. 清除旧列表项 (保留 lbl_wifi_empty) */
    uint32_t child_cnt = lv_obj_get_child_cnt(settings->list_wifi);
    for (int i = (int)child_cnt - 1; i >= 0; i--) {
        lv_obj_t *child = lv_obj_get_child(settings->list_wifi, i);
        if (child && child != settings->lbl_wifi_empty) {
            lv_obj_delete(child);
        }
    }

    /* 2. 获取扫描快照 */
    net_wifi_ap_info_t aps[20];
    int count = net_mgr_get_cached_scan_results(aps, 20);

    /* 3. 获取当前已连接 SSID */
    char cur_ssid[34] = {0};
    net_mgr_get_ssid(cur_ssid, sizeof(cur_ssid));
    net_mode_t cur_mode = net_mgr_get_mode();

    if (count <= 0) {
        if (settings->lbl_wifi_empty) {
            lv_obj_clear_flag(settings->lbl_wifi_empty, LV_OBJ_FLAG_HIDDEN);
            if (net_mgr_is_scanning()) {
                lv_label_set_text(settings->lbl_wifi_empty, "📡 正在搜索周边 Wi-Fi...");
                lv_obj_set_style_text_color(settings->lbl_wifi_empty, lv_color_hex(0xFFB700), 0);
            } else {
                lv_label_set_text(settings->lbl_wifi_empty, "未搜索到 Wi-Fi，请点击右上角刷新");
                lv_obj_set_style_text_color(settings->lbl_wifi_empty, lv_color_hex(0x7E92AD), 0);
                /* 自动触发一次后台扫描 */
                net_mgr_trigger_async_scan();
            }
        }
        return;
    }

    /* 3.1 排序策略：
     * (1) 若当前已连接 Wi-Fi，置顶排在第 1 位
     * (2) 其余热点严格按照信号强度 RSSI 由强到弱降序排列 (例如 -45dBm > -65dBm > -85dBm)
     */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - 1 - i; j++) {
            bool j_is_conn = (cur_mode == NET_MODE_STA_CONNECTED && strcmp(cur_ssid, aps[j].ssid) == 0);
            bool next_is_conn = (cur_mode == NET_MODE_STA_CONNECTED && strcmp(cur_ssid, aps[j + 1].ssid) == 0);

            bool need_swap = false;
            if (next_is_conn && !j_is_conn) {
                /* 已连接热点优先置顶 */
                need_swap = true;
            } else if (!next_is_conn && !j_is_conn) {
                /* 其余按 RSSI 降序排列 (信号强/数值大者排在前面) */
                if (aps[j].rssi < aps[j + 1].rssi) {
                    need_swap = true;
                }
            }

            if (need_swap) {
                net_wifi_ap_info_t tmp = aps[j];
                aps[j] = aps[j + 1];
                aps[j + 1] = tmp;
            }
        }
    }

    if (settings->lbl_wifi_empty) {
        lv_obj_add_flag(settings->lbl_wifi_empty, LV_OBJ_FLAG_HIDDEN);
    }

    /* 4. 遍历创建 Wi-Fi 列表条目 */
    int valid_idx = 0;
    for (int i = 0; i < count; i++) {
        if (aps[i].ssid[0] == '\0') continue;

        lv_obj_t *btn_ap = lv_btn_create(settings->list_wifi);
        lv_obj_set_size(btn_ap, 248, 30);
        lv_obj_set_pos(btn_ap, 1, (lv_coord_t)(valid_idx * 34));
        lv_obj_set_style_radius(btn_ap, 4, 0);
        lv_obj_set_style_pad_all(btn_ap, 2, 0);
        valid_idx++;

        bool is_connected = (cur_mode == NET_MODE_STA_CONNECTED && strcmp(cur_ssid, aps[i].ssid) == 0);
        if (is_connected) {
            lv_obj_set_style_bg_color(btn_ap, lv_color_hex(0x0C2B22), 0);
            lv_obj_set_style_border_color(btn_ap, lv_color_hex(0x00FF88), 0);
            lv_obj_set_style_border_width(btn_ap, 1, 0);
        } else {
            lv_obj_set_style_bg_color(btn_ap, lv_color_hex(0x0E1726), 0);
            lv_obj_set_style_border_color(btn_ap, lv_color_hex(0x1C2F4D), 0);
            lv_obj_set_style_border_width(btn_ap, 1, 0);
        }

        lv_obj_add_event_cb(btn_ap, on_wifi_item_clicked, LV_EVENT_CLICKED, settings);

        /* 左侧: SSID 与信号 */
        lv_obj_t *lbl_name = lv_label_create(btn_ap);
        lv_obj_align(lbl_name, LV_ALIGN_LEFT_MID, 6, 0);
        if (settings->font) lv_obj_set_style_text_font(lbl_name, settings->font, 0);

        char title_buf[48];
        snprintf(title_buf, sizeof(title_buf), "📶 %s", aps[i].ssid);
        lv_label_set_text(lbl_name, title_buf);
        if (is_connected) {
            lv_obj_set_style_text_color(lbl_name, lv_color_hex(0x00FF88), 0);
        } else {
            lv_obj_set_style_text_color(lbl_name, lv_color_hex(0xE2E8F0), 0);
        }

        /* 右侧: 加密锁与信号强度等级 */
        lv_obj_t *lbl_tag = lv_label_create(btn_ap);
        lv_obj_align(lbl_tag, LV_ALIGN_RIGHT_MID, -6, 0);
        if (settings->font) lv_obj_set_style_text_font(lbl_tag, settings->font, 0);

        if (is_connected) {
            lv_label_set_text(lbl_tag, "[已连接]");
            lv_obj_set_style_text_color(lbl_tag, lv_color_hex(0x00FF88), 0);
        } else {
            char tag_buf[24];
            bool is_locked = (aps[i].auth[0] != '\0' && strstr(aps[i].auth, "OPEN") == NULL);
            const char *sig_str = (aps[i].rssi >= -60) ? "强" : (aps[i].rssi >= -75) ? "中" : "弱";
            snprintf(tag_buf, sizeof(tag_buf), "%s %s", is_locked ? "🔒" : "开放", sig_str);
            lv_label_set_text(lbl_tag, tag_buf);
            lv_obj_set_style_text_color(lbl_tag, is_locked ? lv_color_hex(0x7E92AD) : lv_color_hex(0x8B9EB5), 0);
        }
    }
}

static void on_wifi_refresh_clicked(lv_event_t *e)
{
    (void)e;
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    LOG_I(TAG, "用户手动点击刷新 Wi-Fi 扫描");
    net_mgr_trigger_async_scan();
    if (s->lbl_wifi_status) {
        lv_label_set_text(s->lbl_wifi_status, "📡 正在搜索周边 Wi-Fi...");
        lv_obj_set_style_text_color(s->lbl_wifi_status, lv_color_hex(0xFFB700), 0);
    }
    ui_settings_refresh_wifi_list(s);
}

static void on_wifi_item_clicked(lv_event_t *e)
{
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!btn || !s) return;

    /* 获取第 1 个子控件 label */
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (!lbl) return;
    const char *text = lv_label_get_text(lbl);
    if (!text) return;

    /* 跳过开头的 "📶 " */
    const char *ssid = text;
    const char *p = strchr(text, ' ');
    if (p) {
        ssid = p + 1;
    }

    char cur_ssid[34] = {0};
    net_mgr_get_ssid(cur_ssid, sizeof(cur_ssid));
    if (net_mgr_get_mode() == NET_MODE_STA_CONNECTED && strcmp(cur_ssid, ssid) == 0) {
        LOG_I(TAG, "当前已连该 Wi-Fi: %s", ssid);
        return;
    }

    LOG_I(TAG, "用户选择目标 Wi-Fi: [%s], 弹出全屏输入软键盘", ssid);
    ui_settings_show_password_dialog(s, ssid);
}

static void on_pwd_connect_clicked(lv_event_t *e)
{
    (void)e;
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s || !s->ta_pwd_input) return;

    const char *pwd = lv_textarea_get_text(s->ta_pwd_input);
    size_t pwd_len = pwd ? strlen(pwd) : 0;

    if (pwd_len > 0 && pwd_len < 8) {
        if (s->lbl_pwd_hint) {
            lv_label_set_text(s->lbl_pwd_hint, "⚠️ 密码长度不能少于 8 位");
            lv_obj_set_style_text_color(s->lbl_pwd_hint, lv_color_hex(0xFF5555), 0);
        }
        return;
    }

    if (s->lbl_pwd_hint) {
        lv_label_set_text(s->lbl_pwd_hint, "⏳ 正在发起连接，请稍候...");
        lv_obj_set_style_text_color(s->lbl_pwd_hint, lv_color_hex(0x00FF88), 0);
    }

    LOG_I(TAG, "用户在界面输入密码并触发连接 Wi-Fi: [%s]", s->selected_ssid);
    net_mgr_connect_sta(s->selected_ssid, pwd ? pwd : "");

    ui_settings_close_password_dialog(s);
    ui_settings_refresh_data(s);
}

static void on_pwd_close_clicked(lv_event_t *e)
{
    (void)e;
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (s) {
        ui_settings_close_password_dialog(s);
    }
}

static void on_pwd_eye_clicked(lv_event_t *e)
{
    (void)e;
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s || !s->ta_pwd_input) return;

    s->is_pwd_obscure = !s->is_pwd_obscure;
    lv_textarea_set_password_mode(s->ta_pwd_input, s->is_pwd_obscure);
    if (s->lbl_pwd_eye) {
        lv_label_set_text(s->lbl_pwd_eye, s->is_pwd_obscure ? "密" : "明");
        lv_obj_set_style_text_color(s->lbl_pwd_eye,
            s->is_pwd_obscure ? lv_color_hex(0x7E92AD) : lv_color_hex(0x00E5FF), 0);
    }
}

static void on_pwd_kb_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    if (code == LV_EVENT_READY) {
        on_pwd_connect_clicked(e);
    } else if (code == LV_EVENT_CANCEL) {
        ui_settings_close_password_dialog(s);
    }
}

static void on_wifi_forget_clicked(lv_event_t *e)
{
    (void)e;
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    LOG_I(TAG, "用户点击清除已存 Wi-Fi 配置");
    net_mgr_clear_config();
    ui_settings_refresh_data(s);
    ui_settings_refresh_wifi_list(s);
}

static void on_wifi_ap_mode_clicked(lv_event_t *e)
{
    (void)e;
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    net_mode_t mode = net_mgr_get_mode();
    if (mode == NET_MODE_SOFTAP_CONFIG) {
        LOG_I(TAG, "用户在 Wi-Fi 面板手动关闭 SoftAP 热点");
        net_mgr_stop_softap();
    } else {
        LOG_I(TAG, "用户在 Wi-Fi 面板手动启动 SoftAP 应急热点");
        net_mgr_start_softap("Gemini-Agent-Setup");
    }
    ui_settings_refresh_data(s);
}

