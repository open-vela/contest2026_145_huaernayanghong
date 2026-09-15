/**
 * @file demo_ui.h
 * @brief OpenVela 硬件测试图形控制仪表盘 (基于 LVGL)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef __DEMO_UI_H
#define __DEMO_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 OpenVela 硬件测试图形化仪表盘
 * @return 0: 成功, <0: 失败
 */
int demo_ui_launch(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_UI_H */
