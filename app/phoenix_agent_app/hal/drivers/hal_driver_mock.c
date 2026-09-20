/**
 * @file hal_driver_mock.c
 * @brief Phoenix HoloDesk-S1 Mock HAL Driver Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "hal_driver_mock.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

/* Mock internal state */
static bool g_has_tap = false;
static hal_tap_intensity_t g_pending_tap_intensity = HAL_TAP_NORMAL;
static uint32_t g_mock_lux = 350;
static uint8_t g_mock_battery_pct = 90;
static bool g_mock_battery_charging = true;
static float g_mock_temperature = 41.2f;
static float g_mock_env_temp_c = 26.5f;
static float g_mock_env_humi_pct = 58.0f;
static bool g_mock_env_valid = true;

static int g_last_sound = -1;
static int g_last_haptic = -1;
static char g_last_launched_app[128] = {0};

static uint64_t mock_get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

void hal_mock_inject_tap(hal_tap_intensity_t intensity)
{
    g_has_tap = true;
    g_pending_tap_intensity = intensity;
}

void hal_mock_set_light(uint32_t lux)
{
    g_mock_lux = lux;
}

void hal_mock_set_battery(uint8_t percentage, bool is_charging)
{
    g_mock_battery_pct = (percentage > 100) ? 100 : percentage;
    g_mock_battery_charging = is_charging;
}

void hal_mock_set_temperature(float temp_c)
{
    g_mock_temperature = temp_c;
}

void hal_mock_set_env(float temp_c, float humi_pct, bool is_valid)
{
    g_mock_env_temp_c = temp_c;
    g_mock_env_humi_pct = humi_pct;
    g_mock_env_valid = is_valid;
}

int hal_mock_get_last_sound(void)
{
    return g_last_sound;
}

int hal_mock_get_last_haptic_pattern(void)
{
    return g_last_haptic;
}

const char* hal_mock_get_last_launched_app(void)
{
    return g_last_launched_app;
}

void hal_mock_reset(void)
{
    g_has_tap = false;
    g_pending_tap_intensity = HAL_TAP_NORMAL;
    g_mock_lux = 350;
    g_mock_battery_pct = 90;
    g_mock_battery_charging = true;
    g_mock_temperature = 41.2f;
    g_mock_env_temp_c = 26.5f;
    g_mock_env_humi_pct = 58.0f;
    g_mock_env_valid = true;
    g_last_sound = -1;
    g_last_haptic = -1;
    memset(g_last_launched_app, 0, sizeof(g_last_launched_app));
}

/* ========================================================================= */
/* Sensor Ops Implementation                                                 */
/* ========================================================================= */
static int mock_sensor_init(void) { return 0; }
static int mock_sensor_deinit(void) { return 0; }

static bool mock_sensor_poll_tap(hal_tap_event_t *out_tap)
{
    if (g_has_tap && out_tap) {
        out_tap->intensity = g_pending_tap_intensity;
        out_tap->timestamp_ms = mock_get_time_ms();
        g_has_tap = false;
        return true;
    }
    return false;
}

static int mock_sensor_read_light(hal_light_data_t *out_light)
{
    if (!out_light) return -1;
    out_light->lux = g_mock_lux;
    out_light->is_dark_environment = (g_mock_lux < 30);
    out_light->is_direct_sunlight = (g_mock_lux > 1000);
    return 0;
}

static int mock_sensor_read_battery(hal_battery_data_t *out_battery)
{
    if (!out_battery) return -1;
    out_battery->percentage = g_mock_battery_pct;
    out_battery->is_charging = g_mock_battery_charging;
    out_battery->is_low_power = (g_mock_battery_pct < 15);
    out_battery->voltage_mv = 3600 + (uint16_t)(g_mock_battery_pct * 6);
    return 0;
}

static int mock_sensor_read_env(hal_env_data_t *out_env)
{
    if (!out_env) return -1;
    out_env->temperature_c = g_mock_env_temp_c;
    out_env->humidity_pct = g_mock_env_humi_pct;
    out_env->is_valid = g_mock_env_valid;
    out_env->timestamp_us = mock_get_time_ms() * 1000;
    return 0;
}

/* ========================================================================= */
/* Actuator Ops Implementation                                               */
/* ========================================================================= */
static int mock_actuator_init(const char *sound_root)
{
    (void)sound_root;
    return 0;
}

static int mock_actuator_deinit(void) { return 0; }

static int mock_actuator_play_sound(hal_sound_type_t sound_type)
{
    g_last_sound = (int)sound_type;
    return 0;
}

static void mock_actuator_set_volume(uint8_t volume_pct)
{
    (void)volume_pct;
}

static int mock_actuator_trigger_haptic(hal_haptic_pattern_t pattern)
{
    g_last_haptic = (int)pattern;
    return 0;
}

static int mock_actuator_set_led(hal_led_mode_t mode, uint32_t rgb, uint8_t brightness)
{
    (void)mode;
    (void)rgb;
    (void)brightness;
    return 0;
}

static int mock_actuator_blink_led(int count, uint32_t interval_ms)
{
    printf("[HAL:Mock] 💡 Board LED mock blink: %d times, interval %u ms\n",
           count, (unsigned)interval_ms);
    return 0;
}

/* ========================================================================= */
/* System Ops Implementation                                                 */
/* ========================================================================= */
static int mock_system_init(void) { return 0; }
static int mock_system_deinit(void) { return 0; }

static int mock_system_get_telemetry(hal_system_telemetry_t *out_telem)
{
    if (!out_telem) return -1;
    strncpy(out_telem->board_model, "Host Simulation (Apple M-Series/x86_64)", sizeof(out_telem->board_model) - 1);
    strncpy(out_telem->os_version, "Host POSIX Mock (HAL v1.0)", sizeof(out_telem->os_version) - 1);
    out_telem->cpu_temperature_c = g_mock_temperature;
    out_telem->cpu_freq_mhz = 2400;
    out_telem->cpu_load_pct = 15;
    out_telem->mem_total_kb = 128 * 1024;
    out_telem->mem_free_kb = 68 * 1024;
    out_telem->mem_used_pct = 46;
    out_telem->uptime_seconds = (uint64_t)(mock_get_time_ms() / 1000);
    out_telem->fps = 60;
    return 0;
}

static int mock_system_launch_app(const char *app_package_or_alias)
{
    if (app_package_or_alias) {
        strncpy(g_last_launched_app, app_package_or_alias, sizeof(g_last_launched_app) - 1);
    }
    return 0;
}

static const char* mock_system_get_storage_base_path(void)
{
    return "/tmp/phoenix_host";
}

/* ========================================================================= */
/* Audio In Ops Implementation                                               */
/* ========================================================================= */
static int16_t g_mock_pcm_buf[320] = {0};
static size_t g_mock_pcm_count = 0;
static bool g_mock_is_speech = false;
static bool g_mock_audio_streaming = false;

void hal_mock_inject_pcm_frame(const int16_t *samples, size_t count, bool is_speech)
{
    if (count > sizeof(g_mock_pcm_buf) / sizeof(g_mock_pcm_buf[0])) {
        count = sizeof(g_mock_pcm_buf) / sizeof(g_mock_pcm_buf[0]);
    }
    if (samples) {
        memcpy(g_mock_pcm_buf, samples, count * sizeof(int16_t));
    } else {
        memset(g_mock_pcm_buf, 0, count * sizeof(int16_t));
    }
    g_mock_pcm_count = count;
    g_mock_is_speech = is_speech;
}

static int mock_audio_in_init(uint32_t sample_rate, uint8_t channels)
{
    (void)sample_rate; (void)channels;
    g_mock_pcm_count = 0;
    g_mock_audio_streaming = false;
    return 0;
}

static int mock_audio_in_deinit(void)
{
    g_mock_audio_streaming = false;
    return 0;
}

static int mock_audio_in_start_stream(void)
{
    g_mock_audio_streaming = true;
    return 0;
}

static int mock_audio_in_read_frame(hal_audio_pcm_frame_t *frame_out, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (!frame_out || !g_mock_audio_streaming) return -1;
    frame_out->sample_rate = 16000;
    frame_out->channels = 1;
    frame_out->bit_depth = 16;
    frame_out->frame_length = g_mock_pcm_count > 0 ? g_mock_pcm_count : 160;
    frame_out->pcm_data = g_mock_pcm_buf;
    frame_out->is_speech = g_mock_is_speech;
    return 0;
}

static int mock_audio_in_stop_stream(void)
{
    g_mock_audio_streaming = false;
    return 0;
}

/* ========================================================================= */
/* Mock HAL Driver Definition                                                */
/* ========================================================================= */
const hal_driver_t g_hal_driver_mock = {
    .driver_name = "HostSimulationMockDriver",
    .sensor_ops = {
        .init = mock_sensor_init,
        .deinit = mock_sensor_deinit,
        .poll_tap = mock_sensor_poll_tap,
        .read_light = mock_sensor_read_light,
        .read_battery = mock_sensor_read_battery,
        .read_env = mock_sensor_read_env
    },
    .actuator_ops = {
        .init = mock_actuator_init,
        .deinit = mock_actuator_deinit,
        .play_sound = mock_actuator_play_sound,
        .set_volume = mock_actuator_set_volume,
        .trigger_haptic = mock_actuator_trigger_haptic,
        .set_led = mock_actuator_set_led,
        .blink_led = mock_actuator_blink_led
    },
    .system_ops = {
        .init = mock_system_init,
        .deinit = mock_system_deinit,
        .get_telemetry = mock_system_get_telemetry,
        .launch_app = mock_system_launch_app,
        .get_storage_base_path = mock_system_get_storage_base_path
    },
    .audio_in_ops = {
        .init = mock_audio_in_init,
        .deinit = mock_audio_in_deinit,
        .start_stream = mock_audio_in_start_stream,
        .read_frame = mock_audio_in_read_frame,
        .stop_stream = mock_audio_in_stop_stream
    }
};
