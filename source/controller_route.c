#include "controller_route.h"
#include <stdio.h>

Action controller_route_select(int selected) {
    return selected==0?ACTION_CONTROLLER_STANDARD:selected==1?ACTION_MACRO_STUDIO:ACTION_MACRO_LIBRARY;
}
bool controller_route_event_current(const View *v,Screen sampled_screen,bool consumed) {
    return v && v->screen==sampled_screen && !consumed;
}
void controller_route_navigate(View *v,uint32_t down) {
    if(!v) return;
    if(v->screen==SCREEN_CONFIRM && v->composing) { view_compose_navigate(v,down);return; }
    int direction=(down&(MON_DOWN|MON_RIGHT))?1:(down&(MON_UP|MON_LEFT))?-1:0;
    if(v->screen==SCREEN_HOME && direction) v->selected=view_home_move(v->selected,direction);
    else if(down&(MON_DOWN|MON_UP)) {
        direction=(down&MON_DOWN)?1:-1;
        if(v->screen==SCREEN_KEYBOARD) v->selected=view_keyboard_move(v->selected,direction);
        else if(v->screen==SCREEN_CONTROLLER_MENU) v->selected=(v->selected+(direction>0?1:2))%3;
        else if(v->screen==SCREEN_MACRO_STUDIO) v->selected=view_studio_move(v->selected,direction);
    }
}
bool controller_route_input(const View *v,uint32_t down,bool monitor_exit,Action *action) {
    if(!v || !action) return false;
    *action=ACTION_NONE;
    if(v->screen==SCREEN_DIAGNOSTICS) {
        if(down&MON_PLUS) *action=ACTION_QUIT;
        else if(down&MON_B) *action=ACTION_BACK;
        else if(down&MON_X) *action=ACTION_CREATE_USB_LOG;
        else if(down&MON_A) *action=ACTION_SEND; /* Reset USB, never Standard Controller. */
        return true;
    }
    if(v->screen==SCREEN_CONTROLLER) { if(monitor_exit) *action=ACTION_BACK;return true; }
    if(v->screen==SCREEN_HOME) {
        *action=view_home_action(v->screen,!!(down&MON_PLUS),!!(down&MON_B),!!(down&MON_Y),!!(down&MON_A),v->selected);return true;
    }
    if(v->screen==SCREEN_CONTROLLER_MENU) {
        if(down&MON_B) *action=ACTION_BACK;
        else if(down&MON_A) *action=controller_route_select(v->selected);
        return true;
    }
    if(v->screen==SCREEN_MACRO_STUDIO) {
        if(down&MON_B) *action=ACTION_BACK;
        else if(down&MON_A) *action=v->selected==0?ACTION_STUDIO_MANUAL:v->selected==1?ACTION_STUDIO_RECORD:(Action)ACTION_MS_LIVE_RECORD;
        return true;
    }
    if(v->screen==SCREEN_KEYBOARD) {
        if(down&MON_B) *action=ACTION_BACK;
        else if(down&MON_A) *action=view_keyboard_select(v->selected);
        else if(down&MON_MINUS) *action=ACTION_DIAGNOSTICS;
        return true;
    }
    if(v->screen==SCREEN_CONFIRM && v->composing) {
        if(down&MON_B) *action=ACTION_BACK;
        else if(down&MON_A) *action=view_compose_action(v,(unsigned)v->selected);
        else if(down&MON_X) *action=ACTION_COMPOSE;
        else if(down&MON_ZR) *action=ACTION_PACE;
        else if(down&MON_R) *action=ACTION_ENTER_PAUSE;
        else if(down&MON_L) *action=ACTION_AUTOINDENT;
        return true;
    }
    if(v->screen==SCREEN_MACRO_LIBRARY || v->screen==SCREEN_MACRO_DETAILS || v->screen==SCREEN_MACRO_DELETE) {
        if(down&(MON_B|MON_PLUS)) *action=ACTION_BACK;
        else if(v->screen==SCREEN_MACRO_DELETE && (down&MON_A)) *action=ACTION_MACRO_DELETE_CONFIRM;
        else if(v->screen==SCREEN_MACRO_LIBRARY && (down&MON_UP)) *action=ACTION_MACRO_UP;
        else if(v->screen==SCREEN_MACRO_LIBRARY && (down&MON_DOWN)) *action=ACTION_MACRO_DOWN;
        else if(v->screen==SCREEN_MACRO_LIBRARY && (down&MON_A)) *action=ACTION_MACRO_OPEN;
        else if(v->screen==SCREEN_MACRO_LIBRARY && (down&MON_X)) *action=ACTION_MACRO_REFRESH;
        else if(v->screen==SCREEN_MACRO_LIBRARY && (down&MON_Y)) *action=ACTION_MACRO_DELETE_ASK;
        else if(v->screen==SCREEN_MACRO_DETAILS && (down&MON_LEFT)) *action=ACTION_MACRO_PREV;
        else if(v->screen==SCREEN_MACRO_DETAILS && (down&MON_RIGHT)) *action=ACTION_MACRO_NEXT;
        return true;
    }
    return false;
}
bool controller_route_action(ControllerRoute *r,View *v,Action action) {
    if(!r || !v) return false;
    if(v->screen==SCREEN_CIV_PLACEHOLDER && (v->civ.progress.owned || v->civ.progress.shutdown_unresolved)) return true;
    switch(action) {
    case ACTION_MENU_CIV:
        if(v->screen==SCREEN_HOME) { v->screen=SCREEN_CIV_PLACEHOLDER;v->selected=0;v->civ=(CivView){0}; }
        return true;
    case ACTION_MENU_KEYBOARD: if(v->screen==SCREEN_HOME) { v->selected=0;v->compose_error[0]=0;v->screen=SCREEN_KEYBOARD; }return true;
    case ACTION_MENU_CONTROLLER: if(v->screen==SCREEN_HOME) { v->selected=0;v->macro_status[0]=0;v->screen=SCREEN_CONTROLLER_MENU; }return true;
    case ACTION_CONTROLLER_STANDARD:
        if(v->screen==SCREEN_CONTROLLER_MENU) {
            if(controller_mode_enter(&r->mode)) {
                controller_monitor_init(&v->monitor);v->controller_armed=false;v->controller_exiting=false;
                v->controller_interrupted=false;v->controller_synchronizing=true;v->controller_usb_ready=true;
                v->screen=SCREEN_CONTROLLER;
            }
            else snprintf(v->macro_status,sizeof(v->macro_status),"USB controller not ready.");
        }
        return true;
    case ACTION_MACRO_STUDIO:
        if(v->screen==SCREEN_CONTROLLER_MENU) { v->selected=0;v->screen=SCREEN_MACRO_STUDIO; }
        return true;
    case ACTION_MACRO_LIBRARY:
        if(v->screen==SCREEN_CONTROLLER_MENU) v->screen=SCREEN_MACRO_LIBRARY;
        return true;
    case ACTION_DIAGNOSTICS:
        if(v->screen!=SCREEN_CONTROLLER) return false;
        if(!r->mode.exiting) { r->after_exit=ACTION_DIAGNOSTICS;controller_mode_request_exit(&r->mode); }
        return true;
    case ACTION_BACK:
        if(v->screen==SCREEN_CONTROLLER) {
            if(!r->mode.exiting) { r->after_exit=ACTION_BACK;controller_mode_request_exit(&r->mode); }
        } else if(v->screen==SCREEN_MACRO_LIBRARY || v->screen==SCREEN_MACRO_STUDIO) {
            v->screen=SCREEN_CONTROLLER_MENU;v->selected=0;v->macro_status[0]=0;
        } else if(v->screen==SCREEN_CONTROLLER_MENU) { v->screen=SCREEN_HOME;v->selected=2; }
        else if(v->screen==SCREEN_KEYBOARD || v->screen==SCREEN_CIV_PLACEHOLDER) { v->screen=SCREEN_HOME;v->selected=0; }
        else return false;
        return true;
    default: return false;
    }
}
bool controller_route_tick(ControllerRoute *r,View *v,bool connected,uint32_t held,
                           int32_t lx,int32_t ly,int32_t rx,int32_t ry) {
    if(!r || !v || v->screen!=SCREEN_CONTROLLER) return false;
    controller_monitor_update(&v->monitor,held,connected);v->controller_connected=connected;
    GamepadExclusiveState owner=gamepad_exclusive_status();
    bool exited=controller_mode_tick(&r->mode,connected,held,lx,ly,rx,ry);
    v->controller_armed=r->mode.armed;v->controller_report=controller_monitor_report(held,lx,ly,rx,ry,connected);
    v->controller_exiting=r->mode.exiting;v->controller_interrupted=owner==GAMEPAD_EXCLUSIVE_INTERRUPTED;
    v->controller_synchronizing=owner==GAMEPAD_EXCLUSIVE_SYNC;v->controller_usb_ready=gamepad_ready();
    if(exited) {
        v->screen=r->after_exit==ACTION_DIAGNOSTICS?SCREEN_DIAGNOSTICS:SCREEN_CONTROLLER_MENU;
        v->selected=0;r->mode.exiting=false;
    }
    return exited;
}
