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
 * @brief 设置中心顶部横向导航标签页定义
 */
typedef enum {
    UI_SETTINGS_TAB_NET = 0,      /**< 标签页 1: Wi-Fi 网络连接与热点 */
    UI_SETTINGS_TAB_BLE,          /**< 标签页 2: 蓝牙配网广播开关与服务 */
    UI_SETTINGS_TAB_AGENT,        /**< 标签页 3: 灵眸大模型与 Prompt */
    UI_SETTINGS_TAB_SYSTEM,       /**< 标签页 4: 极客系统健康与遥测 */
    UI_SETTINGS_TAB_STORAGE,      /**< 标签页 5: 存储卡与外脑日志 */
    UI_SETTINGS_TAB_ABOUT,        /**< 标签页 6: 关于设备 */
    UI_SETTINGS_TAB_AUDIO,        /**< 标签页 7: 音频录放与声学实验室 */

    /* 兼容历史枚举别名 */
    UI_SETTINGS_TAB_HOTSPOT   = 0,
    UI_SETTINGS_PAGE_MAIN     = 0,
    UI_SETTINGS_PAGE_NETWORK  = 0,
    UI_SETTINGS_PAGE_BLE      = 1,
    UI_SETTINGS_PAGE_AGENT    = 2,
    UI_SETTINGS_PAGE_SYSTEM   = 3,
    UI_SETTINGS_PAGE_STORAGE  = 4,
    UI_SETTINGS_PAGE_ABOUT    = 5,
    UI_SETTINGS_PAGE_AUDIO    = 6
} ui_settings_page_t;

typedef ui_settings_page_t ui_settings_tab_t;

/**
 * @brief 设置中心上下文结构体 (全宽 274px 舞台 + 顶部胶囊导航栏)
 */
typedef struct {
    lv_obj_t *container;          /**< 设置主舞台容器 (同级视图: 274x216, x=46, y=24) */
    lv_obj_t *drawer;             /**< 兼容指针 (指向 container) */

    /* 1. 顶部导航条 (高 30px, 宽 274px, x=0, y=0) */
    lv_obj_t *top_tab_bar;
    lv_obj_t *btn_top_close;      /**< 顶栏左侧按钮 (主菜单显示 ✕ 退出，详情页显示 < 返回) */
    lv_obj_t *lbl_top_close;
    lv_obj_t *lbl_top_title;      /**< 顶栏标题 (主菜单显示 ⚙️ 系统设置，详情页显示模块名) */

    /* 2. 状态标识 */
    bool is_in_detail;            /**< 当前是否下钻进入二级详情页 */

    /* 3. 视图 1: 垂直卡片菜单列表 (宽 274px, 高 186px, 纵向平滑滚动) */
    lv_obj_t *view_menu_list;
    lv_obj_t *btn_menu_net;       /**< 菜单项 1: Wi-Fi网络 */
    lv_obj_t *lbl_menu_net_sub;   /**< 菜单项 1 摘要: 已连接 / 广播中 */
    lv_obj_t *btn_menu_ble;       /**< 菜单项 2: 蓝牙配网 (独立开关面板) */
    lv_obj_t *lbl_menu_ble_sub;   /**< 菜单项 2 摘要: 广播中 / 未开启 */
    lv_obj_t *btn_menu_agent;     /**< 菜单项 3: 灵眸模型 */
    lv_obj_t *lbl_menu_agent_sub; /**< 菜单项 3 摘要: DeepSeek */
    lv_obj_t *btn_menu_system;    /**< 菜单项 4: 硬件状态 */
    lv_obj_t *lbl_menu_system_sub;/**< 菜单项 4 摘要: 正常 / 60FPS */
    lv_obj_t *btn_menu_storage;   /**< 菜单项 5: 存储日志 */
    lv_obj_t *lbl_menu_storage_sub;/**< 菜单项 5 摘要: 28.6GB */
    lv_obj_t *btn_menu_about;     /**< 菜单项 6: 关于设备 */
    lv_obj_t *lbl_menu_about_sub; /**< 菜单项 6 摘要: OpenVela */
    lv_obj_t *btn_menu_audio;     /**< 菜单项 7: 音频调试 */
    lv_obj_t *lbl_menu_audio_sub; /**< 菜单项 7 摘要: 录放音 > */

    /* 4. 视图 2: 二级下钻详情区域 (宽 274px, 高 186px) */
    lv_obj_t *view_detail_area;
    lv_obj_t *panel_about;        /**< 关于设备卡片 */
    lv_obj_t *panel_audio;        /**< 音频调试主卡片 */

    /* 4.1 音频调试面板内部控件 */
    lv_obj_t *lbl_audio_rec_status;
    lv_obj_t *bar_audio_energy;
    lv_obj_t *btn_audio_rec;
    lv_obj_t *lbl_audio_rec_btn;
    lv_obj_t *lbl_audio_play_status;
    lv_obj_t *btn_audio_play_rec;
    lv_obj_t *lbl_audio_play_rec;
    lv_obj_t *btn_audio_play_tone;
    lv_obj_t *lbl_audio_play_tone;
    lv_obj_t *sw_audio_loopback;
    lv_obj_t *lbl_audio_loopback;
    lv_obj_t *slider_audio_vol;
    lv_obj_t *lbl_audio_vol_val;


    /* 兼容保留字段指针 */
    lv_obj_t *btn_tab_net;
    lv_obj_t *lbl_tab_net;
    lv_obj_t *btn_tab_agent;
    lv_obj_t *lbl_tab_agent;
    lv_obj_t *btn_tab_system;
    lv_obj_t *lbl_tab_system;
    lv_obj_t *btn_tab_storage;
    lv_obj_t *lbl_tab_storage;
    lv_obj_t *sub_sidebar;
    lv_obj_t *btn_tab_hotspot;
    lv_obj_t *lbl_tab_hotspot;
    lv_obj_t *btn_tab_ble;
    lv_obj_t *lbl_tab_ble;

    /* 5. 主内容容器 */
    lv_obj_t *content_area;
    lv_obj_t *header_bar;
    lv_obj_t *lbl_header_title;
    lv_obj_t *body_area;

    /* 2.1 独立热点与 Wi-Fi 扫描直连面板 */
    lv_obj_t *panel_hotspot;      /**< 兼容指针 */
    lv_obj_t *panel_wifi;         /**< 扫描与直连主面板 */
    lv_obj_t *box_wifi_header;    /**< 顶部状态与刷新条 */
    lv_obj_t *lbl_wifi_status;    /**< 状态: 已连 / 未连接 / 正在连接 */
    lv_obj_t *btn_wifi_refresh;   /**< 刷新扫描按钮 */
    lv_obj_t *lbl_wifi_refresh;
    lv_obj_t *list_wifi;          /**< 扫描到的热点纵向滚动列表容器 */
    lv_obj_t *lbl_wifi_empty;     /**< 扫描中/无热点提示标签 */
    lv_obj_t *box_wifi_footer;    /**< 底部辅助工具栏 */
    lv_obj_t *btn_wifi_forget;    /**< 清空已存网络按钮 */
    lv_obj_t *lbl_wifi_forget;
    lv_obj_t *btn_wifi_ap_mode;   /**< 应急热点模式切换按钮 */
    lv_obj_t *lbl_wifi_ap_mode;

    /* 2.1.1 全屏密码输入模态对话框与 LVGL 软键盘 (320x240) */
    lv_obj_t *dlg_pwd_modal;      /**< 全屏遮罩容器 (parent: lv_screen_active) */
    lv_obj_t *lbl_pwd_target;     /**< 目标 SSID 标签 */
    lv_obj_t *btn_pwd_close;      /**< 弹窗右上角关闭按钮 */
    lv_obj_t *lbl_pwd_close;
    lv_obj_t *ta_pwd_input;       /**< 密码输入文本域 (lv_textarea) */
    lv_obj_t *btn_pwd_eye;        /**< 密码明/密文切换按钮 */
    lv_obj_t *lbl_pwd_eye;
    lv_obj_t *btn_pwd_connect;    /**< 提交连接按钮 */
    lv_obj_t *lbl_pwd_connect;
    lv_obj_t *lbl_pwd_hint;       /**< 错误/连接中状态提示 */
    lv_obj_t *kb_pwd;             /**< 底部全尺寸软键盘 (lv_keyboard) */
    char     selected_ssid[34];   /**< 当前选中的目标 SSID */
    bool     is_pwd_obscure;      /**< 密码当前是否密文遮蔽 */
    uint32_t last_scan_req_time;  /**< 上次请求扫描时间戳，防止过频刷新 */

    /* 2.1.2 网络属性与 DHCP 详情模态弹窗组件 */
    lv_obj_t *dlg_dhcp_modal;     /**< DHCP 详情浮层遮罩容器 */
    lv_obj_t *card_dhcp_content;  /**< 详情卡片 */
    lv_obj_t *lbl_dhcp_title;     /**< 标题: 网络配置与 DHCP 租约 */
    lv_obj_t *btn_dhcp_close;     /**< 右上角关闭按钮 */
    lv_obj_t *lbl_dhcp_close;
    lv_obj_t *lbl_dhcp_ssid;      /**< 当前网络名称 (SSID) */
    lv_obj_t *lbl_dhcp_mode;      /**< 分配方式 (DHCP 动态分配) */
    lv_obj_t *lbl_dhcp_ip;        /**< 本机 IPv4 地址 */
    lv_obj_t *lbl_dhcp_mask;      /**< 子网掩码 */
    lv_obj_t *lbl_dhcp_gw;        /**< 默认网关 */
    lv_obj_t *lbl_dhcp_dns;       /**< DNS 服务器 */
    lv_obj_t *lbl_dhcp_mac;       /**< 物理 MAC 地址 */
    lv_obj_t *lbl_dhcp_lease;     /**< 租约时间 */
    lv_obj_t *btn_dhcp_renew;     /**< 重新获取租约按钮 */
    lv_obj_t *lbl_dhcp_renew;
    lv_obj_t *btn_dhcp_ok;        /**< 返回确定按钮 */
    lv_obj_t *lbl_dhcp_ok;

    /* 兼容历史指针保留 */
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
 * @brief 切换内部标签页
 */
void ui_settings_switch_tab(ui_settings_t *settings, ui_settings_tab_t tab);

/**
 * @brief 下钻进入二级模块详情页
 */
void ui_settings_enter_detail(ui_settings_t *settings, ui_settings_tab_t tab);

/**
 * @brief 从二级详情页平滑返回设置主菜单列表
 */
void ui_settings_back_to_menu(ui_settings_t *settings);

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
 * @brief 弹出全屏密码输入软键盘对话框
 * @param settings 设置对象
 * @param ssid 目标 Wi-Fi SSID
 */
void ui_settings_show_password_dialog(ui_settings_t *settings, const char *ssid);

/**
 * @brief 关闭全屏密码输入软键盘对话框
 * @param settings 设置对象
 */
void ui_settings_close_password_dialog(ui_settings_t *settings);

/**
 * @brief 刷新 Wi-Fi 热点列表展示
 * @param settings 设置对象
 */
void ui_settings_refresh_wifi_list(ui_settings_t *settings);

/**
 * @brief 显示网络配置与 DHCP 详细信息模态对话框
 * @param settings 设置对象
 */
void ui_settings_show_dhcp_dialog(ui_settings_t *settings);

/**
 * @brief 关闭网络配置与 DHCP 详细信息模态对话框
 * @param settings 设置对象
 */
void ui_settings_close_dhcp_dialog(ui_settings_t *settings);

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
