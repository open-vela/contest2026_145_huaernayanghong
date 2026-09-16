/**
 * @file ui_sidebar.c
 * @brief 侧边常驻导航栏组件实现 (Side Navigation Rail)
 * @author OpenVela Contest 2026 Team 145
 */

#include "ui_sidebar.h"
#include "../core/cartridge_mgr.h"
#include "../utils/log_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "UISidebar"

static const char* get_cartridge_icon(const char *id)
{
    if (!id) return "📦";
    if (strcmp(id, "home") == 0) return "🏠";
    if (strcmp(id, "clock") == 0) return "⏰";
    if (strcmp(id, "zen") == 0) return "🐟";
    if (strcmp(id, "memo") == 0) return "📝";
    if (strcmp(id, "agent") == 0) return "🤖";
    if (strcmp(id, "familiar") == 0) return "👁️";
    return "💡";
}

static void on_item_clicked(lv_event_t *e)
{
    ui_sidebar_item_t *item = (ui_sidebar_item_t *)lv_event_get_user_data(e);
    if (!item || !item->id[0]) return;

    LOG_I(TAG, "用户点击侧边栏切换卡带: [%s]", item->id);
    cartridge_mgr_switch(item->id);
}

ui_sidebar_t* ui_sidebar_create(lv_obj_t *parent, const lv_font_t *font)
{
    if (!parent) return NULL;

    ui_sidebar_t *sb = (ui_sidebar_t *)calloc(1, sizeof(ui_sidebar_t));
    if (!sb) return NULL;

    sb->font = font;

    /* 1. 侧边栏垂直容器 (宽 36px, 高 216px, x=0, y=24) */
    sb->container = lv_obj_create(parent);
    lv_obj_set_size(sb->container, UI_SIDEBAR_WIDTH, 216);
    lv_obj_set_pos(sb->container, 0, 24);
    lv_obj_set_style_bg_color(sb->container, lv_color_hex(0x070B14), 0);
    lv_obj_set_style_bg_opa(sb->container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(sb->container, lv_color_hex(0x152238), 0);
    lv_obj_set_style_border_width(sb->container, 1, 0);
    lv_obj_set_style_border_side(sb->container, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_radius(sb->container, 0, 0);
    lv_obj_set_style_pad_all(sb->container, 2, 0);
    lv_obj_clear_flag(sb->container, LV_OBJ_FLAG_SCROLLABLE);

    ui_sidebar_refresh(sb);
    return sb;
}

void ui_sidebar_destroy(ui_sidebar_t *sidebar)
{
    if (!sidebar) return;
    if (sidebar->container) {
        lv_obj_del(sidebar->container);
        sidebar->container = NULL;
    }
    free(sidebar);
}

void ui_sidebar_refresh(ui_sidebar_t *sidebar)
{
    if (!sidebar || !sidebar->container) return;

    /* 清理旧图标按钮 */
    lv_obj_clean(sidebar->container);
    sidebar->item_count = 0;

    size_t count = cartridge_mgr_get_count();
    if (count > UI_SIDEBAR_MAX_ITEMS) count = UI_SIDEBAR_MAX_ITEMS;

    /* 获取当前活跃卡带 ID */
    cartridge_t *cur = cartridge_mgr_get_current();
    if (cur && cur->ops.id) {
        strncpy(sidebar->active_id, cur->ops.id, sizeof(sidebar->active_id) - 1);
    } else if (sidebar->active_id[0] == '\0') {
        strncpy(sidebar->active_id, "home", sizeof(sidebar->active_id) - 1);
    }

    int y_step = (count > 0) ? (210 / (int)count) : 34;
    if (y_step > 36) y_step = 36;

    for (size_t i = 0; i < count; i++) {
        cartridge_t *c = cartridge_mgr_get_by_index(i);
        if (!c || !c->ops.id) continue;

        ui_sidebar_item_t *it = &sidebar->items[sidebar->item_count];
        memset(it, 0, sizeof(ui_sidebar_item_t));
        strncpy(it->id, c->ops.id, sizeof(it->id) - 1);
        strncpy(it->icon, get_cartridge_icon(c->ops.id), sizeof(it->icon) - 1);

        bool is_active = (strcmp(it->id, sidebar->active_id) == 0);

        it->btn = lv_btn_create(sidebar->container);
        lv_obj_set_size(it->btn, 30, 28);
        lv_obj_set_pos(it->btn, 1, (lv_coord_t)(i * y_step + 4));
        lv_obj_set_style_radius(it->btn, 6, 0);
        lv_obj_set_style_pad_all(it->btn, 0, 0);
        lv_obj_set_ext_click_area(it->btn, 6);

        if (is_active) {
            lv_obj_set_style_bg_color(it->btn, lv_color_hex(0x13273F), 0);
            lv_obj_set_style_border_color(it->btn, lv_color_hex(0x00E5FF), 0);
            lv_obj_set_style_border_width(it->btn, 1, 0);
        } else {
            lv_obj_set_style_bg_color(it->btn, lv_color_hex(0x0A101C), 0);
            lv_obj_set_style_border_color(it->btn, lv_color_hex(0x1B2A40), 0);
            lv_obj_set_style_border_width(it->btn, 1, 0);
        }

        it->lbl = lv_label_create(it->btn);
        lv_obj_center(it->lbl);
        if (sidebar->font) lv_obj_set_style_text_font(it->lbl, sidebar->font, 0);
        lv_label_set_text(it->lbl, it->icon);
        lv_obj_set_style_text_color(it->lbl, is_active ? lv_color_hex(0x00E5FF) : lv_color_hex(0x7E92AD), 0);

        lv_obj_add_event_cb(it->btn, on_item_clicked, LV_EVENT_CLICKED, it);

        sidebar->item_count++;
    }
}

void ui_sidebar_set_active(ui_sidebar_t *sidebar, const char *cartridge_id)
{
    if (!sidebar || !cartridge_id) return;
    if (strcmp(sidebar->active_id, cartridge_id) == 0) return;

    strncpy(sidebar->active_id, cartridge_id, sizeof(sidebar->active_id) - 1);
    ui_sidebar_refresh(sidebar);
}
