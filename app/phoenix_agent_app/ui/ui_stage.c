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

static void stage_touch_event_cb(lv_event_t *e)
{
    ui_stage_t *stage = (ui_stage_t *)lv_event_get_user_data(e);
    if (!stage) return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;

    uint32_t now = lv_tick_get();

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

        /* 轻敲判定 (位移 < 15px) 分发敲击事件至当前活跃卡带 (敲木鱼/互动) */
        if (abs_dx < 15 && abs_dy < 15) {
            uint32_t elapsed = lv_tick_elaps(stage->press_time_ms);
            if (elapsed > 20 && elapsed < 500) {
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
    lv_obj_set_size(stage->container, 274, 216);
    lv_obj_set_pos(stage->container, 46, 24);

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

void ui_stage_play_enter_anim(ui_stage_t *stage, bool slide_to_left)
{
    if (!stage || !stage->container) return;

    uint32_t cnt = lv_obj_get_child_count(stage->container);
    if (cnt == 0) return;

    lv_obj_t *top_view = lv_obj_get_child(stage->container, (int32_t)cnt - 1);
    if (!top_view) return;

    lv_coord_t w = lv_obj_get_width(stage->container);
    if (w <= 0) w = 274;

    lv_coord_t enter_start_x = slide_to_left ? w : -w;

    lv_anim_del(top_view, NULL);
    lv_obj_set_x(top_view, enter_start_x);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, top_view);
    lv_anim_set_values(&a, enter_start_x, 0);
    lv_anim_set_time(&a, STAGE_ANIM_DURATION_MS);
    lv_anim_set_exec_cb(&a, stage_anim_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}
