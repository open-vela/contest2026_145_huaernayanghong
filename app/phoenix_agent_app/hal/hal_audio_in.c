/**
 * @file hal_audio_in.c
 * @brief Phoenix HoloDesk-S1 Audio Input HAL Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "hal_audio_in.h"
#include "hal_driver.h"
#include <stdio.h>

int hal_audio_in_init(uint32_t sample_rate, uint8_t channels)
{
    const hal_driver_t *drv = hal_get_active_driver();
    if (!drv || !drv->audio_in_ops.init) {
        return -1;
    }
    return drv->audio_in_ops.init(sample_rate, channels);
}

int hal_audio_in_start(void)
{
    const hal_driver_t *drv = hal_get_active_driver();
    if (!drv || !drv->audio_in_ops.start_stream) {
        return -1;
    }
    return drv->audio_in_ops.start_stream();
}

int hal_audio_in_read_frame(hal_audio_pcm_frame_t *frame_out, uint32_t timeout_ms)
{
    const hal_driver_t *drv = hal_get_active_driver();
    if (!drv || !drv->audio_in_ops.read_frame) {
        return -1;
    }
    return drv->audio_in_ops.read_frame(frame_out, timeout_ms);
}

int hal_audio_in_stop(void)
{
    const hal_driver_t *drv = hal_get_active_driver();
    if (!drv || !drv->audio_in_ops.stop_stream) {
        return -1;
    }
    return drv->audio_in_ops.stop_stream();
}

int hal_audio_in_deinit(void)
{
    const hal_driver_t *drv = hal_get_active_driver();
    if (!drv || !drv->audio_in_ops.deinit) {
        return -1;
    }
    return drv->audio_in_ops.deinit();
}
