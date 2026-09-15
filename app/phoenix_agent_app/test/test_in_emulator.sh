#!/usr/bin/env bash
# ==============================================================================
# Phoenix HoloDesk-S1 模拟器自动化测试一键入口脚本
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="$(which python3)"

if [ -z "$PYTHON_BIN" ]; then
    echo "❌ 错误: 未找到 python3 命令"
    exit 1
fi

echo "===================================================="
echo " 🤖 启动 Phoenix 模拟器自动化测试与 AI 视觉验收"
echo "===================================================="

"$PYTHON_BIN" "$SCRIPT_DIR/run_auto_test.py" "$@"
