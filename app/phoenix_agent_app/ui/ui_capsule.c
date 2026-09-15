/**
 * @file ui_capsule.c
 * @brief 顶部极窄微状态胶囊组件实现 (LVGL 8/9)
 * @author OpenVela Contest 2026 Team 145
 */

#include "ui_capsule.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPSULE_HEIGHT       22
#define COLOR_CAPSULE_BG     lv_color_hex(0x0a101d)
#define COLOR_CAPSULE_BORDER lv_color_hex(0x182438)
#define COLOR_TEXT_WHITE     lv_color_hex(0xffffff)
#define COLOR_TEXT_MUTED     lv_color_hex(0x7e92ad)
#define COLOR_ACCENT_CYAN    lv_color_hex(0x00e5ff)
#define COLOR_HEALTH_GREEN   lv_color_hex(0x00e676)

ui_capsule_t* ui_capsule_create(lv_obj_t *parent, const lv_font_t *font)
{
    if (!parent) return NULL;

    ui_capsule_t *capsule = (ui_capsule_t *)calloc(1, sizeof(ui_capsule_t));
    if (!capsule) return NULL;

    capsule->font = font;
    capsule->current_temp_c = 26.0f;
    capsule->current_humi_pct = 60;
    capsule->current_battery = 85;
    strncpy(capsule->current_net_str, "Wi-Fi", sizeof(capsule->current_net_str) - 1);

    /* 1. 极窄高透外壳容器 (高度 22px，圆角胶囊风格) */
    capsule->container = lv_obj_create(parent);
    lv_obj_set_size(capsule->container, lv_pct(97), CAPSULE_HEIGHT);
    lv_obj_align(capsule->container, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_set_style_bg_color(capsule->container, COLOR_CAPSULE_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(capsule->container, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_color(capsule->container, COLOR_CAPSULE_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(capsule->container, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(capsule->container, 11, LV_PART_MAIN);
    lv_obj_set_style_pad_all(capsule->container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(capsule->container, 6, LV_PART_MAIN);
    lv_obj_clear_flag(capsule->container, LV_OBJ_FLAG_SCROLLABLE);

    /* 2. 左侧：温湿度微标签 (如 "26℃ 60%") */
    capsule->lbl_env = lv_label_create(capsule->container);
    lv_obj_set_width(capsule->lbl_env, 80);
    lv_obj_set_style_text_align(capsule->lbl_env, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(capsule->lbl_env, LV_ALIGN_LEFT_MID, 4, 0);
    if (font) lv_obj_set_style_text_font(capsule->lbl_env, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(capsule->lbl_env, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_label_set_text(capsule->lbl_env, "26C 60%");
    capsule->lbl_cartridge = capsule->lbl_env; /* 兼容别名 */

    /* 3. 居中：全局 Agent 心智微胶囊 (低调科技感底色，不抢主屏视觉) */
    capsule->pill_status = lv_obj_create(capsule->container);
    lv_obj_set_size(capsule->pill_status, LV_SIZE_CONTENT, CAPSULE_HEIGHT - 6);
    lv_obj_align(capsule->pill_status, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(capsule->pill_status, lv_color_hex(0x0C1424), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(capsule->pill_status, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(capsule->pill_status, lv_color_hex(0x223650), LV_PART_MAIN);
    lv_obj_set_style_border_width(capsule->pill_status, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(capsule->pill_status, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(capsule->pill_status, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(capsule->pill_status, 0, LV_PART_MAIN);
    lv_obj_clear_flag(capsule->pill_status, LV_OBJ_FLAG_SCROLLABLE);

    capsule->lbl_status = lv_label_create(capsule->pill_status);
    lv_obj_center(capsule->lbl_status);
    if (font) lv_obj_set_style_text_font(capsule->lbl_status, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(capsule->lbl_status, COLOR_ACCENT_CYAN, LV_PART_MAIN);
    lv_label_set_text(capsule->lbl_status, "● 待命");

    /* 4. 右侧：网络状态与电池电量 (如 "Wi-Fi 85%") */
    capsule->lbl_telemetry = lv_label_create(capsule->container);
    lv_obj_set_width(capsule->lbl_telemetry, 80);
    lv_obj_set_style_text_align(capsule->lbl_telemetry, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(capsule->lbl_telemetry, LV_ALIGN_RIGHT_MID, -4, 0);
    if (font) lv_obj_set_style_text_font(capsule->lbl_telemetry, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(capsule->lbl_telemetry, COLOR_HEALTH_GREEN, LV_PART_MAIN);
    lv_label_set_text(capsule->lbl_telemetry, "Wi-Fi 85%");

    capsule->dim_timer = NULL;
    capsule->is_dimmed = false;

    return capsule;
}

void ui_capsule_destroy(ui_capsule_t *capsule)
{
    if (!capsule) return;
    if (capsule->container) {
        lv_obj_del(capsule->container);
        capsule->container = NULL;
    }
    free(capsule);
}

void ui_capsule_wake(ui_capsule_t *capsule)
{
    if (!capsule || !capsule->container) return;
    if (capsule->is_dimmed) {
        lv_obj_set_style_opa(capsule->container, LV_OPA_80, LV_PART_MAIN);
        capsule->is_dimmed = false;
    }
}

void ui_capsule_update_env(ui_capsule_t *capsule, float temp_c, uint8_t humidity_pct)
{
    if (!capsule || !capsule->lbl_env) return;
    ui_capsule_wake(capsule);

    capsule->current_temp_c = temp_c;
    capsule->current_humi_pct = humidity_pct;

    char buf[24];
    snprintf(buf, sizeof(buf), "%dC %d%%", (int)(temp_c + 0.5f), (int)humidity_pct);
    lv_label_set_text(capsule->lbl_env, buf);
}

void ui_capsule_update_cartridge(ui_capsule_t *capsule, const char *name, const char *icon)
{
    (void)icon;
    (void)name;
    /* 保持接口，左侧由温湿度接管，不再被卡带名覆盖 */
}

void ui_capsule_set_status(ui_capsule_t *capsule, const char *text, lv_color_t color, bool pulse)
{
    (void)pulse;
    if (!capsule || !capsule->lbl_status) return;
    ui_capsule_wake(capsule);

    lv_label_set_text(capsule->lbl_status, text ? text : "● 待命");
    lv_obj_set_style_text_color(capsule->lbl_status, color, LV_PART_MAIN);
}

void ui_capsule_update_net_battery(ui_capsule_t *capsule, const char *net_status, uint8_t battery_pct)
{
    if (!capsule || !capsule->lbl_telemetry) return;
    ui_capsule_wake(capsule);

    if (battery_pct > 100) battery_pct = 100;
    capsule->current_battery = battery_pct;
    if (net_status && net_status[0]) {
        strncpy(capsule->current_net_str, net_status, sizeof(capsule->current_net_str) - 1);
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "%s %d%%", capsule->current_net_str, (int)capsule->current_battery);
    lv_label_set_text(capsule->lbl_telemetry, buf);
}

void ui_capsule_update_telemetry(ui_capsule_t *capsule, const char *net_ip, uint8_t battery_pct)
{
    (void)net_ip;
    ui_capsule_update_net_battery(capsule, NULL, battery_pct);
}
