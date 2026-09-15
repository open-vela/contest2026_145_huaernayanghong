/**
 * @file perception.c
 * @brief Phoenix HoloDesk-S1 Embodied Perception Engine Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "perception.h"
#include "hal/hal_sensor.h"
#include "hal/hal_actuator.h"
#include "core/event_bus.h"
#include "core/store.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

static bool g_perception_running = false;
static phoenix_perception_config_t g_perception_cfg;

static uint64_t g_last_tap_time_ms = 0;
static bool g_prev_is_dark = false;
static bool g_prev_is_low_battery = false;

static uint64_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

int phoenix_perception_init(const phoenix_perception_config_t *config)
{
    if (g_perception_running) {
        return 0;
    }

    if (config) {
        g_perception_cfg = *config;
    } else {
        g_perception_cfg.poll_interval_ms = 50;
        g_perception_cfg.auto_bridge_to_event_bus = true;
        g_perception_cfg.enable_tap_to_wooden_fish = true;
    }

    g_last_tap_time_ms = 0;
    g_prev_is_dark = false;
    g_prev_is_low_battery = false;
    g_perception_running = true;

    printf("[PhoenixPerception] 👁️ Embodied Perception Engine started (Poll: %ums, AutoBridge: %d)\n",
           g_perception_cfg.poll_interval_ms, g_perception_cfg.auto_bridge_to_event_bus);
    return 0;
}

void phoenix_perception_step(void)
{
    if (!g_perception_running) return;

    uint64_t now = get_time_ms();

    /* 1. Poll Physical Tap / Vibration Sensor */
    hal_tap_event_t tap_event;
    if (hal_sensor_poll_tap(&tap_event)) {
        /* Anti-chatter debouncing (at least 80ms) */
        if (now - g_last_tap_time_ms >= 80) {
            g_last_tap_time_ms = now;

            printf("[PhoenixPerception] 💥 Sensory Event: Physical Tap Detected (Intensity %d)\n",
                   (int)tap_event.intensity);

            /* Publish Raw Sensor Event to Event Bus */
            if (g_perception_cfg.auto_bridge_to_event_bus) {
                phoenix_event_data_t evt;
                memset(&evt, 0, sizeof(evt));
                evt.type = PHOENIX_EVT_HAL_TAP;
                evt.data.tap.intensity = (int)tap_event.intensity;
                evt.data.tap.timestamp_ms = tap_event.timestamp_ms;
                phoenix_event_publish(&evt);
            }

            /* Embodied Action: Tap-to-Wooden-Fish Feedback Loop */
            if (g_perception_cfg.enable_tap_to_wooden_fish) {
                /* Trigger Haptic Motor & Audio Actuators */
                hal_actuator_trigger_haptic(HAL_HAPTIC_CLICK);
                hal_actuator_play_sound(HAL_SOUND_WOODEN_FISH);

                /* Increment Merit in persistent store */
                uint32_t current_merit = phoenix_store_add_merit(1);

                /* HUD Flying text */
                phoenix_event_data_t fly_evt;
                memset(&fly_evt, 0, sizeof(fly_evt));
                fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
                fly_evt.data.flying_text.text = "功德 +1 (物理敲击)";
                fly_evt.data.flying_text.color_rgb = 0xffd700;
                phoenix_event_publish(&fly_evt);

                /* Stats updated */
                phoenix_event_data_t stats_evt;
                memset(&stats_evt, 0, sizeof(stats_evt));
                stats_evt.type = PHOENIX_EVT_MERIT_UPDATED;
                stats_evt.data.stats.total_merit = current_merit;
                phoenix_event_publish(&stats_evt);
            }
        }
    }

    /* 2. Poll Ambient Light Sensor (ALS) */
    hal_light_data_t light_data;
    if (hal_sensor_read_light(&light_data) == 0) {
        if (g_perception_cfg.auto_bridge_to_event_bus) {
            phoenix_event_data_t evt;
            memset(&evt, 0, sizeof(evt));
            evt.type = PHOENIX_EVT_HAL_LIGHT;
            evt.data.light.lux = light_data.lux;
            evt.data.light.is_dark_mode = light_data.is_dark_environment;
            phoenix_event_publish(&evt);
        }

        /* Detect Transition into Dark Environment */
        if (light_data.is_dark_environment && !g_prev_is_dark) {
            printf("[PhoenixPerception] 🌙 Sensory Transition: Low Ambient Light (%u Lux)\n", light_data.lux);

            /* Proactive Autonomous Intervention */
            phoenix_event_data_t pro_evt;
            memset(&pro_evt, 0, sizeof(pro_evt));
            pro_evt.type = PHOENIX_EVT_PROACTIVE_INTERVENE;
            pro_evt.data.proactive.proactive_type = PROACTIVE_ENV_DARK_SLEEP;
            pro_evt.data.proactive.title = "环境夜间暗光伴睡模式";
            pro_evt.data.proactive.suggestion = "检测到环境光线变暗，已为您切换低蓝光柔光模式，需要开启助眠白噪音吗？";
            pro_evt.data.proactive.auto_action = "开启白噪音助眠";
            phoenix_event_publish(&pro_evt);
        }
        g_prev_is_dark = light_data.is_dark_environment;
    }

    /* 3. Poll Battery Power Sensor */
    hal_battery_data_t battery_data;
    if (hal_sensor_read_battery(&battery_data) == 0) {
        if (g_perception_cfg.auto_bridge_to_event_bus) {
            phoenix_event_data_t evt;
            memset(&evt, 0, sizeof(evt));
            evt.type = PHOENIX_EVT_HAL_BATTERY;
            evt.data.battery.percentage = battery_data.percentage;
            evt.data.battery.is_charging = battery_data.is_charging;
            evt.data.battery.is_low_power = battery_data.is_low_power;
            phoenix_event_publish(&evt);
        }

        /* Detect Transition into Low Battery State */
        if (battery_data.is_low_power && !battery_data.is_charging && !g_prev_is_low_battery) {
            printf("[PhoenixPerception] 🔋 Sensory Transition: Low Battery Alert (%u%%)\n",
                   battery_data.percentage);

            /* Trigger Warning Sound Actuator */
            hal_actuator_play_sound(HAL_SOUND_ALERT);

            /* Proactive Intervention */
            phoenix_event_data_t pro_evt;
            memset(&pro_evt, 0, sizeof(pro_evt));
            pro_evt.type = PHOENIX_EVT_PROACTIVE_INTERVENE;
            pro_evt.data.proactive.proactive_type = PROACTIVE_BATTERY_LOW;
            pro_evt.data.proactive.title = "低电量保护警告";
            pro_evt.data.proactive.suggestion = "当前剩余电量不足 15%，请尽快连接充电器以保证灵眸持续守护！";
            pro_evt.data.proactive.auto_action = "开启省电模式";
            phoenix_event_publish(&pro_evt);
        }
        g_prev_is_low_battery = battery_data.is_low_power;
    }
}

void phoenix_perception_deinit(void)
{
    if (!g_perception_running) return;

    printf("[PhoenixPerception] 🛑 Perception Engine stopped.\n");
    g_perception_running = false;
}

bool phoenix_perception_is_running(void)
{
    return g_perception_running;
}
