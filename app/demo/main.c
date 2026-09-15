/**
 * @file main.c
 * @brief OpenVela 硬件功能调用测试应用主入口 (CLI & GUI 双模)
 * @author OpenVela Contest 2026 Team 145
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hal/vela_hal.h"
#include "test/demo_test.h"
#include "ui/demo_ui.h"

static void print_banner(void)
{
    printf("\n");
    printf("==============================================================\n");
    printf("     OpenVela Hardware Diagnostic & Telemetry App (demo)      \n");
    printf("           2026 OpenVela AI Hardware Contest Team 145         \n");
    printf("==============================================================\n");
}

static void print_usage(const char *progname)
{
    print_banner();
    printf("用法 (Usage):\n");
    printf("  %s <command> [arguments]\n\n", progname);
    printf("硬件功能调用测试指令集:\n");
    printf("  power [count]          - 测量电源电量、总线电压、实时电流与功耗 (SOC%%)\n");
    printf("  mic status             - 查询麦克风状态、采样率与拾音通道\n");
    printf("  mic mute | off         - 关闭麦克风 (硬件静音保护)\n");
    printf("  mic unmute | on        - 开启麦克风 (恢复拾音通道)\n");
    printf("  mic vu [sec]           - 实时监测麦克风拾音音量跳动 (VU 动态柱状图)\n");
    printf("  brightness <0-100|get> - 调节或查询屏幕背光亮度\n");
    printf("  volume <0-100>         - 调节扬声器主音量百分比\n");
    printf("  volume mute | unmute   - 扬声器静音或恢复发声\n");
    printf("  volume amp <on|off>    - 开启或关闭扬声器功放电源\n");
    printf("  beep [freq] [ms]       - 扬声器发声测试 (默认 440Hz, 350ms)\n");
    printf("  pattern [0|1|2]        - 屏幕测试 (0: RGB彩条, 1: 渐变色阶, 2: 网格)\n");
    printf("  imu                    - 读取六轴姿态传感器 (三轴加速度与三轴陀螺仪)\n");
    printf("  rtc                    - 读取硬件 Real-Time Clock 实时时钟\n");
    printf("  rail <usb 0|1> <ext>   - 控制 USB-A 5V 与 EXT 5V 外设供电轨道\n");
    printf("  all                    - 运行全功能硬件全自动化综合自检\n");
    printf("  gui                    - 启动触摸屏图形化卡片仪表盘 (LVGL 仪表)\n");
    printf("==============================================================\n\n");
}

int main(int argc, char *argv[])
{
    /* 初始化 OpenVela 硬件抽象层 */
    vela_hal_init();

    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    } else if (strcmp(cmd, "power") == 0) {
        return test_cmd_power(argc - 1, &argv[1]);
    } else if (strcmp(cmd, "mic") == 0) {
        return test_cmd_mic(argc - 1, &argv[1]);
    } else if (strcmp(cmd, "brightness") == 0) {
        return test_cmd_brightness(argc - 1, &argv[1]);
    } else if (strcmp(cmd, "volume") == 0) {
        return test_cmd_volume(argc - 1, &argv[1]);
    } else if (strcmp(cmd, "beep") == 0) {
        return test_cmd_beep(argc - 1, &argv[1]);
    } else if (strcmp(cmd, "pattern") == 0) {
        return test_cmd_pattern(argc - 1, &argv[1]);
    } else if (strcmp(cmd, "imu") == 0) {
        return test_cmd_imu(argc - 1, &argv[1]);
    } else if (strcmp(cmd, "rtc") == 0) {
        return test_cmd_rtc(argc - 1, &argv[1]);
    } else if (strcmp(cmd, "rail") == 0) {
        return test_cmd_rail(argc - 1, &argv[1]);
    } else if (strcmp(cmd, "all") == 0) {
        return test_cmd_all(argc - 1, &argv[1]);
    } else if (strcmp(cmd, "gui") == 0) {
        print_banner();
        printf("[OpenVela] Launching Touchscreen GUI Diagnostic Dashboard...\n");
        return demo_ui_launch();
    } else {
        printf("[-] 未知指令: '%s'\n", cmd);
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}
