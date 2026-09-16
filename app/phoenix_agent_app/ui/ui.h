/**
 * @file ui.h
 * @brief Phoenix HoloDesk-S1 Minimalist Cyber-Toy UI (LVGL 9)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_UI_H
#define PHOENIX_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl/lvgl.h>
#include "eye_anim.h"
#include "ui_capsule.h"
#include "ui_stage.h"
#include "ui_settings.h"
#include "ui_sidebar.h"
#include "core/agent_core.h"
#include "core/event_bus.h"

/**
 * @brief Phoenix 极简小屏潮玩 UI 结构体
 */
typedef struct {
    lv_obj_t *screen;

    /* 1. 顶部微状态胶囊 (Top Status Capsule, 24px) */
    ui_capsule_t *capsule;

    /* 2. 核心舞台视窗：承载各卡带渲染 (宽 274px, X=46) */
    ui_stage_t   *stage;

    /* 3. 左侧常驻导航栏：点击直达切卡 (宽 46px, X=0) */
    ui_sidebar_t *sidebar;

    /* 4. 顶部控制中心抽屉与二级设置面板 (Settings Drawer) */
    ui_settings_t *settings;

    /* 兼容保留字段用于灵眸卡带或飞字气泡 */
    lv_obj_t *top_capsule;
    lv_obj_t *lbl_conn_battery;
    lv_obj_t *state_pill;
    lv_obj_t *state_pill_label;
    lv_obj_t *lbl_merit_badge;
    lv_obj_t *eye_stage;
    phoenix_eye_t *eye;

    /* 3. 底部动态交互气泡 (Dynamic Speech Bubble) */
    lv_obj_t *bubble_card;
    lv_obj_t *bubble_label;
    lv_timer_t *bubble_hide_timer; /**< 自动隐藏定时器 */

    /* 核心上下文与状态指标 */
    phoenix_agent_ctx_t *agent_core;
    lv_timer_t *heartbeat_timer;
    lv_timer_t *flush_timer;
    uint32_t uptime_sec;
    uint32_t merit_count;
    uint8_t  battery_pct;
    bool     pomodoro_active;
    uint16_t pomodoro_remain_s;

    const lv_font_t *font_chinese;
} phoenix_ui_t;

/**
 * @brief 创建 Phoenix 极简小屏潮玩 UI
 * @param parent 父容器对象 (通常为 lv_screen_active())
 * @param core 智能体中枢指针
 * @return UI 上下文指针
 */
phoenix_ui_t* phoenix_ui_create(lv_obj_t *parent, phoenix_agent_ctx_t *core);

/**
 * @brief 弹出轻量上扬飞字动画
 * @param ui UI 上下文
 * @param text 飞字内容
 * @param color 文本色彩
 */
void phoenix_ui_show_flying_text(phoenix_ui_t *ui, const char *text, lv_color_t color);

/**
 * @brief 弹出底部动态交互气泡
 * @param ui UI 上下文
 * @param text 对话文本
 * @param auto_hide_ms 自动隐藏毫秒数 (0 表示不自动隐藏)
 */
void phoenix_ui_show_bubble(phoenix_ui_t *ui, const char *text, uint32_t auto_hide_ms);

/**
 * @brief 隐藏底部动态交互气泡
 * @param ui UI 上下文
 */
void phoenix_ui_hide_bubble(phoenix_ui_t *ui);

/**
 * @brief 销毁 UI 资源
 * @param ui UI 上下文
 */
void phoenix_ui_destroy(phoenix_ui_t *ui);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_UI_H */
