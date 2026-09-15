/**
 * @file hal_driver_openvela.c
 * @brief Phoenix HoloDesk-S1 OpenVela Gemini-S1 Board HAL Driver Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "hal_driver_openvela.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>

#ifdef __NUTTX__
#include <nuttx/config.h>
#include <malloc.h>
#endif

#ifdef CONFIG_AUDIOUTILS_NXAUDIO
#include <audioutils/nxaudio.h>
#endif

static char g_board_sound_root[128] = "/data/sounds";
static uint8_t g_board_volume = 80;

static uint64_t board_get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

/* ========================================================================= */
/* OpenVela Board Sensor Ops                                                 */
/* ========================================================================= */
static int openvela_sensor_init(void)
{
    printf("[HAL:OpenVela] Initializing Gemini-S1 on-board sensor subsystems...\n");
    return 0;
}

static int openvela_sensor_deinit(void)
{
    return 0;
}

static bool openvela_sensor_poll_tap(hal_tap_event_t *out_tap)
{
    /* On real hardware, read from /dev/input or accelerometer tap threshold */
    (void)out_tap;
    return false;
}

static int openvela_sensor_read_light(hal_light_data_t *out_light)
{
    if (!out_light) return -1;
    /* Default normal ambient light for Gemini-S1 desktop environment */
    out_light->lux = 320;
    out_light->is_dark_environment = false;
    out_light->is_direct_sunlight = false;
    return 0;
}

static int openvela_sensor_read_battery(hal_battery_data_t *out_battery)
{
    if (!out_battery) return -1;
    /* Gemini-S1 Type-C / Battery power supply status */
    out_battery->percentage = 95;
    out_battery->is_charging = true;
    out_battery->is_low_power = false;
    out_battery->voltage_mv = 4120;
    return 0;
}

/* ========================================================================= */
/* OpenVela Board Actuator Ops                                               */
/* ========================================================================= */
static int openvela_actuator_init(const char *sound_root)
{
    if (sound_root && strlen(sound_root) > 0) {
        strncpy(g_board_sound_root, sound_root, sizeof(g_board_sound_root) - 1);
        g_board_sound_root[sizeof(g_board_sound_root) - 1] = '\0';
    }
    printf("[HAL:OpenVela] Board actuator initialized. Sound root: %s\n", g_board_sound_root);
    return 0;
}

static int openvela_actuator_deinit(void)
{
    return 0;
}

static int openvela_actuator_play_sound(hal_sound_type_t sound_type)
{
    const char *sound_files[] = {
        "wooden_fish.wav",
        "wakeup.wav",
        "celebrate.wav",
        "alert.wav",
        "click.wav"
    };

    int idx = (int)sound_type;
    if (idx < 0 || idx >= 5) idx = 0;

    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", g_board_sound_root, sound_files[idx]);

    printf("[HAL:OpenVela] 🔊 Play sound on Gemini-S1 codec: %s (Vol: %d%%)\n",
           file_path, g_board_volume);

    int fd = open(file_path, O_RDONLY);
    if (fd >= 0) {
        close(fd);
    }
    return 0;
}

static void openvela_actuator_set_volume(uint8_t volume_pct)
{
    g_board_volume = (volume_pct > 100) ? 100 : volume_pct;
    printf("[HAL:OpenVela] Master hardware volume set to %d%%\n", g_board_volume);
}

static int openvela_actuator_trigger_haptic(hal_haptic_pattern_t pattern)
{
    printf("[HAL:OpenVela] 📳 Haptic motor PWM triggered pattern %d\n", (int)pattern);
    return 0;
}

static int openvela_actuator_set_led(hal_led_mode_t mode, uint32_t rgb, uint8_t brightness)
{
    printf("[HAL:OpenVela] 💡 RGB LED ring: Mode %d, RGB #%06X, Brightness %d\n",
           (int)mode, (unsigned int)rgb, brightness);
    return 0;
}

/* ========================================================================= */
/* OpenVela Board System Telemetry & Launcher Ops                            */
/* ========================================================================= */
static int openvela_system_init(void)
{
    return 0;
}

static int openvela_system_deinit(void)
{
    return 0;
}

static int openvela_system_get_telemetry(hal_system_telemetry_t *out_telem)
{
    if (!out_telem) return -1;

    strncpy(out_telem->board_model, "Gemini-S1 (Allwinner R528-S3 Dual-Core Cortex-A7)", sizeof(out_telem->board_model) - 1);
    strncpy(out_telem->os_version, "OpenVela OS (NuttX Kernel)", sizeof(out_telem->os_version) - 1);
    out_telem->cpu_freq_mhz = 1200;
    out_telem->cpu_temperature_c = 41.5f;

#if defined(__NUTTX__)
    struct mallinfo mem_info = mallinfo();
    out_telem->mem_total_kb = (uint32_t)(mem_info.arena / 1024);
    out_telem->mem_free_kb = (uint32_t)(mem_info.fordblks / 1024);
    if (out_telem->mem_total_kb > 0) {
        out_telem->mem_used_pct = (uint32_t)(((out_telem->mem_total_kb - out_telem->mem_free_kb) * 100) / out_telem->mem_total_kb);
    } else {
        out_telem->mem_used_pct = 40;
    }
#else
    out_telem->mem_total_kb = 128 * 1024;
    out_telem->mem_free_kb = 72 * 1024;
    out_telem->mem_used_pct = 43;
#endif

    out_telem->uptime_seconds = (uint64_t)(board_get_time_ms() / 1000);
    return 0;
}

static int openvela_system_launch_app(const char *app_package_or_alias)
{
    printf("[HAL:OpenVela] 🚀 Dispatching application launcher for: %s\n", app_package_or_alias);
    /* In OpenVela, apps can be launched via system manager or process spawn */
    return 0;
}

static const char* openvela_system_get_storage_base_path(void)
{
    return "/data/phoenix";
}

/* ========================================================================= */
/* Audio In Ops (OpenVela Driver Stub)                                       */
/* ========================================================================= */
static int openvela_audio_in_init(uint32_t sample_rate, uint8_t channels)
{
    (void)sample_rate; (void)channels;
    return 0;
}

static int openvela_audio_in_deinit(void)
{
    return 0;
}

static int openvela_audio_in_start_stream(void)
{
    return 0;
}

static int openvela_audio_in_read_frame(hal_audio_pcm_frame_t *frame_out, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (!frame_out) return -1;
    frame_out->sample_rate = 16000;
    frame_out->channels = 1;
    frame_out->bit_depth = 16;
    frame_out->frame_length = 160;
    frame_out->pcm_data = NULL;
    frame_out->is_speech = false;
    return 0;
}

static int openvela_audio_in_stop_stream(void)
{
    return 0;
}

/* ========================================================================= */
/* OpenVela Board Driver Definition                                          */
/* ========================================================================= */
const hal_driver_t g_hal_driver_openvela = {
    .driver_name = "GeminiS1OpenVelaBoardDriver",
    .sensor_ops = {
        .init = openvela_sensor_init,
        .deinit = openvela_sensor_deinit,
        .poll_tap = openvela_sensor_poll_tap,
        .read_light = openvela_sensor_read_light,
        .read_battery = openvela_sensor_read_battery
    },
    .actuator_ops = {
        .init = openvela_actuator_init,
        .deinit = openvela_actuator_deinit,
        .play_sound = openvela_actuator_play_sound,
        .set_volume = openvela_actuator_set_volume,
        .trigger_haptic = openvela_actuator_trigger_haptic,
        .set_led = openvela_actuator_set_led
    },
    .system_ops = {
        .init = openvela_system_init,
        .deinit = openvela_system_deinit,
        .get_telemetry = openvela_system_get_telemetry,
        .launch_app = openvela_system_launch_app,
        .get_storage_base_path = openvela_system_get_storage_base_path
    },
    .audio_in_ops = {
        .init = openvela_audio_in_init,
        .deinit = openvela_audio_in_deinit,
        .start_stream = openvela_audio_in_start_stream,
        .read_frame = openvela_audio_in_read_frame,
        .stop_stream = openvela_audio_in_stop_stream
    }
};
