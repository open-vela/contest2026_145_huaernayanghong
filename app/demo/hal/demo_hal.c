/**
 * @file demo_hal.c
 * @brief M5Stack Tab5 HAL 总线管理与系统初始化实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "demo_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __NUTTX__
#include <nuttx/config.h>
#include <nuttx/i2c/i2c_master.h>
#endif

#define TAB5_I2C0_BUS_FREQ 400000
#define INA226_ADDR        0x41
#define PI4IOE1_ADDR       0x43
#define PI4IOE2_ADDR       0x44
#define ES8388_ADDR        0x10
#define ES7210_ADDR        0x40
#define BMI270_ADDR        0x68
#define RX8130_ADDR        0x32

#ifdef __NUTTX__
static struct i2c_master_s *g_i2c0 = NULL;
#endif
static bool g_hal_initialized = false;

/* 通用 I2C 寄存器写入辅助函数 */
int demo_hal_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len)
{
#if defined(__NUTTX__) && defined(CONFIG_ESPRESSIF_I2C0)
    if (!g_i2c0) return -ENODEV;

    uint8_t buf[len + 1];
    buf[0] = reg;
    if (len > 0 && data) {
        memcpy(&buf[1], data, len);
    }

    struct i2c_msg_s msg = {
        .frequency = TAB5_I2C0_BUS_FREQ,
        .addr      = addr,
        .flags     = 0,
        .buffer    = buf,
        .length    = len + 1
    };

    return I2C_TRANSFER(g_i2c0, &msg, 1);
#else
    (void)addr; (void)reg; (void)data; (void)len;
    return 0;
#endif
}

/* 通用 I2C 寄存器读取辅助函数 */
int demo_hal_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
#if defined(__NUTTX__) && defined(CONFIG_ESPRESSIF_I2C0)
    if (!g_i2c0 || !data || len == 0) return -ENODEV;

    struct i2c_msg_s msgs[2] = {
        {
            .frequency = TAB5_I2C0_BUS_FREQ,
            .addr      = addr,
            .flags     = 0,
            .buffer    = &reg,
            .length    = 1
        },
        {
            .frequency = TAB5_I2C0_BUS_FREQ,
            .addr      = addr,
            .flags     = I2C_M_READ,
            .buffer    = data,
            .length    = len
        }
    };

    return I2C_TRANSFER(g_i2c0, msgs, 2);
#else
    (void)addr; (void)reg; (void)data; (void)len;
    return 0;
#endif
}

int demo_hal_init(void)
{
    if (g_hal_initialized) {
        return 0;
    }

    printf("[HAL] Initializing Tab5 Hardware Abstraction Layer...\n");

#if defined(__NUTTX__) && defined(CONFIG_ESPRESSIF_I2C0)
    extern struct i2c_master_s *esp_i2cbus_initialize(int port);
    g_i2c0 = esp_i2cbus_initialize(0);
    if (!g_i2c0) {
        printf("[HAL] WARNING: Failed to initialize ESP32-P4 I2C0 bus!\n");
    } else {
        printf("[HAL] ESP32-P4 I2C0 bus attached at 400kHz (SDA=31, SCL=32).\n");
    }
#endif

    g_hal_initialized = true;
    return 0;
}
