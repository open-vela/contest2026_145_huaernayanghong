/**
 * @file vela_hal_display.c
 * @brief 基于 OpenVela 显示框架的屏幕亮度调节与测试图案实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "vela_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#ifdef __NUTTX__
#include <nuttx/config.h>
#include <nuttx/video/fb.h>
#endif

#define OPENVELA_FB_DEV "/dev/fb0"

static uint8_t g_display_brightness = 80;

int vela_hal_display_get_brightness(uint8_t *out_brightness)
{
    if (!out_brightness) return -EINVAL;
    *out_brightness = g_display_brightness;
    return 0;
}

int vela_hal_display_set_brightness(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;
    g_display_brightness = brightness;

    printf("[OpenVela:Display] Set Display Backlight Brightness: %d%%\n", brightness);

#if defined(CONFIG_ARCH_BOARD_M5STACK_TAB5)
    extern int tab5_lcd_set_backlight(uint8_t brightness);
    tab5_lcd_set_backlight(brightness);
#endif

    return 0;
}

int vela_hal_display_test_pattern(int pattern_id)
{
    printf("[OpenVela:Display] Drawing test pattern #%d on /dev/fb0...\n", pattern_id);

    int fd = open(OPENVELA_FB_DEV, O_RDWR);
    if (fd < 0) {
        printf("[OpenVela:Display] Note: /dev/fb0 not available, display test simulated.\n");
        return 0;
    }

#if defined(__NUTTX__) && defined(CONFIG_VIDEO_FB)
    struct fb_videoinfo_s vinfo;
    struct fb_planeinfo_s pinfo;

    if (ioctl(fd, FBIOGET_VIDEOINFO, (unsigned long)&vinfo) < 0 ||
        ioctl(fd, FBIOGET_PLANEINFO, (unsigned long)&pinfo) < 0) {
        printf("[OpenVela:Display] ERROR: Failed to get framebuffer info.\n");
        close(fd);
        return -EIO;
    }

    if (pinfo.fbmem != NULL && vinfo.xres > 0 && vinfo.yres > 0) {
        uint16_t *pixels = (uint16_t *)pinfo.fbmem;
        int width = vinfo.xres;
        int height = vinfo.yres;

        if (pattern_id == 0) {
            /* 1. RGB 彩色条纹 */
            uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x07FF, 0xF81F, 0xFFE0};
            int num_colors = sizeof(colors) / sizeof(colors[0]);
            int bar_w = width / num_colors;

            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    int c_idx = x / bar_w;
                    if (c_idx >= num_colors) c_idx = num_colors - 1;
                    pixels[y * width + x] = colors[c_idx];
                }
            }
        } else if (pattern_id == 1) {
            /* 2. 灰度渐变色阶 */
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    uint8_t gray = (uint8_t)((x * 255) / width);
                    /* RGB565: R(5), G(6), B(5) */
                    uint16_t color = ((gray >> 3) << 11) | ((gray >> 2) << 5) | (gray >> 3);
                    pixels[y * width + x] = color;
                }
            }
        } else {
            /* 3. 网格棋盘线 */
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    bool grid = ((x % 64 == 0) || (y % 64 == 0));
                    pixels[y * width + x] = grid ? 0xFFFF : 0x0000;
                }
            }
        }
        printf("[OpenVela:Display] Pattern #%d rendered successfully (%dx%d).\n",
               pattern_id, width, height);
    }
#endif

    close(fd);
    return 0;
}
