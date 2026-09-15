/**
 * @file test_display.c
 * @brief 屏幕背光亮度调整、获取与测试图案显示调用测试
 * @author OpenVela Contest 2026 Team 145
 */

#include "demo_test.h"
#include "../hal/vela_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_cmd_brightness(int argc, char *argv[])
{
    if (argc < 2 || strcmp(argv[1], "get") == 0) {
        uint8_t br = 0;
        vela_hal_display_get_brightness(&br);

        printf("============================================================\n");
        printf(" [OpenVela Test] Display Backlight Brightness Status\n");
        printf("============================================================\n");
        printf(" Current Brightness : %u%%\n", br);
        printf(" Backlight Power    : %s\n", (br > 0) ? "ON" : "OFF");
        printf("============================================================\n");
        return 0;
    }

    int target = atoi(argv[1]);
    if (target < 0) target = 0;
    if (target > 100) target = 100;

    int ret = vela_hal_display_set_brightness((uint8_t)target);
    if (ret == 0) {
        printf("[+] Display backlight brightness adjusted to %d%% successfully.\n", target);
    } else {
        printf("[-] Failed to adjust display brightness (code: %d)\n", ret);
    }

    return ret;
}

int test_cmd_pattern(int argc, char *argv[])
{
    int pattern_id = 0;
    if (argc >= 2) {
        pattern_id = atoi(argv[1]);
    }

    printf("============================================================\n");
    printf(" [OpenVela Test] Rendering display test pattern #%d\n", pattern_id);
    printf(" (0: RGB Bars, 1: Grayscale Gradient, 2: Grid Checkboard)\n");
    printf("============================================================\n");

    return vela_hal_display_test_pattern(pattern_id);
}
