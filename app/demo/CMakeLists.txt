# ##############################################################################
# packages/demos/contest2026_145_demo/CMakeLists.txt
#
# OpenVela Hardware Diagnostic & Telemetry App CMake Build Configuration
# ##############################################################################

if(CONFIG_LVX_USE_DEMO_CONTEST2026_145_DEMO)
  set(PROGNAME demo)
  set(PRIORITY 100)
  set(STACKSIZE 32768)
  set(MODULE ${CONFIG_LVX_USE_DEMO_CONTEST2026_145_DEMO})
  set(MAINSRC main.c)

  set(CSRCS
      hal/vela_hal_power.c
      hal/vela_hal_audio.c
      hal/vela_hal_display.c
      hal/vela_hal_sensors.c
      test/test_power.c
      test/test_mic.c
      test/test_audio.c
      test/test_display.c
      test/test_all.c
      ui/demo_ui.c)

  nuttx_add_application(
    NAME ${PROGNAME}
    PRIORITY ${PRIORITY}
    STACKSIZE ${STACKSIZE}
    MODULE ${MODULE}
    DEPENDS lvgl
    SRCS ${MAINSRC} ${CSRCS})
endif()
