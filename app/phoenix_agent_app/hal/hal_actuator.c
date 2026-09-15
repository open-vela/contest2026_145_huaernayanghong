/**
 * @file hal_actuator.c
 * @brief Phoenix HoloDesk-S1 Actuator HAL Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "hal_actuator.h"
#include "hal_driver.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>

static uint8_t g_current_volume = 80;
static uint64_t g_last_sound_time_ms = 0;
static pthread_mutex_t g_actuator_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint64_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

int hal_actuator_init(const char *sound_data_root)
{
    pthread_mutex_lock(&g_actuator_mutex);
    g_last_sound_time_ms = 0;
    const hal_driver_t *drv = hal_get_active_driver();
    int ret = 0;
    if (drv && drv->actuator_ops.init) {
        ret = drv->actuator_ops.init(sound_data_root);
    }
    pthread_mutex_unlock(&g_actuator_mutex);
    return ret;
}

int hal_actuator_deinit(void)
{
    pthread_mutex_lock(&g_actuator_mutex);
    g_last_sound_time_ms = 0;
    const hal_driver_t *drv = hal_get_active_driver();
    int ret = 0;
    if (drv && drv->actuator_ops.deinit) {
        ret = drv->actuator_ops.deinit();
    }
    pthread_mutex_unlock(&g_actuator_mutex);
    return ret;
}

int hal_actuator_play_sound(hal_sound_type_t sound_type)
{
    pthread_mutex_lock(&g_actuator_mutex);

    uint64_t now = get_time_ms();
    /* Anti-jitter debouncing (minimum 100ms between concurrent sounds) */
    if (now - g_last_sound_time_ms < 100) {
        pthread_mutex_unlock(&g_actuator_mutex);
        return 0;
    }
    g_last_sound_time_ms = now;

    const hal_driver_t *drv = hal_get_active_driver();
    int ret = 0;
    if (drv && drv->actuator_ops.play_sound) {
        ret = drv->actuator_ops.play_sound(sound_type);
    } else {
        /* Fallback console log */
        printf("[HAL:Actuator] Play sound ID %d (Vol: %d%%)\n", (int)sound_type, g_current_volume);
    }

    pthread_mutex_unlock(&g_actuator_mutex);
    return ret;
}

void hal_actuator_set_volume(uint8_t volume_percent)
{
    pthread_mutex_lock(&g_actuator_mutex);
    g_current_volume = (volume_percent > 100) ? 100 : volume_percent;

    const hal_driver_t *drv = hal_get_active_driver();
    if (drv && drv->actuator_ops.set_volume) {
        drv->actuator_ops.set_volume(g_current_volume);
    } else {
        printf("[HAL:Actuator] Volume set to %d%%\n", g_current_volume);
    }
    pthread_mutex_unlock(&g_actuator_mutex);
}

int hal_actuator_trigger_haptic(hal_haptic_pattern_t pattern)
{
    pthread_mutex_lock(&g_actuator_mutex);
    const hal_driver_t *drv = hal_get_active_driver();
    int ret = 0;
    if (drv && drv->actuator_ops.trigger_haptic) {
        ret = drv->actuator_ops.trigger_haptic(pattern);
    } else {
        printf("[HAL:Actuator] 📳 Haptic motor triggered: Pattern %d\n", (int)pattern);
    }
    pthread_mutex_unlock(&g_actuator_mutex);
    return ret;
}

int hal_actuator_set_led(hal_led_mode_t mode, uint32_t rgb, uint8_t brightness)
{
    pthread_mutex_lock(&g_actuator_mutex);
    const hal_driver_t *drv = hal_get_active_driver();
    int ret = 0;
    if (drv && drv->actuator_ops.set_led) {
        ret = drv->actuator_ops.set_led(mode, rgb, brightness);
    } else {
        printf("[HAL:Actuator] 💡 LED Ring: Mode %d, Color 0x%06X, Brightness %d\n",
               (int)mode, (unsigned int)rgb, brightness);
    }
    pthread_mutex_unlock(&g_actuator_mutex);
    return ret;
}
