#include "controller_route.h"
#include "ui.h"
#include "pace.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool gamepad_ready(void) { return true; }
GamepadExclusiveState gamepad_exclusive_status(void) { return GAMEPAD_EXCLUSIVE_OFF; }
bool gamepad_exclusive_begin(void) { return true; }
bool gamepad_exclusive_submit(const SwitchGamepadReport *report) { (void)report;return true; }
bool gamepad_exclusive_stop(void) { return true; }
static uint32_t *actual,*expected;
static uint32_t rgb(unsigned n) { return 0xff000000u|((n&255)<<16)|(n&0xff00)|((n>>16)&255); }
static void text_at(int x,int y,int size,unsigned color,const char *text,int width) {
    assert(ui_text_width(size,text)<=width);
    ui_begin(expected,1280*4);ui_text(x,y,size,rgb(color),text,width);
    unsigned ink=0;int w=ui_text_width(size,text);
    for(int yy=y;yy<y+size;yy++) for(int xx=x;xx<x+w;xx++)
        if(expected[yy*1280+xx]==rgb(color)) { assert(actual[yy*1280+xx]==rgb(color));ink++; }
    assert(ink);
}
static void keyboard_menu(void) {
    View v={0};ControllerRoute route={0};assert(controller_route_action(&route,&v,ACTION_MENU_KEYBOARD));
    assert(v.screen==SCREEN_KEYBOARD && v.selected==0);
    const char *titles[]={"Compose & Send","Live Keyboard","Send Text File"};
    const char *descriptions[]={"Write, review, and send complete text.","Send individual keystrokes in real time.","Select a .txt file from your SD card to send."};
    for(int selected=0;selected<3;selected++) {
        v.selected=selected;view_render(&v,actual,1280*4);
        text_at(48,82,32,0xffffff,"USB KEYBOARD",1184);
        for(int i=0;i<3;i++) {
            int y=150+i*135;
            text_at(75,y+17,32,0xffffff,titles[i],1080);
            text_at(75,y+66,20,0xc2cddd,descriptions[i],1080);
            assert(actual[y*1280+640]==rgb(i==selected?0xd9aa54:0x40536b));
            assert(view_touch(&v,1000,y+40)==view_keyboard_select(i));
        }
        for(int x=48;x<1232;x++) for(int y=590;y<630;y++) assert(view_touch(&v,x,y)==ACTION_NONE);
        Action a=ACTION_NONE;assert(controller_route_input(&v,MON_A,false,&a) && a==view_keyboard_select(selected));
        controller_route_navigate(&v,MON_DOWN);assert(v.selected==(selected+1)%3);
    }
    for(unsigned i=0;i<3;i++) {
        const uint32_t removed[]={MON_X,MON_ZR,MON_PLUS};Action a=ACTION_BACK;
        assert(controller_route_input(&v,removed[i],false,&a) && a==ACTION_NONE);
    }
    ViewFooter f=view_footer(&v);assert(f.count==1 && f.buttons[0].action==ACTION_BACK);
    Action a=ACTION_NONE;assert(controller_route_input(&v,MON_MINUS,false,&a) && a==ACTION_DIAGNOSTICS);
    assert(controller_route_input(&v,MON_B,false,&a) && a==ACTION_BACK);
    assert(controller_route_action(&route,&v,a) && v.screen==SCREEN_HOME);
    assert(controller_route_input(&v,MON_PLUS,false,&a) && a==ACTION_QUIT);
}
static void compose_review(void) {
    View v={0};v.screen=SCREEN_CONFIRM;v.composing=true;v.caps_state=-1;v.compose_pages=3;v.compose_page=1;
    v.usb_ready=v.compose_valid=true;v.enter_pause_ms=200;v.characters=64;strcpy(v.compose_rows[0],"Review this text before sending.");
    assert(view_footer(&v).count==0);unsigned backs=0;
    for(unsigned i=0;i<COMPOSE_CONTROL_COUNT;i++) {
        v.selected=(int)i;view_render(&v,actual,1280*4);
        text_at(48,101,32,0xffffff,"COMPOSE & SEND",1184);
        ViewControl c=view_compose_control(i);char label[96];view_compose_label(&v,i,label,sizeof(label));
        assert(ui_text_width(i<3?24:20,label)<=c.w-32);
        assert(actual[c.y*1280+c.x+c.w/2]==rgb(0xd9aa54));
        assert(view_touch(&v,c.x+c.w/2,c.y+c.h/2)==c.action);
        Action a=ACTION_NONE;assert(controller_route_input(&v,MON_A,false,&a) && a==c.action);
        backs+=c.action==ACTION_BACK;
        for(unsigned j=i+1;j<COMPOSE_CONTROL_COUNT;j++) {
            ViewControl d=view_compose_control(j);
            assert(c.x+c.w<=d.x || d.x+d.w<=c.x || c.y+c.h<=d.y || d.y+d.h<=c.y);
        }
        controller_route_navigate(&v,MON_RIGHT);assert(v.selected>=0 && v.selected<COMPOSE_CONTROL_COUNT);
    }
    assert(backs==1);
    for(int y=648;y<704;y++) for(int x=48;x<1232;x++) assert(view_touch(&v,x,y)==ACTION_NONE);
    v.selected=999;controller_route_navigate(&v,MON_UP);assert(v.selected>=0 && v.selected<COMPOSE_CONTROL_COUNT);
    Action a=ACTION_NONE;
    assert(controller_route_input(&v,MON_X,false,&a) && a==ACTION_COMPOSE);
    const uint32_t keys[]={MON_ZR,MON_R,MON_L};const Action actions[]={ACTION_PACE,ACTION_ENTER_PAUSE,ACTION_AUTOINDENT};
    for(unsigned i=0;i<3;i++) {
        assert(controller_route_input(&v,keys[i],false,&a) && a==actions[i]);
        assert(view_adjust_typing_setting(&v,a));
    }
    assert(v.pace==PACE_FAST && v.enter_pause_ms==400 && v.cancel_autoindent);
    v.selected=1;assert(controller_route_input(&v,MON_A|MON_X,false,&a) && a==ACTION_COMPOSE);
    for(int i=0;i<100;i++) assert(controller_route_input(&v,0,false,&a) && a==ACTION_NONE);
    assert(controller_route_input(&v,MON_MINUS,false,&a) && a==ACTION_NONE);
    assert(controller_route_input(&v,MON_B,false,&a) && a==ACTION_BACK);
    v.compose_page=0;assert(view_compose_action(&v,7)==ACTION_NONE);
    v.compose_page=2;assert(view_compose_action(&v,8)==ACTION_NONE);
    v.compose_valid=false;assert(view_compose_action(&v,0)==ACTION_NONE);
    v.compose_valid=true;v.usb_ready=false;assert(view_compose_action(&v,0)==ACTION_NONE && view_compose_action(&v,6)==ACTION_NONE);
    v.usb_ready=true;assert(view_compose_action(&v,6)==ACTION_CAPS_LOCK);
    char label[96];view_compose_label(&v,6,label,sizeof(label));assert(strstr(label,"no host LED") && !strstr(label,"unknown"));
    v.caps_state=1;view_compose_label(&v,6,label,sizeof(label));assert(strstr(label,"on (host)"));
    v.caps_state=0;view_compose_label(&v,6,label,sizeof(label));assert(strstr(label,"off (host)"));
    v.caps_pending=true;assert(view_compose_action(&v,6)==ACTION_NONE);
    v.caps_pending=false;v.civ.progress.owned=true;assert(view_compose_action(&v,0)==ACTION_NONE && !view_adjust_typing_setting(&v,ACTION_PACE));
    v.civ.progress.owned=false;v.characters=0;v.compose_valid=false;v.selected=1;view_render(&v,actual,1280*4);
    text_at(72,216,24,0xc2cddd,"No text yet. Choose EDIT to write a message.",1100);
    v.screen=SCREEN_HOME;assert(controller_route_input(&v,MON_Y,false,&a) && a==ACTION_DIAGNOSTICS);
}
int main(void) {
    assert(ui_init());actual=calloc(1280u*720,4);expected=calloc(1280u*720,4);assert(actual && expected);
    keyboard_menu();compose_review();free(actual);free(expected);ui_shutdown();
    puts("PASS: production keyboard/Compose UI: three cards, labels/chrome/focus/hitboxes, removed menu actions, single Back, Send/Edit/pages, shared settings, truthful Caps and ownership isolation.");
    return 0;
}
