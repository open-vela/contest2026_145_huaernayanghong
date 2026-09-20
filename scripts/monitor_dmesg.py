#!/usr/bin/env python3
"""
实时持续监听并打印板端 dmesg 和 phoenix.log
"""
import subprocess
import time
import sys

def get_dmesg():
    try:
        p = subprocess.Popen(['adb', 'shell', 'dmesg'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        out, _ = p.communicate(timeout=3.0)
        return out.splitlines()
    except Exception as e:
        return []

def main():
    print("=== 开始持续监听板端 dmesg ===")
    seen_lines = set()
    
    # 初始读取
    initial = get_dmesg()
    for l in initial:
        seen_lines.add(l)
    print(f"已加载初始 dmesg ({len(initial)} 行)，等待新增输出...")
    
    while True:
        lines = get_dmesg()
        new_lines = []
        for l in lines:
            if l not in seen_lines:
                new_lines.append(l)
                seen_lines.add(l)
        for l in new_lines:
            print(f"[DMESG] {l}")
            sys.stdout.flush()
        time.sleep(0.5)

if __name__ == "__main__":
    main()
