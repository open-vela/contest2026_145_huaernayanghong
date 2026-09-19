/**
 * @file ui_capsule.h
 * @brief 顶部极窄微状态胶囊组件 (22~24px Top Status Capsule)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef UI_CAPSULE_H
#define UI_CAPSULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl/lvgl.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    lv_obj_t *container;        /**< 胶囊外壳容器 */
    lv_obj_t *lbl_env;          /**< 左侧：环境温湿度 "26℃ 60%" */
    lv_obj_t *lbl_cartridge;    /**< 兼容别名指针，指向 lbl_env */
    lv_obj_t *pill_status;      /**< 居中：全局 Agent 心智微胶囊 */
    lv_obj_t *lbl_status;       /**< 状态文本 "● 待命" / "🎙️ 聆听" / "🧠 思考" */
    lv_obj_t *lbl_telemetry;    /**< 右侧：网络状态与电池电量 "Wi-Fi 85%" */

    lv_timer_t *dim_timer;      /**< 自动休眠淡化定时器 */
    const lv_font_t *font;
    bool is_dimmed;

    /* 动态音频跳动波形条 (3 根微柱) */
    lv_obj_t   *wave_bars[3];
    lv_timer_t *wave_timer;
    bool        is_wave_active;

    /* Wi-Fi 呼吸微光 */
    lv_timer_t *breath_timer;
    int16_t     breath_val;
    int8_t      breath_step;

    /* 番茄钟后台专注微标常驻 */
    bool        pomo_active;
    uint16_t    pomo_remaining_s;

    float   current_temp_c;
    uint8_t current_humi_pct;
    uint8_t current_battery;
    char    current_net_str[16];
} ui_capsule_t;

/**
 * @brief 创建微状态胶囊组件
 * @param parent 父容器
 * @param font 中文字体
 * @return 胶囊上下文指针
 */
ui_capsule_t* ui_capsule_create(lv_obj_t *parent, const lv_font_t *font);

/**
 * @brief 销毁胶囊组件
 * @param capsule 胶囊上下文
 */
void ui_capsule_destroy(ui_capsule_t *capsule);

/**
 * @brief 更新环境温湿度或核心温度 (左侧)
 * @param capsule 胶囊上下文
 * @param temp_c 温度摄氏度 (物理环境温度或 R528 核心温度)
 * @param humidity_pct 相对湿度百分比 (0~100)
 * @param is_valid 是否为板载物理环境温湿度 (若 false 则退化展示核心温度)
 */
void ui_capsule_update_env(ui_capsule_t *capsule, float temp_c, uint8_t humidity_pct, bool is_valid);

/**
 * @brief 更新番茄钟专注流微标与倒计时
 * @param capsule 胶囊上下文
 * @param is_active 是否运行中
 * @param remaining_s 剩余秒数
 */
void ui_capsule_update_pomodoro(ui_capsule_t *capsule, bool is_active, uint16_t remaining_s);

/**
 * @brief 更新当前卡带展示 (兼容保留)
 * @param capsule 胶囊上下文
 * @param name 卡带名称
 * @param icon 卡带图标
 */
void ui_capsule_update_cartridge(ui_capsule_t *capsule, const char *name, const char *icon);

/**
 * @brief 更新 Agent 全局状态 (居中)
 * @param capsule 胶囊上下文
 * @param text 状态文字
 * @param color 状态色彩
 * @param pulse 是否开启呼吸脉冲动效
 */
void ui_capsule_set_status(ui_capsule_t *capsule, const char *text, lv_color_t color, bool pulse);

/**
 * @brief 更新网络与电池电量 (右侧)
 * @param capsule 胶囊上下文
 * @param net_status 网络文字或图标 (如 "Wi-Fi" 或 "AP" 或 "Off")
 * @param battery_pct 电池百分比 (0~100)
 */
void ui_capsule_update_net_battery(ui_capsule_t *capsule, const char *net_status, uint8_t battery_pct);

/**
 * @brief 兼容旧版遥测更新接口
 * @param capsule 胶囊上下文
 * @param net_ip 当前 IP 或端口
 * @param battery_pct 电池百分比
 */
void ui_capsule_update_telemetry(ui_capsule_t *capsule, const char *net_ip, uint8_t battery_pct);

/**
 * @brief 唤醒并点亮胶囊
 * @param capsule 胶囊上下文
 */
void ui_capsule_wake(ui_capsule_t *capsule);

#ifdef __cplusplus
}
#endif

#endif /* UI_CAPSULE_H */
