/**
 * @file demo_test.h
 * @brief 硬件调用测试套件声明
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef __DEMO_TEST_H
#define __DEMO_TEST_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 电源与电量测试用例 */
int test_cmd_power(int argc, char *argv[]);

/* 麦克风状态与控制测试用例 (关闭/静音/开启/VU监测) */
int test_cmd_mic(int argc, char *argv[]);

/* 喇叭声量调节与音频测试用例 */
int test_cmd_volume(int argc, char *argv[]);
int test_cmd_beep(int argc, char *argv[]);

/* 屏幕背光亮度测试用例 */
int test_cmd_brightness(int argc, char *argv[]);
int test_cmd_pattern(int argc, char *argv[]);

/* 传感器与外设测试 */
int test_cmd_imu(int argc, char *argv[]);
int test_cmd_rtc(int argc, char *argv[]);
int test_cmd_rail(int argc, char *argv[]);

/* 全自动综合健康自检 */
int test_cmd_all(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_TEST_H */
