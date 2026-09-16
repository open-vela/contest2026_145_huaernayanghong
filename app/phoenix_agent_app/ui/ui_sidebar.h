/**
 * @file ui_sidebar.h
 * @brief 侧边常驻导航栏组件 (Side Navigation Rail)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef UI_SIDEBAR_H
#define UI_SIDEBAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl/lvgl.h>
#include <stdbool.h>

#define UI_SIDEBAR_WIDTH 36
#define UI_SIDEBAR_MAX_ITEMS 8

typedef struct {
    char id[16];
    char icon[8];
    lv_obj_t *btn;
    lv_obj_t *lbl;
} ui_sidebar_item_t;

typedef void (*ui_sidebar_settings_cb_t)(void *user_data);

typedef struct {
    lv_obj_t *container;
    const lv_font_t *font;
    ui_sidebar_item_t items[UI_SIDEBAR_MAX_ITEMS];
    size_t item_count;
    char active_id[16];

    /* 底部控制中心独立入口 */
    lv_obj_t *sep_line;
    lv_obj_t *btn_settings;
    lv_obj_t *lbl_settings;
    bool is_settings_active;

    ui_sidebar_settings_cb_t on_settings_cb;
    void *settings_user_data;
} ui_sidebar_t;

/**
 * @brief 创建侧边导航栏
 * @param parent 屏幕根容器
 * @param font 字体指针
 * @return 侧边栏上下文
 */
ui_sidebar_t* ui_sidebar_create(lv_obj_t *parent, const lv_font_t *font);

/**
 * @brief 销毁侧边导航栏
 * @param sidebar 侧边栏上下文
 */
void ui_sidebar_destroy(ui_sidebar_t *sidebar);

/**
 * @brief 设置当前高亮选中的卡带
 * @param sidebar 侧边栏上下文
 * @param cartridge_id 卡带ID
 */
void ui_sidebar_set_active(ui_sidebar_t *sidebar, const char *cartridge_id);

/**
 * @brief 注册底部控制中心按钮点击回调
 */
void ui_sidebar_set_settings_cb(ui_sidebar_t *sidebar, ui_sidebar_settings_cb_t cb, void *user_data);

/**
 * @brief 设置底部控制中心高亮选中态
 */
void ui_sidebar_set_settings_active(ui_sidebar_t *sidebar, bool active);

/**
 * @brief 刷新侧边栏项目与高亮状态
 * @param sidebar 侧边栏上下文
 */
void ui_sidebar_refresh(ui_sidebar_t *sidebar);

#ifdef __cplusplus
}
#endif

#endif /* UI_SIDEBAR_H */
