#include "controller_mode.h"
#include "controller_monitor.h"
#include <string.h>

bool controller_mode_enter(ControllerMode *mode) {
    if(!mode || !gamepad_exclusive_begin()) return false;
    memset(mode,0,sizeof(*mode));return true;
}
void controller_mode_request_exit(ControllerMode *mode) {
    if(!mode || mode->exiting) return;
    mode->exiting=true;mode->armed=false;
    if(gamepad_ready()) (void)gamepad_exclusive_stop();
}
bool controller_mode_tick(ControllerMode *mode,bool connected,uint32_t held,
                          int32_t lx,int32_t ly,int32_t rx,int32_t ry) {
    if(!mode) return false;
    GamepadExclusiveState owner=gamepad_exclusive_status();bool ready=gamepad_ready();
    if(mode->exiting || !connected || !ready || owner==GAMEPAD_EXCLUSIVE_INTERRUPTED) mode->armed=false;
    else if(controller_monitor_can_arm(held,connected,ready,owner==GAMEPAD_EXCLUSIVE_READY)) mode->armed=true;
    SwitchGamepadReport output=controller_monitor_report(held,lx,ly,rx,ry,connected && ready && mode->armed);
    if(!mode->exiting && owner==GAMEPAD_EXCLUSIVE_READY &&
       (!mode->has_submitted || memcmp(&output,&mode->submitted,sizeof(output)))) {
        if(gamepad_exclusive_submit(&output)) { mode->submitted=output;mode->has_submitted=true; }
    }
    if(mode->exiting && ready && owner==GAMEPAD_EXCLUSIVE_INTERRUPTED) (void)gamepad_exclusive_stop();
    return mode->exiting && owner==GAMEPAD_EXCLUSIVE_OFF;
}
