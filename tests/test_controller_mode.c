#include "controller_mode.h"
#include "controller_monitor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Simulated completion-driven transport; no real-time waits. */
static GamepadExclusiveState owner=GAMEPAD_EXCLUSIVE_OFF;
static bool ready=true;
static SwitchGamepadReport submitted;
static unsigned submissions,stops;
bool gamepad_ready(void) { return ready; }
GamepadExclusiveState gamepad_exclusive_status(void) { return owner; }
bool gamepad_exclusive_begin(void) {
    if(!ready || owner!=GAMEPAD_EXCLUSIVE_OFF) return false;
    owner=GAMEPAD_EXCLUSIVE_SYNC;return true;
}
bool gamepad_exclusive_submit(const SwitchGamepadReport *r) {
    if(!ready || owner!=GAMEPAD_EXCLUSIVE_READY) return false;
    submitted=*r;submissions++;owner=GAMEPAD_EXCLUSIVE_PENDING;return true;
}
bool gamepad_exclusive_stop(void) {
    if(!ready || owner==GAMEPAD_EXCLUSIVE_OFF) return false;
    stops++;owner=GAMEPAD_EXCLUSIVE_STOPPING;return true;
}
static bool tick(ControllerMode *m,uint32_t held) { return controller_mode_tick(m,true,held,0,0,0,0); }
int main(void) {
    ControllerMode m={0};
    assert(!submissions && owner==GAMEPAD_EXCLUSIVE_OFF); /* menus do not enter mode */
    assert(controller_mode_enter(&m));assert(owner==GAMEPAD_EXCLUSIVE_SYNC);
    assert(!tick(&m,MON_A) && !m.armed && !submissions); /* entry A and sync */
    owner=GAMEPAD_EXCLUSIVE_READY;
    assert(!tick(&m,MON_A) && !m.armed && submissions==1 && submitted.buttons==0);
    owner=GAMEPAD_EXCLUSIVE_READY;
    assert(!tick(&m,0) && m.armed); /* release required */
    assert(!tick(&m,MON_A) && submissions==2 && submitted.buttons==SWITCH_GAMEPAD_A);
    assert(!tick(&m,0) && submissions==2); /* in flight: no competing writer */
    owner=GAMEPAD_EXCLUSIVE_READY;assert(!tick(&m,0) && submissions==3 && submitted.buttons==0);
    controller_mode_request_exit(&m);assert(owner==GAMEPAD_EXCLUSIVE_STOPPING && stops==1);
    assert(!tick(&m,MON_A) && !m.armed);
    ready=false;owner=GAMEPAD_EXCLUSIVE_INTERRUPTED;assert(!tick(&m,0));
    ready=true;assert(!tick(&m,0) && owner==GAMEPAD_EXCLUSIVE_STOPPING && stops==2);
    owner=GAMEPAD_EXCLUSIVE_OFF;assert(tick(&m,0));
    assert(controller_mode_enter(&m));owner=GAMEPAD_EXCLUSIVE_INTERRUPTED;ready=false;
    assert(!tick(&m,MON_A) && !m.armed); /* disconnect during entry */
    controller_mode_request_exit(&m);assert(!tick(&m,0));
    ready=true;assert(!tick(&m,0) && owner==GAMEPAD_EXCLUSIVE_STOPPING);
    owner=GAMEPAD_EXCLUSIVE_OFF;assert(tick(&m,0));
    assert(controller_mode_enter(&m));owner=GAMEPAD_EXCLUSIVE_READY;
    assert(!tick(&m,MON_A) && submitted.buttons==0); /* no stale state on reentry */
    puts("PASS: application controller mode, entry gating, exit and simulated reconnect.");
    return 0;
}
