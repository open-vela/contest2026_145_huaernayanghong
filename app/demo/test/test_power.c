/**
 * @file test_power.c
 * @brief 电源电量、电压电流与充放电调用测试
 * @author OpenVela Contest 2026 Team 145
 */

#include "demo_test.h"
#include "../hal/vela_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int test_cmd_power(int argc, char *argv[])
{
    int samples = 1;
    if (argc >= 2) {
        samples = atoi(argv[1]);
        if (samples <= 0) samples = 1;
        if (samples > 30) samples = 30;
    }

    printf("============================================================\n");
    printf(" [OpenVela Test] Power & Battery Telemetry Subsystem\n");
    printf("============================================================\n");

    for (int i = 0; i < samples; i++) {
        vela_power_data_t pdata;
        int ret = vela_hal_power_get_data(&pdata);
        if (ret < 0) {
            printf("[-] Error reading power telemetry (code: %d)\n", ret);
            return ret;
        }

        printf("[%02d] Bus Voltage   : %.3f V\n", i + 1, pdata.bus_voltage_v);
        printf("     Shunt Voltage : %.3f mV\n", pdata.shunt_voltage_mv);
        printf("     Current Draw  : %+.1f mA (%s)\n",
               pdata.current_ma,
               (pdata.current_ma < -10.0f) ? "CHARGING" : "DISCHARGING");
        printf("     Active Power  : %.2f mW\n", pdata.power_mw);
        printf("     Battery SOC   : %d%% [%s%s]\n",
               pdata.battery_soc,
               pdata.status_str,
               pdata.is_online ? ", AC Online" : "");

        if (i + 1 < samples) {
            usleep(500000); /* 500ms 间隔 */
        }
    }

    printf("[+] Power and battery telemetry verification PASSED.\n");
    return 0;
}
