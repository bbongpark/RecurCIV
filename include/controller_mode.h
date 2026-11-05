#pragma once
#include "usb_gamepad.h"

/* Application-level Standard Controller ownership. The transport is polled by
   the caller before each tick. A completed exit is reported only after OFF. */
typedef struct {
    bool armed,exiting,has_submitted;
    SwitchGamepadReport submitted;
} ControllerMode;

bool controller_mode_enter(ControllerMode *mode);
void controller_mode_request_exit(ControllerMode *mode);
bool controller_mode_tick(ControllerMode *mode,bool connected,uint32_t held,
                          int32_t lx,int32_t ly,int32_t rx,int32_t ry);
