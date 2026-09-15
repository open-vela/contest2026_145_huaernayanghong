/**
 * @file voice_pipeline.c
 * @brief Voice Capture, VAD & Pipeline Engine Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "voice_pipeline.h"
#include "../hal/hal_audio_in.h"
#include "../core/event_bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static phoenix_voice_state_t g_voice_state = VOICE_STATE_IDLE;
static phoenix_voice_text_cb_t g_text_cb = NULL;
static void *g_cb_user_data = NULL;
static bool g_pipeline_active = false;
static uint32_t g_speech_frames_count = 0;
static uint32_t g_silence_frames_count = 0;

int phoenix_voice_pipeline_init(phoenix_voice_text_cb_t on_text_cb, void *user_data)
{
    g_text_cb = on_text_cb;
    g_cb_user_data = user_data;
    g_voice_state = VOICE_STATE_IDLE;
    g_pipeline_active = false;
    g_speech_frames_count = 0;
    g_silence_frames_count = 0;

    hal_audio_in_init(16000, 1);
    printf("[PhoenixVoice] 🎙️ Voice Pipeline initialized (16kHz 16-bit Mono).\n");
    return 0;
}

int phoenix_voice_pipeline_start(void)
{
    g_pipeline_active = true;
    hal_audio_in_start();
    g_voice_state = VOICE_STATE_IDLE;
    printf("[PhoenixVoice] 🎙️ Voice Pipeline streaming started.\n");
    return 0;
}

int phoenix_voice_pipeline_feed(const int16_t *pcm_samples, size_t sample_count, bool is_speech_active)
{
    (void)pcm_samples;
    (void)sample_count;

    if (!g_pipeline_active) {
        return -1;
    }

    if (is_speech_active) {
        g_speech_frames_count++;
        g_silence_frames_count = 0;

        if (g_voice_state == VOICE_STATE_IDLE) {
            g_voice_state = VOICE_STATE_WAKENED;
            printf("[PhoenixVoice] 🔊 Wake word / Speech energy detected! State -> WAKENED\n");
            g_voice_state = VOICE_STATE_LISTENING;
        }
    } else {
        if (g_voice_state == VOICE_STATE_LISTENING) {
            g_silence_frames_count++;
            /* After 3 consecutive silence frames, end of utterance reached */
            if (g_silence_frames_count >= 3) {
                g_voice_state = VOICE_STATE_RECOGNIZING;
                printf("[PhoenixVoice] 🔇 Utterance end detected. State -> RECOGNIZING\n");

                /* Synthesize recognized utterance */
                const char *simulated_text = "灵眸敲木鱼";
                if (g_speech_frames_count > 10) {
                    simulated_text = "灵眸，帮我巡检系统状态！";
                }

                if (g_text_cb) {
                    g_text_cb(simulated_text, g_cb_user_data);
                }

                g_voice_state = VOICE_STATE_IDLE;
                g_speech_frames_count = 0;
                g_silence_frames_count = 0;
            }
        }
    }

    return 0;
}

void phoenix_voice_pipeline_tick(void)
{
    if (!g_pipeline_active) return;

    hal_audio_pcm_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    if (hal_audio_in_read_frame(&frame, 10) == 0) {
        phoenix_voice_pipeline_feed(frame.pcm_data, frame.frame_length, frame.is_speech);
    }
}

phoenix_voice_state_t phoenix_voice_pipeline_get_state(void)
{
    return g_voice_state;
}

void phoenix_voice_pipeline_deinit(void)
{
    hal_audio_in_stop();
    hal_audio_in_deinit();
    g_pipeline_active = false;
    g_voice_state = VOICE_STATE_IDLE;
    g_text_cb = NULL;
    g_cb_user_data = NULL;
}
