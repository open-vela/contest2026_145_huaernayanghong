/**
 * @file ui_font.h
 * @brief 统一中文字体访问适配头文件
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_UI_FONT_H
#define PHOENIX_UI_FONT_H

#include <lvgl/lvgl.h>

#ifdef HOST_TEST_RUNNER
static inline const lv_font_t* phoenix_ui_get_font(void) {
    return (const lv_font_t*)1;
}
static inline const lv_font_t* phoenix_ui_get_font_large(void) {
    return (const lv_font_t*)1;
}
#else
LV_FONT_DECLARE(lv_font_chinese_16);
LV_FONT_DECLARE(lv_font_montserrat_30);

static inline const lv_font_t* phoenix_ui_get_font(void) {
    return &lv_font_chinese_16;
}

static inline const lv_font_t* phoenix_ui_get_font_large(void) {
    return &lv_font_montserrat_30;
}
#endif

#endif /* PHOENIX_UI_FONT_H */
