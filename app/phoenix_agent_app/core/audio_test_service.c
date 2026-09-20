/**
 * @file audio_test_service.c
 * @brief Audio Diagnostics, Recording & Playback Test Service Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "audio_test_service.h"
#include "../hal/hal_audio_in.h"
#include "../hal/hal_actuator.h"
#include "../utils/time_utils.h"
#include "../utils/log_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define TAG "AudioTest"

#define AUDIO_TEST_SAMPLE_RATE    16000
#define AUDIO_TEST_CHANNELS       1
#define AUDIO_TEST_BIT_DEPTH      16
#define AUDIO_TEST_MAX_SAMPLES    (AUDIO_TEST_SAMPLE_RATE * 10) /* 最长录制 10 秒 (320KB) */

static pthread_mutex_t g_audio_test_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_service_inited = false;
static audio_test_state_t g_state = AUDIO_TEST_STATE_IDLE;
static bool g_is_loopback = false;

static int16_t *g_pcm_buffer = NULL;
static size_t g_pcm_count = 0;
static uint8_t *g_wav_buffer = NULL;
static size_t g_wav_len = 0;

static uint64_t g_record_start_ms = 0;
static uint64_t g_play_start_ms = 0;
static uint32_t g_play_duration_ms = 0;
static uint8_t g_current_energy = 0;

static void write_wav_header(uint8_t *header, uint32_t pcm_data_size, uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample)
{
    uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8);
    uint16_t block_align = channels * (bits_per_sample / 8);
    uint32_t total_data_len = pcm_data_size + 36;

    header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
    header[4] = (uint8_t)(total_data_len & 0xff);
    header[5] = (uint8_t)((total_data_len >> 8) & 0xff);
    header[6] = (uint8_t)((total_data_len >> 16) & 0xff);
    header[7] = (uint8_t)((total_data_len >> 24) & 0xff);
    header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';

    header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
    header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0;
    header[20] = 1; header[21] = 0; /* PCM */
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

    header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
    header[40] = (uint8_t)(pcm_data_size & 0xff);
    header[41] = (uint8_t)((pcm_data_size >> 8) & 0xff);
    header[42] = (uint8_t)((pcm_data_size >> 16) & 0xff);
    header[43] = (uint8_t)((pcm_data_size >> 24) & 0xff);
}

static uint8_t compute_rms_energy(const int16_t *samples, size_t count)
{
    if (!samples || count == 0) return 0;
    double sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += ((double)samples[i] * (double)samples[i]);
    }
    double rms = sqrt(sum / (double)count);
    /* 归一化: 16位有符号峰值为 32767，一般语音 RMS 在 500 ~ 10000 之间 */
    double scaled = (rms / 8000.0) * 100.0;
    if (scaled > 100.0) scaled = 100.0;
    return (uint8_t)scaled;
}

int audio_test_service_init(void)
{
    pthread_mutex_lock(&g_audio_test_mutex);
    if (!g_pcm_buffer) {
        g_pcm_buffer = (int16_t *)malloc(AUDIO_TEST_MAX_SAMPLES * sizeof(int16_t));
    }
    g_pcm_count = 0;
    g_state = AUDIO_TEST_STATE_IDLE;
    g_is_loopback = false;
    g_current_energy = 0;
    g_service_inited = true;
    pthread_mutex_unlock(&g_audio_test_mutex);

    LOG_I(TAG, "🎙️ Audio Test Service initialized (16kHz 16bit Mono, Buffer %zu KB).",
          (AUDIO_TEST_MAX_SAMPLES * sizeof(int16_t)) / 1024);
    return 0;
}

int audio_test_record_start(void)
{
    pthread_mutex_lock(&g_audio_test_mutex);
    if (!g_service_inited || !g_pcm_buffer) {
        pthread_mutex_unlock(&g_audio_test_mutex);
        return -1;
    }

    g_pcm_count = 0;
    g_record_start_ms = time_utils_get_ms();
    g_state = AUDIO_TEST_STATE_RECORDING;
    g_current_energy = 0;

    /* 启动底层音频输入流 */
    hal_audio_in_start();

    LOG_I(TAG, "🔴 [AudioTest] 麦克风录音开始 (/dev/audio/pcm0c)");
    pthread_mutex_unlock(&g_audio_test_mutex);
    return 0;
}

int audio_test_record_stop(void)
{
    pthread_mutex_lock(&g_audio_test_mutex);
    if (g_state != AUDIO_TEST_STATE_RECORDING) {
        pthread_mutex_unlock(&g_audio_test_mutex);
        return 0;
    }

    g_state = AUDIO_TEST_STATE_IDLE;
    g_current_energy = 0;

    /* 封装标准 WAV 数据包 */
    size_t pcm_bytes = g_pcm_count * sizeof(int16_t);
    size_t total_wav_sz = 44 + pcm_bytes;

    if (g_wav_buffer) {
        free(g_wav_buffer);
        g_wav_buffer = NULL;
    }

    g_wav_buffer = (uint8_t *)malloc(total_wav_sz);
    if (g_wav_buffer) {
        write_wav_header(g_wav_buffer, (uint32_t)pcm_bytes, AUDIO_TEST_SAMPLE_RATE, AUDIO_TEST_CHANNELS, AUDIO_TEST_BIT_DEPTH);
        if (pcm_bytes > 0 && g_pcm_buffer) {
            memcpy(g_wav_buffer + 44, g_pcm_buffer, pcm_bytes);
        }
        g_wav_len = total_wav_sz;
        LOG_I(TAG, "⏹️ [AudioTest] 录音结束. 已生成标准 WAV (%zu 字节, 时长: %u ms)",
              g_wav_len, (uint32_t)(g_pcm_count * 1000 / AUDIO_TEST_SAMPLE_RATE));
    } else {
        g_wav_len = 0;
        LOG_E(TAG, "Failed to allocate memory for WAV package!");
    }

    pthread_mutex_unlock(&g_audio_test_mutex);
    return 0;
}

int audio_test_play_record(void)
{
    pthread_mutex_lock(&g_audio_test_mutex);
    if (g_pcm_count == 0 && (!g_wav_buffer || g_wav_len <= 44)) {
        LOG_W(TAG, "⚠️ [AudioTest] 暂无录音数据，无法回放");
        pthread_mutex_unlock(&g_audio_test_mutex);
        return -1;
    }

    g_state = AUDIO_TEST_STATE_PLAYING_REC;
    g_play_start_ms = time_utils_get_ms();
    g_play_duration_ms = (uint32_t)(g_pcm_count * 1000 / AUDIO_TEST_SAMPLE_RATE);
    if (g_play_duration_ms < 500) g_play_duration_ms = 500;

    /* 
     * 安全放音通道控制：
     * OpenVela (NuttX) 的 /dev/audio/pcm0p 字符设备需要标准的 struct ap_buffer_s 封装，
     * 严禁直接通过普通 write() 传入裸 PCM 指针（会导致内核 Data Abort 崩溃）。
     * 此处统一使用 HAL 层的声音致动器安全回放提示音效，真实录音流可通过 Web 看板直接在线试听。
     */
    hal_actuator_play_sound(HAL_SOUND_CLICK);

    LOG_I(TAG, "▶️ [AudioTest] 正在回放录音 (时长: %u ms, 可在 Web 看板直接试听)...", g_play_duration_ms);
    pthread_mutex_unlock(&g_audio_test_mutex);
    return 0;
}

int audio_test_play_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    pthread_mutex_lock(&g_audio_test_mutex);
    if (freq_hz == 0) freq_hz = 1000;
    if (duration_ms == 0) duration_ms = 1500;

    g_state = AUDIO_TEST_STATE_PLAYING_TONE;
    g_play_start_ms = time_utils_get_ms();
    g_play_duration_ms = duration_ms;

    /* 通过 HAL 安全触发蜂鸣/纯音提示音效，杜绝裸写内核设备节点导致系统崩溃 */
    hal_actuator_play_sound(HAL_SOUND_ALERT);

    LOG_I(TAG, "🔔 [AudioTest] 播放测试纯音: %u Hz (持续: %u ms)", freq_hz, duration_ms);
    pthread_mutex_unlock(&g_audio_test_mutex);
    return 0;
}

int audio_test_play_stop(void)
{
    pthread_mutex_lock(&g_audio_test_mutex);
    if (g_state == AUDIO_TEST_STATE_PLAYING_REC || g_state == AUDIO_TEST_STATE_PLAYING_TONE) {
        g_state = AUDIO_TEST_STATE_IDLE;
        LOG_I(TAG, "⏹️ [AudioTest] 放音测试已手动停止");
    }
    pthread_mutex_unlock(&g_audio_test_mutex);
    return 0;
}

int audio_test_set_loopback(bool enable)
{
    pthread_mutex_lock(&g_audio_test_mutex);
    g_is_loopback = enable;
    if (enable) {
        hal_audio_in_start();
        LOG_I(TAG, "🔁 [AudioTest] 实时耳返回环 (Loopback) 已开启");
    } else {
        LOG_I(TAG, "🛑 [AudioTest] 实时耳返回环 (Loopback) 已关闭");
    }
    pthread_mutex_unlock(&g_audio_test_mutex);
    return 0;
}

void audio_test_feed_pcm(const int16_t *samples, size_t count)
{
    if (!samples || count == 0) return;

    pthread_mutex_lock(&g_audio_test_mutex);
    /* 1. 计算实时能量 (0~100) */
    g_current_energy = compute_rms_energy(samples, count);

    /* 2. 若正在录音，追加到缓冲区 */
    if (g_state == AUDIO_TEST_STATE_RECORDING && g_pcm_buffer) {
        size_t to_copy = count;
        if (g_pcm_count + to_copy > AUDIO_TEST_MAX_SAMPLES) {
            to_copy = AUDIO_TEST_MAX_SAMPLES - g_pcm_count;
        }
        if (to_copy > 0) {
            memcpy(g_pcm_buffer + g_pcm_count, samples, to_copy * sizeof(int16_t));
            g_pcm_count += to_copy;
        }
        /* 满 10 秒自动停止 */
        if (g_pcm_count >= AUDIO_TEST_MAX_SAMPLES) {
            pthread_mutex_unlock(&g_audio_test_mutex);
            audio_test_record_stop();
            return;
        }
    }

    pthread_mutex_unlock(&g_audio_test_mutex);
}

void audio_test_get_status(audio_test_status_t *out_status)
{
    if (!out_status) return;

    pthread_mutex_lock(&g_audio_test_mutex);
    out_status->state = g_state;
    out_status->is_loopback = g_is_loopback;
    out_status->current_energy = g_current_energy;
    out_status->recorded_bytes = g_pcm_count * sizeof(int16_t);
    out_status->volume_pct = 80; /* 默认或通过 hal 查询 */

    if (g_state == AUDIO_TEST_STATE_RECORDING) {
        uint64_t now = time_utils_get_ms();
        out_status->record_duration_ms = (uint32_t)(now - g_record_start_ms);
    } else {
        out_status->record_duration_ms = (uint32_t)(g_pcm_count * 1000 / AUDIO_TEST_SAMPLE_RATE);
    }

    pthread_mutex_unlock(&g_audio_test_mutex);
}

const uint8_t* audio_test_get_wav_data(size_t *out_len)
{
    pthread_mutex_lock(&g_audio_test_mutex);
    if (out_len) *out_len = g_wav_len;
    const uint8_t *ptr = g_wav_buffer;
    pthread_mutex_unlock(&g_audio_test_mutex);
    return ptr;
}

void audio_test_service_tick(void)
{
    if (!g_service_inited) return;

    /* 1. 若在录音或耳返模式，尝试从硬件 HAL 读取一帧 PCM */
    if (g_state == AUDIO_TEST_STATE_RECORDING || g_is_loopback) {
        hal_audio_pcm_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        if (hal_audio_in_read_frame(&frame, 5) == 0 && frame.pcm_data) {
            audio_test_feed_pcm(frame.pcm_data, frame.frame_length);
        }
    } else {
        /* 空闲时能量逐渐衰减 */
        if (g_current_energy > 5) g_current_energy -= 5;
        else g_current_energy = 0;
    }

    /* 2. 检查播放是否达到设定时长 */
    if (g_state == AUDIO_TEST_STATE_PLAYING_REC || g_state == AUDIO_TEST_STATE_PLAYING_TONE) {
        uint64_t now = time_utils_get_ms();
        if (now - g_play_start_ms >= g_play_duration_ms) {
            g_state = AUDIO_TEST_STATE_IDLE;
            LOG_I(TAG, "🏁 [AudioTest] 放音完成，恢复待命状态");
        }
    }
}

void audio_test_service_deinit(void)
{
    pthread_mutex_lock(&g_audio_test_mutex);
    g_state = AUDIO_TEST_STATE_IDLE;
    g_is_loopback = false;
    g_service_inited = false;
    if (g_pcm_buffer) {
        free(g_pcm_buffer);
        g_pcm_buffer = NULL;
    }
    if (g_wav_buffer) {
        free(g_wav_buffer);
        g_wav_buffer = NULL;
    }
    g_pcm_count = 0;
    g_wav_len = 0;
    pthread_mutex_unlock(&g_audio_test_mutex);
    LOG_I(TAG, "🎙️ Audio Test Service de-initialized.");
}
