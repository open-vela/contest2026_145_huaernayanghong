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

static const char *get_cartridge_icon(const char *id) {
  if (!id)
    return "卡";
  if (strcmp(id, "home") == 0)
    return "主";
  if (strcmp(id, "clock") == 0)
    return "茄";
  if (strcmp(id, "zen") == 0)
    return "禅";
  if (strcmp(id, "memo") == 0)
    return "记";
  if (strcmp(id, "agent") == 0)
    return "眸";
  if (strcmp(id, "familiar") == 0)
    return "宠";
  if (strcmp(id, "voice") == 0 || strcmp(id, "audio") == 0)
    return "语";
  return "卡";
}

static void on_item_clicked(lv_event_t *e) {
  ui_sidebar_item_t *item = (ui_sidebar_item_t *)lv_event_get_user_data(e);
  if (!item || !item->id[0])
    return;

  ui_sidebar_t *sb = (ui_sidebar_t *)item->parent_sidebar;
  LOG_I(TAG, "用户点击侧边栏功能项: [%s]", item->id);
  if (sb && sb->on_action_cb) {
    if (sb->on_action_cb(item->id, sb->action_user_data)) {
      return;
    }
  }
  cartridge_mgr_switch_to(item->id);
}

static void on_settings_clicked(lv_event_t *e) {
  ui_sidebar_t *sb = (ui_sidebar_t *)lv_event_get_user_data(e);
  if (!sb)
    return;

  LOG_I(TAG, "用户点击侧边栏底部控制中心按钮 [设]");
  if (sb->on_settings_cb) {
    sb->on_settings_cb(sb->settings_user_data);
  }
}

static void update_sidebar_styles(ui_sidebar_t *sidebar) {
  if (!sidebar || !sidebar->container)
    return;

  for (size_t i = 0; i < sidebar->item_count; i++) {
    ui_sidebar_item_t *it = &sidebar->items[i];
    if (!it->btn || !it->lbl)
      continue;

    bool is_active = (!sidebar->is_settings_active &&
                      strcmp(it->id, sidebar->active_id) == 0);

    if (is_active) {
      lv_obj_set_style_bg_color(it->btn, lv_color_hex(0x153152), 0);
      lv_obj_set_style_border_color(it->btn, lv_color_hex(0x00E5FF), 0);
      lv_obj_set_style_border_width(it->btn, 2, 0);
      lv_obj_set_style_shadow_color(it->btn, lv_color_hex(0x00E5FF), 0);
      lv_obj_set_style_shadow_width(it->btn, 10, 0);
      lv_obj_set_style_shadow_opa(it->btn, LV_OPA_50, 0);
      lv_obj_set_style_text_color(it->lbl, lv_color_hex(0x00E5FF), 0);
    } else {
      lv_obj_set_style_bg_color(it->btn, lv_color_hex(0x0B1220), 0);
      lv_obj_set_style_border_color(it->btn, lv_color_hex(0x1C2B42), 0);
      lv_obj_set_style_border_width(it->btn, 1, 0);
      lv_obj_set_style_shadow_width(it->btn, 0, 0);
      lv_obj_set_style_shadow_opa(it->btn, LV_OPA_TRANSP, 0);
      lv_obj_set_style_text_color(it->lbl, lv_color_hex(0x8EA4C0), 0);
    }
  }

  if (sidebar->btn_settings && sidebar->lbl_settings) {
    if (sidebar->is_settings_active) {
      lv_obj_set_style_bg_color(sidebar->btn_settings, lv_color_hex(0x1C3252), 0);
      lv_obj_set_style_border_color(sidebar->btn_settings, lv_color_hex(0x00E5FF), 0);
      lv_obj_set_style_border_width(sidebar->btn_settings, 2, 0);
      lv_obj_set_style_shadow_color(sidebar->btn_settings, lv_color_hex(0x00E5FF), 0);
      lv_obj_set_style_shadow_width(sidebar->btn_settings, 8, 0);
      lv_obj_set_style_shadow_opa(sidebar->btn_settings, LV_OPA_40, 0);
      lv_obj_set_style_text_color(sidebar->lbl_settings, lv_color_hex(0x00E5FF), 0);
    } else {
      lv_obj_set_style_bg_color(sidebar->btn_settings, lv_color_hex(0x0B1220), 0);
      lv_obj_set_style_border_color(sidebar->btn_settings, lv_color_hex(0x223654), 0);
      lv_obj_set_style_border_width(sidebar->btn_settings, 1, 0);
      lv_obj_set_style_shadow_width(sidebar->btn_settings, 0, 0);
      lv_obj_set_style_shadow_opa(sidebar->btn_settings, LV_OPA_TRANSP, 0);
      lv_obj_set_style_text_color(sidebar->lbl_settings, lv_color_hex(0x8EA4C0), 0);
    }
  }
}

static void init_sidebar_widgets(ui_sidebar_t *sb) {
  if (!sb || !sb->container)
    return;

  lv_obj_clean(sb->container);
  sb->item_count = 0;
  sb->btn_settings = NULL;
  sb->lbl_settings = NULL;
  sb->sep_line = NULL;

  /* 核心常驻功能大磁贴：灵眸 [眸]、主页 [主]、番茄钟 [茄]、语音实验室 [语] */
  static const char *s_nav_items[] = {"agent", "home", "clock", "voice"};
  const size_t nav_count = sizeof(s_nav_items) / sizeof(s_nav_items[0]);

  const lv_coord_t btn_h = 32;
  const lv_coord_t start_y = 6;
  const lv_coord_t step_y = 38;
  const lv_coord_t sep_y = 158;
  const lv_coord_t set_y = 164;
  const lv_coord_t set_h = 38;

  /* 1. 顶部卡带大按钮区域 (4 项) */
  for (size_t i = 0; i < nav_count; i++) {
    const char *cid = s_nav_items[i];
    ui_sidebar_item_t *it = &sb->items[sb->item_count];
    memset(it, 0, sizeof(ui_sidebar_item_t));
    it->parent_sidebar = sb;
    strncpy(it->id, cid, sizeof(it->id) - 1);
    strncpy(it->icon, get_cartridge_icon(cid), sizeof(it->icon) - 1);

    it->btn = lv_btn_create(sb->container);
    lv_obj_set_size(it->btn, 38, btn_h);
    lv_obj_set_pos(it->btn, 4, (lv_coord_t)(start_y + i * step_y));
    lv_obj_set_style_radius(it->btn, 8, 0);
    lv_obj_set_style_pad_all(it->btn, 0, 0);
    lv_obj_set_ext_click_area(it->btn, 6);

    it->lbl = lv_label_create(it->btn);
    lv_obj_center(it->lbl);
    if (sb->font)
      lv_obj_set_style_text_font(it->lbl, sb->font, 0);
    lv_label_set_text(it->lbl, it->icon);

    lv_obj_add_event_cb(it->btn, on_item_clicked, LV_EVENT_CLICKED, it);

    sb->item_count++;
  }

  /* 2. 科技感分隔线 */
  sb->sep_line = lv_obj_create(sb->container);
  lv_obj_set_size(sb->sep_line, 32, 1);
  lv_obj_set_pos(sb->sep_line, 7, sep_y);
  lv_obj_set_style_bg_color(sb->sep_line, lv_color_hex(0x1B2C47), 0);
  lv_obj_set_style_border_width(sb->sep_line, 0, 0);
  lv_obj_clear_flag(sb->sep_line, LV_OBJ_FLAG_SCROLLABLE);

  /* 3. 底部控制中心大号专属按钮 [设] */
  sb->btn_settings = lv_btn_create(sb->container);
  lv_obj_set_size(sb->btn_settings, 38, set_h);
  lv_obj_set_pos(sb->btn_settings, 4, set_y);
  lv_obj_set_style_radius(sb->btn_settings, 8, 0);
  lv_obj_set_style_pad_all(sb->btn_settings, 0, 0);
  lv_obj_set_ext_click_area(sb->btn_settings, 6);

  sb->lbl_settings = lv_label_create(sb->btn_settings);
  lv_obj_center(sb->lbl_settings);
  if (sb->font)
    lv_obj_set_style_text_font(sb->lbl_settings, sb->font, 0);
  lv_label_set_text(sb->lbl_settings, "设");

  lv_obj_add_event_cb(sb->btn_settings, on_settings_clicked,
                      LV_EVENT_CLICKED, sb);

  update_sidebar_styles(sb);
}

ui_sidebar_t *ui_sidebar_create(lv_obj_t *parent, const lv_font_t *font) {
  if (!parent)
    return NULL;

  ui_sidebar_t *sb = (ui_sidebar_t *)calloc(1, sizeof(ui_sidebar_t));
  if (!sb)
    return NULL;

  sb->font = font;
  strncpy(sb->active_id, "agent", sizeof(sb->active_id) - 1);

  /* 1. 侧边栏垂直容器 (宽 46px, 高 216px, x=0, y=24) */
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

  init_sidebar_widgets(sb);
  return sb;
}

void ui_sidebar_destroy(ui_sidebar_t *sidebar) {
  if (!sidebar)
    return;
  if (sidebar->container) {
    lv_obj_del(sidebar->container);
    sidebar->container = NULL;
  }
  free(sidebar);
}

void ui_sidebar_refresh(ui_sidebar_t *sidebar) {
  if (!sidebar || !sidebar->container)
    return;
  update_sidebar_styles(sidebar);
}

void ui_sidebar_set_active(ui_sidebar_t *sidebar, const char *cartridge_id) {
  if (!sidebar || !cartridge_id)
    return;

  sidebar->is_settings_active = false;

  /* 智能映射：zen 融入 clock，memo 融入 home */
  const char *mapped_id = cartridge_id;
  if (strcmp(cartridge_id, "zen") == 0) {
    mapped_id = "clock";
  } else if (strcmp(cartridge_id, "memo") == 0) {
    mapped_id = "home";
  }

  strncpy(sidebar->active_id, mapped_id, sizeof(sidebar->active_id) - 1);
  update_sidebar_styles(sidebar);
}

void ui_sidebar_set_settings_cb(ui_sidebar_t *sidebar,
                                ui_sidebar_settings_cb_t cb, void *user_data) {
  if (!sidebar)
    return;
  sidebar->on_settings_cb = cb;
  sidebar->settings_user_data = user_data;
}

void ui_sidebar_set_action_cb(ui_sidebar_t *sidebar,
                              ui_sidebar_action_cb_t cb, void *user_data) {
  if (!sidebar)
    return;
  sidebar->on_action_cb = cb;
  sidebar->action_user_data = user_data;
}

void ui_sidebar_set_settings_active(ui_sidebar_t *sidebar, bool active) {
  if (!sidebar)
    return;
  sidebar->is_settings_active = active;
  update_sidebar_styles(sidebar);
}
