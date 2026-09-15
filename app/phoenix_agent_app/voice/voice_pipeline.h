/**
 * @file voice_pipeline.h
 * @brief Voice Capture, VAD & Pipeline Engine for Phoenix HoloDesk-S1
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_VOICE_PIPELINE_H
#define PHOENIX_VOICE_PIPELINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Voice interaction state machine
 */
typedef enum {
    VOICE_STATE_IDLE = 0,       /**< Silent standby, listening for wake word or physical tap */
    VOICE_STATE_WAKENED,        /**< Wake word detected ("灵眸" / Cyber Eye) */
    VOICE_STATE_LISTENING,      /**< Active speech recording (VAD active) */
    VOICE_STATE_RECOGNIZING,    /**< Speech recognition in progress (ASR) */
    VOICE_STATE_SPEAKING        /**< Agent replying via TTS / audio output */
} phoenix_voice_state_t;

/**
 * @brief Callback when speech recognition produces recognized user text
 */
typedef void (*phoenix_voice_text_cb_t)(const char *recognized_text, void *user_data);

/**
 * @brief Initialize voice pipeline
 * @param on_text_cb Callback to route recognized text
 * @param user_data Context pointer
 * @return 0 on success
 */
int phoenix_voice_pipeline_init(phoenix_voice_text_cb_t on_text_cb, void *user_data);

/**
 * @brief Start continuous voice capture stream
 * @return 0 on success
 */
int phoenix_voice_pipeline_start(void);

/**
 * @brief Feed a PCM audio frame manually into pipeline (for mock testing or ISR)
 * @param pcm_samples Pointer to int16 PCM samples
 * @param sample_count Number of samples (e.g. 160)
 * @param is_speech_active VAD speech presence hint
 * @return 0 on success
 */
int phoenix_voice_pipeline_feed(const int16_t *pcm_samples, size_t sample_count, bool is_speech_active);

/**
 * @brief Poll/process voice pipeline tick (checks audio frames and VAD state transitions)
 */
void phoenix_voice_pipeline_tick(void);

/**
 * @brief Get current voice pipeline state
 */
phoenix_voice_state_t phoenix_voice_pipeline_get_state(void);

/**
 * @brief Stop and de-initialize voice pipeline
 */
void phoenix_voice_pipeline_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_VOICE_PIPELINE_H */
