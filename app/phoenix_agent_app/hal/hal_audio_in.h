/**
 * @file hal_audio_in.h
 * @brief Phoenix HoloDesk-S1 Audio Input HAL Public Interface
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef HAL_AUDIO_IN_H
#define HAL_AUDIO_IN_H

#include "hal_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化音频输入子系统 (默认 16kHz 16-bit 单声道) */
int hal_audio_in_init(uint32_t sample_rate, uint8_t channels);

/** 开启麦克风音频采集流 */
int hal_audio_in_start(void);

/** 从采集队列读取一帧 PCM 音频数据 */
int hal_audio_in_read_frame(hal_audio_pcm_frame_t *frame_out, uint32_t timeout_ms);

/** 停止麦克风音频流 */
int hal_audio_in_stop(void);

/** 析构音频输入子系统 */
int hal_audio_in_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_AUDIO_IN_H */
