/**
 * @file ui.c
 * @brief Phoenix HoloDesk-S1 Minimalist Cyber-Toy UI Implementation (LVGL 9)
 * @author OpenVela Contest 2026 Team 145
 */

#include "ui.h"
#include "audio_ctl.h"
#include "core/tool_registry.h"
#include "core/store.h"
#include "core/cartridge_mgr.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

LV_FONT_DECLARE(lv_font_chinese_16);

static phoenix_ui_t g_phoenix_ui;

/* Cyberpunk Minimalist Color Palette */
#define COLOR_SCREEN_BG        lv_color_hex(0x06080e)
#define COLOR_CAPSULE_BG       lv_color_hex(0x0e1420)
#define COLOR_CAPSULE_BORDER   lv_color_hex(0x1a2638)
#define COLOR_BUBBLE_BG        lv_color_hex(0x0a101d)
#define COLOR_BUBBLE_BORDER    lv_color_hex(0x1b2840)
#define COLOR_TEXT_WHITE       lv_color_hex(0xffffff)
#define COLOR_TEXT_MUTED       lv_color_hex(0x7e92ad)
#define COLOR_PRIMARY_GOLD     lv_color_hex(0xffb300)
#define COLOR_ACCENT_CYAN      lv_color_hex(0x00e5ff)
#define COLOR_HEALTH_GREEN     lv_color_hex(0x00e676)
#define COLOR_ALERT_RED        lv_color_hex(0xff1744)
#define COLOR_POMO_ORANGE      lv_color_hex(0xff5722)
#define COLOR_THINK_PURPLE     lv_color_hex(0xaa00ff)

static const lv_font_t* init_chinese_font(void)
{
    return &lv_font_chinese_16;
}

static void bubble_hide_timer_cb(lv_timer_t *timer)
{
    phoenix_ui_t *ui = (phoenix_ui_t *)lv_timer_get_user_data(timer);
    if (!ui || !ui->bubble_card || !ui->bubble_label) return;

    /* 隐藏动态气泡 */
    lv_obj_add_flag(ui->bubble_card, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(timer);
}

void phoenix_ui_show_bubble(phoenix_ui_t *ui, const char *text, uint32_t auto_hide_ms)
{
    if (!ui || !ui->bubble_card || !ui->bubble_label || !text) return;

    /* 当处于主动式 Agent 卡带界面，或者控制中心抽屉展开时，不叠加底部气泡，保持界面纯净 */
    cartridge_t *cur = cartridge_mgr_get_current();
    if (cur && strcmp(cur->ops.id, "agent") == 0) return;
    if (ui->settings && ui->settings->is_open) return;

    lv_label_set_text(ui->bubble_label, text);
    lv_obj_set_style_text_color(ui->bubble_label, COLOR_TEXT_WHITE, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui->bubble_card, COLOR_ACCENT_CYAN, LV_PART_MAIN);
    lv_obj_clear_flag(ui->bubble_card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui->bubble_card);

    if (ui->bubble_hide_timer) {
        if (auto_hide_ms > 0) {
            lv_timer_set_period(ui->bubble_hide_timer, auto_hide_ms);
            lv_timer_reset(ui->bubble_hide_timer);
            lv_timer_resume(ui->bubble_hide_timer);
        } else {
            lv_timer_pause(ui->bubble_hide_timer);
        }
    }
}

void phoenix_ui_hide_bubble(phoenix_ui_t *ui)
{
    if (!ui || !ui->bubble_card) return;
    lv_obj_add_flag(ui->bubble_card, LV_OBJ_FLAG_HIDDEN);
    if (ui->bubble_hide_timer) {
        lv_timer_pause(ui->bubble_hide_timer);
    }
}

/* 飞字动效回调 */
static void flying_text_anim_cb(void *var, int32_t val)
{
    lv_obj_t *label = (lv_obj_t *)var;
    if (label) lv_obj_set_y(label, val);
}

static void flying_text_ready_cb(lv_anim_t *a)
{
    lv_obj_t *label = (lv_obj_t *)a->var;
    if (label) lv_obj_delete_async(label);
}

void phoenix_ui_show_flying_text(phoenix_ui_t *ui, const char *text, lv_color_t color)
{
    if (!ui || !ui->screen || !text) return;

    lv_obj_t *fly_label = lv_label_create(ui->screen);
    if (ui->font_chinese) {
        lv_obj_set_style_text_font(fly_label, ui->font_chinese, LV_PART_MAIN);
    }
    lv_label_set_text(fly_label, text);
    lv_obj_set_style_text_color(fly_label, color, LV_PART_MAIN);

    int32_t scr_h = lv_obj_get_height(ui->screen);
    int32_t start_y = scr_h / 2 - 20;

    lv_obj_align(fly_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_y(fly_label, start_y);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, fly_label);
    lv_anim_set_values(&a, start_y, start_y - 60);
    lv_anim_set_duration(&a, 1200);
    lv_anim_set_exec_cb(&a, flying_text_anim_cb);
    lv_anim_set_ready_cb(&a, flying_text_ready_cb);
    lv_anim_start(&a);
}

/* 刷新顶部微状态胶囊 */
static void refresh_status_capsule(phoenix_ui_t *ui)
{
    if (!ui) return;

    if (ui->capsule) {
        /* 左侧：环境温湿度 (26℃ 60%) */
        ui_capsule_update_env(ui->capsule, 26.0f, 60);

        /* 右侧：网络状态与电池电量 (Wi-Fi 85%) */
        ui_capsule_update_net_battery(ui->capsule, "Wi-Fi", ui->battery_pct);

        if (ui->pomodoro_active) {
            uint16_t m = ui->pomodoro_remain_s / 60;
            uint16_t s = ui->pomodoro_remain_s % 60;
            char pbuf[32];
            snprintf(pbuf, sizeof(pbuf), "专注 %02u:%02u", m, s);
            ui_capsule_set_status(ui->capsule, pbuf, COLOR_POMO_ORANGE, false);
        }
    }
}

/* 顶部微状态胶囊点击呼出控制中心抽屉 */
static void on_capsule_clicked(lv_event_t *e)
{
    phoenix_ui_t *ui = (phoenix_ui_t *)lv_event_get_user_data(e);
    if (ui && ui->settings) {
        ui_settings_toggle(ui->settings);
    }
}

/* 事件总线监听 */
static void on_event_bus_event(const phoenix_event_data_t *event, void *user_data)
{
    phoenix_ui_t *ui = (phoenix_ui_t *)user_data;
    if (!ui || !event) return;

    switch (event->type) {
        case PHOENIX_EVT_STATE_CHANGED: {
            int new_state = event->data.state.new_state;

            if (ui->pomodoro_active && new_state != AGENT_STATE_ALERT) {
                break; /* 番茄钟激活时保持倒计时药丸 */
            }

            switch (new_state) {
                case AGENT_STATE_IDLE:
                    if (ui->capsule) ui_capsule_set_status(ui->capsule, "● 待命", lv_color_hex(0x00E5FF), false);
                    if (ui->eye) phoenix_eye_set_emotion(ui->eye, PHOENIX_EYE_IDLE);
                    break;
                case AGENT_STATE_LISTENING:
                    if (ui->capsule) ui_capsule_set_status(ui->capsule, "● 倾听", lv_color_hex(0x00E5FF), false);
                    if (ui->eye) phoenix_eye_set_emotion(ui->eye, PHOENIX_EYE_LISTENING);
                    phoenix_ui_show_bubble(ui, "正在倾听指令...", 0);
                    break;
                case AGENT_STATE_THINKING:
                    if (ui->capsule) ui_capsule_set_status(ui->capsule, "● 思考", COLOR_THINK_PURPLE, false);
                    if (ui->eye) phoenix_eye_set_emotion(ui->eye, PHOENIX_EYE_THINKING);
                    phoenix_ui_show_bubble(ui, "正在深度推导意图...", 0);
                    break;
                case AGENT_STATE_EXECUTING:
                    if (ui->capsule) ui_capsule_set_status(ui->capsule, "● 执行", lv_color_hex(0x00FF66), false);
                    break;
                case AGENT_STATE_CELEBRATING:
                    if (ui->capsule) ui_capsule_set_status(ui->capsule, "● 圆满", COLOR_PRIMARY_GOLD, false);
                    if (ui->eye) phoenix_eye_set_emotion(ui->eye, PHOENIX_EYE_HAPPY);
                    break;
                case AGENT_STATE_ALERT:
                    if (ui->capsule) ui_capsule_set_status(ui->capsule, "● 警戒", COLOR_ALERT_RED, false);
                    if (ui->eye) phoenix_eye_set_emotion(ui->eye, PHOENIX_EYE_ALERT);
                    break;
                default:
                    break;
            }
            break;
        }

        case PHOENIX_EVT_FLYING_TEXT:
            phoenix_ui_show_flying_text(ui, event->data.flying_text.text, lv_color_hex(0xffd700));
            break;

        case PHOENIX_EVT_LLM_FINISHED:
            if (event->data.llm_text.text) {
                phoenix_ui_show_bubble(ui, event->data.llm_text.text, 4000);
            }
            break;

        case PHOENIX_EVT_PROACTIVE_INTERVENE: {
            char pbuf[128];
            snprintf(pbuf, sizeof(pbuf), "[%s] %s",
                     event->data.proactive.title, event->data.proactive.suggestion);
            phoenix_ui_show_bubble(ui, pbuf, 5000);
            phoenix_ui_show_flying_text(ui, "主动心智干预", COLOR_PRIMARY_GOLD);
            break;
        }

        case PHOENIX_EVT_PLAY_SOUND:
            phoenix_audio_play(event->data.sound.sound_id);
            break;

        case PHOENIX_EVT_POMODORO_TICK:
            ui->pomodoro_active = event->data.stats.is_active;
            ui->pomodoro_remain_s = event->data.stats.remaining_s;
            refresh_status_capsule(ui);
            break;

        case PHOENIX_EVT_MERIT_UPDATED:
            ui->merit_count = event->data.stats.total_merit;
            refresh_status_capsule(ui);
            break;

        case PHOENIX_EVT_HAL_BATTERY:
            ui->battery_pct = (uint8_t)event->data.battery.percentage;
            refresh_status_capsule(ui);
            if (ui->capsule) {
                ui_capsule_update_telemetry(ui->capsule, "8080", ui->battery_pct);
            }
            break;

        case PHOENIX_EVT_CARTRIDGE_SWITCHED:
            if (ui->capsule) {
                ui_capsule_update_cartridge(ui->capsule, 
                                            event->data.cartridge.name, 
                                            event->data.cartridge.icon);
                ui_capsule_set_status(ui->capsule, "● 已载入", COLOR_HEALTH_GREEN, false);
            }
            break;

        default:
            break;
    }
}

static void heartbeat_timer_cb(lv_timer_t *timer)
{
    phoenix_ui_t *ui = (phoenix_ui_t *)lv_timer_get_user_data(timer);
    if (!ui) return;

    ui->uptime_sec++;
    if (ui->agent_core) {
        phoenix_agent_tick_1s(ui->agent_core);
    }
    refresh_status_capsule(ui);
}

static void flush_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    phoenix_store_flush();
}

static void on_stage_swipe_down(void *user_data)
{
    phoenix_ui_t *ui = (phoenix_ui_t *)user_data;
    if (ui && ui->settings) {
        ui_settings_open(ui->settings);
    }
}

phoenix_ui_t* phoenix_ui_create(lv_obj_t *parent, phoenix_agent_ctx_t *core)
{
    phoenix_ui_t *ui = &g_phoenix_ui;
    memset(ui, 0, sizeof(phoenix_ui_t));

    ui->screen = parent;
    ui->agent_core = core;
    ui->font_chinese = init_chinese_font();
    ui->uptime_sec = 0;
    ui->merit_count = 0;
    ui->battery_pct = 85; /* 默认电量 */
    ui->pomodoro_active = false;
    ui->pomodoro_remain_s = 0;

    int32_t scr_w = lv_obj_get_width(parent);
    int32_t scr_h = lv_obj_get_height(parent);
    if (scr_w <= 0) scr_w = 320;
    if (scr_h <= 0) scr_h = 240;

    /* 设置屏幕纯黑科技感背景与无滚动限制 */
    lv_obj_set_style_bg_color(ui->screen, COLOR_SCREEN_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(ui->screen, LV_OBJ_FLAG_SCROLLABLE);

    /* 1. 注册事件总线监听 (优先就绪) */
    phoenix_event_subscribe(PHOENIX_EVT_STATE_CHANGED, on_event_bus_event, ui);
    phoenix_event_subscribe(PHOENIX_EVT_FLYING_TEXT, on_event_bus_event, ui);
    phoenix_event_subscribe(PHOENIX_EVT_LLM_FINISHED, on_event_bus_event, ui);
    phoenix_event_subscribe(PHOENIX_EVT_PROACTIVE_INTERVENE, on_event_bus_event, ui);
    phoenix_event_subscribe(PHOENIX_EVT_PLAY_SOUND, on_event_bus_event, ui);
    phoenix_event_subscribe(PHOENIX_EVT_POMODORO_TICK, on_event_bus_event, ui);
    phoenix_event_subscribe(PHOENIX_EVT_MERIT_UPDATED, on_event_bus_event, ui);
    phoenix_event_subscribe(PHOENIX_EVT_HAL_BATTERY, on_event_bus_event, ui);
    phoenix_event_subscribe(PHOENIX_EVT_CARTRIDGE_SWITCHED, on_event_bus_event, ui);

    /* 2. 创建卡带主舞台视窗 (Shell Viewport & Touch Engine) */
    ui->stage = ui_stage_create(ui->screen);
    if (ui->stage) {
        ui_stage_set_swipe_down_cb(ui->stage, on_stage_swipe_down, ui);
        cartridge_mgr_set_stage(ui_stage_get_canvas(ui->stage));
    }

    /* 3. 顶部极窄微状态胶囊 (22px，半透明常驻，点击呼出控制中心) */
    ui->capsule = ui_capsule_create(ui->screen, ui->font_chinese);
    if (ui->capsule && ui->capsule->container) {
        lv_obj_add_flag(ui->capsule->container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(ui->capsule->container, 8);
        lv_obj_add_event_cb(ui->capsule->container, on_capsule_clicked, LV_EVENT_CLICKED, ui);
        if (ui->capsule->pill_status) {
            lv_obj_add_flag(ui->capsule->pill_status, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(ui->capsule->pill_status, on_capsule_clicked, LV_EVENT_CLICKED, ui);
        }
    }

    /* 4. 顶部控制中心抽屉与设置面板 (Settings Drawer，默认滑入在屏幕上方外) */
    ui->settings = ui_settings_create(ui->screen, ui->font_chinese);

    /* 5. 底部动态交互气泡 (Dynamic Speech Bubble，平时隐藏，主动干预时浮现) */
    int32_t bubble_h = (scr_h < 260) ? 36 : 46;
    ui->bubble_card = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->bubble_card, scr_w - 20, bubble_h);
    lv_obj_align(ui->bubble_card, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(ui->bubble_card, COLOR_BUBBLE_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui->bubble_card, COLOR_BUBBLE_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->bubble_card, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(ui->bubble_card, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(ui->bubble_card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(ui->bubble_card, 4, LV_PART_MAIN);
    lv_obj_clear_flag(ui->bubble_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui->bubble_card, LV_OBJ_FLAG_HIDDEN);

    ui->bubble_label = lv_label_create(ui->bubble_card);
    lv_obj_set_width(ui->bubble_label, scr_w - 44);
    lv_obj_align(ui->bubble_label, LV_ALIGN_CENTER, 0, 0);
    if (ui->font_chinese) lv_obj_set_style_text_font(ui->bubble_label, ui->font_chinese, LV_PART_MAIN);
    lv_label_set_long_mode(ui->bubble_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(ui->bubble_label, "灵眸陪伴中");
    lv_obj_set_style_text_color(ui->bubble_label, COLOR_TEXT_MUTED, LV_PART_MAIN);

    /* 气泡自动隐藏定时器 (默认暂停) */
    ui->bubble_hide_timer = lv_timer_create(bubble_hide_timer_cb, 4000, ui);
    lv_timer_pause(ui->bubble_hide_timer);

    /* 启动秒级心跳与状态持久化定时器 */
    ui->heartbeat_timer = lv_timer_create(heartbeat_timer_cb, 1000, ui);
    ui->flush_timer = lv_timer_create(flush_timer_cb, 5000, ui);

    /* 同步当前已持久化的功德数 */
    phoenix_stats_t stats;
    phoenix_store_get_stats(&stats);
    ui->merit_count = stats.total_merit;
    refresh_status_capsule(ui);

    return ui;
}

void phoenix_ui_destroy(phoenix_ui_t *ui)
{
    if (!ui) return;

    if (ui->settings) {
        ui_settings_destroy(ui->settings);
        ui->settings = NULL;
    }
    if (ui->capsule) {
        ui_capsule_destroy(ui->capsule);
        ui->capsule = NULL;
    }
    if (ui->stage) {
        ui_stage_destroy(ui->stage);
        ui->stage = NULL;
    }

    if (ui->heartbeat_timer) lv_timer_delete(ui->heartbeat_timer);
    if (ui->flush_timer) lv_timer_delete(ui->flush_timer);
    if (ui->bubble_hide_timer) lv_timer_delete(ui->bubble_hide_timer);
    if (ui->eye) phoenix_eye_destroy(ui->eye);

    phoenix_event_unsubscribe(PHOENIX_EVT_STATE_CHANGED, on_event_bus_event, ui);
    phoenix_event_unsubscribe(PHOENIX_EVT_FLYING_TEXT, on_event_bus_event, ui);
    phoenix_event_unsubscribe(PHOENIX_EVT_LLM_FINISHED, on_event_bus_event, ui);
    phoenix_event_unsubscribe(PHOENIX_EVT_PROACTIVE_INTERVENE, on_event_bus_event, ui);
    phoenix_event_unsubscribe(PHOENIX_EVT_PLAY_SOUND, on_event_bus_event, ui);
    phoenix_event_unsubscribe(PHOENIX_EVT_POMODORO_TICK, on_event_bus_event, ui);
    phoenix_event_unsubscribe(PHOENIX_EVT_MERIT_UPDATED, on_event_bus_event, ui);
    phoenix_event_unsubscribe(PHOENIX_EVT_HAL_BATTERY, on_event_bus_event, ui);
}
