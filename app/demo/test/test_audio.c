/**
 * @file test_audio.c
 * @brief 扬声器喇叭音量调节、功放控制与测试音发声调用测试
 * @author OpenVela Contest 2026 Team 145
 */

#include "demo_test.h"
#include "../hal/vela_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_cmd_volume(int argc, char *argv[])
{
    if (argc < 2) {
        vela_speaker_status_t status;
        vela_hal_speaker_get_status(&status);

        printf("============================================================\n");
        printf(" [OpenVela Test] Speaker Volume & Audio Output Status\n");
        printf("============================================================\n");
        printf(" Device Available : %s\n", status.is_available ? "YES" : "NO");
        printf(" Current Volume   : %u%%\n", status.volume);
        printf(" Mute Status      : %s\n", status.is_muted ? "MUTED" : "UNMUTED");
        printf(" Power Amp State  : %s\n", status.amplifier_enabled ? "ON" : "OFF");
        printf("============================================================\n");
        return 0;
    }

    if (strcmp(argv[1], "mute") == 0) {
        vela_hal_speaker_set_mute(true);
        printf("[+] Speaker MUTED successfully.\n");
        return 0;
    }

    if (strcmp(argv[1], "unmute") == 0) {
        vela_hal_speaker_set_mute(false);
        printf("[+] Speaker UNMUTED successfully.\n");
        return 0;
    }

    if (strcmp(argv[1], "amp") == 0) {
        bool enable = (argc >= 3 && (strcmp(argv[2], "on") == 0 || strcmp(argv[2], "1") == 0));
        vela_hal_speaker_set_amplifier(enable);
        printf("[+] Speaker Amplifier set to: %s\n", enable ? "ON" : "OFF");
        return 0;
    }

    /* 数字参数：设置音量 0 - 100 */
    int vol = atoi(argv[1]);
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;

    int ret = vela_hal_speaker_set_volume((uint8_t)vol);
    if (ret == 0) {
        printf("[+] Master speaker volume set to %d%% successfully.\n", vol);
    } else {
        printf("[-] Failed to set volume (code: %d)\n", ret);
    }

    return ret;
}

int test_cmd_beep(int argc, char *argv[])
{
    uint32_t freq = 440;       /* 440Hz 标准 A 音 */
    uint32_t duration = 350;   /* 350 毫秒 */

    if (argc >= 2) {
        freq = (uint32_t)atoi(argv[1]);
        if (freq < 100) freq = 100;
        if (freq > 8000) freq = 8000;
    }
    if (argc >= 3) {
        duration = (uint32_t)atoi(argv[2]);
        if (duration < 50) duration = 50;
        if (duration > 3000) duration = 3000;
    }

    printf("[OpenVela Test] Beep test: frequency=%u Hz, duration=%u ms\n",
           (unsigned int)freq, (unsigned int)duration);

    return vela_hal_speaker_play_tone(freq, duration);
}
