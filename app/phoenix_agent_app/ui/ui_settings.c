/**
 * @file ui_settings.c
 * @brief 设置与控制中心同级视图组件实现 (Peer Stage Settings & Sub-Sidebar Navigation)
 * @author OpenVela Contest 2026 Team 145
 */

#include "ui_settings.h"
#include "../hal/network_mgr.h"
#include "../hal/ble_prov_service.h"
#include "../hal/hal_manager.h"
#include "../core/config.h"
#include "../utils/log_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "UISettings"

/* 按钮点击事件处理声明 */
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
    s->current_tab = UI_SETTINGS_TAB_HOTSPOT;

    /* 1. 设置主舞台容器 (与各卡带主画布同级对等: W=274, H=216, X=46, Y=24) */
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

    /* 2. 内部专属二级侧边栏 (宽 56px, 高 216px, x=0, y=0) */
    s->sub_sidebar = lv_obj_create(s->container);
    lv_obj_set_size(s->sub_sidebar, 56, 216);
    lv_obj_set_pos(s->sub_sidebar, 0, 0);
    lv_obj_set_style_bg_color(s->sub_sidebar, lv_color_hex(0x0A0F1A), 0);
    lv_obj_set_style_bg_opa(s->sub_sidebar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s->sub_sidebar, lv_color_hex(0x18263B), 0);
    lv_obj_set_style_border_width(s->sub_sidebar, 1, 0);
    lv_obj_set_style_border_side(s->sub_sidebar, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_radius(s->sub_sidebar, 0, 0);
    lv_obj_set_style_pad_all(s->sub_sidebar, 2, 0);
    lv_obj_clear_flag(s->sub_sidebar, LV_OBJ_FLAG_SCROLLABLE);

    /* 2.1 创建 5 个纵向二级导航按钮 */
    const char *tab_labels[5] = { "AP\n热点", "BLE\n蓝牙", "AI\n模型", "SYS\n遥测", "SD\n存储" };
    lv_obj_t **btns[5] = { &s->btn_tab_hotspot, &s->btn_tab_ble, &s->btn_tab_agent, &s->btn_tab_system, &s->btn_tab_storage };
    lv_obj_t **lbls[5] = { &s->lbl_tab_hotspot, &s->lbl_tab_ble, &s->lbl_tab_agent, &s->lbl_tab_system, &s->lbl_tab_storage };

    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = lv_btn_create(s->sub_sidebar);
        lv_obj_set_size(b, 48, 38);
        lv_obj_set_pos(b, 2, (lv_coord_t)(i * 42 + 4));
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_set_ext_click_area(b, 4);
        lv_obj_add_event_cb(b, on_sub_tab_btn_clicked, LV_EVENT_CLICKED, s);

        lv_obj_t *lbl = lv_label_create(b);
        lv_obj_center(lbl);
        if (s->font) lv_obj_set_style_text_font(lbl, s->font, 0);
        lv_label_set_text(lbl, tab_labels[i]);

        *btns[i] = b;
        *lbls[i] = lbl;
    }

    /* 3. 右侧内容区 (宽 218px, 高 216px, x=56, y=0) */
    s->content_area = lv_obj_create(s->container);
    lv_obj_set_size(s->content_area, 218, 216);
    lv_obj_set_pos(s->content_area, 56, 0);
    lv_obj_set_style_bg_opa(s->content_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->content_area, 0, 0);
    lv_obj_set_style_pad_all(s->content_area, 0, 0);
    lv_obj_clear_flag(s->content_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 3.1 顶部标签/标题栏 (高 24px) */
    s->header_bar = lv_obj_create(s->content_area);
    lv_obj_set_size(s->header_bar, 218, 24);
    lv_obj_set_pos(s->header_bar, 0, 0);
    lv_obj_set_style_bg_color(s->header_bar, lv_color_hex(0x0C1422), 0);
    lv_obj_set_style_border_color(s->header_bar, lv_color_hex(0x1B2C47), 0);
    lv_obj_set_style_border_width(s->header_bar, 1, 0);
    lv_obj_set_style_border_side(s->header_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(s->header_bar, 0, 0);
    lv_obj_clear_flag(s->header_bar, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_header_title = lv_label_create(s->header_bar);
    lv_obj_align(s->lbl_header_title, LV_ALIGN_LEFT_MID, 8, 0);
    if (s->font) lv_obj_set_style_text_font(s->lbl_header_title, s->font, 0);
    lv_label_set_text(s->lbl_header_title, "⚡ 独立热点配网 (SoftAP)");
    lv_obj_set_style_text_color(s->lbl_header_title, lv_color_hex(0x00E5FF), 0);

    /* 3.2 视图主画布容器 (高 192px) */
    s->body_area = lv_obj_create(s->content_area);
    lv_obj_set_size(s->body_area, 218, 192);
    lv_obj_set_pos(s->body_area, 0, 24);
    lv_obj_set_style_bg_opa(s->body_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->body_area, 0, 0);
    lv_obj_set_style_pad_all(s->body_area, 4, 0);
    lv_obj_clear_flag(s->body_area, LV_OBJ_FLAG_SCROLLABLE);

    /* =====================================================================
     * 4. 独立标签页 1: 热点配网 (SoftAP)
     * ===================================================================== */
    s->panel_hotspot = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_hotspot, 210, 184);
    lv_obj_align(s->panel_hotspot, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_hotspot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_hotspot, 0, 0);
    lv_obj_set_style_pad_all(s->panel_hotspot, 0, 0);
    lv_obj_clear_flag(s->panel_hotspot, LV_OBJ_FLAG_SCROLLABLE);

    /* 4.1 常规/广播待配网卡片 */
    s->box_hotspot_idle = lv_obj_create(s->panel_hotspot);
    lv_obj_set_size(s->box_hotspot_idle, 210, 184);
    lv_obj_set_pos(s->box_hotspot_idle, 0, 0);
    lv_obj_set_style_bg_opa(s->box_hotspot_idle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->box_hotspot_idle, 0, 0);
    lv_obj_set_style_pad_all(s->box_hotspot_idle, 0, 0);
    lv_obj_clear_flag(s->box_hotspot_idle, LV_OBJ_FLAG_SCROLLABLE);

    s->card_hotspot_info = lv_obj_create(s->box_hotspot_idle);
    lv_obj_set_size(s->card_hotspot_info, 206, 92);
    lv_obj_align(s->card_hotspot_info, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->card_hotspot_info, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->card_hotspot_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_hotspot_info, 1, 0);
    lv_obj_set_style_radius(s->card_hotspot_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_hotspot_info, 4, 0);
    lv_obj_clear_flag(s->card_hotspot_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_hotspot_ssid = lv_label_create(s->card_hotspot_info);
    lv_obj_align(s->lbl_hotspot_ssid, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_ssid, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_ssid, "热点: Gemini-Agent-Setup");
    lv_obj_set_style_text_color(s->lbl_hotspot_ssid, lv_color_hex(0x00FF88), 0);

    s->lbl_hotspot_ip = lv_label_create(s->card_hotspot_info);
    lv_obj_align(s->lbl_hotspot_ip, LV_ALIGN_TOP_LEFT, 2, 24);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_ip, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_ip, "网址: http://192.168.4.1/");
    lv_obj_set_style_text_color(s->lbl_hotspot_ip, lv_color_hex(0x00E5FF), 0);

    s->lbl_hotspot_hint = lv_label_create(s->card_hotspot_info);
    lv_obj_align(s->lbl_hotspot_hint, LV_ALIGN_TOP_LEFT, 2, 46);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_hint, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_hint, "● 手机连此热点进入网页配网\n内嵌 MiniDHCP 服务已就绪");
    lv_obj_set_style_text_color(s->lbl_hotspot_hint, lv_color_hex(0x8B9EB5), 0);

    /* 重启热点按钮 */
    s->btn_hotspot_action = lv_btn_create(s->box_hotspot_idle);
    lv_obj_set_size(s->btn_hotspot_action, 206, 36);
    lv_obj_align(s->btn_hotspot_action, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_bg_color(s->btn_hotspot_action, lv_color_hex(0x13273F), 0);
    lv_obj_set_style_border_color(s->btn_hotspot_action, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->btn_hotspot_action, 1, 0);
    lv_obj_set_style_radius(s->btn_hotspot_action, 6, 0);
    lv_obj_add_event_cb(s->btn_hotspot_action, on_hotspot_action_clicked, LV_EVENT_CLICKED, s);

    s->lbl_hotspot_action = lv_label_create(s->btn_hotspot_action);
    lv_obj_center(s->lbl_hotspot_action);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_action, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_action, "[📡 重启热点广播]");
    lv_obj_set_style_text_color(s->lbl_hotspot_action, lv_color_hex(0x00E5FF), 0);

    /* 重置网络配置按钮 */
    s->btn_hotspot_reset = lv_btn_create(s->box_hotspot_idle);
    lv_obj_set_size(s->btn_hotspot_reset, 206, 34);
    lv_obj_align(s->btn_hotspot_reset, LV_ALIGN_TOP_MID, 0, 142);
    lv_obj_set_style_bg_color(s->btn_hotspot_reset, lv_color_hex(0x151B27), 0);
    lv_obj_set_style_border_color(s->btn_hotspot_reset, lv_color_hex(0x28384E), 0);
    lv_obj_set_style_border_width(s->btn_hotspot_reset, 1, 0);
    lv_obj_set_style_radius(s->btn_hotspot_reset, 6, 0);
    lv_obj_add_event_cb(s->btn_hotspot_reset, on_hotspot_reset_clicked, LV_EVENT_CLICKED, s);

    s->lbl_hotspot_reset = lv_label_create(s->btn_hotspot_reset);
    lv_obj_center(s->lbl_hotspot_reset);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_reset, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_reset, "[🔄 重置网络配置]");
    lv_obj_set_style_text_color(s->lbl_hotspot_reset, lv_color_hex(0x7E92AD), 0);

    /* 4.2 热点配网步进状态机进度卡片 (网页提交后接管并展示) */
    s->box_hotspot_progress = lv_obj_create(s->panel_hotspot);
    lv_obj_set_size(s->box_hotspot_progress, 210, 184);
    lv_obj_set_pos(s->box_hotspot_progress, 0, 0);
    lv_obj_set_style_bg_opa(s->box_hotspot_progress, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->box_hotspot_progress, 0, 0);
    lv_obj_set_style_pad_all(s->box_hotspot_progress, 0, 0);
    lv_obj_clear_flag(s->box_hotspot_progress, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN);

    s->lbl_prog_title = lv_label_create(s->box_hotspot_progress);
    lv_obj_align(s->lbl_prog_title, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_title, s->font, 0);
    lv_label_set_text(s->lbl_prog_title, "📡 正在加入网络...");
    lv_obj_set_style_text_color(s->lbl_prog_title, lv_color_hex(0xFFB700), 0);

    s->lbl_prog_step1 = lv_label_create(s->box_hotspot_progress);
    lv_obj_align(s->lbl_prog_step1, LV_ALIGN_TOP_LEFT, 2, 24);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_step1, s->font, 0);
    lv_label_set_text(s->lbl_prog_step1, "[✓] 1. 收到网页指令，已关闭热点");
    lv_obj_set_style_text_color(s->lbl_prog_step1, lv_color_hex(0x00FF88), 0);

    s->lbl_prog_step2 = lv_label_create(s->box_hotspot_progress);
    lv_obj_align(s->lbl_prog_step2, LV_ALIGN_TOP_LEFT, 2, 44);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_step2, s->font, 0);
    lv_label_set_text(s->lbl_prog_step2, "[⟳] 2. 正在关联目标 Wi-Fi 路由...");
    lv_obj_set_style_text_color(s->lbl_prog_step2, lv_color_hex(0xFFB700), 0);

    s->lbl_prog_step3 = lv_label_create(s->box_hotspot_progress);
    lv_obj_align(s->lbl_prog_step3, LV_ALIGN_TOP_LEFT, 2, 64);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_step3, s->font, 0);
    lv_label_set_text(s->lbl_prog_step3, "[⟳] 3. 申请 DHCP 局域网 IP 租约...");
    lv_obj_set_style_text_color(s->lbl_prog_step3, lv_color_hex(0x7E92AD), 0);

    /* 结果大卡片 */
    s->card_prog_result = lv_obj_create(s->box_hotspot_progress);
    lv_obj_set_size(s->card_prog_result, 206, 56);
    lv_obj_align(s->card_prog_result, LV_ALIGN_TOP_MID, 0, 88);
    lv_obj_set_style_bg_color(s->card_prog_result, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_prog_result, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_prog_result, 1, 0);
    lv_obj_set_style_radius(s->card_prog_result, 6, 0);
    lv_obj_set_style_pad_all(s->card_prog_result, 4, 0);
    lv_obj_clear_flag(s->card_prog_result, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_prog_result_ip = lv_label_create(s->card_prog_result);
    lv_obj_align(s->lbl_prog_result_ip, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_result_ip, s->font, 0);
    lv_label_set_text(s->lbl_prog_result_ip, "正在与网关握手...");
    lv_obj_set_style_text_color(s->lbl_prog_result_ip, lv_color_hex(0xFFB700), 0);

    s->lbl_prog_result_url = lv_label_create(s->card_prog_result);
    lv_obj_align(s->lbl_prog_result_url, LV_ALIGN_TOP_LEFT, 2, 24);
    if (s->font) lv_obj_set_style_text_font(s->lbl_prog_result_url, s->font, 0);
    lv_label_set_text(s->lbl_prog_result_url, "请稍候，连网完成后将更新 IP");
    lv_obj_set_style_text_color(s->lbl_prog_result_url, lv_color_hex(0x8B9EB5), 0);

    /* 步进操作按钮 */
    s->btn_prog_done = lv_btn_create(s->box_hotspot_progress);
    lv_obj_set_size(s->btn_prog_done, 206, 32);
    lv_obj_align(s->btn_prog_done, LV_ALIGN_TOP_MID, 0, 148);
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
     * 5. 独立标签页 2: 蓝牙配网 (Web Bluetooth)
     * ===================================================================== */
    s->panel_ble = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_ble, 210, 184);
    lv_obj_align(s->panel_ble, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_ble, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_ble, 0, 0);
    lv_obj_set_style_pad_all(s->panel_ble, 0, 0);
    lv_obj_clear_flag(s->panel_ble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->panel_ble, LV_OBJ_FLAG_HIDDEN);

    s->card_ble_info = lv_obj_create(s->panel_ble);
    lv_obj_set_size(s->card_ble_info, 206, 80);
    lv_obj_align(s->card_ble_info, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->card_ble_info, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->card_ble_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_ble_info, 1, 0);
    lv_obj_set_style_radius(s->card_ble_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_ble_info, 4, 0);
    lv_obj_clear_flag(s->card_ble_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_ble_status = lv_label_create(s->card_ble_info);
    lv_obj_align(s->lbl_ble_status, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_status, s->font, 0);
    lv_label_set_text(s->lbl_ble_status, "● 蓝牙状态: 广播中");
    lv_obj_set_style_text_color(s->lbl_ble_status, lv_color_hex(0x00FF88), 0);

    s->lbl_ble_dev_name = lv_label_create(s->card_ble_info);
    lv_obj_align(s->lbl_ble_dev_name, LV_ALIGN_TOP_LEFT, 2, 24);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_dev_name, s->font, 0);
    lv_label_set_text(s->lbl_ble_dev_name, "广播设备名: Phoenix-Setup");
    lv_obj_set_style_text_color(s->lbl_ble_dev_name, lv_color_hex(0x00E5FF), 0);

    s->lbl_ble_uuid = lv_label_create(s->card_ble_info);
    lv_obj_align(s->lbl_ble_uuid, LV_ALIGN_TOP_LEFT, 2, 46);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_uuid, s->font, 0);
    lv_label_set_text(s->lbl_ble_uuid, "GATT 配网服务 UUID: 0xFFE0");
    lv_obj_set_style_text_color(s->lbl_ble_uuid, lv_color_hex(0x8B9EB5), 0);

    /* 启动/停止蓝牙广播按钮 */
    s->btn_ble_toggle = lv_btn_create(s->panel_ble);
    lv_obj_set_size(s->btn_ble_toggle, 206, 36);
    lv_obj_align(s->btn_ble_toggle, LV_ALIGN_TOP_MID, 0, 88);
    lv_obj_set_style_bg_color(s->btn_ble_toggle, lv_color_hex(0x13273F), 0);
    lv_obj_set_style_border_color(s->btn_ble_toggle, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->btn_ble_toggle, 1, 0);
    lv_obj_set_style_radius(s->btn_ble_toggle, 6, 0);
    lv_obj_add_event_cb(s->btn_ble_toggle, on_ble_toggle_clicked, LV_EVENT_CLICKED, s);

    s->lbl_ble_toggle = lv_label_create(s->btn_ble_toggle);
    lv_obj_center(s->lbl_ble_toggle);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_toggle, s->font, 0);
    lv_label_set_text(s->lbl_ble_toggle, "[⚡ 重启蓝牙配网广播]");
    lv_obj_set_style_text_color(s->lbl_ble_toggle, lv_color_hex(0x00E5FF), 0);

    /* 蓝牙操作指引卡片 */
    s->card_ble_guide = lv_obj_create(s->panel_ble);
    lv_obj_set_size(s->card_ble_guide, 206, 52);
    lv_obj_align(s->card_ble_guide, LV_ALIGN_TOP_MID, 0, 128);
    lv_obj_set_style_bg_color(s->card_ble_guide, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_border_color(s->card_ble_guide, lv_color_hex(0x1A283D), 0);
    lv_obj_set_style_border_width(s->card_ble_guide, 1, 0);
    lv_obj_set_style_radius(s->card_ble_guide, 6, 0);
    lv_obj_set_style_pad_all(s->card_ble_guide, 4, 0);
    lv_obj_clear_flag(s->card_ble_guide, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_ble_guide = lv_label_create(s->card_ble_guide);
    lv_obj_align(s->lbl_ble_guide, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_ble_guide, s->font, 0);
    lv_label_set_text(s->lbl_ble_guide, "1. 浏览器打开伴侣看板 /ble_setup\n2. 扫描直连 Phoenix-Setup 极速配网");
    lv_obj_set_style_text_color(s->lbl_ble_guide, lv_color_hex(0x7E92AD), 0);

    /* =====================================================================
     * 6. 独立标签页 3: AI 大模型 (Agent)
     * ===================================================================== */
    s->panel_agent = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_agent, 210, 184);
    lv_obj_align(s->panel_agent, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_agent, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_agent, 0, 0);
    lv_obj_set_style_pad_all(s->panel_agent, 0, 0);
    lv_obj_clear_flag(s->panel_agent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->panel_agent, LV_OBJ_FLAG_HIDDEN);

    s->card_agent_info = lv_obj_create(s->panel_agent);
    lv_obj_set_size(s->card_agent_info, 206, 178);
    lv_obj_align(s->card_agent_info, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->card_agent_info, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->card_agent_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_agent_info, 1, 0);
    lv_obj_set_style_radius(s->card_agent_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_agent_info, 6, 0);
    lv_obj_clear_flag(s->card_agent_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_agent_model = lv_label_create(s->card_agent_info);
    lv_obj_align(s->lbl_agent_model, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_model, s->font, 0);
    lv_label_set_text(s->lbl_agent_model, "端云大模型: DeepSeek-V3/R1");
    lv_obj_set_style_text_color(s->lbl_agent_model, lv_color_hex(0x00FF88), 0);

    s->lbl_agent_key_st = lv_label_create(s->card_agent_info);
    lv_obj_align(s->lbl_agent_key_st, LV_ALIGN_TOP_LEFT, 2, 26);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_key_st, s->font, 0);
    lv_label_set_text(s->lbl_agent_key_st, "API Key: [已加载持久化密钥]");
    lv_obj_set_style_text_color(s->lbl_agent_key_st, lv_color_hex(0x00E5FF), 0);

    s->lbl_agent_prompt = lv_label_create(s->card_agent_info);
    lv_obj_align(s->lbl_agent_prompt, LV_ALIGN_TOP_LEFT, 2, 50);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_prompt, s->font, 0);
    lv_label_set_text(s->lbl_agent_prompt, "模式: 极客桌宠 (Cyber-Familiar)\n端侧拟人化动态多模态交互");
    lv_obj_set_style_text_color(s->lbl_agent_prompt, lv_color_hex(0x8B9EB5), 0);

    s->lbl_agent_hint = lv_label_create(s->card_agent_info);
    lv_obj_align(s->lbl_agent_hint, LV_ALIGN_BOTTOM_LEFT, 2, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_hint, s->font, 0);
    lv_label_set_text(s->lbl_agent_hint, "💡 浏览器访问 :8080/ 修改密钥与人设");
    lv_obj_set_style_text_color(s->lbl_agent_hint, lv_color_hex(0x7E92AD), 0);

    /* =====================================================================
     * 7. 独立标签页 4: 系统健康与遥测 (System)
     * ===================================================================== */
    s->panel_system = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_system, 210, 184);
    lv_obj_align(s->panel_system, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_system, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_system, 0, 0);
    lv_obj_set_style_pad_all(s->panel_system, 0, 0);
    lv_obj_clear_flag(s->panel_system, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->panel_system, LV_OBJ_FLAG_HIDDEN);

    s->card_system_info = lv_obj_create(s->panel_system);
    lv_obj_set_size(s->card_system_info, 206, 178);
    lv_obj_align(s->card_system_info, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->card_system_info, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->card_system_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_system_info, 1, 0);
    lv_obj_set_style_radius(s->card_system_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_system_info, 6, 0);
    lv_obj_clear_flag(s->card_system_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_system_uptime = lv_label_create(s->card_system_info);
    lv_obj_align(s->lbl_system_uptime, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_system_uptime, s->font, 0);
    lv_label_set_text(s->lbl_system_uptime, "运行时长: 00:00:00");
    lv_obj_set_style_text_color(s->lbl_system_uptime, lv_color_hex(0x00FF88), 0);

    s->lbl_system_cpu = lv_label_create(s->card_system_info);
    lv_obj_align(s->lbl_system_cpu, LV_ALIGN_TOP_LEFT, 2, 26);
    if (s->font) lv_obj_set_style_text_font(s->lbl_system_cpu, s->font, 0);
    lv_label_set_text(s->lbl_system_cpu, "CPU 负载: 15% | 核心 0/1");
    lv_obj_set_style_text_color(s->lbl_system_cpu, lv_color_hex(0x00E5FF), 0);

    s->lbl_system_ram = lv_label_create(s->card_system_info);
    lv_obj_align(s->lbl_system_ram, LV_ALIGN_TOP_LEFT, 2, 50);
    if (s->font) lv_obj_set_style_text_font(s->lbl_system_ram, s->font, 0);
    lv_label_set_text(s->lbl_system_ram, "RAM 内存: 18.2MB / 32.0MB");
    lv_obj_set_style_text_color(s->lbl_system_ram, lv_color_hex(0x8B9EB5), 0);

    s->lbl_system_fps = lv_label_create(s->card_system_info);
    lv_obj_align(s->lbl_system_fps, LV_ALIGN_TOP_LEFT, 2, 74);
    if (s->font) lv_obj_set_style_text_font(s->lbl_system_fps, s->font, 0);
    lv_label_set_text(s->lbl_system_fps, "LVGL: 60 FPS | 触控手势就绪");
    lv_obj_set_style_text_color(s->lbl_system_fps, lv_color_hex(0x8B9EB5), 0);

    s->lbl_system_ver = lv_label_create(s->card_system_info);
    lv_obj_align(s->lbl_system_ver, LV_ALIGN_BOTTOM_LEFT, 2, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_system_ver, s->font, 0);
    lv_label_set_text(s->lbl_system_ver, "OS: OpenVela R528-S3 (Gemini-S1)");
    lv_obj_set_style_text_color(s->lbl_system_ver, lv_color_hex(0x7E92AD), 0);

    /* =====================================================================
     * 8. 独立标签页 5: 存储卡与外脑日志 (Storage)
     * ===================================================================== */
    s->panel_storage = lv_obj_create(s->body_area);
    lv_obj_set_size(s->panel_storage, 210, 184);
    lv_obj_align(s->panel_storage, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s->panel_storage, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->panel_storage, 0, 0);
    lv_obj_set_style_pad_all(s->panel_storage, 0, 0);
    lv_obj_clear_flag(s->panel_storage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->panel_storage, LV_OBJ_FLAG_HIDDEN);

    s->card_storage_info = lv_obj_create(s->panel_storage);
    lv_obj_set_size(s->card_storage_info, 206, 178);
    lv_obj_align(s->card_storage_info, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->card_storage_info, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_border_color(s->card_storage_info, lv_color_hex(0x1C2F4D), 0);
    lv_obj_set_style_border_width(s->card_storage_info, 1, 0);
    lv_obj_set_style_radius(s->card_storage_info, 6, 0);
    lv_obj_set_style_pad_all(s->card_storage_info, 6, 0);
    lv_obj_clear_flag(s->card_storage_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_storage_sd_st = lv_label_create(s->card_storage_info);
    lv_obj_align(s->lbl_storage_sd_st, LV_ALIGN_TOP_LEFT, 2, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_storage_sd_st, s->font, 0);
    lv_label_set_text(s->lbl_storage_sd_st, "TF 卡: 已检测挂载 (/data)");
    lv_obj_set_style_text_color(s->lbl_storage_sd_st, lv_color_hex(0x00FF88), 0);

    s->lbl_storage_cap = lv_label_create(s->card_storage_info);
    lv_obj_align(s->lbl_storage_cap, LV_ALIGN_TOP_LEFT, 2, 26);
    if (s->font) lv_obj_set_style_text_font(s->lbl_storage_cap, s->font, 0);
    lv_label_set_text(s->lbl_storage_cap, "可用容量: 28.6 GB / 32.0 GB");
    lv_obj_set_style_text_color(s->lbl_storage_cap, lv_color_hex(0x00E5FF), 0);

    s->lbl_storage_log = lv_label_create(s->card_storage_info);
    lv_obj_align(s->lbl_storage_log, LV_ALIGN_TOP_LEFT, 2, 50);
    if (s->font) lv_obj_set_style_text_font(s->lbl_storage_log, s->font, 0);
    lv_label_set_text(s->lbl_storage_log, "外脑速记: 自动持久化开启\n黑匣日志: Ramlog/Syslog 双通");
    lv_obj_set_style_text_color(s->lbl_storage_log, lv_color_hex(0x8B9EB5), 0);

    /* 兼容性绑定 */
    s->lbl_net_status = s->lbl_hotspot_ssid;
    s->lbl_net_ip = s->lbl_hotspot_ip;
    s->btn_ble_prov = s->btn_ble_toggle;
    s->lbl_ble_prov_btn = s->lbl_ble_toggle;
    s->btn_hotspot = s->btn_hotspot_action;
    s->lbl_hotspot_btn = s->lbl_hotspot_action;
    s->lbl_sys_uptime = s->lbl_system_uptime;
    s->lbl_sys_cpu = s->lbl_system_cpu;

    /* 默认激活第 0 页: 独立热点配网 */
    ui_settings_switch_tab(s, UI_SETTINGS_TAB_HOTSPOT);

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

    ui_settings_refresh_data(settings);
    LOG_I(TAG, "设置同级视图已激活展示");
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
    ui_settings_switch_tab(settings, page);
}

ui_settings_page_t ui_settings_get_page(const ui_settings_t *settings)
{
    return settings ? settings->current_tab : UI_SETTINGS_TAB_HOTSPOT;
}

/* ========================================================================= */
/*                          二级侧边栏标签页切换                             */
/* ========================================================================= */

void ui_settings_switch_tab(ui_settings_t *settings, ui_settings_tab_t tab)
{
    if (!settings) return;
    if (tab > UI_SETTINGS_TAB_STORAGE) tab = UI_SETTINGS_TAB_HOTSPOT;

    settings->current_tab = tab;

    /* 1. 更新二级侧边栏 5 个按钮的高亮状态 */
    lv_obj_t *btns[5] = { settings->btn_tab_hotspot, settings->btn_tab_ble, settings->btn_tab_agent, settings->btn_tab_system, settings->btn_tab_storage };
    lv_obj_t *lbls[5] = { settings->lbl_tab_hotspot, settings->lbl_tab_ble, settings->lbl_tab_agent, settings->lbl_tab_system, settings->lbl_tab_storage };

    for (int i = 0; i < 5; i++) {
        if (!btns[i] || !lbls[i]) continue;
        if (i == (int)tab) {
            lv_obj_set_style_bg_color(btns[i], lv_color_hex(0x152E4D), 0);
            lv_obj_set_style_border_color(btns[i], lv_color_hex(0x00E5FF), 0);
            lv_obj_set_style_border_width(btns[i], 2, 0);
            lv_obj_set_style_text_color(lbls[i], lv_color_hex(0x00E5FF), 0);
        } else {
            lv_obj_set_style_bg_color(btns[i], lv_color_hex(0x0C1422), 0);
            lv_obj_set_style_border_color(btns[i], lv_color_hex(0x1C2B42), 0);
            lv_obj_set_style_border_width(btns[i], 1, 0);
            lv_obj_set_style_text_color(lbls[i], lv_color_hex(0x7E92AD), 0);
        }
    }

    /* 2. 隐藏全部面板，仅展示目标标签面板 */
    lv_obj_t *panels[5] = { settings->panel_hotspot, settings->panel_ble, settings->panel_agent, settings->panel_system, settings->panel_storage };
    for (int i = 0; i < 5; i++) {
        if (panels[i]) {
            if (i == (int)tab) lv_obj_clear_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 3. 动态更新顶栏标题 */
    const char *titles[5] = {
        "⚡ 独立热点配网 (SoftAP)",
        "⚡ 蓝牙极速配网 (Web BLE)",
        "🤖 灵眸大模型参数配置",
        "📊 系统健康与硬件遥测",
        "💾 TF 存储卡与外脑日志"
    };
    if (settings->lbl_header_title) {
        lv_label_set_text(settings->lbl_header_title, titles[tab]);
    }

    ui_settings_refresh_data(settings);
}

/* ========================================================================= */
/*                      步进配网状态机与数据同步                             */
/* ========================================================================= */

void ui_settings_update_net_progress(ui_settings_t *settings, int mode, const char *ssid, const char *ip, const char *msg)
{
    if (!settings) return;

    if (mode == 1 /* NET_MODE_STA_CONNECTING */) {
        /* 立即切换显示步进状态机面板 */
        if (settings->box_hotspot_idle) lv_obj_add_flag(settings->box_hotspot_idle, LV_OBJ_FLAG_HIDDEN);
        if (settings->box_hotspot_progress) lv_obj_clear_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN);

        char tbuf[64];
        snprintf(tbuf, sizeof(tbuf), "📡 正在加入网络: %s", (ssid && ssid[0]) ? ssid : "目标Wi-Fi");
        if (settings->lbl_prog_title) lv_label_set_text(settings->lbl_prog_title, tbuf);

        bool is_dhcp_stage = (msg && (strstr(msg, "DHCP") || strstr(msg, "租约") || strstr(msg, "申请")));

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
                lv_label_set_text(settings->lbl_prog_step3, "[⟳] 3. 正在申请 DHCP 局域网 IP...");
                lv_obj_set_style_text_color(settings->lbl_prog_step3, lv_color_hex(0xFFB700), 0);
            }
            if (settings->lbl_prog_result_ip) {
                lv_label_set_text(settings->lbl_prog_result_ip, "Wi-Fi 关联成功，正在协商 IP...");
                lv_obj_set_style_text_color(settings->lbl_prog_result_ip, lv_color_hex(0xFFB700), 0);
            }
        } else {
            if (settings->lbl_prog_step2) {
                lv_label_set_text(settings->lbl_prog_step2, "[⟳] 2. 正在关联目标 Wi-Fi 路由...");
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
            lv_label_set_text(settings->lbl_prog_done, "[⟳ 连网进行中...]");
            lv_obj_set_style_text_color(settings->lbl_prog_done, lv_color_hex(0xFFB700), 0);
        }
    } else if (mode == 2 /* NET_MODE_STA_CONNECTED */) {
        if (settings->box_hotspot_idle) lv_obj_add_flag(settings->box_hotspot_idle, LV_OBJ_FLAG_HIDDEN);
        if (settings->box_hotspot_progress) lv_obj_clear_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN);

        if (settings->lbl_prog_title) {
            lv_label_set_text(settings->lbl_prog_title, "🎉 Wi-Fi 连接成功!");
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
    } else if (mode == 0 /* DISCONNECTED / 失败 */) {
        if (settings->box_hotspot_progress && !lv_obj_has_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN)) {
            if (settings->lbl_prog_title) {
                lv_label_set_text(settings->lbl_prog_title, "⚠️ 连网超时或密码错误");
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
        ui_settings_update_net_progress(settings, 1, ssid_buf, ip_buf, NULL);
    } else if (mode == NET_MODE_STA_CONNECTED) {
        ui_settings_update_net_progress(settings, 2, ssid_buf, ip_buf, NULL);
    } else {
        if (settings->box_hotspot_idle) lv_obj_clear_flag(settings->box_hotspot_idle, LV_OBJ_FLAG_HIDDEN);
        if (settings->box_hotspot_progress) lv_obj_add_flag(settings->box_hotspot_progress, LV_OBJ_FLAG_HIDDEN);

        if (settings->lbl_hotspot_ssid) {
            char buf[64];
            snprintf(buf, sizeof(buf), "热点: %s", (mode == NET_MODE_SOFTAP_CONFIG) ? ssid_buf : "Gemini-Agent-Setup");
            lv_label_set_text(settings->lbl_hotspot_ssid, buf);
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
            lv_label_set_text(settings->lbl_ble_toggle, "[🛑 停止蓝牙广播]");
            lv_obj_set_style_text_color(settings->lbl_ble_toggle, lv_color_hex(0xFF7043), 0);
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
    } else {
        if (settings->lbl_ble_status) {
            lv_label_set_text(settings->lbl_ble_status, "● 蓝牙状态: 未广播 (点击开启)");
            lv_obj_set_style_text_color(settings->lbl_ble_status, lv_color_hex(0x7E92AD), 0);
        }
        if (settings->lbl_ble_toggle) {
            lv_label_set_text(settings->lbl_ble_toggle, "[⚡ 启动蓝牙极速配网]");
            lv_obj_set_style_text_color(settings->lbl_ble_toggle, lv_color_hex(0x00E5FF), 0);
        }
    }

    /* 3. 刷新系统遥测与大模型页面 */
    hal_sys_info_t sys;
    if (hal_system_get_info(&sys) == 0) {
        if (settings->lbl_system_uptime) {
            uint32_t s = sys.uptime_sec;
            char ubuf[64];
            snprintf(ubuf, sizeof(ubuf), "运行时长: %02u:%02u:%02u", s / 3600, (s % 3600) / 60, s % 60);
            lv_label_set_text(settings->lbl_system_uptime, ubuf);
        }
        if (settings->lbl_system_cpu) {
            char cbuf[64];
            snprintf(cbuf, sizeof(cbuf), "CPU 负载: %u%% | 状态正常", sys.cpu_usage_pct);
            lv_label_set_text(settings->lbl_system_cpu, cbuf);
        }
        if (settings->lbl_system_ram) {
            char rbuf[64];
            float free_mb = (float)sys.free_ram_bytes / (1024.0f * 1024.0f);
            float total_mb = (float)sys.total_ram_bytes / (1024.0f * 1024.0f);
            snprintf(rbuf, sizeof(rbuf), "RAM 剩余: %.1fMB / %.1fMB", free_mb, total_mb);
            lv_label_set_text(settings->lbl_system_ram, rbuf);
        }
    }
}

/* ========================================================================= */
/*                              按钮事件处理                                 */
/* ========================================================================= */

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
