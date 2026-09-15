/**
 * @file demo_ui.c
 * @brief 基于 OpenVela LVGL 的硬件测试图形卡片仪表盘 (全自适应 & 驱动绑定)
 * @author OpenVela Contest 2026 Team 145
 */

#include "demo_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <nuttx/config.h>

#ifdef CONFIG_GRAPHICS_LVGL
#include <lvgl/lvgl.h>
#include "../hal/vela_hal.h"

/* UI 全局控件与状态 */
static volatile bool g_ui_running = false;
static lv_obj_t *g_screen = NULL;
static lv_timer_t *g_refresh_timer = NULL;

/* 电源卡片 */
static lv_obj_t *g_lbl_power_val = NULL;
static lv_obj_t *g_bar_battery = NULL;
static lv_obj_t *g_lbl_battery_soc = NULL;

/* 麦克风卡片 */
static lv_obj_t *g_sw_mic = NULL;
static lv_obj_t *g_lbl_mic_status = NULL;
static lv_obj_t *g_bar_mic_vu = NULL;

/* 屏幕亮度卡片 */
static lv_obj_t *g_slider_brightness = NULL;
static lv_obj_t *g_lbl_brightness_val = NULL;

/* 扬声器音量卡片 */
static lv_obj_t *g_slider_volume = NULL;
static lv_obj_t *g_lbl_volume_val = NULL;

/* 传感器卡片 */
static lv_obj_t *g_lbl_sensor_val = NULL;

static void sigint_handler(int signo)
{
    (void)signo;
    g_ui_running = false;
}

static void on_exit_clicked(lv_event_t *e)
{
    (void)e;
    g_ui_running = false;
}

static void on_brightness_changed(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    vela_hal_display_set_brightness((uint8_t)val);

    if (g_lbl_brightness_val) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", val);
        lv_label_set_text(g_lbl_brightness_val, buf);
    }
}

static void on_volume_changed(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    vela_hal_speaker_set_volume((uint8_t)val);

    if (g_lbl_volume_val) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", val);
        lv_label_set_text(g_lbl_volume_val, buf);
    }
}

static void on_mic_switch_toggled(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    /* 开启开关 => 打开麦克风 (mute=false)；关闭开关 => 关闭麦克风 (mute=true) */
    vela_hal_mic_set_mute(!is_on);

    if (g_lbl_mic_status) {
        lv_label_set_text(g_lbl_mic_status, is_on ? "MIC: ACTIVE (ON)" : "MIC: MUTED (OFF)");
        lv_obj_set_style_text_color(g_lbl_mic_status,
            is_on ? lv_color_hex(0x22C55E) : lv_color_hex(0xEF4444), 0);
    }
}

static void on_beep_clicked(lv_event_t *e)
{
    (void)e;
    vela_hal_speaker_play_tone(880, 200);
}

static void ui_refresh_timer_cb(lv_timer_t *t)
{
    (void)t;

    /* 1. 刷新电源与电量 */
    vela_power_data_t pdata;
    if (vela_hal_power_get_data(&pdata) == 0) {
        if (g_lbl_power_val) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.2f V | %+.0f mA | %.1f W",
                     pdata.bus_voltage_v, pdata.current_ma, pdata.power_mw / 1000.0f);
            lv_label_set_text(g_lbl_power_val, buf);
        }
        if (g_bar_battery) {
            lv_bar_set_value(g_bar_battery, pdata.battery_soc, LV_ANIM_OFF);
        }
        if (g_lbl_battery_soc) {
            char buf[64];
            snprintf(buf, sizeof(buf), "SOC: %d%% (%s)", pdata.battery_soc, pdata.status_str);
            lv_label_set_text(g_lbl_battery_soc, buf);
        }
    }

    /* 2. 刷新麦克风 VU 电平 */
    int16_t vu = 0;
    vela_hal_mic_read_vu(&vu);
    if (g_bar_mic_vu) {
        int pct = vu / 40;
        if (pct > 100) pct = 100;
        lv_bar_set_value(g_bar_mic_vu, pct, LV_ANIM_OFF);
    }

    /* 3. 刷新传感器 */
    vela_imu_data_t imu;
    vela_rtc_time_t rtc;
    vela_hal_sensors_read_imu(&imu);
    vela_hal_sensors_read_rtc(&rtc);
    if (g_lbl_sensor_val) {
        char buf[128];
        snprintf(buf, sizeof(buf), "IMU: Acc(%.1f, %.1f, %.1f)g\nRTC: %02d:%02d:%02d",
                 imu.accel_x, imu.accel_y, imu.accel_z,
                 rtc.hour, rtc.minute, rtc.second);
        lv_label_set_text(g_lbl_sensor_val, buf);
    }
}

static lv_obj_t* create_card(lv_obj_t *parent, const char *title, int32_t height)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(96));
    lv_obj_set_height(card, height);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_title = lv_label_create(card);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 0, 0);

    return card;
}

static void create_dashboard_ui(void)
{
    g_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_screen, lv_color_hex(0x0F172A), 0);

    /* 顶部导航标题栏 */
    lv_obj_t *hdr = lv_obj_create(g_screen);
    lv_obj_set_size(hdr, lv_pct(100), 36);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x0284C7), 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_hor(hdr, 8, 0);
    lv_obj_set_style_pad_ver(hdr, 4, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_title = lv_label_create(hdr);
    lv_label_set_text(lbl_title, "Vela Hardware Console");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 0, 0);

    /* 退出按钮 */
    lv_obj_t *btn_exit = lv_button_create(hdr);
    lv_obj_set_size(btn_exit, 46, 24);
    lv_obj_align(btn_exit, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_exit, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(btn_exit, 4, 0);
    lv_obj_add_event_cb(btn_exit, on_exit_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_exit = lv_label_create(btn_exit);
    lv_label_set_text(lbl_exit, "Exit");
    lv_obj_set_style_text_color(lbl_exit, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_exit);

    /* 滚动主容器 (纵向流式卡片列表) */
    lv_obj_t *cont = lv_obj_create(g_screen);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(85));
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 6, 0);
    lv_obj_set_style_pad_row(cont, 8, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);

    /* 卡片 1: 电源电量监控 */
    lv_obj_t *card_pwr = create_card(cont, "POWER & BATTERY", 90);
    g_lbl_power_val = lv_label_create(card_pwr);
    lv_label_set_text(g_lbl_power_val, "-- V | -- mA");
    lv_obj_set_style_text_color(g_lbl_power_val, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(g_lbl_power_val, LV_ALIGN_TOP_LEFT, 0, 18);

    g_bar_battery = lv_bar_create(card_pwr);
    lv_obj_set_size(g_bar_battery, lv_pct(95), 10);
    lv_obj_align(g_bar_battery, LV_ALIGN_CENTER, 0, 8);
    lv_bar_set_range(g_bar_battery, 0, 100);
    lv_bar_set_value(g_bar_battery, 75, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_bar_battery, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_bar_battery, lv_color_hex(0x10B981), LV_PART_INDICATOR);

    g_lbl_battery_soc = lv_label_create(card_pwr);
    lv_label_set_text(g_lbl_battery_soc, "SOC: --%");
    lv_obj_set_style_text_color(g_lbl_battery_soc, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(g_lbl_battery_soc, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* 卡片 2: 麦克风状态与静音开关 */
    lv_obj_t *card_mic = create_card(cont, "MICROPHONE (CLOSE / MUTE)", 90);
    g_sw_mic = lv_switch_create(card_mic);
    lv_obj_align(g_sw_mic, LV_ALIGN_TOP_RIGHT, 0, 10);
    lv_obj_add_state(g_sw_mic, LV_STATE_CHECKED);
    lv_obj_add_event_cb(g_sw_mic, on_mic_switch_toggled, LV_EVENT_VALUE_CHANGED, NULL);

    g_lbl_mic_status = lv_label_create(card_mic);
    lv_label_set_text(g_lbl_mic_status, "MIC: ACTIVE (ON)");
    lv_obj_set_style_text_color(g_lbl_mic_status, lv_color_hex(0x22C55E), 0);
    lv_obj_align(g_lbl_mic_status, LV_ALIGN_TOP_LEFT, 0, 18);

    g_bar_mic_vu = lv_bar_create(card_mic);
    lv_obj_set_size(g_bar_mic_vu, lv_pct(95), 8);
    lv_obj_align(g_bar_mic_vu, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_range(g_bar_mic_vu, 0, 100);
    lv_obj_set_style_bg_color(g_bar_mic_vu, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_bar_mic_vu, lv_color_hex(0x06B6D4), LV_PART_INDICATOR);

    /* 卡片 3: 屏幕背光调节 */
    lv_obj_t *card_disp = create_card(cont, "DISPLAY BRIGHTNESS", 80);
    g_lbl_brightness_val = lv_label_create(card_disp);
    lv_label_set_text(g_lbl_brightness_val, "80%");
    lv_obj_set_style_text_color(g_lbl_brightness_val, lv_color_hex(0xFACC15), 0);
    lv_obj_align(g_lbl_brightness_val, LV_ALIGN_TOP_LEFT, 0, 18);

    g_slider_brightness = lv_slider_create(card_disp);
    lv_obj_set_size(g_slider_brightness, lv_pct(95), 12);
    lv_obj_align(g_slider_brightness, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_slider_set_range(g_slider_brightness, 0, 100);
    lv_slider_set_value(g_slider_brightness, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(g_slider_brightness, on_brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);

    /* 卡片 4: 扬声器音量调节与发声测试 */
    lv_obj_t *card_spk = create_card(cont, "SPEAKER & AUDIO", 85);
    g_lbl_volume_val = lv_label_create(card_spk);
    lv_label_set_text(g_lbl_volume_val, "70%");
    lv_obj_set_style_text_color(g_lbl_volume_val, lv_color_hex(0xA855F7), 0);
    lv_obj_align(g_lbl_volume_val, LV_ALIGN_TOP_LEFT, 0, 18);

    lv_obj_t *btn_beep = lv_button_create(card_spk);
    lv_obj_set_size(btn_beep, 55, 24);
    lv_obj_align(btn_beep, LV_ALIGN_TOP_RIGHT, 0, 12);
    lv_obj_set_style_bg_color(btn_beep, lv_color_hex(0x6366F1), 0);
    lv_obj_add_event_cb(btn_beep, on_beep_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_beep = lv_label_create(btn_beep);
    lv_label_set_text(lbl_beep, "Beep");
    lv_obj_center(lbl_beep);

    g_slider_volume = lv_slider_create(card_spk);
    lv_obj_set_size(g_slider_volume, lv_pct(95), 12);
    lv_obj_align(g_slider_volume, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_slider_set_range(g_slider_volume, 0, 100);
    lv_slider_set_value(g_slider_volume, 70, LV_ANIM_OFF);
    lv_obj_add_event_cb(g_slider_volume, on_volume_changed, LV_EVENT_VALUE_CHANGED, NULL);

    /* 卡片 5: 传感器与系统时钟 */
    lv_obj_t *card_sens = create_card(cont, "SENSORS & REAL-TIME CLOCK", 75);
    g_lbl_sensor_val = lv_label_create(card_sens);
    lv_label_set_text(g_lbl_sensor_val, "Reading IMU and RTC...");
    lv_obj_set_style_text_color(g_lbl_sensor_val, lv_color_hex(0xE2E8F0), 0);
    lv_obj_align(g_lbl_sensor_val, LV_ALIGN_LEFT_MID, 0, 8);

    lv_screen_load(g_screen);
}

int demo_ui_launch(void)
{
    printf("[OpenVela:GUI] Initializing OpenVela Hardware Test Dashboard...\n");

    lv_nuttx_dsc_t info;
    lv_nuttx_result_t result;
    bool need_deinit = false;

    if (!lv_is_initialized()) {
        lv_init();
        lv_nuttx_dsc_init(&info);
#ifdef CONFIG_LV_USE_NUTTX_LCD
        info.fb_path = "/dev/lcd0";
#endif
#ifdef CONFIG_INPUT_TOUCHSCREEN
        info.input_path = "/dev/input0";
#endif
        lv_nuttx_init(&info, &result);
        need_deinit = true;

        if (result.disp == NULL) {
            printf("[-] [OpenVela:GUI] Error: Failed to initialize display device (/dev/fb0).\n");
            return -ENODEV;
        }
    }

    create_dashboard_ui();

    /* 启动定时刷新 */
    g_refresh_timer = lv_timer_create(ui_refresh_timer_cb, 500, NULL);

    /* 监听终止信号 */
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    g_ui_running = true;
    printf("[OpenVela:GUI] Dashboard active! Tap 'Exit' or press Ctrl+C to return to NSH.\n");

    while (g_ui_running) {
        uint32_t idle = lv_timer_handler();
        idle = idle ? idle : 5;
        if (idle > 50) idle = 50;
        usleep(idle * 1000);
    }

    printf("[OpenVela:GUI] Shutting down dashboard...\n");

    if (g_refresh_timer) {
        lv_timer_delete(g_refresh_timer);
        g_refresh_timer = NULL;
    }

    if (g_screen) {
        lv_obj_delete(g_screen);
        g_screen = NULL;
    }

    if (need_deinit) {
        lv_nuttx_deinit(&result);
        lv_deinit();
    }

    return 0;
}

#else

int demo_ui_launch(void)
{
    printf("[OpenVela:GUI] Warning: CONFIG_GRAPHICS_LVGL is not enabled in this build.\n");
    printf("[OpenVela:GUI] Please run hardware tests via CLI mode (e.g. 'demo all' or 'demo power').\n");
    return -ENOSYS;
}

#endif
