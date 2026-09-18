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

struct ui_stage_s {
    lv_obj_t *container;              /**< 舞台总容器 */
    lv_obj_t *current_view;           /**< 当前活跃卡带的根控件 */

    lv_point_t press_point;
    uint32_t   press_time_ms;
    bool       is_pressed;
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
 * @brief 驱动当前舞台顶部卡带视图执行平滑推拉进场过渡动效
 * @param stage 舞台上下文
 * @param slide_to_left true=从右往左滑入, false=从左往右滑入
 */
void ui_stage_play_enter_anim(ui_stage_t *stage, bool slide_to_left);

#ifdef __cplusplus
}
#endif

#endif /* UI_STAGE_H */
