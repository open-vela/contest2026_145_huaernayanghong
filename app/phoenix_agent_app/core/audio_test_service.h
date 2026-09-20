/**
 * @file audio_test_service.h
 * @brief Audio Diagnostics, Recording & Playback Test Service for Phoenix HoloDesk-S1
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_AUDIO_TEST_SERVICE_H
#define PHOENIX_AUDIO_TEST_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Audio test operational states
 */
typedef enum {
    AUDIO_TEST_STATE_IDLE = 0,       /**< Idle / Standby */
    AUDIO_TEST_STATE_RECORDING,      /**< Actively recording mic input */
    AUDIO_TEST_STATE_PLAYING_REC,    /**< Playing back recorded audio */
    AUDIO_TEST_STATE_PLAYING_TONE    /**< Generating and playing 1000Hz pure test tone */
} audio_test_state_t;

/**
 * @brief Audio test status snapshot
 */
typedef struct {
    audio_test_state_t state;
    bool is_loopback;                /**< Whether realtime mic->speaker loopback is active */
    uint32_t record_duration_ms;     /**< Elapsed record time in ms */
    size_t recorded_bytes;           /**< Accumulated PCM bytes */
    uint8_t current_energy;          /**< Realtime RMS audio energy (0 ~ 100) */
    uint8_t volume_pct;              /**< Current master volume (0 ~ 100) */
} audio_test_status_t;

/**
 * @brief Initialize Audio Test Service
 * @return 0 on success
 */
int audio_test_service_init(void);

/**
 * @brief Start recording from microphone (/dev/audio/pcm0c or HAL)
 * @return 0 on success, negative error code on failure
 */
int audio_test_record_start(void);

/**
 * @brief Stop current recording and build standard WAV package
 * @return 0 on success
 */
int audio_test_record_stop(void);

/**
 * @brief Playback the recorded audio through speaker (/dev/audio/pcm0p or HAL)
 * @return 0 on success
 */
int audio_test_play_record(void);

/**
 * @brief Play a pure sine test tone (e.g. 1000Hz) to test DAC/Speaker path
 * @param freq_hz Frequency in Hz (e.g. 1000)
 * @param duration_ms Duration in milliseconds (e.g. 2000)
 * @return 0 on success
 */
int audio_test_play_tone(uint32_t freq_hz, uint32_t duration_ms);

/**
 * @brief Stop any active playback or tone output
 * @return 0 on success
 */
int audio_test_play_stop(void);

/**
 * @brief Enable or disable realtime mic-to-speaker loopback / ear-return
 * @param enable True to enable loopback
 * @return 0 on success
 */
int audio_test_set_loopback(bool enable);

/**
 * @brief Feed raw PCM frame into test service (for manual feed or ISR)
 * @param samples Int16 PCM samples
 * @param count Number of samples
 */
void audio_test_feed_pcm(const int16_t *samples, size_t count);

/**
 * @brief Get current audio test status snapshot
 * @param out_status Destination status struct
 */
void audio_test_get_status(audio_test_status_t *out_status);

/**
 * @brief Get complete in-memory WAV audio buffer (including 44-byte header)
 * @param out_len Destination pointer to receive byte length
 * @return Pointer to WAV data, or NULL if no recording exists
 */
const uint8_t* audio_test_get_wav_data(size_t *out_len);

/**
 * @brief Service periodic tick (processes ongoing recording, playback & energy decay)
 */
void audio_test_service_tick(void);

/**
 * @brief De-initialize and release Audio Test Service
 */
void audio_test_service_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_AUDIO_TEST_SERVICE_H */
