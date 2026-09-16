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
    return "钟";
  if (strcmp(id, "zen") == 0)
    return "禅";
  if (strcmp(id, "memo") == 0)
    return "记";
  if (strcmp(id, "agent") == 0)
    return "AI";
  if (strcmp(id, "familiar") == 0)
    return "宠";
  return "卡";
}

static void on_item_clicked(lv_event_t *e) {
  ui_sidebar_item_t *item = (ui_sidebar_item_t *)lv_event_get_user_data(e);
  if (!item || !item->id[0])
    return;

  LOG_I(TAG, "用户点击侧边栏切换卡带: [%s]", item->id);
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

ui_sidebar_t *ui_sidebar_create(lv_obj_t *parent, const lv_font_t *font) {
  if (!parent)
    return NULL;

  ui_sidebar_t *sb = (ui_sidebar_t *)calloc(1, sizeof(ui_sidebar_t));
  if (!sb)
    return NULL;

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

  /* 清理旧图标按钮与子部件 */
  lv_obj_clean(sidebar->container);
  sidebar->item_count = 0;
  sidebar->btn_settings = NULL;
  sidebar->lbl_settings = NULL;
  sidebar->sep_line = NULL;

  /* 黄金三大超级核心卡带：主页全景 [主]、机械翻页钟 [钟]、使魔灵眸生命体 [宠] */
  static const char *s_golden_trio[] = {"home", "clock", "familiar"};
  const size_t golden_count = sizeof(s_golden_trio) / sizeof(s_golden_trio[0]);

  /* 获取当前活跃卡带 ID，支持兼容性合体映射 */
  cartridge_t *cur = cartridge_mgr_get_current();
  const char *cur_id = (cur && cur->ops.id[0] != '\0') ? cur->ops.id : "home";
  
  /* 智能映射：agent 与 zen 融入使魔 familiar；memo 融入主页 home */
  const char *mapped_active_id = cur_id;
  if (strcmp(cur_id, "agent") == 0 || strcmp(cur_id, "zen") == 0) {
    mapped_active_id = "familiar";
  } else if (strcmp(cur_id, "memo") == 0) {
    mapped_active_id = "home";
  }
  strncpy(sidebar->active_id, mapped_active_id, sizeof(sidebar->active_id) - 1);

  /* 黄金三大卡带大磁贴排布 (容器高 216px: 38x36px, 步进 46px, 底部设置 38x44px) */
  const lv_coord_t btn_h = 36;
  const lv_coord_t start_y = 8;
  const lv_coord_t step_y = 46;
  const lv_coord_t sep_y = 152;
  const lv_coord_t set_y = 160;
  const lv_coord_t set_h = 44;

  /* 1. 顶部卡带大按钮区域 (3 项) */
  for (size_t i = 0; i < golden_count; i++) {
    const char *cid = s_golden_trio[i];
    cartridge_t *c = cartridge_mgr_get_by_id(cid);
    if (!c || c->ops.id[0] == '\0') {
      /* 兜底以防特定环境顺序 */
      c = cartridge_mgr_get_by_index(i);
    }
    if (!c || c->ops.id[0] == '\0')
      continue;

    ui_sidebar_item_t *it = &sidebar->items[sidebar->item_count];
    memset(it, 0, sizeof(ui_sidebar_item_t));
    strncpy(it->id, cid, sizeof(it->id) - 1);
    strncpy(it->icon, get_cartridge_icon(cid), sizeof(it->icon) - 1);

    /* 若设置处于激活态，则卡带按钮不高亮 */
    bool is_active = (!sidebar->is_settings_active &&
                      strcmp(it->id, sidebar->active_id) == 0);

    it->btn = lv_btn_create(sidebar->container);
    lv_obj_set_size(it->btn, 38, btn_h);
    lv_obj_set_pos(it->btn, 4, (lv_coord_t)(start_y + i * step_y));
    lv_obj_set_style_radius(it->btn, 8, 0);
    lv_obj_set_style_pad_all(it->btn, 0, 0);
    lv_obj_set_ext_click_area(it->btn, 6);

    if (is_active) {
      lv_obj_set_style_bg_color(it->btn, lv_color_hex(0x153152), 0);
      lv_obj_set_style_border_color(it->btn, lv_color_hex(0x00E5FF), 0);
      lv_obj_set_style_border_width(it->btn, 2, 0);
      lv_obj_set_style_shadow_color(it->btn, lv_color_hex(0x00E5FF), 0);
      lv_obj_set_style_shadow_width(it->btn, 10, 0);
      lv_obj_set_style_shadow_opa(it->btn, LV_OPA_50, 0);
    } else {
      lv_obj_set_style_bg_color(it->btn, lv_color_hex(0x0B1220), 0);
      lv_obj_set_style_border_color(it->btn, lv_color_hex(0x1C2B42), 0);
      lv_obj_set_style_border_width(it->btn, 1, 0);
    }

    it->lbl = lv_label_create(it->btn);
    lv_obj_center(it->lbl);
    if (sidebar->font)
      lv_obj_set_style_text_font(it->lbl, sidebar->font, 0);
    lv_label_set_text(it->lbl, it->icon);
    lv_obj_set_style_text_color(
        it->lbl, is_active ? lv_color_hex(0x00E5FF) : lv_color_hex(0x8EA4C0),
        0);

    lv_obj_add_event_cb(it->btn, on_item_clicked, LV_EVENT_CLICKED, it);

    sidebar->item_count++;
  }

  /* 2. 科技感分隔线 */
  sidebar->sep_line = lv_obj_create(sidebar->container);
  lv_obj_set_size(sidebar->sep_line, 32, 1);
  lv_obj_set_pos(sidebar->sep_line, 7, sep_y);
  lv_obj_set_style_bg_color(sidebar->sep_line, lv_color_hex(0x1B2C47), 0);
  lv_obj_set_style_border_width(sidebar->sep_line, 0, 0);
  lv_obj_clear_flag(sidebar->sep_line, LV_OBJ_FLAG_SCROLLABLE);

  /* 3. 底部控制中心大号专属按钮 [设] */
  sidebar->btn_settings = lv_btn_create(sidebar->container);
  lv_obj_set_size(sidebar->btn_settings, 38, set_h);
  lv_obj_set_pos(sidebar->btn_settings, 4, set_y);
  lv_obj_set_style_radius(sidebar->btn_settings, 8, 0);
  lv_obj_set_style_pad_all(sidebar->btn_settings, 0, 0);
  lv_obj_set_ext_click_area(sidebar->btn_settings, 6);

  if (sidebar->is_settings_active) {
    lv_obj_set_style_bg_color(sidebar->btn_settings, lv_color_hex(0x1C3252), 0);
    lv_obj_set_style_border_color(sidebar->btn_settings, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(sidebar->btn_settings, 2, 0);
    lv_obj_set_style_shadow_color(sidebar->btn_settings, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_shadow_width(sidebar->btn_settings, 8, 0);
    lv_obj_set_style_shadow_opa(sidebar->btn_settings, LV_OPA_40, 0);
  } else {
    lv_obj_set_style_bg_color(sidebar->btn_settings, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_border_color(sidebar->btn_settings, lv_color_hex(0x223654), 0);
    lv_obj_set_style_border_width(sidebar->btn_settings, 1, 0);
  }

  sidebar->lbl_settings = lv_label_create(sidebar->btn_settings);
  lv_obj_center(sidebar->lbl_settings);
  if (sidebar->font)
    lv_obj_set_style_text_font(sidebar->lbl_settings, sidebar->font, 0);
  lv_label_set_text(sidebar->lbl_settings, "设");
  lv_obj_set_style_text_color(sidebar->lbl_settings,
                              sidebar->is_settings_active
                                  ? lv_color_hex(0x00E5FF)
                                  : lv_color_hex(0x8EA4C0),
                              0);

  lv_obj_add_event_cb(sidebar->btn_settings, on_settings_clicked,
                      LV_EVENT_CLICKED, sidebar);
}

void ui_sidebar_set_active(ui_sidebar_t *sidebar, const char *cartridge_id) {
  if (!sidebar || !cartridge_id)
    return;
  sidebar->is_settings_active = false;
  if (strcmp(sidebar->active_id, cartridge_id) == 0) {
    ui_sidebar_refresh(sidebar);
    return;
  }

  strncpy(sidebar->active_id, cartridge_id, sizeof(sidebar->active_id) - 1);
  ui_sidebar_refresh(sidebar);
}

void ui_sidebar_set_settings_cb(ui_sidebar_t *sidebar,
                                ui_sidebar_settings_cb_t cb, void *user_data) {
  if (!sidebar)
    return;
  sidebar->on_settings_cb = cb;
  sidebar->settings_user_data = user_data;
}

void ui_sidebar_set_settings_active(ui_sidebar_t *sidebar, bool active) {
  if (!sidebar)
    return;
  if (sidebar->is_settings_active == active)
    return;
  sidebar->is_settings_active = active;
  ui_sidebar_refresh(sidebar);
}
