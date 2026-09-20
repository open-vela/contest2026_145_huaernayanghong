#!/usr/bin/env bash
# ==============================================================================
# Phoenix HoloDesk-S1 Host Native Build & Test Script (macOS / Linux)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(dirname "$SCRIPT_DIR")"
WORKSPACE_ROOT="$(cd "$APP_DIR/../../.." && pwd)"
CJSON_DIR="$WORKSPACE_ROOT/apps/netutils/cjson/cJSON"
BIN_OUT="$SCRIPT_DIR/phoenix_host_test"

echo "===================================================="
echo " 🔨 Compiling Phoenix Agent Host Test on macOS/Host..."
echo "===================================================="

# Auto-sync HTML assets to C arrays if python3 is available
if command -v python3 >/dev/null 2>&1; then
    if [ -f "$APP_DIR/../../web/sync_assets.py" ]; then
        python3 "$APP_DIR/../../web/sync_assets.py"
    elif [ -f "$APP_DIR/web/sync_assets.py" ]; then
        python3 "$APP_DIR/web/sync_assets.py"
    fi
fi

COMPILER="${CC:-gcc}"
$COMPILER -O2 -Wall -Wextra -std=gnu99 \
    -DHOST_TEST_RUNNER \
    -I "$APP_DIR" \
    -I "$APP_DIR/utils" \
    -I "$APP_DIR/core" \
    -I "$APP_DIR/core/web_api" \
    -I "$APP_DIR/hal" \
    -I "$APP_DIR/perception" \
    -I "$APP_DIR/tools" \
    -I "$APP_DIR/harness" \
    -I "$APP_DIR/voice" \
    -I "$APP_DIR/cartridges" \
    -I "$APP_DIR/ui" \
    -I "$SCRIPT_DIR" \
    -I "$CJSON_DIR" \
    "$SCRIPT_DIR/host_runner.c" \
    "$APP_DIR/utils/log_mgr.c" \
    "$APP_DIR/utils/ring_buffer.c" \
    "$APP_DIR/utils/time_utils.c" \
    "$APP_DIR/utils/time_sync.c" \
    "$APP_DIR/core/event_bus.c" \
    "$APP_DIR/core/tool_registry.c" \
    "$APP_DIR/core/store.c" \
    "$APP_DIR/core/config.c" \
    "$APP_DIR/core/weather_service.c" \
    "$APP_DIR/core/todo_mgr.c" \
    "$APP_DIR/core/audio_test_service.c" \
    "$APP_DIR/core/expression.c" \
    "$APP_DIR/core/intent_router.c" \
    "$APP_DIR/core/cartridge_mgr.c" \
    "$APP_DIR/cartridges/cartridge_home.c" \
    "$APP_DIR/cartridges/cartridge_familiar.c" \
    "$APP_DIR/cartridges/cartridge_memo.c" \
    "$APP_DIR/cartridges/cartridge_clock.c" \
    "$APP_DIR/cartridges/cartridge_zen.c" \
    "$APP_DIR/cartridges/cartridge_agent.c" \
    "$APP_DIR/core/web_assets.c" \
    "$APP_DIR/core/web_router.c" \
    "$APP_DIR/core/web_api/api_system.c" \
    "$APP_DIR/core/web_api/api_wifi.c" \
    "$APP_DIR/core/web_api/api_cartridge.c" \
    "$APP_DIR/core/web_api/api_config.c" \
    "$APP_DIR/core/web_api/api_logs.c" \
    "$APP_DIR/core/web_api/api_sdcard.c" \
    "$APP_DIR/core/web_api/api_agent.c" \
    "$APP_DIR/core/web_api/api_weather.c" \
    "$APP_DIR/core/web_api/api_todo.c" \
    "$APP_DIR/core/web_api/api_audio.c" \
    "$APP_DIR/core/web_portal.c" \
    "$APP_DIR/core/app.c" \
    "$APP_DIR/core/agent_core.c" \
    "$APP_DIR/hal/hal_sensor.c" \
    "$APP_DIR/hal/hal_actuator.c" \
    "$APP_DIR/hal/hal_audio_in.c" \
    "$APP_DIR/hal/hal_system.c" \
    "$APP_DIR/hal/hal_sdcard.c" \
    "$APP_DIR/hal/hal_manager.c" \
    "$APP_DIR/hal/ble_prov_service.c" \
    "$APP_DIR/hal/ble_prov_main.c" \
    "$APP_DIR/hal/network_mgr.c" \
    "$APP_DIR/hal/drivers/hal_driver_mock.c" \
    "$APP_DIR/hal/drivers/hal_driver_openvela.c" \
    "$APP_DIR/perception/perception.c" \
    "$APP_DIR/voice/voice_pipeline.c" \
    "$APP_DIR/tools/tools.c" \
    "$APP_DIR/tools/tool_wooden_fish.c" \
    "$APP_DIR/tools/tool_pomodoro.c" \
    "$APP_DIR/tools/tool_eye_emotion.c" \
    "$APP_DIR/tools/tool_system_health.c" \
    "$APP_DIR/tools/tool_launch_app.c" \
    "$APP_DIR/tools/tool_todo.c" \
    "$APP_DIR/tools/tool_environment.c" \
    "$APP_DIR/harness/llm_mock_backend.c" \
    "$APP_DIR/harness/llm_cloud_backend.c" \
    "$APP_DIR/harness/llm_provider.c" \
    "$APP_DIR/harness/asr_provider.c" \
    "$APP_DIR/harness/tts_provider.c" \
    "$CJSON_DIR/cJSON.c" \
    -o "$BIN_OUT" -lpthread -lm -lcurl

echo "✅ Compilation Succeeded! Binary: $BIN_OUT"
echo ""

# Execute host test
"$BIN_OUT" "$@"
