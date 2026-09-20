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
#include "hal/network_mgr.h"
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
    phoenix_ui_t *ui = (phoenix_ui_t *)a->user_data;
    if (ui && ui->flying_label) {
        lv_obj_delete(ui->flying_label);
        ui->flying_label = NULL;
    }
}

void phoenix_ui_show_flying_text(phoenix_ui_t *ui, const char *text, lv_color_t color)
{
    if (!ui || !ui->screen || !text) return;

    /* 单例防护：若已有未完成的飞字动画，先安全销毁旧动画与 Label，杜绝多实例堆积与异步删除竞争 */
    if (ui->flying_label) {
        lv_anim_delete(ui->flying_label, NULL);
        lv_obj_delete(ui->flying_label);
        ui->flying_label = NULL;
    }

    lv_obj_t *fly_label = lv_label_create(ui->screen);
    ui->flying_label = fly_label;

    if (ui->font_chinese) {
        lv_obj_set_style_text_font(fly_label, ui->font_chinese, LV_PART_MAIN);
    }
    lv_label_set_text(fly_label, text);
    lv_obj_set_style_text_color(fly_label, color, LV_PART_MAIN);

    int32_t scr_h = lv_obj_get_height(ui->screen);
    if (scr_h <= 0) scr_h = 240;
    int32_t start_y = scr_h / 2 - 20;

    lv_obj_align(fly_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_y(fly_label, start_y);
    lv_obj_move_foreground(fly_label);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, fly_label);
    lv_anim_set_values(&a, start_y, start_y - 60);
    lv_anim_set_duration(&a, 1200);
    lv_anim_set_exec_cb(&a, flying_text_anim_cb);
    lv_anim_set_ready_cb(&a, flying_text_ready_cb);
    lv_anim_set_user_data(&a, ui);
    lv_anim_start(&a);
}

/* 刷新顶部微状态胶囊 (纯事件驱动，使用已缓存网络信息，杜绝加锁轮询) */
static void refresh_status_capsule(phoenix_ui_t *ui)
{
    if (!ui) return;

    if (ui->capsule) {
        /* 左侧：环境温湿度或 R528 核心芯片温度 (动态感知，杜绝假固定数据) */
        float display_temp = ui->cached_temp_c;
        uint8_t display_humi = ui->cached_humi_pct;
        bool is_valid = ui->cached_env_valid;

        if (!is_valid) {
            /* 若无外接环境传感器，优雅 fallback 到全志 R528 芯片真实核心温度 */
            hal_system_telemetry_t telem;
            if (hal_system_get_telemetry(&telem) == 0 && telem.cpu_temperature_c > 0.0f) {
                display_temp = telem.cpu_temperature_c;
            }
        }
        ui_capsule_update_env(ui->capsule, display_temp, display_humi, is_valid);

        /* 右侧：真实网络感知与电池电量 (直接从 UI 只读缓存中读取，杜绝锁竞争) */
        int mode = ui->cached_net_mode;
        const char *ip_buf = ui->cached_net_ip;

        const char *net_label = "未连网";
        if (mode == 2 /* NET_MODE_STA_CONNECTED */) {
            net_label = (ip_buf && ip_buf[0]) ? ip_buf : "已连网";
        } else if (mode == 1 /* NET_MODE_STA_CONNECTING */) {
            net_label = "连网中";
        } else if (mode == 3 /* NET_MODE_SOFTAP_CONFIG */) {
            net_label = "AP配网";
        } else {
            net_label = "未连网";
        }
        ui_capsule_update_net_battery(ui->capsule, net_label, ui->battery_pct);

        /* 中间心智状态：专注流倒计时 (不被外设网络状态冲占，保持 Agent 具身心智生命感) */
        if (ui->pomodoro_active) {
            uint16_t m = ui->pomodoro_remain_s / 60;
            uint16_t s = ui->pomodoro_remain_s % 60;
            char pbuf[32];
            snprintf(pbuf, sizeof(pbuf), "专注 %02u:%02u", m, s);
            ui_capsule_set_status(ui->capsule, pbuf, COLOR_POMO_ORANGE, false);
        }
    }
}

/* 顶部微状态胶囊点击呼出控制中心 */
static void on_capsule_clicked(lv_event_t *e)
{
    phoenix_ui_t *ui = (phoenix_ui_t *)lv_event_get_user_data(e);
    if (!ui || !ui->settings) return;

    if (ui_settings_is_open(ui->settings)) {
        ui_settings_close(ui->settings);
        if (ui->stage && ui->stage->container) {
            lv_obj_clear_flag(ui->stage->container, LV_OBJ_FLAG_HIDDEN);
        }
        if (ui->sidebar) {
            ui_sidebar_set_settings_active(ui->sidebar, false);
        }
    } else {
        if (ui->stage && ui->stage->container) {
            lv_obj_add_flag(ui->stage->container, LV_OBJ_FLAG_HIDDEN);
        }
        ui_settings_open(ui->settings);
        if (ui->sidebar) {
            ui_sidebar_set_settings_active(ui->sidebar, true);
        }
    }
}

/* 侧边栏功能项/卡带点击处理 */
static bool on_sidebar_item_action(const char *id, void *user_data)
{
    phoenix_ui_t *ui = (phoenix_ui_t *)user_data;
    if (!ui || !id) return false;

    if (strcmp(id, "voice") == 0) {
        /* 如果设置处于打开状态且已经在音频实验室，再次点击则关闭抽屉返回主舞台卡带 */
        if (ui_settings_is_open(ui->settings) && 
            ui_settings_get_page(ui->settings) == UI_SETTINGS_TAB_AUDIO) {
            ui_settings_close(ui->settings);
            if (ui->stage && ui->stage->container) {
                lv_obj_clear_flag(ui->stage->container, LV_OBJ_FLAG_HIDDEN);
            }
            if (ui->sidebar) {
                cartridge_t *cur = cartridge_mgr_get_current();
                ui_sidebar_set_active(ui->sidebar, (cur && cur->ops.id[0]) ? cur->ops.id : "home");
            }
        } else {
            /* 隐藏主舞台，打开设置面板并直达音频/语音实验室 */
            if (ui->stage && ui->stage->container) {
                lv_obj_add_flag(ui->stage->container, LV_OBJ_FLAG_HIDDEN);
            }
            ui_settings_open_detail(ui->settings, UI_SETTINGS_TAB_AUDIO);
            if (ui->sidebar) {
                ui_sidebar_set_active(ui->sidebar, "voice");
            }
        }
        return true;
    } else {
        /* 用户点击了常规卡带项 (home, clock, familiar)，如果当前在设置抽屉中则关闭抽屉返回主舞台 */
        if (ui_settings_is_open(ui->settings)) {
            ui_settings_close(ui->settings);
            if (ui->stage && ui->stage->container) {
                lv_obj_clear_flag(ui->stage->container, LV_OBJ_FLAG_HIDDEN);
            }
        }
        cartridge_mgr_switch_to(id);
        if (ui->sidebar) {
            ui_sidebar_set_active(ui->sidebar, id);
        }
        return true;
    }
}

/* 侧边栏底部设置按钮点击: 与卡带完全同级切换 */
static void on_sidebar_settings_clicked(void *user_data)
{
    phoenix_ui_t *ui = (phoenix_ui_t *)user_data;
    if (!ui || !ui->settings) return;

    if (ui_settings_is_open(ui->settings)) {
        ui_settings_close(ui->settings);
        if (ui->stage && ui->stage->container) {
            lv_obj_clear_flag(ui->stage->container, LV_OBJ_FLAG_HIDDEN);
        }
        if (ui->sidebar) {
            ui_sidebar_set_settings_active(ui->sidebar, false);
            cartridge_t *cur = cartridge_mgr_get_current();
            ui_sidebar_set_active(ui->sidebar, (cur && cur->ops.id[0]) ? cur->ops.id : "home");
        }
    } else {
        if (ui->stage && ui->stage->container) {
            lv_obj_add_flag(ui->stage->container, LV_OBJ_FLAG_HIDDEN);
        }
        ui_settings_open(ui->settings);
        if (ui->sidebar) {
            ui_sidebar_set_settings_active(ui->sidebar, true);
        }
    }
}

/* 设置面板关闭时联动恢复主舞台卡带与侧边栏高亮 */
static void on_settings_closed(void *user_data)
{
    phoenix_ui_t *ui = (phoenix_ui_t *)user_data;
    if (ui) {
        if (ui->stage && ui->stage->container) {
            lv_obj_clear_flag(ui->stage->container, LV_OBJ_FLAG_HIDDEN);
        }
        if (ui->sidebar) {
            ui_sidebar_set_settings_active(ui->sidebar, false);
            cartridge_t *cur = cartridge_mgr_get_current();
            ui_sidebar_set_active(ui->sidebar, (cur && cur->ops.id[0]) ? cur->ops.id : "home");
        }
    }
}

/* 配网完成点击返回主页卡带 */
static void on_settings_switch_home(void *user_data)
{
    phoenix_ui_t *ui = (phoenix_ui_t *)user_data;
    if (ui) {
        if (ui->settings) {
            ui_settings_close(ui->settings);
        }
        if (ui->stage && ui->stage->container) {
            lv_obj_clear_flag(ui->stage->container, LV_OBJ_FLAG_HIDDEN);
        }
        if (ui->sidebar) {
            ui_sidebar_set_settings_active(ui->sidebar, false);
        }
        cartridge_mgr_switch_to("home");
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
            if (ui->capsule) {
                ui_capsule_update_pomodoro(ui->capsule, ui->pomodoro_active, ui->pomodoro_remain_s);
            }
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
                ui_capsule_update_telemetry(ui->capsule, NULL, ui->battery_pct);
            }
            break;

        case PHOENIX_EVT_HAL_ENV:
            ui->cached_temp_c = event->data.env.temp_c;
            ui->cached_humi_pct = event->data.env.humi_pct;
            ui->cached_env_valid = event->data.env.is_valid;
            refresh_status_capsule(ui);
            break;

        case PHOENIX_EVT_CARTRIDGE_SWITCHED: {
            static size_t s_prev_index = 0;
            bool slide_to_left = (event->data.cartridge.index >= s_prev_index);
            s_prev_index = event->data.cartridge.index;

            /* 切换卡带时，若设置界面处于展示状态，自动收起设置并呈现卡带舞台 */
            if (ui->settings && ui_settings_is_open(ui->settings)) {
                ui_settings_close(ui->settings);
            }
            if (ui->stage && ui->stage->container) {
                lv_obj_clear_flag(ui->stage->container, LV_OBJ_FLAG_HIDDEN);
            }
            if (ui->sidebar) {
                ui_sidebar_set_settings_active(ui->sidebar, false);
            }

            /* 驱动舞台执行平滑进场动效 */
            if (ui->stage) {
                ui_stage_play_enter_anim(ui->stage, slide_to_left);
            }

            /* 联动更新左侧侧边栏选中高亮态 */
            if (ui->sidebar && event->data.cartridge.to_id) {
                ui_sidebar_set_active(ui->sidebar, event->data.cartridge.to_id);
            }

            if (ui->capsule) {
                ui_capsule_update_cartridge(ui->capsule, 
                                            event->data.cartridge.name, 
                                            event->data.cartridge.icon);
                ui_capsule_set_status(ui->capsule, "● 已载入", COLOR_HEALTH_GREEN, false);
            }
            break;
        }

        case PHOENIX_EVT_NET_STATUS: {
            int mode = event->data.net.mode;
            const char *ip = event->data.net.ip ? event->data.net.ip : "";
            const char *ssid = event->data.net.ssid ? event->data.net.ssid : "";
            const char *msg = event->data.net.msg;
            char bbuf[128];

            /* UI 级防重去抖 (Debounce)：避免同状态重复派发导致刷屏与动画雪崩 */
            static int  s_last_handled_mode = -1;
            static char s_last_handled_ip[32] = {0};
            static char s_last_handled_ssid[32] = {0};

            bool state_changed = (mode != s_last_handled_mode || 
                                  strcmp(ip, s_last_handled_ip) != 0 ||
                                  strcmp(ssid, s_last_handled_ssid) != 0);

            s_last_handled_mode = mode;
            strncpy(s_last_handled_ip, ip, sizeof(s_last_handled_ip) - 1);
            strncpy(s_last_handled_ssid, ssid, sizeof(s_last_handled_ssid) - 1);

            /* 更新 UI 只读事件缓存并按需刷新胶囊 */
            ui->cached_net_mode = mode;
            strncpy(ui->cached_net_ip, ip, sizeof(ui->cached_net_ip) - 1);
            strncpy(ui->cached_net_ssid, ssid, sizeof(ui->cached_net_ssid) - 1);
            refresh_status_capsule(ui);

            if (ui->settings) {
                ui_settings_update_net_progress(ui->settings, mode, ssid, ip, msg);
                if (mode != 1 /* NET_MODE_STA_CONNECTING */) {
                    ui_settings_refresh_data(ui->settings);
                }
            }

            /* 若核心网络模式及 IP/SSID 均未改变且已处于 SoftAP 或断网稳定态，忽略冗余视觉飞字 */
            if (!state_changed && (mode == 3 || mode == 0)) {
                break;
            }

            if (mode == 1 /* NET_MODE_STA_CONNECTING */) {
                if (state_changed) {
                    phoenix_ui_show_flying_text(ui, "正在连入 Wi-Fi...", lv_color_hex(0xFFB700));
                }
                snprintf(bbuf, sizeof(bbuf), "正在连接 Wi-Fi: [%s]...", ssid && ssid[0] ? ssid : "目标路由");
                phoenix_ui_show_bubble(ui, bbuf, 6000);
                if (ui->capsule) {
                    ui_capsule_update_telemetry(ui->capsule, "连网", ui->battery_pct);
                }
            } else if (mode == 2 /* NET_MODE_STA_CONNECTED */) {
                phoenix_ui_show_flying_text(ui, "Wi-Fi 已连入!", lv_color_hex(0x00E676));
                snprintf(bbuf, sizeof(bbuf), "[Wi-Fi 就绪] %s (IP: %s)", ssid, ip);
                phoenix_ui_show_bubble(ui, bbuf, 6000);
                if (ui->capsule) {
                    ui_capsule_update_telemetry(ui->capsule, "WiFi", ui->battery_pct);
                }
            } else if (mode == 3 /* NET_MODE_SOFTAP_CONFIG */) {
                phoenix_ui_show_flying_text(ui, "独立热点已就绪", lv_color_hex(0xFFB300));
                snprintf(bbuf, sizeof(bbuf), "[热点广播] %s (192.168.4.1)", ssid && ssid[0] ? ssid : "Gemini-Setup");
                phoenix_ui_show_bubble(ui, bbuf, 6000);
                if (ui->capsule) {
                    ui_capsule_update_telemetry(ui->capsule, "AP", ui->battery_pct);
                }
            } else {
                phoenix_ui_show_flying_text(ui, "连网失败", lv_color_hex(0xFF5252));
                phoenix_ui_show_bubble(ui, "Wi-Fi 连接失败，请检查密码或重试", 4000);
                if (ui->capsule) {
                    ui_capsule_update_telemetry(ui->capsule, "--", ui->battery_pct);
                }
            }
            break;
        }

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

    /* 当板载环境传感器未就绪时，随芯片工作负载每秒平滑同步真实核心温度，呈现数字生命感知力 */
    if (!ui->cached_env_valid && ui->capsule) {
        refresh_status_capsule(ui);
    }

    /* 当抽屉展开处于前台时，每秒自动刷新遥测与蓝牙/Wi-Fi实时状态 */
    if (ui->settings && ui->settings->is_open) {
        ui_settings_refresh_data(ui->settings);
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
    ui->cached_temp_c = 0.0f;
    ui->cached_humi_pct = 0;
    ui->cached_env_valid = false;
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
    phoenix_event_subscribe(PHOENIX_EVT_HAL_ENV, on_event_bus_event, ui);
    phoenix_event_subscribe(PHOENIX_EVT_CARTRIDGE_SWITCHED, on_event_bus_event, ui);
    phoenix_event_subscribe(PHOENIX_EVT_NET_STATUS, on_event_bus_event, ui);

    /* 2. 创建卡带主舞台视窗 (X=46, Y=24, W=274, H=216) */
    ui->stage = ui_stage_create(ui->screen);
    if (ui->stage) {
        cartridge_mgr_set_stage(ui_stage_get_canvas(ui->stage));
    }

    /* 3. 左侧常驻导航栏：点击直达切卡 + 最底部控制中心入口 (宽 46px, Y=24) */
    ui->sidebar = ui_sidebar_create(ui->screen, ui->font_chinese);
    if (ui->sidebar) {
        ui_sidebar_set_settings_cb(ui->sidebar, on_sidebar_settings_clicked, ui);
        ui_sidebar_set_action_cb(ui->sidebar, on_sidebar_item_action, ui);
    }

    /* 4. 顶部极窄微状态胶囊 (22px，半透明常驻，点击呼出控制中心，热区外扩 12px) */
    ui->capsule = ui_capsule_create(ui->screen, ui->font_chinese);
    if (ui->capsule && ui->capsule->container) {
        lv_obj_add_flag(ui->capsule->container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(ui->capsule->container, 12);
        lv_obj_add_event_cb(ui->capsule->container, on_capsule_clicked, LV_EVENT_CLICKED, ui);
        if (ui->capsule->pill_status) {
            lv_obj_add_flag(ui->capsule->pill_status, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_ext_click_area(ui->capsule->pill_status, 8);
            lv_obj_add_event_cb(ui->capsule->pill_status, on_capsule_clicked, LV_EVENT_CLICKED, ui);
        }
    }

    /* 5. 控制中心面板与二级设置 (贴合侧边栏, X=46, Y=24, W=274, H=216) */
    ui->settings = ui_settings_create(ui->screen, ui->font_chinese);
    if (ui->settings) {
        ui_settings_set_close_cb(ui->settings, on_settings_closed, ui);
        ui_settings_set_switch_home_cb(ui->settings, on_settings_switch_home, ui);
    }

    /* 6. 底部动态交互气泡 (Dynamic Speech Bubble，平时隐藏，主动干预时浮现) */
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

    /* 初始化网络状态只读缓存 */
    ui->cached_net_mode = (int)net_mgr_get_mode();
    net_mgr_get_ip(ui->cached_net_ip, sizeof(ui->cached_net_ip));
    net_mgr_get_ssid(ui->cached_net_ssid, sizeof(ui->cached_net_ssid));

    /* 启动秒级心跳定时器 */
    ui->heartbeat_timer = lv_timer_create(heartbeat_timer_cb, 1000, ui);

    /* 同步当前已持久化的功德数 */
    phoenix_stats_t stats;
    phoenix_store_get_stats(&stats);
    ui->merit_count = stats.total_merit;
    refresh_status_capsule(ui);

    /* 若当前手动拉起或处于 SoftAP 配网模式，主动弹出显性引导气泡 (视觉飞字由 EventBus 统一驱动) */
    if (net_mgr_get_mode() == NET_MODE_SOFTAP_CONFIG) {
        phoenix_ui_show_bubble(ui, "热点已开启: 请连热点 [Gemini-Agent-Setup] 极速配网", 8000);
    }

    return ui;
}

void phoenix_ui_destroy(phoenix_ui_t *ui)
{
    if (!ui) return;

    if (ui->flying_label) {
        lv_anim_delete(ui->flying_label, NULL);
        lv_obj_delete(ui->flying_label);
        ui->flying_label = NULL;
    }

    if (ui->settings) {
        ui_settings_destroy(ui->settings);
        ui->settings = NULL;
    }
    if (ui->sidebar) {
        ui_sidebar_destroy(ui->sidebar);
        ui->sidebar = NULL;
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
    phoenix_event_unsubscribe(PHOENIX_EVT_HAL_ENV, on_event_bus_event, ui);
    phoenix_event_unsubscribe(PHOENIX_EVT_NET_STATUS, on_event_bus_event, ui);
}
