/**
 * @file ble_prov_main.c
 * @brief 独立蓝牙配网与智能体配置命令行工具 (NSH Builtin: ble_prov)
 * @author OpenVela Contest 2026 Team 145
 */

#include "ble_prov_service.h"
#include "network_mgr.h"
#include "../core/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static void print_usage(void)
{
    printf("====================================================\n");
    printf(" ⚡ Phoenix BLE Provisioning CLI Tool (双向通信)\n");
    printf("====================================================\n");
    printf("Usage: ble_prov <command> [arguments...]\n\n");
    printf("Commands:\n");
    printf("  start [name]       独立启动 BLE 配网广播 (默认: Phoenix-Setup)\n");
    printf("  stop               停止并注销 BLE 配网服务\n");
    printf("  status             查看当前配网服务状态与设备名称\n");
    printf("  scan               触发板端 Wi-Fi 扫描并通过 BLE 推送\n");
    printf("  get_config         通过协议回读当前 Wi-Fi/模型/Key 掩码配置\n");
    printf("  set_config <k> <v> 更新单项配置 (api_key, model, prompt)\n");
    printf("  cmd <json_str>     向 BLE 配网协议引擎直接注入 JSON 指令\n");
    printf("  help               显示本帮助信息\n\n");
}

int ble_prov_main(int argc, char *argv[])
{
    if (argc < 2 || strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage();
        return 0;
    }

    const char *subcmd = argv[1];

    if (strcmp(subcmd, "start") == 0) {
        const char *name = (argc >= 3) ? argv[2] : NULL;
        int ret = ble_prov_service_init(name);
        if (ret == 0) {
            char dev_name[64] = {0};
            ble_prov_service_get_dev_name(dev_name, sizeof(dev_name));
            printf("✅ BLE 配网服务已成功独立启动！广播名称: [%s]\n", dev_name);
            printf("   GATT Service UUID: 0xFFE0, Write: 0xFFE1, Notify: 0xFFE2\n");
        } else {
            printf("❌ 启动 BLE 配网服务失败: %d\n", ret);
        }
        return ret;
    }
    else if (strcmp(subcmd, "stop") == 0) {
        ble_prov_service_deinit();
        printf("🛑 BLE 配网服务已安全停止并注销广播。\n");
        return 0;
    }
    else if (strcmp(subcmd, "status") == 0) {
        ble_prov_state_t st = ble_prov_service_get_state();
        char dev_name[64] = {0};
        ble_prov_service_get_dev_name(dev_name, sizeof(dev_name));

        const char *st_str = "UNKNOWN";
        switch (st) {
            case BLE_PROV_STATE_IDLE: st_str = "IDLE (未运行)"; break;
            case BLE_PROV_STATE_ADVERTISING: st_str = "ADVERTISING (广播中，等待网页/App连接)"; break;
            case BLE_PROV_STATE_CONNECTED: st_str = "CONNECTED (已连接 GATT Client)"; break;
            case BLE_PROV_STATE_PROVISIONING: st_str = "PROVISIONING (正在尝试加入 Wi-Fi)"; break;
            case BLE_PROV_STATE_PROVISIONED: st_str = "PROVISIONED (连网成功，持有 IP)"; break;
            case BLE_PROV_STATE_STOPPED: st_str = "STOPPED (已停止)"; break;
        }

        printf("📊 BLE 配网服务状态报告:\n");
        printf("   - 运行状态: %s\n", st_str);
        printf("   - 广播名称: %s\n", dev_name);
        printf("   - 处于活跃态: %s\n", ble_prov_service_is_active() ? "是 (YES)" : "否 (NO)");
        return 0;
    }
    else if (strcmp(subcmd, "scan") == 0) {
        printf("📡 正在触发周围 Wi-Fi 热点扫描...\n");
        int count = ble_prov_service_notify_wifi_scan();
        printf("✅ Wi-Fi 扫描完毕，发现 %d 个热点并通过 BLE 广播推送。\n", count);
        return 0;
    }
    else if (strcmp(subcmd, "get_config") == 0) {
        printf("🔄 正在触发双向配置回读 (get_config)...\n");
        ble_prov_service_notify_config();
        printf("✅ 配置已打包并通过 BLE Notify 推送至客户端。\n");
        return 0;
    }
    else if (strcmp(subcmd, "set_config") == 0) {
        if (argc < 4) {
            printf("❌ 参数不足: ble_prov set_config <api_key|model|prompt> <value>\n");
            return -1;
        }
        const char *key = argv[2];
        const char *val = argv[3];

        char json_buf[1024];
        if (strcmp(key, "api_key") == 0) {
            snprintf(json_buf, sizeof(json_buf), "{\"cmd\":\"set_config\",\"agent\":{\"api_key\":\"%s\"}}", val);
        } else if (strcmp(key, "model") == 0) {
            snprintf(json_buf, sizeof(json_buf), "{\"cmd\":\"set_config\",\"agent\":{\"model\":\"%s\"}}", val);
        } else if (strcmp(key, "prompt") == 0) {
            snprintf(json_buf, sizeof(json_buf), "{\"cmd\":\"set_config\",\"agent\":{\"prompt\":\"%s\"}}", val);
        } else {
            printf("❌ 不支持的配置键: %s (仅支持 api_key, model, prompt)\n", key);
            return -1;
        }

        printf("📝 正在更新配置: %s -> %s\n", key, val);
        return ble_prov_service_handle_command(json_buf);
    }
    else if (strcmp(subcmd, "cmd") == 0) {
        if (argc < 3) {
            printf("❌ 参数不足: ble_prov cmd <json_string>\n");
            return -1;
        }
        return ble_prov_service_handle_command(argv[2]);
    }
    else {
        printf("❌ 未知命令: %s\n", subcmd);
        print_usage();
        return -1;
    }
}
