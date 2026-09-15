/**
 * @file audio_ctl.c
 * @brief Legacy Audio Controller Shim forwarding to HAL Actuator Layer
 * @author OpenVela Contest 2026 Team 145
 */

#include "audio_ctl.h"

/* Legacy symbols already mapped to static inlines in hal/hal_actuator.h */
/* This file is kept to ensure existing build systems and link targets remain intact. */
int phoenix_audio_compat_anchor(void)
{
    return 0;
}
