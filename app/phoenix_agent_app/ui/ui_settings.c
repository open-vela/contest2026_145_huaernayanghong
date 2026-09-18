/**
 * @file ui_settings.c
 * @brief 设置与控制中心同级视图组件实现 (Peer Stage Settings & Sub-Sidebar Navigation)
 * @author OpenVela Contest 2026 Team 145
 */

#include "ui_settings.h"
#include "../hal/network_mgr.h"
#include "../hal/ble_prov_service.h"
#include "../hal/hal_manager.h"
#include "../hal/hal_system.h"
#include "../hal/hal_sdcard.h"
#include "../core/config.h"
#include "../utils/log_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "UISettings"

/* 按钮点击事件处理声明 */
static void on_top_close_clicked(lv_event_t *e);
static void on_menu_item_clicked(lv_event_t *e);
static void on_top_tab_btn_clicked(lv_event_t *e);
static void on_sub_tab_btn_clicked(lv_event_t *e);
static void on_hotspot_action_clicked(lv_event_t *e);
static void on_hotspot_reset_clicked(lv_event_t *e);
static void on_ble_toggle_clicked(lv_event_t *e);
static void on_prog_done_clicked(lv_event_t *e);

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

    /* 创建 6 个菜单项卡片 (高度 34px, 宽度 260px，使用字库安全符号) */
    const char *menu_names[6] = {
        "● Wi-Fi网络",
        "● 蓝牙配网",
        "● 灵眸模型",
        "● 硬件状态",
        "● 存储日志",
        "● 关于设备"
    };
    const char *menu_defaults[6] = {
        "已连接 >",
        "未开启 >",
        "DeepSeek >",
        "60FPS / 正常 >",
        "28.6GB >",
        "R528-S3 >"
    };
    lv_obj_t **menu_btns[6] = {
        &s->btn_menu_net,
        &s->btn_menu_ble,
        &s->btn_menu_agent,
        &s->btn_menu_system,
        &s->btn_menu_storage,
        &s->btn_menu_about
    };
    lv_obj_t **menu_subs[6] = {
        &s->lbl_menu_net_sub,
        &s->lbl_menu_ble_sub,
        &s->lbl_menu_agent_sub,
        &s->lbl_menu_system_sub,
        &s->lbl_menu_storage_sub,
        &s->lbl_menu_about_sub
    };

    for (int i = 0; i < 6; i++) {
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
        lv_obj_set_style_text_color(lbl_sub, (i == 0 || i == 1 || i == 3) ? lv_color_hex(0x00FF88) : lv_color_hex(0x00E5FF), 0);

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
     * 4. 统一网络配网面板 (Magic Provisioning: SoftAP + BLE 一体)
     * ===================================================================== */
    s->panel_hotspot = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_hotspot, 266, 176);
    lv_obj_align(s->panel_hotspot, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_hotspot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_hotspot, 0, 0);
    lv_obj_set_style_pad_all(s->panel_hotspot, 0, 0);
    lv_obj_clear_flag(s->panel_hotspot, LV_OBJ_FLAG_SCROLLABLE);

    /* 4.1 常规/已连/广播待配网卡片 */
    s->box_hotspot_idle = lv_obj_create(s->panel_hotspot);
    lv_obj_set_size(s->box_hotspot_idle, 266, 176);
    lv_obj_set_pos(s->box_hotspot_idle, 0, 0);
    lv_obj_set_style_bg_opa(s->box_hotspot_idle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->box_hotspot_idle, 0, 0);
    lv_obj_set_style_pad_all(s->box_hotspot_idle, 0, 0);
    lv_obj_clear_flag(s->box_hotspot_idle, LV_OBJ_FLAG_SCROLLABLE);

    s->card_hotspot_info = lv_obj_create(s->box_hotspot_idle);
    lv_obj_set_size(s->card_hotspot_info, 260, 92);
    lv_obj_align(s->card_hotspot_info, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->card_hotspot_info, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->card_hotspot_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_hotspot_info, 1, 0);
    lv_obj_set_style_radius(s->card_hotspot_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_hotspot_info, 4, 0);
    lv_obj_clear_flag(s->card_hotspot_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_hotspot_ssid = lv_label_create(s->card_hotspot_info);
    lv_obj_align(s->lbl_hotspot_ssid, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_ssid, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_ssid, "热点: Gemini-Agent-Setup");
    lv_obj_set_style_text_color(s->lbl_hotspot_ssid, lv_color_hex(0x00FF88), 0);

    s->lbl_hotspot_ip = lv_label_create(s->card_hotspot_info);
    lv_obj_align(s->lbl_hotspot_ip, LV_ALIGN_TOP_LEFT, 4, 24);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_ip, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_ip, "网址: http://192.168.4.1/");
    lv_obj_set_style_text_color(s->lbl_hotspot_ip, lv_color_hex(0x00E5FF), 0);

    s->lbl_hotspot_hint = lv_label_create(s->card_hotspot_info);
    lv_obj_align(s->lbl_hotspot_hint, LV_ALIGN_TOP_LEFT, 4, 46);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_hint, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_hint, "● 手机连此热点进入网页配网 (BLE广播已并行)\n内嵌 MiniDHCP 服务已就绪");
    lv_obj_set_style_text_color(s->lbl_hotspot_hint, lv_color_hex(0x8B9EB5), 0);

    /* 重启热点按钮 */
    s->btn_hotspot_action = lv_btn_create(s->box_hotspot_idle);
    lv_obj_set_size(s->btn_hotspot_action, 260, 34);
    lv_obj_align(s->btn_hotspot_action, LV_ALIGN_TOP_MID, 0, 98);
    lv_obj_set_style_bg_color(s->btn_hotspot_action, lv_color_hex(0x13273F), 0);
    lv_obj_set_style_border_color(s->btn_hotspot_action, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->btn_hotspot_action, 1, 0);
    lv_obj_set_style_radius(s->btn_hotspot_action, 6, 0);
    lv_obj_add_event_cb(s->btn_hotspot_action, on_hotspot_action_clicked, LV_EVENT_CLICKED, s);

    s->lbl_hotspot_action = lv_label_create(s->btn_hotspot_action);
    lv_obj_center(s->lbl_hotspot_action);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_action, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_action, "[● 重启热点广播]");
    lv_obj_set_style_text_color(s->lbl_hotspot_action, lv_color_hex(0x00E5FF), 0);

    /* 重置网络配置按钮 */
    s->btn_hotspot_reset = lv_btn_create(s->box_hotspot_idle);
    lv_obj_set_size(s->btn_hotspot_reset, 260, 32);
    lv_obj_align(s->btn_hotspot_reset, LV_ALIGN_TOP_MID, 0, 138);
    lv_obj_set_style_bg_color(s->btn_hotspot_reset, lv_color_hex(0x151B27), 0);
    lv_obj_set_style_border_color(s->btn_hotspot_reset, lv_color_hex(0x28384E), 0);
    lv_obj_set_style_border_width(s->btn_hotspot_reset, 1, 0);
    lv_obj_set_style_radius(s->btn_hotspot_reset, 6, 0);
    lv_obj_add_event_cb(s->btn_hotspot_reset, on_hotspot_reset_clicked, LV_EVENT_CLICKED, s);

    s->lbl_hotspot_reset = lv_label_create(s->btn_hotspot_reset);
    lv_obj_center(s->lbl_hotspot_reset);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_reset, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_reset, "[清空网络配置]");
    lv_obj_set_style_text_color(s->lbl_hotspot_reset, lv_color_hex(0x7E92AD), 0);

    /* 4.2 热点配网步进状态机进度卡片 */
    s->box_hotspot_progress = lv_obj_create(s->panel_hotspot);
    lv_obj_set_size(s->box_hotspot_progress, 266, 176);
    lv_obj_set_pos(s->box_hotspot_progress, 0, 0);
    lv_obj_set_style_bg_opa(s->box_hotspot_progress, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->box_hotspot_progress, 0, 0);
    lv_obj_set_style_pad_all(s->box_hotspot_progress, 0, 0);
    lv_obj_clear_flag(s->box_hotspot_progress, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN);

    s->lbl_prog_title = lv_label_create(s->box_hotspot_progress);
    lv_obj_align(s->lbl_prog_title, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_title, s->font, 0);
    lv_label_set_text(s->lbl_prog_title, "[>>] 正在加入网络...");
    lv_obj_set_style_text_color(s->lbl_prog_title, lv_color_hex(0xFFB700), 0);

    s->lbl_prog_step1 = lv_label_create(s->box_hotspot_progress);
    lv_obj_align(s->lbl_prog_step1, LV_ALIGN_TOP_LEFT, 4, 24);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_step1, s->font, 0);
    lv_label_set_text(s->lbl_prog_step1, "[✓] 1. 收到网页指令，已关闭热点");
    lv_obj_set_style_text_color(s->lbl_prog_step1, lv_color_hex(0x00FF88), 0);

    s->lbl_prog_step2 = lv_label_create(s->box_hotspot_progress);
    lv_obj_align(s->lbl_prog_step2, LV_ALIGN_TOP_LEFT, 4, 44);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_step2, s->font, 0);
    lv_label_set_text(s->lbl_prog_step2, "[..] 2. 正在关联目标 Wi-Fi 路由...");
    lv_obj_set_style_text_color(s->lbl_prog_step2, lv_color_hex(0xFFB700), 0);

    s->lbl_prog_step3 = lv_label_create(s->box_hotspot_progress);
    lv_obj_align(s->lbl_prog_step3, LV_ALIGN_TOP_LEFT, 4, 64);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_step3, s->font, 0);
    lv_label_set_text(s->lbl_prog_step3, "[..] 3. 申请 DHCP 局域网 IP 租约...");
    lv_obj_set_style_text_color(s->lbl_prog_step3, lv_color_hex(0x7E92AD), 0);

    /* 结果大卡片 */
    s->card_prog_result = lv_obj_create(s->box_hotspot_progress);
    lv_obj_set_size(s->card_prog_result, 260, 56);
    lv_obj_align(s->card_prog_result, LV_ALIGN_TOP_MID, 0, 86);
    lv_obj_set_style_bg_color(s->card_prog_result, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_prog_result, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_prog_result, 1, 0);
    lv_obj_set_style_radius(s->card_prog_result, 6, 0);
    lv_obj_set_style_pad_all(s->card_prog_result, 4, 0);
    lv_obj_clear_flag(s->card_prog_result, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_prog_result_ip = lv_label_create(s->card_prog_result);
    lv_obj_align(s->lbl_prog_result_ip, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_result_ip, s->font, 0);
    lv_label_set_text(s->lbl_prog_result_ip, "正在与网关握手...");
    lv_obj_set_style_text_color(s->lbl_prog_result_ip, lv_color_hex(0xFFB700), 0);

    s->lbl_prog_result_url = lv_label_create(s->card_prog_result);
    lv_obj_align(s->lbl_prog_result_url, LV_ALIGN_TOP_LEFT, 4, 24);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_result_url, s->font, 0);
    lv_label_set_text(s->lbl_prog_result_url, "请稍候，连网完成后将更新 IP");
    lv_obj_set_style_text_color(s->lbl_prog_result_url, lv_color_hex(0x8B9EB5), 0);

    /* 步进操作按钮 */
    s->btn_prog_done = lv_btn_create(s->box_hotspot_progress);
    lv_obj_set_size(s->btn_prog_done, 260, 32);
    lv_obj_align(s->btn_prog_done, LV_ALIGN_TOP_MID, 0, 144);
    lv_obj_set_style_bg_color(s->btn_prog_done, lv_color_hex(0x13273F), 0);
    lv_obj_set_style_border_color(s->btn_prog_done, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->btn_prog_done, 1, 0);
    lv_obj_set_style_radius(s->btn_prog_done, 6, 0);
    lv_obj_add_event_cb(s->btn_prog_done, on_prog_done_clicked, LV_EVENT_CLICKED, s);

    s->lbl_prog_done = lv_label_create(s->btn_prog_done);
    lv_obj_center(s->lbl_prog_done);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_done, s->font, 0);
    lv_label_set_text(s->lbl_prog_done, "[✓ 连网中...]");
    lv_obj_set_style_text_color(s->lbl_prog_done, lv_color_hex(0x00E5FF), 0);

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
    lv_label_set_text(s->lbl_ble_status, "● 蓝牙状态: 广播中");
    lv_obj_set_style_text_color(s->lbl_ble_status, lv_color_hex(0x00FF88), 0);

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
    lv_label_set_text(s->lbl_ble_toggle, "[● 重启蓝牙配网广播]");
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

    const char *tab_titles[6] = {
        "Wi-Fi网络连接",
        "蓝牙极速配网",
        "灵眸大模型",
        "硬件状态与遥测",
        "存储卡与外脑日志",
        "关于设备"
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
    lv_obj_t *panels[6] = {
        settings->panel_hotspot,
        settings->panel_ble,
        settings->panel_agent,
        settings->panel_system,
        settings->panel_storage,
        settings->panel_about
    };
    for (int i = 0; i < 6; i++) {
        if (panels[i]) {
            if (i == (int)tab) lv_obj_clear_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    ui_settings_refresh_data(settings);
    LOG_I(TAG, "已下钻进入二级设置详情: [%s]", tab_titles[(int)tab]);
}

void ui_settings_back_to_menu(ui_settings_t *settings)
{
    if (!settings) return;

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
                lv_label_set_text(settings->lbl_prog_step3, "[..] 3. 正在申请 DHCP 局域网 IP...");
                lv_obj_set_style_text_color(settings->lbl_prog_step3, lv_color_hex(0xFFB700), 0);
            }
            if (settings->lbl_prog_result_ip) {
                lv_label_set_text(settings->lbl_prog_result_ip, "Wi-Fi 关联成功，正在协商 IP...");
                lv_obj_set_style_text_color(settings->lbl_prog_result_ip, lv_color_hex(0xFFB700), 0);
            }
        } else {
            if (settings->lbl_prog_step2) {
                lv_label_set_text(settings->lbl_prog_step2, "[..] 2. 正在关联目标 Wi-Fi 路由...");
                lv_obj_set_style_text_color(settings->lbl_prog_step2, lv_color_hex(0xFFB700), 0);
            }
            if (settings->lbl_prog_step3) {
                lv_label_set_text(settings->lbl_prog_step3, "[..] 3. 申请 DHCP 局域网 IP 租约...");
                lv_obj_set_style_text_color(settings->lbl_prog_step3, lv_color_hex(0x7E92AD), 0);
            }
            if (settings->lbl_prog_result_ip) {
                lv_label_set_text(settings->lbl_prog_result_ip, "已向射频下发凭证，正在握手...");
                lv_obj_set_style_text_color(settings->lbl_prog_result_ip, lv_color_hex(0xFFB700), 0);
            }
        }
        if (settings->lbl_prog_result_url) {
            lv_label_set_text(settings->lbl_prog_result_url, "设备正面微胶囊将同步呈现连网");
            lv_obj_set_style_text_color(settings->lbl_prog_result_url, lv_color_hex(0x8B9EB5), 0);
        }
        if (settings->lbl_prog_done) {
            lv_label_set_text(settings->lbl_prog_done, "[.. 连网进行中...]");
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
                lv_label_set_text(settings->lbl_prog_step2, "[x] 2. 路由关联或 DHCP 失败");
                lv_obj_set_style_text_color(settings->lbl_prog_step2, lv_color_hex(0xFF5252), 0);
            }
            if (settings->lbl_prog_result_ip) {
                lv_label_set_text(settings->lbl_prog_result_ip, "正在自动重启 SoftAP 热点...");
                lv_obj_set_style_text_color(settings->lbl_prog_result_ip, lv_color_hex(0xFFB700), 0);
            }
            if (settings->lbl_prog_done) {
                lv_label_set_text(settings->lbl_prog_done, "[返回热点重试]");
                lv_obj_set_style_text_color(settings->lbl_prog_done, lv_color_hex(0xFF5252), 0);
            }
        }
    }
}

void ui_settings_refresh_data(ui_settings_t *settings)
{
    if (!settings) return;

    char ip_buf[32] = {0};
    char ssid_buf[32] = {0};
    net_mode_t mode = net_mgr_get_mode();
    net_mgr_get_ip(ip_buf, sizeof(ip_buf));
    net_mgr_get_ssid(ssid_buf, sizeof(ssid_buf));

    /* 1. 刷新热点页面 */
    if (mode == NET_MODE_STA_CONNECTING) {
        ui_settings_update_net_progress(settings, NET_MODE_STA_CONNECTING, ssid_buf, ip_buf, NULL);
    } else if (mode == NET_MODE_STA_CONNECTED) {
        ui_settings_update_net_progress(settings, NET_MODE_STA_CONNECTED, ssid_buf, ip_buf, NULL);
        /* 仅在非配网进度展示阶段，才呈现常规待配网/热点广播卡片 */
        if (!settings->box_hotspot_progress || lv_obj_has_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN)) {
            if (settings->box_hotspot_idle) lv_obj_clear_flag(settings->box_hotspot_idle, LV_OBJ_FLAG_HIDDEN);
            if (settings->box_hotspot_progress) lv_obj_add_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN);
        }

        if (settings->lbl_hotspot_ssid) {
            char buf[64];
            snprintf(buf, sizeof(buf), "热点: %s", ssid_buf[0] ? ssid_buf : "Gemini-Agent-Setup");
            lv_label_set_text(settings->lbl_hotspot_ssid, buf);
        }
    } else if (mode == NET_MODE_SOFTAP_CONFIG) {
        /* SoftAP 广播就绪后，按钮立即自动恢复为 [● 重启热点广播]，消除卡死 */
        if (settings->box_hotspot_idle) lv_obj_clear_flag(settings->box_hotspot_idle, LV_OBJ_FLAG_HIDDEN);
        if (settings->box_hotspot_progress) lv_obj_add_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN);

        if (settings->lbl_hotspot_ssid) {
            char buf[64];
            snprintf(buf, sizeof(buf), "热点: %s", ssid_buf[0] ? ssid_buf : "Gemini-Agent-Setup");
            lv_label_set_text(settings->lbl_hotspot_ssid, buf);
        }
        if (settings->lbl_hotspot_action) {
            lv_label_set_text(settings->lbl_hotspot_action, "[● 重启热点广播]");
            lv_obj_set_style_text_color(settings->lbl_hotspot_action, lv_color_hex(0x00E5FF), 0);
        }
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
            ble_prov_state_t bst = ble_prov_service_get_state();
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

    LOG_I(TAG, "用户点击重启 SoftAP 独立热点");
    net_mgr_start_softap("Gemini-Agent-Setup");

    if (s->lbl_hotspot_action) {
        lv_label_set_text(s->lbl_hotspot_action, "[✓ 热点广播已重置]");
        lv_obj_set_style_text_color(s->lbl_hotspot_action, lv_color_hex(0x00FF88), 0);
    }
    ui_settings_refresh_data(s);
}

static void on_hotspot_reset_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    LOG_I(TAG, "用户点击重置网络配置");
    net_mgr_reset_to_softap();

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

    if (ble_prov_service_is_active()) {
        LOG_I(TAG, "用户点击停止蓝牙广播");
        ble_prov_service_deinit();
    } else {
        LOG_I(TAG, "用户点击启动蓝牙配网广播");
        int ret = ble_prov_service_init(NULL);
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
    } else if (mode == NET_MODE_DISCONNECTED) {
        /* 失败状态，点击重新启动热点 */
        net_mgr_start_softap(NULL);
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
