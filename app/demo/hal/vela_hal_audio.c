/**
 * @file vela_hal_audio.c
 * @brief 基于 OpenVela 音频子系统的麦克风控制与喇叭声量调节实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "vela_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <sys/ioctl.h>

#ifdef __NUTTX__
#include <nuttx/config.h>
#include <nuttx/audio/audio.h>
#endif

/* OpenVela 标准音频设备路径 */
#define OPENVELA_AUDIO_DEV_PLAY   "/dev/audio/pcm0p"
#define OPENVELA_AUDIO_DEV_REC    "/dev/audio/pcm0c"
#define OPENVELA_AUDIO_DEV_DSP    "/dev/audio/dsp0"

/* 全局音频状态记录 */
static uint8_t g_speaker_volume = 70;
static bool g_speaker_muted = false;
static bool g_speaker_amp_on = true;

static bool g_mic_muted = false;
static uint8_t g_mic_gain = 80;

/* ========================================================================= */
/* 麦克风接口实现                                                             */
/* ========================================================================= */

int vela_hal_mic_get_status(vela_mic_status_t *out_status)
{
    if (!out_status) return -EINVAL;

    out_status->is_available = true;
    out_status->is_muted = g_mic_muted;
    out_status->sample_rate = 16000;
    out_status->channels = 2; /* 双麦 */
    out_status->gain = g_mic_gain;
    out_status->vu_level = 0;

    /* 尝试从 OpenVela 音频输入设备获取状态 */
    int fd = open(OPENVELA_AUDIO_DEV_REC, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fd = open(OPENVELA_AUDIO_DEV_DSP, O_RDONLY | O_NONBLOCK);
    }

    if (fd >= 0) {
#if defined(__NUTTX__) && defined(AUDIOIOC_GETMUTE)
        bool muted = false;
        if (ioctl(fd, AUDIOIOC_GETMUTE, (unsigned long)&muted) == 0) {
            out_status->is_muted = muted;
            g_mic_muted = muted;
        }
#endif
        close(fd);
    }

    return 0;
}

int vela_hal_mic_set_mute(bool mute)
{
    g_mic_muted = mute;
    printf("[OpenVela:Audio] Microphone %s (Mute: %s)\n",
           mute ? "MUTED / DISABLED" : "UNMUTED / ACTIVE",
           mute ? "true" : "false");

    /* 优先使用 OpenVela 标准 Audio IOCTL 设置静音 */
    int fd = open(OPENVELA_AUDIO_DEV_REC, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        fd = open(OPENVELA_AUDIO_DEV_DSP, O_RDWR | O_NONBLOCK);
    }

    if (fd >= 0) {
#if defined(__NUTTX__) && defined(AUDIOIOC_SETMUTE)
        ioctl(fd, AUDIOIOC_SETMUTE, (unsigned long)mute);
#endif
        close(fd);
    }

    return 0;
}

int vela_hal_mic_read_vu(int16_t *out_level)
{
    if (!out_level) return -EINVAL;

    /* 如果麦克风已被关闭/静音，直接返回 0 电平 */
    if (g_mic_muted) {
        *out_level = 0;
        return 0;
    }

    /* 尝试从 OpenVela 音频录音设备采样一帧数据计算有效值 (RMS) */
    int fd = open(OPENVELA_AUDIO_DEV_REC, O_RDONLY | O_NONBLOCK);
    if (fd >= 0) {
        int16_t pcm_buf[64];
        ssize_t n = read(fd, pcm_buf, sizeof(pcm_buf));
        close(fd);

        if (n > 0) {
            int samples = n / sizeof(int16_t);
            int32_t sum = 0;
            for (int i = 0; i < samples; i++) {
                sum += abs(pcm_buf[i]);
            }
            *out_level = (int16_t)(sum / samples);
            return 0;
        }
    }

    /* 若暂无音频硬件驱动流输入，生成环境轻微底噪模拟电平用于测试 */
    static int16_t sim_wave = 300;
    sim_wave = (sim_wave + 170) % 3800;
    *out_level = 400 + sim_wave;
    return 0;
}

/* ========================================================================= */
/* 扬声器喇叭声量接口实现                                                     */
/* ========================================================================= */

int vela_hal_speaker_get_status(vela_speaker_status_t *out_status)
{
    if (!out_status) return -EINVAL;

    out_status->is_available = true;
    out_status->is_muted = g_speaker_muted;
    out_status->volume = g_speaker_volume;
    out_status->amplifier_enabled = g_speaker_amp_on;

    int fd = open(OPENVELA_AUDIO_DEV_PLAY, O_RDONLY | O_NONBLOCK);
    if (fd >= 0) {
#if defined(__NUTTX__) && defined(AUDIOIOC_GETVOLUME)
        uint16_t vol = 0;
        if (ioctl(fd, AUDIOIOC_GETVOLUME, (unsigned long)&vol) == 0) {
            /* OpenVela volume: 0 - 1000 */
            out_status->volume = (uint8_t)(vol / 10);
            g_speaker_volume = out_status->volume;
        }
#endif
        close(fd);
    }

    return 0;
}

int vela_hal_speaker_set_volume(uint8_t volume_pct)
{
    if (volume_pct > 100) volume_pct = 100;
    g_speaker_volume = volume_pct;

    printf("[OpenVela:Audio] Set Master Speaker Volume: %d%%\n", volume_pct);

    int fd = open(OPENVELA_AUDIO_DEV_PLAY, O_RDWR | O_NONBLOCK);
    if (fd >= 0) {
#if defined(__NUTTX__) && defined(AUDIOIOC_SETVOLUME)
        uint16_t vol = (uint16_t)volume_pct * 10; /* 0 - 1000 */
        ioctl(fd, AUDIOIOC_SETVOLUME, (unsigned long)vol);
#endif
        close(fd);
    }

    return 0;
}

int vela_hal_speaker_set_mute(bool mute)
{
    g_speaker_muted = mute;
    printf("[OpenVela:Audio] Speaker output %s\n", mute ? "MUTED" : "UNMUTED");

    int fd = open(OPENVELA_AUDIO_DEV_PLAY, O_RDWR | O_NONBLOCK);
    if (fd >= 0) {
#if defined(__NUTTX__) && defined(AUDIOIOC_SETMUTE)
        ioctl(fd, AUDIOIOC_SETMUTE, (unsigned long)mute);
#endif
        close(fd);
    }

    return 0;
}

int vela_hal_speaker_set_amplifier(bool enable)
{
    g_speaker_amp_on = enable;
    printf("[OpenVela:Audio] Speaker Amplifier NS4150B: %s\n", enable ? "POWER ON" : "POWER OFF");

#if defined(CONFIG_ARCH_BOARD_M5STACK_TAB5)
    extern int tab5_io_expander_set_speaker(bool enable);
    tab5_io_expander_set_speaker(enable);
#endif

    return 0;
}

int vela_hal_speaker_play_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    printf("[OpenVela:Audio] 🎵 Playing test tone: %u Hz for %u ms (Vol: %d%%)\n",
           (unsigned int)freq_hz, (unsigned int)duration_ms, g_speaker_volume);

    if (g_speaker_muted || g_speaker_volume == 0) {
        printf("[OpenVela:Audio] Note: Speaker is currently muted or volume is 0\n");
        return 0;
    }

    int fd = open(OPENVELA_AUDIO_DEV_PLAY, O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        /* 生成简单正弦波数据输出 */
        const int sample_rate = 16000;
        int total_samples = (sample_rate * duration_ms) / 1000;
        int16_t chunk[256];
        int remaining = total_samples;
        int phase = 0;

        while (remaining > 0) {
            int to_write = (remaining > 256) ? 256 : remaining;
            for (int i = 0; i < to_write; i++) {
                float t = (float)(phase + i) / (float)sample_rate;
                chunk[i] = (int16_t)(sinf(2.0f * 3.14159f * freq_hz * t) * 8000.0f * (g_speaker_volume / 100.0f));
            }
            write(fd, chunk, to_write * sizeof(int16_t));
            remaining -= to_write;
            phase += to_write;
            usleep(5000);
        }
        close(fd);
    } else {
        /* 系统无 PCM 设备节点时的模拟发声提示 */
        usleep(duration_ms * 1000);
    }

    return 0;
}
