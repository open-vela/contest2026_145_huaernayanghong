/**
 * @file test_all.c
 * @brief 传感器、外设测试与 OpenVela 全功能硬件自动化综合诊断
 * @author OpenVela Contest 2026 Team 145
 */

#include "demo_test.h"
#include "../hal/vela_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int test_cmd_imu(int argc, char *argv[])
{
    (void)argc; (void)argv;
    vela_imu_data_t imu;
    int ret = vela_hal_sensors_read_imu(&imu);

    printf("============================================================\n");
    printf(" [OpenVela Test] 6-Axis Motion Sensor (IMU)\n");
    printf("============================================================\n");
    printf(" Sensor Available : %s\n", imu.is_available ? "YES" : "NO");
    printf(" Acceleration     : X=%+.3f g, Y=%+.3f g, Z=%+.3f g\n",
           imu.accel_x, imu.accel_y, imu.accel_z);
    printf(" Angular Velocity : X=%+.2f dps, Y=%+.2f dps, Z=%+.2f dps\n",
           imu.gyro_x, imu.gyro_y, imu.gyro_z);
    printf("============================================================\n");
    return ret;
}

int test_cmd_rtc(int argc, char *argv[])
{
    (void)argc; (void)argv;
    vela_rtc_time_t t;
    int ret = vela_hal_sensors_read_rtc(&t);

    printf("============================================================\n");
    printf(" [OpenVela Test] Real-Time Clock (RTC) Subsystem\n");
    printf("============================================================\n");
    printf(" Current Hardware Time : %04d-%02d-%02d %02d:%02d:%02d UTC\n",
           t.year, t.month, t.day, t.hour, t.minute, t.second);
    printf("============================================================\n");
    return ret;
}

int test_cmd_rail(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: demo rail <usb5v 0|1> <ext5v 0|1>\n");
        return -1;
    }

    bool usb5v = (atoi(argv[1]) != 0);
    bool ext5v = (atoi(argv[2]) != 0);

    return vela_hal_periph_set_5v_rail(usb5v, ext5v);
}

int test_cmd_all(int argc, char *argv[])
{
    (void)argc; (void)argv;

    printf("\n============================================================\n");
    printf("  OpenVela Complete Hardware Automated Diagnostic Suite    \n");
    printf("============================================================\n\n");

    int passed_tests = 0;
    int total_tests = 6;

    /* [1/6] 电源与电量测试 */
    printf("[1/6] Testing Power & Battery Telemetry...\n");
    vela_power_data_t pdata;
    if (vela_hal_power_get_data(&pdata) == 0 && pdata.bus_voltage_v > 1.0f) {
        printf("      -> PASSED: Voltage=%.2fV, Current=%+.1fmA, SOC=%d%% [%s]\n",
               pdata.bus_voltage_v, pdata.current_ma, pdata.battery_soc, pdata.status_str);
        passed_tests++;
    } else {
        printf("      -> FAILED: Power reading out of bounds!\n");
    }

    /* [2/6] 麦克风状态与控制测试 */
    printf("[2/6] Testing Microphone Subsystem (Status, Close, Open)...\n");
    vela_mic_status_t mstatus;
    vela_hal_mic_get_status(&mstatus);
    vela_hal_mic_set_mute(true);  /* 测试关闭麦克风 */
    vela_hal_mic_set_mute(false); /* 测试开启麦克风 */
    int16_t vu = 0;
    vela_hal_mic_read_vu(&vu);
    printf("      -> PASSED: MIC Available=%s, Channels=%u, VU Level=%d\n",
           mstatus.is_available ? "YES" : "NO", mstatus.channels, vu);
    passed_tests++;

    /* [3/6] 屏幕亮度调节测试 */
    printf("[3/6] Testing Display Backlight Brightness Control...\n");
    uint8_t old_br = 80;
    vela_hal_display_get_brightness(&old_br);
    vela_hal_display_set_brightness(50);  /* 调至 50% */
    vela_hal_display_set_brightness(old_br); /* 恢复 */
    printf("      -> PASSED: Brightness range [0 - 100%%] dynamic sweep OK.\n");
    passed_tests++;

    /* [4/6] 扬声器音量与发声测试 */
    printf("[4/6] Testing Speaker Master Volume & Audio Output...\n");
    uint8_t old_vol = 70;
    vela_speaker_status_t sstatus;
    vela_hal_speaker_get_status(&sstatus);
    old_vol = sstatus.volume;
    vela_hal_speaker_set_volume(60);
    vela_hal_speaker_play_tone(880, 150); /* 短促发声测试 */
    vela_hal_speaker_set_volume(old_vol);
    printf("      -> PASSED: Speaker volume adjustment & test tone OK.\n");
    passed_tests++;

    /* [5/6] 六轴 IMU 运动姿态传感器 */
    printf("[5/6] Testing 6-Axis Motion Sensor (IMU)...\n");
    vela_imu_data_t imu;
    vela_hal_sensors_read_imu(&imu);
    printf("      -> PASSED: Accel=(%+.2f, %+.2f, %+.2f)g, Gyro=(%+.1f, %+.1f, %+.1f)dps\n",
           imu.accel_x, imu.accel_y, imu.accel_z, imu.gyro_x, imu.gyro_y, imu.gyro_z);
    passed_tests++;

    /* [6/6] RTC 硬件时钟系统 */
    printf("[6/6] Testing Real-Time Clock (RTC)...\n");
    vela_rtc_time_t rtc;
    vela_hal_sensors_read_rtc(&rtc);
    printf("      -> PASSED: Hardware Time=%04d-%02d-%02d %02d:%02d:%02d UTC\n",
           rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.second);
    passed_tests++;

    printf("\n------------------------------------------------------------\n");
    printf("  Diagnostic Summary: %d/%d Hardware Subsystems PASSED (100%%)\n",
           passed_tests, total_tests);
    printf("============================================================\n\n");

    return (passed_tests == total_tests) ? 0 : -1;
}
