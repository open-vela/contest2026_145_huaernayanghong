#!/usr/bin/env python3
"""
Phoenix HoloDesk-S1 实时 ADB 日志与全链路诊断工具
用途: 秒级抓取与实时流式追踪应用层业务日志 (/data/phoenix/logs/phoenix.log) 与内核驱动日志 (dmesg)
"""

import sys
import time
import subprocess
import argparse
import os

def run_adb_cmd(cmd_str, timeout=4.0):
    try:
        p = subprocess.Popen(['adb', 'shell'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        out, _ = p.communicate(input=f"{cmd_str}\nexit\n", timeout=timeout)
        lines = [l for l in out.splitlines() if not l.strip().startswith("nsh>")]
        return "\n".join(lines).strip()
    except Exception as e:
        return f"[ADB ERROR: {e}]"

def fetch_app_log():
    tmp_path = "/tmp/phoenix_latest.log"
    try:
        res = subprocess.run(['adb', 'pull', '/data/phoenix/logs/phoenix.log', tmp_path], capture_output=True, text=True, timeout=3.0)
        if res.returncode == 0 and os.path.exists(tmp_path):
            with open(tmp_path, 'r', encoding='utf-8', errors='replace') as f:
                return f.read()
    except Exception as e:
        pass
    # Fallback to cat
    return run_adb_cmd("cat /data/phoenix/logs/phoenix.log")

def fetch_dmesg(tail_lines=30):
    out = run_adb_cmd("dmesg")
    lines = out.splitlines()
    if tail_lines and len(lines) > tail_lines:
        return "\n".join(lines[-tail_lines:])
    return out

def get_system_status():
    ps_out = run_adb_cmd("ps")
    if_out = run_adb_cmd("ifconfig")
    
    app_running = "phoenix_agent_app" in ps_out
    wifi_sta_ip = ""
    softap_ip = ""
    
    for l in if_out.splitlines():
        if "wlan0" in l and "inet addr:" in l:
            wifi_sta_ip = l.split("inet addr:")[1].split()[0]
        elif "wlan1" in l and "inet addr:" in l:
            softap_ip = l.split("inet addr:")[1].split()[0]
            
    return {
        "app_running": app_running,
        "wifi_sta_ip": wifi_sta_ip,
        "softap_ip": softap_ip,
        "ps_raw": ps_out,
        "if_raw": if_out
    }

def follow_logs(interval=0.5):
    print("📡 [实时日志流已启动] 正在流式监听 Phoenix 应用层运行日志... (按 Ctrl+C 退出)")
    print("=" * 70)
    last_size = 0
    tmp_path = "/tmp/phoenix_stream.log"
    
    while True:
        try:
            res = subprocess.run(['adb', 'pull', '/data/phoenix/logs/phoenix.log', tmp_path], capture_output=True, timeout=2.0)
            if res.returncode == 0 and os.path.exists(tmp_path):
                cur_size = os.path.getsize(tmp_path)
                if cur_size > last_size:
                    with open(tmp_path, 'rb') as f:
                        f.seek(last_size)
                        new_data = f.read().decode('utf-8', errors='replace')
                        if new_data:
                            sys.stdout.write(new_data)
                            sys.stdout.flush()
                    last_size = cur_size
                elif cur_size < last_size:
                    # Log rotated
                    last_size = 0
            time.sleep(interval)
        except KeyboardInterrupt:
            print("\n🛑 退出日志流监听")
            break
        except Exception as e:
            time.sleep(interval)

def main():
    parser = argparse.ArgumentParser(description="Phoenix HoloDesk-S1 实时 ADB 日志与全链路诊断工具")
    parser.add_argument("-f", "--follow", action="store_true", help="实时流式追踪应用层运行日志")
    parser.add_argument("-k", "--kernel", action="store_true", help="查看内核与驱动日志 (dmesg)")
    parser.add_argument("-a", "--app", action="store_true", help="查看应用层完整日志 (/data/phoenix/logs/phoenix.log)")
    parser.add_argument("-s", "--status", action="store_true", help="查看当前网络与进程健康状态")
    parser.add_argument("-n", "--lines", type=int, default=30, help="输出末尾行数 (默认 30 行)")
    args = parser.parse_args()

    # 校验 ADB 连接
    chk = subprocess.run(['adb', 'get-state'], capture_output=True, text=True)
    if "device" not in chk.stdout:
        print("❌ [ADB 未就绪] 未检测到在线的 ADB 设备！请确保 USB 数据线已连接。")
        sys.exit(1)

    if args.follow:
        follow_logs()
        return

    if args.kernel:
        print(f"📋 [内核驱动日志 dmesg (末尾 {args.lines} 行)]:")
        print("=" * 70)
        print(fetch_dmesg(args.lines))
        return

    if args.app:
        print(f"📋 [应用层完整业务日志 (/data/phoenix/logs/phoenix.log)]:")
        print("=" * 70)
        log = fetch_app_log()
        lines = log.splitlines()
        print("\n".join(lines[-args.lines:]) if len(lines) > args.lines else log)
        return

    # 默认输出综合健康诊断看板
    st = get_system_status()
    print("=======================================================================")
    print("🤖 Phoenix HoloDesk-S1 全链路 ADB 诊断快照看板")
    print("=======================================================================")
    print(f"▶ 进程状态: {'🟢 phoenix_agent_app 正常运行中' if st['app_running'] else '🔴 phoenix_agent_app 未运行'}")
    print(f"▶ Wi-Fi STA 局域网 IP : [{st['wifi_sta_ip'] or '未连接'}]")
    print(f"▶ SoftAP 热点网关 IP  : [{st['softap_ip'] or '未开启'}]")
    print("-----------------------------------------------------------------------")
    print(f"📝 最近应用层业务流转日志 (最近 {args.lines} 行):")
    print("-----------------------------------------------------------------------")
    app_log = fetch_app_log()
    app_lines = app_log.splitlines()
    print("\n".join(app_lines[-args.lines:]) if len(app_lines) > args.lines else app_log)
    print("-----------------------------------------------------------------------")
    print(f"📡 最近内核网络/蓝牙驱动日志 (dmesg 最近 10 行):")
    print("-----------------------------------------------------------------------")
    print(fetch_dmesg(10))
    print("=======================================================================")
    print("💡 提示: 运行 './scripts/adb_live_log.py -f' 可开启实时流式日志监控！")

if __name__ == "__main__":
    main()
