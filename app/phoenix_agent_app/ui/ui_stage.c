/**
 * @file ui_stage.c
 * @brief 场景卡带主舞台与视窗过渡器实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "ui_stage.h"
#include "core/cartridge_mgr.h"
#include <stdio.h>
#include <stdlib.h>

#define STAGE_ANIM_DURATION_MS 220

static void stage_anim_cb(void *var, int32_t val)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    if (obj) {
        lv_obj_set_x(obj, val);
    }
}

static void stage_anim_ready_cb(lv_anim_t *a)
{
    ui_stage_t *stage = (ui_stage_t *)a->user_data;
    if (!stage) return;

    /* 清理滑出的旧卡带视图 */
    if (stage->old_view) {
        lv_obj_del(stage->old_view);
        stage->old_view = NULL;
    }
    stage->is_animating = false;
}

static void stage_touch_event_cb(lv_event_t *e)
{
    ui_stage_t *stage = (ui_stage_t *)lv_event_get_user_data(e);
    if (!stage) return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;

    uint32_t now = lv_tick_get();

    /* 0. 原生手势事件优先处理 (带 400ms 节流防抖) */
    if (code == LV_EVENT_GESTURE) {
        if (now - stage->last_gesture_time_ms < 400) return;
        lv_dir_t dir = lv_indev_get_gesture_dir(indev);
        if (dir == LV_DIR_LEFT) {
            stage->last_gesture_time_ms = now;
            cartridge_mgr_next();
            return;
        } else if (dir == LV_DIR_RIGHT) {
            stage->last_gesture_time_ms = now;
            cartridge_mgr_prev();
            return;
        } else if (dir == LV_DIR_BOTTOM) {
            /* 仅当触控起始位置在屏幕顶部边缘 (Y < 55) 时呼出抽屉 */
            if (stage->press_point.y < 55 && stage->on_swipe_down) {
                stage->last_gesture_time_ms = now;
                stage->on_swipe_down(stage->user_data);
            }
            return;
        }
    }

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &stage->press_point);
        stage->press_time_ms = now;
        stage->is_pressed = true;
    } else if (code == LV_EVENT_RELEASED) {
        if (!stage->is_pressed) return;
        stage->is_pressed = false;

        lv_point_t release_point;
        lv_indev_get_point(indev, &release_point);

        int32_t dx = release_point.x - stage->press_point.x;
        int32_t dy = release_point.y - stage->press_point.y;
        int32_t abs_dx = (dx < 0) ? -dx : dx;
        int32_t abs_dy = (dy < 0) ? -dy : dy;

        /* 防抖检查：忽略 300ms 内的连续误触发 */
        if (now - stage->last_gesture_time_ms < 300) return;

        /* 1. 水平滑动切换卡带：阈值优化为 35px，水平位移大于垂直位移即可切卡 */
        if (abs_dx >= 35 && abs_dx > abs_dy) {
            stage->last_gesture_time_ms = now;
            if (dx < 0) {
                cartridge_mgr_next();
            } else {
                cartridge_mgr_prev();
            }
            return;
        }

        /* 2. 下滑呼出设置抽屉：起始于屏幕顶部 1/3 (Y < 80) 且下滑位移 >= 30px */
        if (stage->press_point.y < 80 && dy >= 30 && abs_dy > abs_dx) {
            stage->last_gesture_time_ms = now;
            if (stage->on_swipe_down) {
                stage->on_swipe_down(stage->user_data);
            }
            return;
        }

        /* 3. 屏幕空白处轻敲点击判定 (位移 < 15px 且时长 < 500ms) */
        if (abs_dx < 15 && abs_dy < 15) {
            uint32_t elapsed = lv_tick_elaps(stage->press_time_ms);
            if (elapsed > 20 && elapsed < 500) {
                /* 分发敲击事件至当前活跃卡带 */
                cartridge_mgr_dispatch_knock(1, 1);
            }
        }
    }
}

void ui_stage_bind_touch(ui_stage_t *stage, lv_obj_t *target)
{
    if (!stage || !target) return;
    lv_obj_add_flag(target, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(target, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(target, stage_touch_event_cb, LV_EVENT_PRESSED, stage);
    lv_obj_add_event_cb(target, stage_touch_event_cb, LV_EVENT_RELEASED, stage);
    lv_obj_add_event_cb(target, stage_touch_event_cb, LV_EVENT_GESTURE, stage);
}

ui_stage_t* ui_stage_create(lv_obj_t *parent)
{
    if (!parent) return NULL;

    ui_stage_t *stage = (ui_stage_t *)calloc(1, sizeof(ui_stage_t));
    if (!stage) return NULL;

    stage->container = lv_obj_create(parent);
    lv_obj_set_size(stage->container, lv_pct(100), lv_pct(100));
    lv_obj_center(stage->container);

    /* 样式：纯净无边框黑底 */
    lv_obj_set_style_bg_color(stage->container, lv_color_hex(0x04060a), LV_PART_MAIN);
    lv_obj_set_style_border_width(stage->container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(stage->container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(stage->container, 0, LV_PART_MAIN);
    lv_obj_clear_flag(stage->container, LV_OBJ_FLAG_SCROLLABLE);

    /* 仅在舞台容器单点监听触摸手势，不再对子视图重复注册，杜绝冒泡三次重入 */
    ui_stage_bind_touch(stage, stage->container);

    return stage;
}

void ui_stage_destroy(ui_stage_t *stage)
{
    if (!stage) return;
    if (stage->old_view) {
        lv_obj_del(stage->old_view);
        stage->old_view = NULL;
    }
    if (stage->current_view) {
        lv_obj_del(stage->current_view);
        stage->current_view = NULL;
    }
    if (stage->container) {
        lv_obj_del(stage->container);
        stage->container = NULL;
    }
    free(stage);
}

lv_obj_t* ui_stage_get_canvas(ui_stage_t *stage)
{
    return stage ? stage->container : NULL;
}

void ui_stage_set_swipe_down_cb(ui_stage_t *stage, ui_stage_swipe_down_cb_t cb, void *user_data)
{
    if (!stage) return;
    stage->on_swipe_down = cb;
    stage->user_data = user_data;
}

void ui_stage_transition_to(ui_stage_t *stage, lv_obj_t *new_view, bool slide_to_left)
{
    if (!stage || !new_view) return;

    /* 确保新进入视窗的卡带具备点击与事件向上冒泡，不重复注册回调 */
    lv_obj_add_flag(new_view, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(new_view, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 架构师优化：强制停止正在运行的过渡动画，避免重入与野指针 */
    if (stage->old_view) {
        lv_anim_del(stage->old_view, NULL);
        lv_obj_del(stage->old_view);
        stage->old_view = NULL;
    }
    if (stage->current_view) {
        lv_anim_del(stage->current_view, NULL);
    }

    stage->old_view = stage->current_view;
    stage->current_view = new_view;
    stage->is_animating = true;

    lv_coord_t w = lv_obj_get_width(stage->container);
    if (w <= 0) w = 320; /* 默认保底宽度 */

    lv_coord_t enter_start_x = slide_to_left ? w : -w;
    lv_coord_t exit_end_x    = slide_to_left ? -w : w;

    /* 1. 新视图从侧边滑入至 0 */
    lv_obj_set_x(new_view, enter_start_x);
    lv_anim_t a_enter;
    lv_anim_init(&a_enter);
    lv_anim_set_var(&a_enter, new_view);
    lv_anim_set_values(&a_enter, enter_start_x, 0);
    lv_anim_set_time(&a_enter, STAGE_ANIM_DURATION_MS);
    lv_anim_set_exec_cb(&a_enter, stage_anim_cb);
    lv_anim_set_path_cb(&a_enter, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&a_enter, stage_anim_ready_cb);
    lv_anim_set_user_data(&a_enter, stage);
    lv_anim_start(&a_enter);

    /* 2. 旧视图同步滑出视窗 */
    if (stage->old_view) {
        lv_anim_t a_exit;
        lv_anim_init(&a_exit);
        lv_anim_set_var(&a_exit, stage->old_view);
        lv_anim_set_values(&a_exit, 0, exit_end_x);
        lv_anim_set_time(&a_exit, STAGE_ANIM_DURATION_MS);
        lv_anim_set_exec_cb(&a_exit, stage_anim_cb);
        lv_anim_set_path_cb(&a_exit, lv_anim_path_ease_out);
        lv_anim_start(&a_exit);
    }
}
