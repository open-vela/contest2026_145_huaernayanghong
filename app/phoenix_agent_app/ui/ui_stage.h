/**
 * @file ui_stage.h
 * @brief 场景卡带主舞台与视窗过渡器 (Cartridge Stage Viewport & Transition)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef UI_STAGE_H
#define UI_STAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl/lvgl.h>
#include <stdbool.h>
#include "core/cartridge.h"

typedef struct ui_stage_s ui_stage_t;

typedef void (*ui_stage_swipe_down_cb_t)(void *user_data);

struct ui_stage_s {
    lv_obj_t *container;              /**< 舞台总容器 */
    lv_obj_t *current_view;           /**< 当前活跃卡带的根控件 */
    lv_obj_t *old_view;               /**< 正在滑出的旧卡带根控件 */

    ui_stage_swipe_down_cb_t on_swipe_down; /**< 顶部下滑回调(呼出设置抽屉) */
    void *user_data;

    lv_point_t press_point;
    uint32_t   press_time_ms;
    bool       is_pressed;
    bool       is_animating;
};

/**
 * @brief 创建卡带舞台视窗
 * @param parent 父屏幕容器
 * @return 舞台上下文指针
 */
ui_stage_t* ui_stage_create(lv_obj_t *parent);

/**
 * @brief 销毁舞台视窗
 * @param stage 舞台上下文
 */
void ui_stage_destroy(ui_stage_t *stage);

/**
 * @brief 获取卡带可以绘制的主画布容器
 * @param stage 舞台上下文
 * @return lv_obj_t 容器指针
 */
lv_obj_t* ui_stage_get_canvas(ui_stage_t *stage);

/**
 * @brief 绑定卡带或组件至舞台交互分发器 (自动注入冒泡与触摸手势)
 * @param stage 舞台上下文
 * @param target 目标容器对象
 */
void ui_stage_bind_touch(ui_stage_t *stage, lv_obj_t *target);

/**
 * @brief 设置下滑手势回调
 * @param stage 舞台上下文
 * @param cb 下滑回调
 * @param user_data 回调参数
 */
void ui_stage_set_swipe_down_cb(ui_stage_t *stage, ui_stage_swipe_down_cb_t cb, void *user_data);

/**
 * @brief 挂载新卡带视图并执行横向平滑过渡动效
 * @param stage 舞台上下文
 * @param new_view 新卡带 UI 根控件
 * @param slide_to_left true=向左滑动切换(下一个), false=向右滑动切换(上一个)
 */
void ui_stage_transition_to(ui_stage_t *stage, lv_obj_t *new_view, bool slide_to_left);

#ifdef __cplusplus
}
#endif

#endif /* UI_STAGE_H */
