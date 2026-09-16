/**
 * @file ui_settings.c
 * @brief 顶部控制中心抽屉与二级设置菜单实现 (Two-Level Settings Drawer)
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
#define SETTINGS_DRAWER_HEIGHT 218

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

static void on_hotspot_btn_clicked(lv_event_t *e)
{
    ui_settings_t *s = (ui_settings_t *)lv_event_get_user_data(e);
    if (!s) return;

    if (net_mgr_get_mode() == NET_MODE_SOFTAP_CONFIG) {
        LOG_I(TAG, "热点已在广播中，无需重复触发");
        return;
    }

    LOG_I(TAG, "用户在二级控制中心触发开启独立热点配网");

    /* 立即给出视觉反馈，杜绝假死与无响应感 */
    if (s->lbl_hotspot_btn) {
        lv_label_set_text(s->lbl_hotspot_btn, "⏳ 正在启动热点广播...");
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

            /* 向上滑动超过 30px 收起抽屉 */
            if (dy < -30 && abs_dy > abs_dx && s->is_open) {
                ui_settings_close(s);
            }
        }
    }
}

ui_settings_t* ui_settings_create(lv_obj_t *parent, const lv_font_t *font)
{
    if (!parent) return NULL;

    ui_settings_t *s = (ui_settings_t *)calloc(1, sizeof(ui_settings_t));
    if (!s) return NULL;

    s->font = font;
    s->drawer_h = SETTINGS_DRAWER_HEIGHT;
    s->is_open = false;
    s->current_page = UI_SETTINGS_PAGE_MAIN;

    /* 1. 半透明遮罩层 (深黑微透) */
    s->mask_bg = lv_obj_create(parent);
    lv_obj_set_size(s->mask_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s->mask_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s->mask_bg, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s->mask_bg, 0, 0);
    lv_obj_clear_flag(s->mask_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->mask_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s->mask_bg, on_mask_clicked, LV_EVENT_CLICKED, s);

    /* 2. 抽屉主容器 (高度 218px) */
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

    /* 3. 顶栏 (返回 + 动态标题 + 小巧关闭) */
    s->header = lv_obj_create(s->drawer);
    lv_obj_set_size(s->header, LV_PCT(100), 22);
    lv_obj_set_style_bg_opa(s->header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->header, 0, 0);
    lv_obj_set_style_pad_all(s->header, 0, 0);
    lv_obj_clear_flag(s->header, LV_OBJ_FLAG_SCROLLABLE);

    /* 返回按钮 (默认隐藏) */
    s->btn_back = lv_btn_create(s->header);
    lv_obj_set_size(s->btn_back, 54, 20);
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
    lv_obj_align(s->lbl_title, LV_ALIGN_LEFT_MID, 6, 0);
    if (s->font) lv_obj_set_style_text_font(s->lbl_title, s->font, 0);
    lv_label_set_text(s->lbl_title, "⚙️ 控制中心");
    lv_obj_set_style_text_color(s->lbl_title, lv_color_hex(0x00E5FF), 0);

    /* 关闭按钮 */
    s->btn_close = lv_btn_create(s->header);
    lv_obj_set_size(s->btn_close, 26, 20);
    lv_obj_align(s->btn_close, LV_ALIGN_RIGHT_MID, -2, 0);
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
     * 4. 一级主菜单视图 (2x2 四宫格卡牌, y=24, h=180)
     * ===================================================================== */
    s->view_main = lv_obj_create(s->drawer);
    lv_obj_set_size(s->view_main, 308, 180);
    lv_obj_align(s->view_main, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_opa(s->view_main, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->view_main, 0, 0);
    lv_obj_set_style_pad_all(s->view_main, 0, 0);
    lv_obj_clear_flag(s->view_main, LV_OBJ_FLAG_SCROLLABLE);

    /* 4.1 卡牌 1: 网络与热点 (左上, 150x84) */
    s->card_nav_net = lv_obj_create(s->view_main);
    lv_obj_set_size(s->card_nav_net, 150, 84);
    lv_obj_set_pos(s->card_nav_net, 2, 2);
    lv_obj_set_style_bg_color(s->card_nav_net, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_nav_net, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->card_nav_net, 1, 0);
    lv_obj_set_style_radius(s->card_nav_net, 8, 0);
    lv_obj_set_style_pad_all(s->card_nav_net, 4, 0);
    lv_obj_set_ext_click_area(s->card_nav_net, 6);
    lv_obj_clear_flag(s->card_nav_net, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->card_nav_net, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s->card_nav_net, on_nav_card_clicked, LV_EVENT_CLICKED, s);

    s->lbl_nav_net_t = lv_label_create(s->card_nav_net);
    lv_obj_align(s->lbl_nav_net_t, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_net_t, s->font, 0);
    lv_label_set_text(s->lbl_nav_net_t, "🌐 无线网络 >");
    lv_obj_set_style_text_color(s->lbl_nav_net_t, lv_color_hex(0x00E5FF), 0);

    s->lbl_nav_net_sub = lv_label_create(s->card_nav_net);
    lv_obj_align(s->lbl_nav_net_sub, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_net_sub, s->font, 0);
    lv_label_set_text(s->lbl_nav_net_sub, "● 配置/开启热点");
    lv_obj_set_style_text_color(s->lbl_nav_net_sub, lv_color_hex(0x8B9EB5), 0);

    /* 4.2 卡牌 2: 系统遥测 (右上, 150x84) */
    s->card_nav_sys = lv_obj_create(s->view_main);
    lv_obj_set_size(s->card_nav_sys, 150, 84);
    lv_obj_set_pos(s->card_nav_sys, 156, 2);
    lv_obj_set_style_bg_color(s->card_nav_sys, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_nav_sys, lv_color_hex(0x1E2B42), 0);
    lv_obj_set_style_border_width(s->card_nav_sys, 1, 0);
    lv_obj_set_style_radius(s->card_nav_sys, 8, 0);
    lv_obj_set_style_pad_all(s->card_nav_sys, 4, 0);
    lv_obj_set_ext_click_area(s->card_nav_sys, 6);
    lv_obj_clear_flag(s->card_nav_sys, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->card_nav_sys, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s->card_nav_sys, on_nav_card_clicked, LV_EVENT_CLICKED, s);

    s->lbl_nav_sys_t = lv_label_create(s->card_nav_sys);
    lv_obj_align(s->lbl_nav_sys_t, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_sys_t, s->font, 0);
    lv_label_set_text(s->lbl_nav_sys_t, "📊 系统健康 >");
    lv_obj_set_style_text_color(s->lbl_nav_sys_t, lv_color_hex(0x00FF88), 0);

    s->lbl_nav_sys_sub = lv_label_create(s->card_nav_sys);
    lv_obj_align(s->lbl_nav_sys_sub, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_sys_sub, s->font, 0);
    lv_label_set_text(s->lbl_nav_sys_sub, "电量 85% | 优");
    lv_obj_set_style_text_color(s->lbl_nav_sys_sub, lv_color_hex(0x8B9EB5), 0);

    /* 4.3 卡牌 3: Agent 模型 (左下, 150x84) */
    s->card_nav_agent = lv_obj_create(s->view_main);
    lv_obj_set_size(s->card_nav_agent, 150, 84);
    lv_obj_set_pos(s->card_nav_agent, 2, 90);
    lv_obj_set_style_bg_color(s->card_nav_agent, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_nav_agent, lv_color_hex(0x1E2B42), 0);
    lv_obj_set_style_border_width(s->card_nav_agent, 1, 0);
    lv_obj_set_style_radius(s->card_nav_agent, 8, 0);
    lv_obj_set_style_pad_all(s->card_nav_agent, 4, 0);
    lv_obj_set_ext_click_area(s->card_nav_agent, 6);
    lv_obj_clear_flag(s->card_nav_agent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->card_nav_agent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s->card_nav_agent, on_nav_card_clicked, LV_EVENT_CLICKED, s);

    s->lbl_nav_agent_t = lv_label_create(s->card_nav_agent);
    lv_obj_align(s->lbl_nav_agent_t, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_agent_t, s->font, 0);
    lv_label_set_text(s->lbl_nav_agent_t, "🧠 灵眸模型 >");
    lv_obj_set_style_text_color(s->lbl_nav_agent_t, lv_color_hex(0xFFB700), 0);

    s->lbl_nav_agent_sub = lv_label_create(s->card_nav_agent);
    lv_obj_align(s->lbl_nav_agent_sub, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_agent_sub, s->font, 0);
    lv_label_set_text(s->lbl_nav_agent_sub, "DeepSeek | 12次");
    lv_obj_set_style_text_color(s->lbl_nav_agent_sub, lv_color_hex(0x8B9EB5), 0);

    /* 4.4 卡牌 4: 存储与伴侣 (右下, 150x84) */
    s->card_nav_store = lv_obj_create(s->view_main);
    lv_obj_set_size(s->card_nav_store, 150, 84);
    lv_obj_set_pos(s->card_nav_store, 156, 90);
    lv_obj_set_style_bg_color(s->card_nav_store, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_nav_store, lv_color_hex(0x1E2B42), 0);
    lv_obj_set_style_border_width(s->card_nav_store, 1, 0);
    lv_obj_set_style_radius(s->card_nav_store, 8, 0);
    lv_obj_set_style_pad_all(s->card_nav_store, 4, 0);
    lv_obj_set_ext_click_area(s->card_nav_store, 6);
    lv_obj_clear_flag(s->card_nav_store, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->card_nav_store, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s->card_nav_store, on_nav_card_clicked, LV_EVENT_CLICKED, s);

    s->lbl_nav_store_t = lv_label_create(s->card_nav_store);
    lv_obj_align(s->lbl_nav_store_t, LV_ALIGN_TOP_LEFT, 4, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_store_t, s->font, 0);
    lv_label_set_text(s->lbl_nav_store_t, "💾 伴侣看板 >");
    lv_obj_set_style_text_color(s->lbl_nav_store_t, lv_color_hex(0xA78BFA), 0);

    s->lbl_nav_store_sub = lv_label_create(s->card_nav_store);
    lv_obj_align(s->lbl_nav_store_sub, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_nav_store_sub, s->font, 0);
    lv_label_set_text(s->lbl_nav_store_sub, "TF卡/Web直达");
    lv_obj_set_style_text_color(s->lbl_nav_store_sub, lv_color_hex(0x8B9EB5), 0);

    /* =====================================================================
     * 5. 二级详情视图总容器 (y=24, h=180, 默认隐藏)
     * ===================================================================== */
    s->view_detail = lv_obj_create(s->drawer);
    lv_obj_set_size(s->view_detail, 308, 180);
    lv_obj_align(s->view_detail, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_opa(s->view_detail, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->view_detail, 0, 0);
    lv_obj_set_style_pad_all(s->view_detail, 0, 0);
    lv_obj_clear_flag(s->view_detail, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->view_detail, LV_OBJ_FLAG_HIDDEN);

    /* 5.1 二级子页 A: 网络与独立热点 */
    s->sec_net_box = lv_obj_create(s->view_detail);
    lv_obj_set_size(s->sec_net_box, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s->sec_net_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->sec_net_box, 0, 0);
    lv_obj_set_style_pad_all(s->sec_net_box, 0, 0);
    lv_obj_clear_flag(s->sec_net_box, LV_OBJ_FLAG_SCROLLABLE);

    s->card_net_info = lv_obj_create(s->sec_net_box);
    lv_obj_set_size(s->card_net_info, 304, 52);
    lv_obj_align(s->card_net_info, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(s->card_net_info, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(s->card_net_info, lv_color_hex(0x1E2B42), 0);
    lv_obj_set_style_border_width(s->card_net_info, 1, 0);
    lv_obj_set_style_radius(s->card_net_info, 8, 0);
    lv_obj_set_style_pad_all(s->card_net_info, 4, 0);
    lv_obj_clear_flag(s->card_net_info, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_net_status = lv_label_create(s->card_net_info);
    lv_obj_align(s->lbl_net_status, LV_ALIGN_TOP_LEFT, 6, 2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_net_status, s->font, 0);
    lv_label_set_text(s->lbl_net_status, "● 状态: 独立热点就绪 (SoftAP)");
    lv_obj_set_style_text_color(s->lbl_net_status, lv_color_hex(0x00FF88), 0);

    s->lbl_net_ip = lv_label_create(s->card_net_info);
    lv_obj_align(s->lbl_net_ip, LV_ALIGN_BOTTOM_LEFT, 6, -2);
    if (s->font) lv_obj_set_style_text_font(s->lbl_net_ip, s->font, 0);
    lv_label_set_text(s->lbl_net_ip, "IP: 192.168.4.1 (免端口直达)");
    lv_obj_set_style_text_color(s->lbl_net_ip, lv_color_hex(0x00E5FF), 0);

    /* 超大热点开关按钮 (高度 44px, 宽度 304px, 极大方便小屏点击) */
    s->btn_hotspot = lv_btn_create(s->sec_net_box);
    lv_obj_set_size(s->btn_hotspot, 304, 44);
    lv_obj_align(s->btn_hotspot, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(s->btn_hotspot, lv_color_hex(0x142033), 0);
    lv_obj_set_style_border_color(s->btn_hotspot, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s->btn_hotspot, 1, 0);
    lv_obj_set_style_radius(s->btn_hotspot, 8, 0);
    lv_obj_set_style_pad_all(s->btn_hotspot, 0, 0);
    lv_obj_set_ext_click_area(s->btn_hotspot, 10);
    lv_obj_add_event_cb(s->btn_hotspot, on_hotspot_btn_clicked, LV_EVENT_CLICKED, s);

    s->lbl_hotspot_btn = lv_label_create(s->btn_hotspot);
    lv_obj_center(s->lbl_hotspot_btn);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_btn, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_btn, "🚀 开启独立热点配网 (192.168.4.1)");
    lv_obj_set_style_text_color(s->lbl_hotspot_btn, lv_color_hex(0x00E5FF), 0);

    s->lbl_hotspot_hint = lv_label_create(s->sec_net_box);
    lv_obj_align(s->lbl_hotspot_hint, LV_ALIGN_TOP_MID, 0, 112);
    if (s->font) lv_obj_set_style_text_font(s->lbl_hotspot_hint, s->font, 0);
    lv_label_set_text(s->lbl_hotspot_hint, "连入 Gemini-Agent-S1 热点即可手机直达配网");
    lv_obj_set_style_text_color(s->lbl_hotspot_hint, lv_color_hex(0x7E92AD), 0);

    /* 5.2 二级子页 B: 系统遥测 */
    s->sec_sys_box = lv_obj_create(s->view_detail);
    lv_obj_set_size(s->sec_sys_box, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s->sec_sys_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->sec_sys_box, 0, 0);
    lv_obj_set_style_pad_all(s->sec_sys_box, 0, 0);
    lv_obj_clear_flag(s->sec_sys_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card_sys_detail = lv_obj_create(s->sec_sys_box);
    lv_obj_set_size(card_sys_detail, 304, 120);
    lv_obj_align(card_sys_detail, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(card_sys_detail, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(card_sys_detail, lv_color_hex(0x1E2B42), 0);
    lv_obj_set_style_border_width(card_sys_detail, 1, 0);
    lv_obj_set_style_radius(card_sys_detail, 8, 0);
    lv_obj_set_style_pad_all(card_sys_detail, 6, 0);
    lv_obj_clear_flag(card_sys_detail, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_sys_env = lv_label_create(card_sys_detail);
    lv_obj_align(s->lbl_sys_env, LV_ALIGN_TOP_LEFT, 6, 4);
    if (s->font) lv_obj_set_style_text_font(s->lbl_sys_env, s->font, 0);
    lv_label_set_text(s->lbl_sys_env, "● 环境温湿: 26.5℃  60%RH  |  光照: 320Lux");
    lv_obj_set_style_text_color(s->lbl_sys_env, lv_color_hex(0x00FF88), 0);

    s->lbl_sys_load = lv_label_create(card_sys_detail);
    lv_obj_align(s->lbl_sys_load, LV_ALIGN_TOP_LEFT, 6, 32);
    if (s->font) lv_obj_set_style_text_font(s->lbl_sys_load, s->font, 0);
    lv_label_set_text(s->lbl_sys_load, "● 核心负载: 优 (0.12)  |  电池电量: 85%");
    lv_obj_set_style_text_color(s->lbl_sys_load, lv_color_hex(0x8B9EB5), 0);

    s->lbl_sys_uptime = lv_label_create(card_sys_detail);
    lv_obj_align(s->lbl_sys_uptime, LV_ALIGN_TOP_LEFT, 6, 60);
    if (s->font) lv_obj_set_style_text_font(s->lbl_sys_uptime, s->font, 0);
    lv_label_set_text(s->lbl_sys_uptime, "● 运行时长: 00:08:20  |  体感敲击感知: 正常");
    lv_obj_set_style_text_color(s->lbl_sys_uptime, lv_color_hex(0x7E92AD), 0);

    /* 5.3 二级子页 C: Agent 与模型 */
    s->sec_agent_box = lv_obj_create(s->view_detail);
    lv_obj_set_size(s->sec_agent_box, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s->sec_agent_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->sec_agent_box, 0, 0);
    lv_obj_set_style_pad_all(s->sec_agent_box, 0, 0);
    lv_obj_clear_flag(s->sec_agent_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card_agent_detail = lv_obj_create(s->sec_agent_box);
    lv_obj_set_size(card_agent_detail, 304, 120);
    lv_obj_align(card_agent_detail, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(card_agent_detail, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(card_agent_detail, lv_color_hex(0x1E2B42), 0);
    lv_obj_set_style_border_width(card_agent_detail, 1, 0);
    lv_obj_set_style_radius(card_agent_detail, 8, 0);
    lv_obj_set_style_pad_all(card_agent_detail, 6, 0);
    lv_obj_clear_flag(card_agent_detail, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_agent_model = lv_label_create(card_agent_detail);
    lv_obj_align(s->lbl_agent_model, LV_ALIGN_TOP_LEFT, 6, 4);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_model, s->font, 0);
    lv_label_set_text(s->lbl_agent_model, "● 驱动大模型: DeepSeek-V3 (端云协同)");
    lv_obj_set_style_text_color(s->lbl_agent_model, lv_color_hex(0xFFB700), 0);

    s->lbl_agent_stats = lv_label_create(card_agent_detail);
    lv_obj_align(s->lbl_agent_stats, LV_ALIGN_TOP_LEFT, 6, 32);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_stats, s->font, 0);
    lv_label_set_text(s->lbl_agent_stats, "● 交互: 12次 | Token: 1.8k | 专注: 25m");
    lv_obj_set_style_text_color(s->lbl_agent_stats, lv_color_hex(0x00E5FF), 0);

    s->lbl_agent_last = lv_label_create(card_agent_detail);
    lv_obj_align(s->lbl_agent_last, LV_ALIGN_TOP_LEFT, 6, 60);
    if (s->font) lv_obj_set_style_text_font(s->lbl_agent_last, s->font, 0);
    lv_label_set_text(s->lbl_agent_last, "最新: 帮我开启25分钟专注流");
    lv_obj_set_style_text_color(s->lbl_agent_last, lv_color_hex(0x8B9EB5), 0);

    /* 5.4 二级子页 D: 存储与极客看板 */
    s->sec_store_box = lv_obj_create(s->view_detail);
    lv_obj_set_size(s->sec_store_box, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s->sec_store_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s->sec_store_box, 0, 0);
    lv_obj_set_style_pad_all(s->sec_store_box, 0, 0);
    lv_obj_clear_flag(s->sec_store_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card_store_detail = lv_obj_create(s->sec_store_box);
    lv_obj_set_size(card_store_detail, 304, 120);
    lv_obj_align(card_store_detail, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(card_store_detail, lv_color_hex(0x101726), 0);
    lv_obj_set_style_border_color(card_store_detail, lv_color_hex(0x1E2B42), 0);
    lv_obj_set_style_border_width(card_store_detail, 1, 0);
    lv_obj_set_style_radius(card_store_detail, 8, 0);
    lv_obj_set_style_pad_all(card_store_detail, 6, 0);
    lv_obj_clear_flag(card_store_detail, LV_OBJ_FLAG_SCROLLABLE);

    s->lbl_store_sd = lv_label_create(card_store_detail);
    lv_obj_align(s->lbl_store_sd, LV_ALIGN_TOP_LEFT, 6, 4);
    if (s->font) lv_obj_set_style_text_font(s->lbl_store_sd, s->font, 0);
    lv_label_set_text(s->lbl_store_sd, "● TF卡状态: 已就绪 (/tmp/phoenix_sdcard)");
    lv_obj_set_style_text_color(s->lbl_store_sd, lv_color_hex(0xA78BFA), 0);

    s->lbl_store_web = lv_label_create(card_store_detail);
    lv_obj_align(s->lbl_store_web, LV_ALIGN_TOP_LEFT, 6, 32);
    if (s->font) lv_obj_set_style_text_font(s->lbl_store_web, s->font, 0);
    lv_label_set_text(s->lbl_store_web, "● 伴侣服务: http://0.0.0.0:8080 (免端口直达)");
    lv_obj_set_style_text_color(s->lbl_store_web, lv_color_hex(0x00FF88), 0);

    /* 兼容映射保留旧指针避免外部野指针 */
    s->card_hotspot = s->card_net_info;
    s->card_status = card_sys_detail;
    s->lbl_status_env = s->lbl_sys_env;
    s->lbl_status_health = s->lbl_sys_load;
    s->card_history = card_agent_detail;
    s->lbl_history_stats = s->lbl_agent_stats;
    s->lbl_history_last = s->lbl_agent_last;

    /* 6. 实体极简拉手条 */
    s->handle_bar = lv_obj_create(s->drawer);
    lv_obj_set_size(s->handle_bar, 36, 3);
    lv_obj_align(s->handle_bar, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(s->handle_bar, lv_color_hex(0x3E4E68), 0);
    lv_obj_set_style_radius(s->handle_bar, 2, 0);
    lv_obj_set_style_border_width(s->handle_bar, 0, 0);
    lv_obj_clear_flag(s->handle_bar, LV_OBJ_FLAG_SCROLLABLE);

    ui_settings_set_page(s, UI_SETTINGS_PAGE_MAIN);
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

void ui_settings_set_page(ui_settings_t *settings, ui_settings_page_t page)
{
    if (!settings) return;
    settings->current_page = page;

    if (page == UI_SETTINGS_PAGE_MAIN) {
        /* 展示一级四宫格，隐藏二级详情 */
        lv_obj_clear_flag(settings->view_main, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(settings->view_detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(settings->btn_back, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(settings->lbl_title, LV_ALIGN_LEFT_MID, 6, 0);
        lv_label_set_text(settings->lbl_title, "⚙️ 控制中心");
    } else {
        /* 隐藏一级四宫格，展示二级详情 */
        lv_obj_add_flag(settings->view_main, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(settings->view_detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(settings->btn_back, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(settings->lbl_title, LV_ALIGN_LEFT_MID, 62, 0);

        /* 隐藏所有二级子区块 */
        lv_obj_add_flag(settings->sec_net_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(settings->sec_sys_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(settings->sec_agent_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(settings->sec_store_box, LV_OBJ_FLAG_HIDDEN);

        if (page == UI_SETTINGS_PAGE_NETWORK) {
            lv_obj_clear_flag(settings->sec_net_box, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(settings->lbl_title, "🌐 无线网络");
        } else if (page == UI_SETTINGS_PAGE_SYSTEM) {
            lv_obj_clear_flag(settings->sec_sys_box, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(settings->lbl_title, "📊 系统健康");
        } else if (page == UI_SETTINGS_PAGE_AGENT) {
            lv_obj_clear_flag(settings->sec_agent_box, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(settings->lbl_title, "🧠 灵眸模型");
        } else if (page == UI_SETTINGS_PAGE_STORAGE) {
            lv_obj_clear_flag(settings->sec_store_box, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(settings->lbl_title, "💾 伴侣看板");
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

    /* 展现遮罩并启动渐变暗光动画 (0 -> 60) */
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

    LOG_I(TAG, "控制中心二级抽屉已滑入展开");
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

    LOG_I(TAG, "控制中心二级抽屉已收起");
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
            snprintf(buf, sizeof(buf), "● 已连入无线网络: %s", ssid_buf[0] ? ssid_buf : "Wi-Fi");
            lv_label_set_text(settings->lbl_net_status, buf);
            lv_obj_set_style_text_color(settings->lbl_net_status, lv_color_hex(0x00FF88), 0);
        } else if (mode == NET_MODE_SOFTAP_CONFIG) {
            lv_label_set_text(settings->lbl_net_status, "● 正在广播独立热点: Gemini-Agent-S1");
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

    if (settings->lbl_hotspot_btn && settings->btn_hotspot) {
        char btn_str[64];
        if (mode == NET_MODE_SOFTAP_CONFIG) {
            snprintf(btn_str, sizeof(btn_str), "● 热点广播中: %s", ip_buf[0] ? ip_buf : "192.168.4.1:8080");
            lv_label_set_text(settings->lbl_hotspot_btn, btn_str);
            lv_obj_set_style_text_color(settings->lbl_hotspot_btn, lv_color_hex(0xFFB700), 0);
            lv_obj_set_style_border_color(settings->btn_hotspot, lv_color_hex(0xFFB700), 0);
            if (settings->lbl_hotspot_hint) {
                lv_label_set_text(settings->lbl_hotspot_hint, "连入 Gemini-Agent-S1 热点即可手机直达配网");
                lv_obj_set_style_text_color(settings->lbl_hotspot_hint, lv_color_hex(0x00FF88), 0);
            }
        } else {
            snprintf(btn_str, sizeof(btn_str), "🚀 开启独立热点配网 (192.168.4.1)");
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
        snprintf(buf, sizeof(buf), "● 运行时长: %02u:%02u:%02u | 感知正常", up_h, up_m, up_s);
        lv_label_set_text(settings->lbl_sys_uptime, buf);
    }

    /* 4. 刷新 Agent 模型遥测 */
    uint32_t interactions = agent ? agent->stats.interaction_count : 12;
    uint32_t tokens = agent ? agent->stats.total_tokens_used : 1850;
    uint32_t focus_m = (agent && agent->stats.continuous_focus_s > 0) ? (agent->stats.continuous_focus_s / 60) : 25;

    if (settings->lbl_agent_stats) {
        char stats_buf[64];
        snprintf(stats_buf, sizeof(stats_buf), "交互: %u次 | Token: %u | 专注: %um", interactions, tokens, focus_m);
        lv_label_set_text(settings->lbl_agent_stats, stats_buf);
    }
    if (settings->lbl_nav_agent_sub) {
        char buf[64];
        snprintf(buf, sizeof(buf), "DeepSeek\n交互: %u次", interactions);
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
