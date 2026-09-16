/**
 * @file ui_settings.h
 * @brief 设置与控制中心同级视图组件 (Peer Stage Settings & Sub-Sidebar Navigation)
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
 * @brief 设置中心二级侧边栏标签页定义
 */
typedef enum {
    UI_SETTINGS_TAB_HOTSPOT = 0,  /**< 二级页 1: 独立热点配网 (SoftAP) */
    UI_SETTINGS_TAB_BLE,          /**< 二级页 2: 蓝牙极速配网 (Web Bluetooth) */
    UI_SETTINGS_TAB_AGENT,        /**< 二级页 3: 灵眸大模型与 Prompt */
    UI_SETTINGS_TAB_SYSTEM,       /**< 二级页 4: 系统健康与遥测 */
    UI_SETTINGS_TAB_STORAGE,      /**< 二级页 5: 存储卡与外脑日志 */

    /* 兼容历史枚举别名 */
    UI_SETTINGS_PAGE_MAIN     = 0,
    UI_SETTINGS_PAGE_NETWORK  = 0,
    UI_SETTINGS_PAGE_SYSTEM   = 3,
    UI_SETTINGS_PAGE_AGENT    = 2,
    UI_SETTINGS_PAGE_STORAGE  = 4
} ui_settings_page_t;

typedef ui_settings_page_t ui_settings_tab_t;

/**
 * @brief 设置中心上下文结构体
 */
typedef struct {
    lv_obj_t *container;          /**< 设置主舞台容器 (同级视图: 274x216, x=46, y=24) */
    lv_obj_t *drawer;             /**< 兼容指针 (指向 container) */

    /* 1. 内部二级侧边栏 (宽 56px) */
    lv_obj_t *sub_sidebar;
    lv_obj_t *btn_tab_hotspot;
    lv_obj_t *lbl_tab_hotspot;
    lv_obj_t *btn_tab_ble;
    lv_obj_t *lbl_tab_ble;
    lv_obj_t *btn_tab_agent;
    lv_obj_t *lbl_tab_agent;
    lv_obj_t *btn_tab_system;
    lv_obj_t *lbl_tab_system;
    lv_obj_t *btn_tab_storage;
    lv_obj_t *lbl_tab_storage;

    /* 2. 右侧内容区 (宽 218px) */
    lv_obj_t *content_area;
    lv_obj_t *header_bar;
    lv_obj_t *lbl_header_title;
    lv_obj_t *body_area;

    /* 2.1 独立热点配网面板 */
    lv_obj_t *panel_hotspot;
    lv_obj_t *box_hotspot_idle;
    lv_obj_t *card_hotspot_info;
    lv_obj_t *lbl_hotspot_ssid;
    lv_obj_t *lbl_hotspot_ip;
    lv_obj_t *lbl_hotspot_hint;
    lv_obj_t *btn_hotspot_action;
    lv_obj_t *lbl_hotspot_action;
    lv_obj_t *btn_hotspot_reset;
    lv_obj_t *lbl_hotspot_reset;

    /* 热点配网步进式状态机进度卡片 (网页提交凭证后接管) */
    lv_obj_t *box_hotspot_progress;
    lv_obj_t *lbl_prog_title;
    lv_obj_t *lbl_prog_step1;     /**< 步骤1: 收到Web指令，关闭热点 [✓] */
    lv_obj_t *lbl_prog_step2;     /**< 步骤2: 关联目标 Wi-Fi [⟳] */
    lv_obj_t *lbl_prog_step3;     /**< 步骤3: DHCP 申请 IP [⟳] */
    lv_obj_t *card_prog_result;   /**< 结果提示卡片 */
    lv_obj_t *lbl_prog_result_ip; /**< 分配到的实际局域网 IP */
    lv_obj_t *lbl_prog_result_url;/**< Web伴侣访问入口 */
    lv_obj_t *btn_prog_done;      /**< 完成并返回主页按钮 */
    lv_obj_t *lbl_prog_done;

    /* 2.2 独立蓝牙配网面板 (Web Bluetooth) */
    lv_obj_t *panel_ble;
    lv_obj_t *card_ble_info;
    lv_obj_t *lbl_ble_status;
    lv_obj_t *lbl_ble_dev_name;
    lv_obj_t *lbl_ble_uuid;
    lv_obj_t *btn_ble_toggle;
    lv_obj_t *lbl_ble_toggle;
    lv_obj_t *card_ble_guide;
    lv_obj_t *lbl_ble_guide;

    /* 2.3 灵眸模型与参数面板 */
    lv_obj_t *panel_agent;
    lv_obj_t *card_agent_info;
    lv_obj_t *lbl_agent_model;
    lv_obj_t *lbl_agent_key_st;
    lv_obj_t *lbl_agent_prompt;
    lv_obj_t *lbl_agent_hint;

    /* 2.4 系统健康与遥测面板 */
    lv_obj_t *panel_system;
    lv_obj_t *card_system_info;
    lv_obj_t *lbl_system_uptime;
    lv_obj_t *lbl_system_cpu;
    lv_obj_t *lbl_system_ram;
    lv_obj_t *lbl_system_fps;
    lv_obj_t *lbl_system_ver;

    /* 2.5 存储与外脑面板 */
    lv_obj_t *panel_storage;
    lv_obj_t *card_storage_info;
    lv_obj_t *lbl_storage_sd_st;
    lv_obj_t *lbl_storage_cap;
    lv_obj_t *lbl_storage_log;

    /* 兼容历史指针以保障外部单测引用 */
    lv_obj_t *mask_bg;
    lv_obj_t *header;
    lv_obj_t *btn_back;
    lv_obj_t *lbl_back;
    lv_obj_t *lbl_title;
    lv_obj_t *btn_close;
    lv_obj_t *view_main;
    lv_obj_t *view_detail;
    lv_obj_t *sec_net_box;
    lv_obj_t *card_net_info;
    lv_obj_t *lbl_net_status;
    lv_obj_t *lbl_net_ip;
    lv_obj_t *btn_ble_prov;
    lv_obj_t *lbl_ble_prov_btn;
    lv_obj_t *btn_hotspot;
    lv_obj_t *lbl_hotspot_btn;
    lv_obj_t *sec_sys_box;
    lv_obj_t *card_sys_info;
    lv_obj_t *lbl_sys_uptime;
    lv_obj_t *lbl_sys_cpu;
    lv_obj_t *lbl_sys_fps;
    lv_obj_t *sec_agent_box;
    lv_obj_t *sec_store_box;
    lv_obj_t *handle_bar;

    const lv_font_t *font;
    bool is_open;
    ui_settings_tab_t current_tab;

    void (*on_close_cb)(void *user_data);
    void *close_user_data;

    void (*on_switch_home_cb)(void *user_data);
    void *switch_home_user_data;
} ui_settings_t;

/**
 * @brief 创建设置中心同级视图组件
 * @param parent 屏幕根容器
 * @param font 中文字体
 * @return ui_settings_t 指针
 */
ui_settings_t* ui_settings_create(lv_obj_t *parent, const lv_font_t *font);

/**
 * @brief 销毁设置中心组件
 */
void ui_settings_destroy(ui_settings_t *settings);

/**
 * @brief 打开/切换到设置视图
 */
void ui_settings_open(ui_settings_t *settings);

/**
 * @brief 关闭/切出设置视图
 */
void ui_settings_close(ui_settings_t *settings);

/**
 * @brief 切换显示/隐藏状态
 */
void ui_settings_toggle(ui_settings_t *settings);

/**
 * @brief 查询当前设置视图是否处于开启状态
 */
bool ui_settings_is_open(const ui_settings_t *settings);

/**
 * @brief 切换内部二级侧边栏标签页
 */
void ui_settings_switch_tab(ui_settings_t *settings, ui_settings_tab_t tab);

/**
 * @brief 兼容接口：设置页面
 */
void ui_settings_set_page(ui_settings_t *settings, ui_settings_page_t page);

/**
 * @brief 兼容接口：获取当前页面
 */
ui_settings_page_t ui_settings_get_page(const ui_settings_t *settings);

/**
 * @brief 刷新设置界面数据 (Wi-Fi 状态、蓝牙状态、系统负载等)
 */
void ui_settings_refresh_data(ui_settings_t *settings);

/**
 * @brief 驱动热点配网步进状态机更新 (正在关联、获取IP、成功展示或失败提示)
 * @param settings 设置对象
 * @param mode 当前网络模式
 * @param ssid 目标网络名
 * @param ip 分配到的 IP
 * @param msg 阶段消息
 */
void ui_settings_update_net_progress(ui_settings_t *settings, int mode, const char *ssid, const char *ip, const char *msg);

/**
 * @brief 注册设置中心关闭回调
 */
void ui_settings_set_close_cb(ui_settings_t *settings, void (*cb)(void *), void *user_data);

/**
 * @brief 注册返回主页回调
 */
void ui_settings_set_switch_home_cb(ui_settings_t *settings, void (*cb)(void *), void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UI_SETTINGS_H */
