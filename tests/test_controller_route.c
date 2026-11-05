#include "controller_route.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Completion events are injected explicitly; time never depends on sleeps. */
static GamepadExclusiveState owner=GAMEPAD_EXCLUSIVE_OFF;
static bool usb=true;
static unsigned begin_count,submit_count,stop_count;
static SwitchGamepadReport last;
bool gamepad_ready(void) { return usb; }
GamepadExclusiveState gamepad_exclusive_status(void) { return owner; }
bool gamepad_exclusive_begin(void) {
    if(!usb || owner!=GAMEPAD_EXCLUSIVE_OFF) return false;
    begin_count++;owner=GAMEPAD_EXCLUSIVE_SYNC;return true;
}
bool gamepad_exclusive_submit(const SwitchGamepadReport *report) {
    if(!usb || owner!=GAMEPAD_EXCLUSIVE_READY) return false;
    submit_count++;last=*report;owner=GAMEPAD_EXCLUSIVE_PENDING;return true;
}
bool gamepad_exclusive_stop(void) {
    if(!usb || owner==GAMEPAD_EXCLUSIVE_OFF) return false;
    stop_count++;owner=GAMEPAD_EXCLUSIVE_STOPPING;return true;
}
static void event_complete(void) {
    if(owner==GAMEPAD_EXCLUSIVE_SYNC || owner==GAMEPAD_EXCLUSIVE_PENDING) owner=GAMEPAD_EXCLUSIVE_READY;
    else if(owner==GAMEPAD_EXCLUSIVE_STOPPING) owner=GAMEPAD_EXCLUSIVE_OFF;
}
static void disconnected(void) { usb=false;if(owner!=GAMEPAD_EXCLUSIVE_OFF) owner=GAMEPAD_EXCLUSIVE_INTERRUPTED; }
static bool tick(ControllerRoute *r,View *v,uint32_t held) {
    return controller_route_tick(r,v,true,held,0,0,0,0);
}
static void press(ControllerRoute *r,View *v,uint32_t edge) {
    Action action=ACTION_NONE;assert(controller_route_input(v,edge,false,&action));
    if(action!=ACTION_NONE) assert(controller_route_action(r,v,action));
}
int main(void) {
    ControllerRoute r={0};View v={0};
    assert(v.screen==SCREEN_HOME);
    for(int i=0;i<4;i++) assert(!tick(&r,&v,MON_A|MON_RIGHT));
    { Action ignored;assert(controller_route_input(&v,MON_X,false,&ignored) && ignored==ACTION_NONE); }
    assert(!begin_count && !submit_count);
    v.selected=2;press(&r,&v,MON_A);assert(v.screen==SCREEN_CONTROLLER_MENU);
    assert(!tick(&r,&v,MON_A) && !begin_count && !submit_count);
    { Action ignored;assert(controller_route_input(&v,MON_X,false,&ignored) && ignored==ACTION_NONE); }
    v.selected=1;press(&r,&v,MON_A);assert(v.screen==SCREEN_MACRO_STUDIO);
    assert(!tick(&r,&v,MON_A) && !submit_count);
    { Action ignored;assert(controller_route_input(&v,MON_X,false,&ignored) && ignored==ACTION_NONE); }
    assert(v.selected==0);
    { Action a;assert(controller_route_input(&v,MON_A,false,&a) && a==ACTION_STUDIO_MANUAL);
      assert(!controller_route_action(&r,&v,a)); } /* MacroApp owns editor initialization. */
    v.selected=view_studio_move(v.selected,1);assert(v.selected==1);
    { Action a;assert(controller_route_input(&v,MON_A,false,&a) && a==ACTION_STUDIO_RECORD);
      assert(!controller_route_action(&r,&v,a)); }
    press(&r,&v,MON_B);assert(v.screen==SCREEN_CONTROLLER_MENU);
    v.selected=2;press(&r,&v,MON_A);assert(v.screen==SCREEN_MACRO_LIBRARY);
    assert(!tick(&r,&v,MON_A|MON_DOWN) && !submit_count);
    press(&r,&v,MON_B);assert(v.screen==SCREEN_CONTROLLER_MENU);
    press(&r,&v,MON_B);assert(v.screen==SCREEN_HOME && v.selected==2);
    v.selected=1;press(&r,&v,MON_A);assert(v.screen==SCREEN_KEYBOARD);
    assert(!tick(&r,&v,MON_A) && !submit_count);
    press(&r,&v,MON_B);assert(v.screen==SCREEN_HOME);
    v.selected=0;press(&r,&v,MON_A);assert(v.screen==SCREEN_CIV_PLACEHOLDER);
    assert(!tick(&r,&v,MON_A) && !submit_count);
    assert(controller_route_action(&r,&v,ACTION_BACK));assert(v.screen==SCREEN_HOME);
    v.selected=2;press(&r,&v,MON_A);
    v.selected=0;press(&r,&v,MON_A);assert(v.screen==SCREEN_CONTROLLER && owner==GAMEPAD_EXCLUSIVE_SYNC);
    assert(!tick(&r,&v,MON_A) && !submit_count && !v.controller_armed);
    event_complete();assert(!tick(&r,&v,MON_A) && !v.controller_armed && last.buttons==0 && submit_count==1);
    event_complete();assert(!tick(&r,&v,0) && v.controller_armed);
    assert(!tick(&r,&v,MON_A) && last.buttons==SWITCH_GAMEPAD_A && submit_count==2);
    assert(!tick(&r,&v,0) && submit_count==2); /* pending report is not overwritten */
    assert(controller_route_action(&r,&v,ACTION_BACK));assert(owner==GAMEPAD_EXCLUSIVE_STOPPING);
    assert(!tick(&r,&v,MON_A) && v.screen==SCREEN_CONTROLLER);
    disconnected();assert(!tick(&r,&v,MON_A) && v.screen==SCREEN_CONTROLLER);
    usb=true;assert(!tick(&r,&v,MON_A) && owner==GAMEPAD_EXCLUSIVE_STOPPING);
    event_complete();assert(tick(&r,&v,MON_A) && v.screen==SCREEN_CONTROLLER_MENU);
    assert(!tick(&r,&v,MON_A) && submit_count==2);
    assert(controller_route_action(&r,&v,ACTION_CONTROLLER_STANDARD));
    disconnected();assert(!tick(&r,&v,MON_A) && v.controller_interrupted);
    assert(controller_route_action(&r,&v,ACTION_BACK));usb=true;
    assert(!tick(&r,&v,MON_A) && owner==GAMEPAD_EXCLUSIVE_STOPPING);
    event_complete();assert(tick(&r,&v,MON_A) && v.screen==SCREEN_CONTROLLER_MENU);
    assert(controller_route_action(&r,&v,ACTION_CONTROLLER_STANDARD));event_complete();
    assert(!tick(&r,&v,MON_A) && last.buttons==0 && !v.controller_armed);
    /* Failed report is represented by the transport's interrupted state. */
    owner=GAMEPAD_EXCLUSIVE_INTERRUPTED;assert(!tick(&r,&v,0) && v.controller_interrupted);
    assert(controller_route_action(&r,&v,ACTION_BACK));
    assert(!tick(&r,&v,0) && owner==GAMEPAD_EXCLUSIVE_STOPPING);
    event_complete();assert(tick(&r,&v,0));
    assert(begin_count==3 && stop_count>=3);
    puts("PASS: production controller screen routing with simulated USB completions and failures.");
    return 0;
}
