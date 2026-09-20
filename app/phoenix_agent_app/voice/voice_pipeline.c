/**
 * @file voice_pipeline.c
 * @brief Voice Capture, VAD & Pipeline Engine Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "voice_pipeline.h"
#include "../hal/hal_audio_in.h"
#include "../harness/asr_provider.h"
#include "../utils/log_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "PhoenixVoice"

#define MAX_SPEECH_SAMPLES (16000 * 6) /* 最多缓冲 6 秒音频 (16kHz 16-bit Mono = 192KB) */

static phoenix_voice_state_t g_voice_state = VOICE_STATE_IDLE;
static phoenix_voice_text_cb_t g_text_cb = NULL;
static void *g_cb_user_data = NULL;
static bool g_pipeline_active = false;
static uint32_t g_speech_frames_count = 0;
static uint32_t g_silence_frames_count = 0;

static int16_t *g_speech_pcm_buf = NULL;
static size_t g_speech_pcm_count = 0;

static void write_wav_header(uint8_t *header, uint32_t pcm_data_size, uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample)
{
    uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8);
    uint16_t block_align = channels * (bits_per_sample / 8);
    uint32_t total_data_len = pcm_data_size + 36;

    /* RIFF chunk descriptor */
    header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
    header[4] = (uint8_t)(total_data_len & 0xff);
    header[5] = (uint8_t)((total_data_len >> 8) & 0xff);
    header[6] = (uint8_t)((total_data_len >> 16) & 0xff);
    header[7] = (uint8_t)((total_data_len >> 24) & 0xff);
    header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';

    /* "fmt " sub-chunk */
    header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
    header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0; /* Subchunk1Size (16 for PCM) */
    header[20] = 1; header[21] = 0; /* AudioFormat (1 = PCM) */
    header[22] = (uint8_t)(channels & 0xff);
    header[23] = (uint8_t)((channels >> 8) & 0xff);
    header[24] = (uint8_t)(sample_rate & 0xff);
    header[25] = (uint8_t)((sample_rate >> 8) & 0xff);
    header[26] = (uint8_t)((sample_rate >> 16) & 0xff);
    header[27] = (uint8_t)((sample_rate >> 24) & 0xff);
    header[28] = (uint8_t)(byte_rate & 0xff);
    header[29] = (uint8_t)((byte_rate >> 8) & 0xff);
    header[30] = (uint8_t)((byte_rate >> 16) & 0xff);
    header[31] = (uint8_t)((byte_rate >> 24) & 0xff);
    header[32] = (uint8_t)(block_align & 0xff);
    header[33] = (uint8_t)((block_align >> 8) & 0xff);
    header[34] = (uint8_t)(bits_per_sample & 0xff);
    header[35] = (uint8_t)((bits_per_sample >> 8) & 0xff);

    /* "data" sub-chunk */
    header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
    header[40] = (uint8_t)(pcm_data_size & 0xff);
    header[41] = (uint8_t)((pcm_data_size >> 8) & 0xff);
    header[42] = (uint8_t)((pcm_data_size >> 16) & 0xff);
    header[43] = (uint8_t)((pcm_data_size >> 24) & 0xff);
}

int phoenix_voice_pipeline_init(phoenix_voice_text_cb_t on_text_cb, void *user_data)
{
    g_text_cb = on_text_cb;
    g_cb_user_data = user_data;
    g_voice_state = VOICE_STATE_IDLE;
    g_pipeline_active = false;
    g_speech_frames_count = 0;
    g_silence_frames_count = 0;
    g_speech_pcm_count = 0;

    if (!g_speech_pcm_buf) {
        g_speech_pcm_buf = (int16_t *)malloc(MAX_SPEECH_SAMPLES * sizeof(int16_t));
    }

    hal_audio_in_init(16000, 1);
    LOG_I(TAG, "🎙️ Voice Pipeline initialized (16kHz 16-bit Mono, Buffer %zu KB).",
          (MAX_SPEECH_SAMPLES * sizeof(int16_t)) / 1024);
    return 0;
}

int phoenix_voice_pipeline_start(void)
{
    g_pipeline_active = true;
    hal_audio_in_start();
    g_voice_state = VOICE_STATE_IDLE;
    g_speech_pcm_count = 0;
    LOG_I(TAG, "🎙️ Voice Pipeline streaming started.");
    return 0;
}

int phoenix_voice_pipeline_feed(const int16_t *pcm_samples, size_t sample_count, bool is_speech_active)
{
    if (!g_pipeline_active) {
        return -1;
    }

    if (is_speech_active) {
        g_speech_frames_count++;
        g_silence_frames_count = 0;

        if (g_voice_state == VOICE_STATE_IDLE) {
            g_voice_state = VOICE_STATE_WAKENED;
            LOG_I(TAG, "🔊 Wake word / Speech energy detected! State -> WAKENED");
            g_voice_state = VOICE_STATE_LISTENING;
            g_speech_pcm_count = 0;
        }

        /* 缓存语音 PCM 采样数据 */
        if (g_speech_pcm_buf && sample_count > 0) {
            size_t to_copy = sample_count;
            if (g_speech_pcm_count + to_copy > MAX_SPEECH_SAMPLES) {
                to_copy = MAX_SPEECH_SAMPLES - g_speech_pcm_count;
            }
            if (to_copy > 0) {
                if (pcm_samples) {
                    memcpy(g_speech_pcm_buf + g_speech_pcm_count, pcm_samples, to_copy * sizeof(int16_t));
                } else {
                    memset(g_speech_pcm_buf + g_speech_pcm_count, 0, to_copy * sizeof(int16_t));
                }
                g_speech_pcm_count += to_copy;
            }
        }
    } else {
        if (g_voice_state == VOICE_STATE_LISTENING) {
            g_silence_frames_count++;
            /* 连续 3 帧静音判定为单句话结束，触发 ASR 转写 */
            if (g_silence_frames_count >= 3) {
                g_voice_state = VOICE_STATE_RECOGNIZING;
                LOG_I(TAG, "🔇 Utterance end detected (%zu samples). State -> RECOGNIZING", g_speech_pcm_count);

                /* 封装标准 44 字节 WAV 头并调用 ASR Provider 转写 */
                size_t pcm_bytes = g_speech_pcm_count * sizeof(int16_t);
                size_t wav_total_sz = 44 + pcm_bytes;
                uint8_t *wav_buf = (uint8_t *)malloc(wav_total_sz);
                char recognized_text[512] = {0};

                if (wav_buf) {
                    write_wav_header(wav_buf, (uint32_t)pcm_bytes, 16000, 1, 16);
                    if (pcm_bytes > 0 && g_speech_pcm_buf) {
                        memcpy(wav_buf + 44, g_speech_pcm_buf, pcm_bytes);
                    }
                    phoenix_asr_transcribe(wav_buf, wav_total_sz, recognized_text, sizeof(recognized_text));
                    free(wav_buf);
                } else {
                    LOG_E(TAG, "Failed to allocate memory for WAV packaging");
                }

                if (recognized_text[0] != '\0' && g_text_cb) {
                    LOG_I(TAG, "🎯 分发识别文本至 Agent Core: \"%s\"", recognized_text);
                    g_text_cb(recognized_text, g_cb_user_data);
                }

                g_voice_state = VOICE_STATE_IDLE;
                g_speech_frames_count = 0;
                g_silence_frames_count = 0;
                g_speech_pcm_count = 0;
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
    if (g_speech_pcm_buf) {
        free(g_speech_pcm_buf);
        g_speech_pcm_buf = NULL;
    }
    g_speech_pcm_count = 0;
}

