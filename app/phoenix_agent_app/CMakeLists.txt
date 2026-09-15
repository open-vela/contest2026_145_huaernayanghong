# ##############################################################################
# packages/demos/contest2026_145_phoenix_agent_app/CMakeLists.txt
#
# Phoenix Agent App CMake Build Configuration
# ##############################################################################

if(CONFIG_LVX_USE_DEMO_CONTEST2026_145_PHOENIX_AGENT_APP)
  set(PROGNAME phoenix_agent_app)
  set(PRIORITY 100)
  set(STACKSIZE 32768)
  set(MODULE ${CONFIG_LVX_USE_DEMO_CONTEST2026_145_PHOENIX_AGENT_APP})
  set(MAINSRC main.c)

  set(CSRCS
      utils/ring_buffer.c
      utils/time_utils.c
      ui/ui.c
      ui/ui_capsule.c
      ui/ui_stage.c
      ui/ui_settings.c
      ui/lv_font_chinese_16.c
      ui/eye_anim.c
      ui/audio_ctl.c
      core/agent_core.c
      core/event_bus.c
      core/tool_registry.c
      core/store.c
      core/config.c
      core/expression.c
      core/intent_router.c
      core/cartridge_mgr.c
      cartridges/cartridge_home.c
      cartridges/cartridge_familiar.c
      cartridges/cartridge_memo.c
      cartridges/cartridge_clock.c
      cartridges/cartridge_agent.c
      core/web_assets.c
      core/web_portal.c
      core/app.c
      hal/hal_sensor.c
      hal/hal_actuator.c
      hal/hal_audio_in.c
      hal/hal_system.c
      hal/hal_manager.c
      hal/network_mgr.c
      hal/drivers/hal_driver_mock.c
      hal/drivers/hal_driver_openvela.c
      perception/perception.c
      voice/voice_pipeline.c
      tools/tools.c
      tools/tool_wooden_fish.c
      tools/tool_pomodoro.c
      tools/tool_eye_emotion.c
      tools/tool_system_health.c
      tools/tool_launch_app.c
      harness/llm_mock_backend.c
      harness/llm_cloud_backend.c
      harness/llm_provider.c
      test/test_autodrive.c)

  nuttx_add_application(
    NAME ${PROGNAME}
    PRIORITY ${PRIORITY}
    STACKSIZE ${STACKSIZE}
    MODULE ${MODULE}
    SRCS ${MAINSRC} ${CSRCS}
    INCLUDE_DIRECTORIES
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/utils
      ${CMAKE_CURRENT_SOURCE_DIR}/core
      ${CMAKE_CURRENT_SOURCE_DIR}/hal
      ${CMAKE_CURRENT_SOURCE_DIR}/perception
      ${CMAKE_CURRENT_SOURCE_DIR}/voice
      ${CMAKE_CURRENT_SOURCE_DIR}/tools
      ${CMAKE_CURRENT_SOURCE_DIR}/harness
      ${CMAKE_CURRENT_SOURCE_DIR}/ui
      ${CMAKE_CURRENT_SOURCE_DIR}/cartridges
      ${NUTTX_APPS_DIR}/packages/ai_agent/include
  )
endif()
