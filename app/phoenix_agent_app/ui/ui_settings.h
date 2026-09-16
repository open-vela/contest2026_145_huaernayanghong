/**
 * @file ui_settings.h
 * @brief 顶部控制中心抽屉与二级设置菜单 (Two-Level Settings Drawer)
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

/**
 * @brief 控制中心两级页面枚举
 */
typedef enum {
    UI_SETTINGS_PAGE_MAIN = 0,    /**< 一级主菜单：2x2 科技感四宫格卡牌 */
    UI_SETTINGS_PAGE_NETWORK,     /**< 二级：无线网络与独立热点配置 */
    UI_SETTINGS_PAGE_SYSTEM,      /**< 二级：系统健康与环境遥测 */
    UI_SETTINGS_PAGE_AGENT,       /**< 二级：灵眸 Agent 与模型参数 */
    UI_SETTINGS_PAGE_STORAGE      /**< 二级：TF卡存储与极客伴侣 */
} ui_settings_page_t;

typedef struct {
    lv_obj_t *drawer;             /**< 抽屉主容器 */
    lv_obj_t *mask_bg;            /**< 半透明黑色遮罩背景 */

    /* 1. 顶栏 (返回按钮 + 动态标题 + 关闭按钮) */
    lv_obj_t *header;
    lv_obj_t *btn_back;
    lv_obj_t *lbl_back;
    lv_obj_t *lbl_title;
    lv_obj_t *btn_close;

    /* 2. 一级主菜单视图 (2x2 四宫格卡牌) */
    lv_obj_t *view_main;
    lv_obj_t *card_nav_net;
    lv_obj_t *lbl_nav_net_t;
    lv_obj_t *lbl_nav_net_sub;
    lv_obj_t *card_nav_sys;
    lv_obj_t *lbl_nav_sys_t;
    lv_obj_t *lbl_nav_sys_sub;
    lv_obj_t *card_nav_agent;
    lv_obj_t *lbl_nav_agent_t;
    lv_obj_t *lbl_nav_agent_sub;
    lv_obj_t *card_nav_store;
    lv_obj_t *lbl_nav_store_t;
    lv_obj_t *lbl_nav_store_sub;

    /* 3. 二级详情视图容器 */
    lv_obj_t *view_detail;

    /* 3.1 二级：网络与热点 */
    lv_obj_t *sec_net_box;
    lv_obj_t *card_net_info;
    lv_obj_t *lbl_net_status;
    lv_obj_t *lbl_net_ip;
    lv_obj_t *btn_hotspot;
    lv_obj_t *lbl_hotspot_btn;
    lv_obj_t *lbl_hotspot_hint;

    /* 3.2 二级：系统遥测 */
    lv_obj_t *sec_sys_box;
    lv_obj_t *lbl_sys_env;
    lv_obj_t *lbl_sys_load;
    lv_obj_t *lbl_sys_uptime;

    /* 3.3 二级：Agent 模型 */
    lv_obj_t *sec_agent_box;
    lv_obj_t *lbl_agent_model;
    lv_obj_t *lbl_agent_stats;
    lv_obj_t *lbl_agent_last;

    /* 3.4 二级：存储看板 */
    lv_obj_t *sec_store_box;
    lv_obj_t *lbl_store_sd;
    lv_obj_t *lbl_store_web;

    /* 兼容保留字段供外部单元测试/引用 */
    lv_obj_t *card_hotspot;
    lv_obj_t *card_status;
    lv_obj_t *lbl_status_env;
    lv_obj_t *lbl_status_health;
    lv_obj_t *card_history;
    lv_obj_t *lbl_history_stats;
    lv_obj_t *lbl_history_last;

    /* 实体拉手条 */
    lv_obj_t *handle_bar;

    const lv_font_t *font;
    bool is_open;
    int16_t drawer_h;
    ui_settings_page_t current_page;
} ui_settings_t;

/**
 * @brief 创建控制中心抽屉组件
 */
ui_settings_t* ui_settings_create(lv_obj_t *parent, const lv_font_t *font);

/**
 * @brief 销毁控制中心抽屉组件
 */
void ui_settings_destroy(ui_settings_t *settings);

/**
 * @brief 打开抽屉面板
 */
void ui_settings_open(ui_settings_t *settings);

/**
 * @brief 收起抽屉面板
 */
void ui_settings_close(ui_settings_t *settings);

/**
 * @brief 切换抽屉展开/收起
 */
void ui_settings_toggle(ui_settings_t *settings);

/**
 * @brief 检查抽屉是否处于展开状态
 */
bool ui_settings_is_open(const ui_settings_t *settings);

/**
 * @brief 切换控制中心两级页面
 * @param settings 设置面板上下文
 * @param page 目标页面
 */
void ui_settings_set_page(ui_settings_t *settings, ui_settings_page_t page);

/**
 * @brief 获取当前所在页面
 */
ui_settings_page_t ui_settings_get_page(const ui_settings_t *settings);

/**
 * @brief 刷新控制中心内部数据与文本
 */
void ui_settings_refresh_data(ui_settings_t *settings);

#ifdef __cplusplus
}
#endif

#endif /* UI_SETTINGS_H */
