/**
 * @file ui_settings.h
 * @brief 顶部控制中心抽屉与设置面板 (Top Pull-down Settings Drawer)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl/lvgl.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    lv_obj_t *drawer;             /**< 抽屉主容器 (可平滑上下滑动) */
    lv_obj_t *mask_bg;            /**< 半透明黑色遮罩背景 */

    /* 1. 顶栏标题与关闭 */
    lv_obj_t *lbl_title;
    lv_obj_t *btn_close;

    /* 2. 外部配网与热点通道 */
    lv_obj_t *card_hotspot;
    lv_obj_t *btn_hotspot;
    lv_obj_t *lbl_hotspot_btn;
    lv_obj_t *lbl_hotspot_hint;

    /* 3. 系统当前运行状态 */
    lv_obj_t *card_status;
    lv_obj_t *lbl_status_env;     /**< 温湿度与电量 */
    lv_obj_t *lbl_status_health;  /**< 系统健康度与运行时间 */

    /* 4. 历史交互数据与遥测 */
    lv_obj_t *card_history;
    lv_obj_t *lbl_history_stats;  /**< 交互次数、Tokens、专注时长 */
    lv_obj_t *lbl_history_last;   /**< 最新一条历史交互指令 */

    /* 底部拉手条 */
    lv_obj_t *handle_bar;

    const lv_font_t *font;
    bool is_open;
    int16_t drawer_h;
} ui_settings_t;

/**
 * @brief 创建控制中心抽屉组件
 * @param parent 顶层父容器 (通常为 lv_scr_act())
 * @param font 中文字体
 * @return ui_settings_t 指针
 */
ui_settings_t* ui_settings_create(lv_obj_t *parent, const lv_font_t *font);

/**
 * @brief 销毁控制中心抽屉组件
 * @param settings 设置面板上下文
 */
void ui_settings_destroy(ui_settings_t *settings);

/**
 * @brief 打开抽屉面板 (向下平滑滑入)
 * @param settings 设置面板上下文
 */
void ui_settings_open(ui_settings_t *settings);

/**
 * @brief 收起抽屉面板 (向上平滑收起)
 * @param settings 设置面板上下文
 */
void ui_settings_close(ui_settings_t *settings);

/**
 * @brief 切换抽屉展开/收起状态
 * @param settings 设置面板上下文
 */
void ui_settings_toggle(ui_settings_t *settings);

/**
 * @brief 查询抽屉当前是否处于展开状态
 * @param settings 设置面板上下文
 * @return true 展开, false 收起
 */
bool ui_settings_is_open(const ui_settings_t *settings);

/**
 * @brief 刷新抽屉内的实时网络、Web服务与系统指标
 * @param settings 设置面板上下文
 */
void ui_settings_refresh_data(ui_settings_t *settings);

#ifdef __cplusplus
}
#endif

#endif /* UI_SETTINGS_H */
