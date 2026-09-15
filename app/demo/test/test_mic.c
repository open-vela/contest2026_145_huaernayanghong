/**
 * @file test_mic.c
 * @brief 麦克风状态读取、关闭麦克风(静音)与实时 VU 电平调用测试
 * @author OpenVela Contest 2026 Team 145
 */

#include "demo_test.h"
#include "../hal/vela_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int test_cmd_mic(int argc, char *argv[])
{
    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        vela_mic_status_t status;
        vela_hal_mic_get_status(&status);

        printf("============================================================\n");
        printf(" [OpenVela Test] Microphone Status & Health\n");
        printf("============================================================\n");
        printf(" Device Available : %s\n", status.is_available ? "YES" : "NO");
        printf(" Microphone State : %s\n", status.is_muted ? "MUTED (CLOSED / OFF)" : "ACTIVE (RECORDING / ON)");
        printf(" Sample Rate      : %u Hz\n", (unsigned int)status.sample_rate);
        printf(" Input Channels   : %u\n", status.channels);
        printf(" Hardware Gain    : %u%%\n", status.gain);
        printf("============================================================\n");
        return 0;
    }

    if (strcmp(argv[1], "mute") == 0 || strcmp(argv[1], "off") == 0 || strcmp(argv[1], "close") == 0) {
        printf("[MIC Control] Closing microphone (Hardware Mute)...\n");
        int ret = vela_hal_mic_set_mute(true);
        if (ret == 0) {
            printf("[+] Microphone successfully CLOSED and MUTED.\n");
        } else {
            printf("[-] Failed to close microphone (code: %d)\n", ret);
        }
        return ret;
    }

    if (strcmp(argv[1], "unmute") == 0 || strcmp(argv[1], "on") == 0 || strcmp(argv[1], "open") == 0) {
        printf("[MIC Control] Opening microphone (Hardware Unmute)...\n");
        int ret = vela_hal_mic_set_mute(false);
        if (ret == 0) {
            printf("[+] Microphone successfully OPENED and UNMUTED.\n");
        } else {
            printf("[-] Failed to open microphone (code: %d)\n", ret);
        }
        return ret;
    }

    if (strcmp(argv[1], "vu") == 0) {
        int duration_sec = 10;
        if (argc >= 3) {
            duration_sec = atoi(argv[2]);
            if (duration_sec <= 0) duration_sec = 5;
        }

        printf("============================================================\n");
        printf(" [OpenVela Test] Live Microphone VU Meter Monitoring (%ds)\n", duration_sec);
        printf(" Speak into microphone or clap to verify level fluctuation\n");
        printf("============================================================\n");

        for (int i = 0; i < duration_sec * 5; i++) {
            int16_t vu = 0;
            vela_hal_mic_read_vu(&vu);

            int bar_len = vu / 150;
            if (bar_len > 35) bar_len = 35;

            char bar[40] = {0};
            for (int b = 0; b < bar_len; b++) {
                bar[b] = '#';
            }

            printf("\r VU: [%-35s] %5d", bar, vu);
            fflush(stdout);
            usleep(200000); /* 200ms 刷新 */
        }
        printf("\n[+] VU meter monitoring finished.\n");
        return 0;
    }

    printf("Usage:\n");
    printf("  demo mic status         - Show microphone device status\n");
    printf("  demo mic mute | off     - Close microphone (hardware mute)\n");
    printf("  demo mic unmute | on    - Open microphone (hardware unmute)\n");
    printf("  demo mic vu [sec]       - Live VU audio level meter monitoring\n");
    return -1;
}
