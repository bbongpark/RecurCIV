#include "controller_route.h"
#include "macro_app.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Use the production input/action router AND the production raster renderer.
   This specifically rejects a valid menu enum painted by a Diagnostics fallback. */
static GamepadExclusiveState owner=GAMEPAD_EXCLUSIVE_OFF;
static unsigned begins,submits,stops;
bool gamepad_ready(void) { return true; }
GamepadExclusiveState gamepad_exclusive_status(void) { return owner; }
bool gamepad_exclusive_begin(void) { if(owner!=GAMEPAD_EXCLUSIVE_OFF) return false;begins++;owner=GAMEPAD_EXCLUSIVE_SYNC;return true; }
bool gamepad_exclusive_submit(const SwitchGamepadReport *r) { assert(r);submits++;owner=GAMEPAD_EXCLUSIVE_PENDING;return true; }
bool gamepad_exclusive_stop(void) { stops++;owner=GAMEPAD_EXCLUSIVE_STOPPING;return true; }
static uint32_t *actual,*expected;
static MacroApp macro_app;
static uint32_t rgb(unsigned n) { return 0xff000000u|((n&255u)<<16)|(n&0xff00u)|((n>>16)&255u); }
static void draw(const View *v) { view_render(v,actual,1280*4); }
static void text_at(int x,int y,int size,unsigned color,const char *text) {
    ui_begin(expected,1280*4);ui_text(x,y,size,rgb(color),text,1232-x);
    int w=ui_text_width(size,text);assert(w>0 && x+w<=1232);
    unsigned ink=0;
    for(int yy=y;yy<y+size;yy++) for(int xx=x;xx<x+w;xx++) {
        uint32_t p=expected[yy*1280+xx];
        /* Opaque glyph pixels are independent of the underlying card fill. */
        if(p==rgb(color)) { assert(actual[yy*1280+xx]==p);ink++; }
    }
    assert(ink>0);
}
static void header_footer(const View *v,const char *name) {
    draw(v);
    text_at(48,20,32,0xffffff,"Recur");
    text_at(48+ui_text_width(32,"Recur"),20,32,0xd9aa54,"CIV");
    text_at(1232-ui_text_width(20,name),27,20,0xc2cddd,name);
    assert(actual[75*1280+640]==rgb(0x40536b));
    assert(actual[640*1280+640]==rgb(0xd9aa54));
    assert(actual[713*1280+640]==rgb(0xd9aa54));
    ViewFooter f=view_footer(v);
    if(f.hints[0]) assert(ui_text_width(20,f.hints)<=(f.count?f.buttons[0].x-82:1140));
    if(f.hints[0]) text_at(66,666,20,0xd9aa54,f.hints);
    unsigned backs=0;
    for(size_t i=0;i<f.count;i++) {
        const ViewFooterButton *b=&f.buttons[i];
        assert(b->x>50 && b->x+b->w<1232 && ui_text_width(20,b->label)<=b->w-24);
        if(i) assert(f.buttons[i-1].x+f.buttons[i-1].w<b->x);
        assert(view_touch(v,b->x+b->w/2,675)==b->action);
        assert(view_touch(v,b->x+b->w/2,704)==ACTION_NONE);
        assert(strncmp(b->label,"B ",2)!=0);
        if(b->action==ACTION_BACK) backs++;
        if(v->screen!=SCREEN_CONFIRM) text_at(b->x+12,666,20,0xd9aa54,b->label);
    }
    assert(backs<=1);
    for(size_t i=0;i<1280u*720;i++) assert(actual[i]!=rgb(0x34c5db));
}
static void edge(ControllerRoute *r,View *v,uint32_t down) {
    Screen sampled=v->screen;Action a=ACTION_NONE;
    if(macro_app_screen(v->screen)) {
        (void)macro_app_input(&macro_app,v,0,0,0,0,0,0,0,0);
        a=macro_app_input(&macro_app,v,down,down,0,0,0,0,0,1);
    } else {
        controller_route_navigate(v,down);
        assert(controller_route_input(v,down,false,&a));
    }
    assert(controller_route_event_current(v,sampled,false));
    if(a!=ACTION_NONE && !macro_app_action(&macro_app,v,a,1,true)) assert(controller_route_action(r,v,a));
}
int main(void) {
    macro_app_init(&macro_app,"build/macro-ui-missing-directory");
    assert(ui_init());actual=calloc(1280u*720,4);expected=calloc(1280u*720,4);assert(actual && expected);
    View v={0};ControllerRoute route={0};
    const char *titles[]={"CIV Exploit","USB Keyboard","USB Controller"};
    const char *subtitles[]={"CIV VII Automation","Text & Live Input","Control & Macros"};
    for(int selected=0;selected<3;selected++) {
        v.selected=selected;header_footer(&v,"HOME");
        for(int i=0;i<3;i++) {
            assert(actual[188*1280+58+i*400+187]==rgb(i==selected?0xd9aa54:0x40536b));
            text_at(view_home_text_x(i,ui_text_width(32,titles[i])),374,32,0xffffff,titles[i]);
            text_at(view_home_text_x(i,ui_text_width(20,subtitles[i])),424,20,0xc2cddd,subtitles[i]);
        }
    }
    assert(view_touch(&v,900,400)==ACTION_MENU_CONTROLLER);
    edge(&route,&v,MON_A);assert(v.screen==SCREEN_CONTROLLER_MENU && v.selected==0 && !begins);
    assert(!controller_route_event_current(&v,SCREEN_HOME,false)); /* old Home event */
    assert(!controller_route_event_current(&v,SCREEN_CONTROLLER_MENU,true)); /* consumed */
    header_footer(&v,"USB CONTROLLER");
    text_at(75,167,32,0xffffff,"Standard Controller");
    text_at(75,302,32,0xffffff,"Macro Studio");
    text_at(75,437,32,0xffffff,"Macro Library");
    assert(actual[150*1280+640]==rgb(0xd9aa54));
    assert(view_touch(&v,100,200)==ACTION_CONTROLLER_STANDARD);
    assert(view_touch(&v,650,675)==ACTION_NONE); /* not the old Reset USB box */
    edge(&route,&v,MON_DOWN);assert(v.selected==1);
    edge(&route,&v,MON_A);assert(v.screen==SCREEN_MACRO_STUDIO && v.selected==0);
    header_footer(&v,"MACRO STUDIO");assert(actual[160*1280+640]==rgb(0xd9aa54));
    edge(&route,&v,MON_DOWN);assert(v.selected==1);
    draw(&v);assert(actual[160*1280+640]==rgb(0x40536b) && actual[315*1280+640]==rgb(0xd9aa54));
    int dx,dy;LiveNav nav={0};
    view_menu_stick_direction(0,11999,&dx,&dy);assert(!dx && !dy);
    view_menu_stick_direction(0,20000,&dx,&dy);assert(dy==-1);
    assert(live_nav_step(&nav,dx,dy,0));edge(&route,&v,MON_UP);assert(v.selected==0);
    assert(!live_nav_step(&nav,dx,dy,349));assert(live_nav_step(&nav,dx,dy,350));
    edge(&route,&v,MON_A);assert(v.screen==SCREEN_MACRO_EDITOR);
    header_footer(&v,"MACRO EDITOR");
    edge(&route,&v,MON_B);assert(v.screen==SCREEN_MACRO_DISCARD);
    assert(macro_app_action(&macro_app,&v,(Action)ACTION_MS_DISCARD,1,true));
    assert(macro_app_action(&macro_app,&v,view_touch(&v,100,400),1,true));assert(v.screen==SCREEN_MACRO_RECORD);
    header_footer(&v,"MACRO RECORDING");text_at(72,210,32,0xd9aa54,"READY TO RECORD");
    edge(&route,&v,MON_B);assert(v.screen==SCREEN_MACRO_STUDIO);
    edge(&route,&v,MON_DOWN);edge(&route,&v,MON_DOWN);assert(v.selected==2);
    draw(&v);assert(actual[470*1280+640]==rgb(0xd9aa54));text_at(75,495,32,0xffffff,"LIVE RECORD");
    edge(&route,&v,MON_A);assert(v.screen==SCREEN_MACRO_RECORD && macro_app.record_live && begins==1);
    header_footer(&v,"MACRO RECORDING");text_at(72,210,32,0xd9aa54,"SYNCHRONIZING NEUTRAL OUTPUT");
    owner=GAMEPAD_EXCLUSIVE_READY;macro_app_tick(&macro_app,&v,true,0,0,0,0,0,0,2);
    header_footer(&v,"MACRO RECORDING");text_at(72,210,32,0xd9aa54,"LIVE CONTROLLER - NOT RECORDING");
    assert(view_touch(&v,850,675)==(Action)ACTION_MS_RECORD_START && view_touch(&v,1100,675)==(Action)ACTION_MS_LIVE_EXIT);
    assert(macro_app_action(&macro_app,&v,(Action)ACTION_MS_LIVE_EXIT,3,true));
    header_footer(&v,"MACRO RECORDING");text_at(72,210,32,0xd9aa54,"NEUTRALIZING USB OUTPUT");
    assert(!view_footer(&v).count);
    v.macro_cleanup_blocked=true;header_footer(&v,"MACRO RECORDING");text_at(72,210,32,0xd9aa54,"USB CLEANUP REQUIRED");
    assert(view_touch(&v,1100,675)==(Action)ACTION_MS_RETRY);
    owner=GAMEPAD_EXCLUSIVE_OFF;macro_app_tick(&macro_app,&v,true,0,0,0,0,0,0,4);
    edge(&route,&v,MON_B);assert(v.screen==SCREEN_MACRO_STUDIO);
    begins=submits=stops=0; /* The completed live session left no USB writer. */
    edge(&route,&v,MON_B);assert(v.screen==SCREEN_CONTROLLER_MENU);
    edge(&route,&v,MON_DOWN);edge(&route,&v,MON_DOWN);edge(&route,&v,MON_A);
    assert(v.screen==SCREEN_MACRO_LIBRARY && !begins && !submits);
    header_footer(&v,"MACRO LIBRARY");text_at(72,240,24,0xc2cddd,"No macros. Create one in Macro Studio, then Save.");
    edge(&route,&v,MON_B);edge(&route,&v,MON_B);assert(v.screen==SCREEN_HOME);
    v.selected=2;edge(&route,&v,MON_A);edge(&route,&v,MON_A);
    assert(v.screen==SCREEN_CONTROLLER && begins==1 && owner==GAMEPAD_EXCLUSIVE_SYNC);
    header_footer(&v,"STANDARD CONTROLLER");
    assert(controller_route_action(&route,&v,view_touch(&v,900,675)));assert(stops==1 && route.mode.exiting);
    assert(!controller_route_tick(&route,&v,true,MON_A,0,0,0,0));
    owner=GAMEPAD_EXCLUSIVE_OFF;assert(controller_route_tick(&route,&v,true,MON_A,0,0,0,0));
    assert(v.screen==SCREEN_DIAGNOSTICS);header_footer(&v,"USB DIAGNOSTICS");
    Action a=ACTION_NONE;assert(controller_route_input(&v,MON_A,false,&a) && a==ACTION_SEND);
    assert(!controller_route_action(&route,&v,a));assert(begins==1 && owner==GAMEPAD_EXCLUSIVE_OFF);
    assert(view_touch(&v,650,675)==a); /* visible Reset USB, same action as A */
    assert(controller_route_input(&v,MON_X,false,&a) && a==ACTION_CREATE_USB_LOG);
    const Screen screens[]={SCREEN_KEYBOARD,SCREEN_FILES,SCREEN_CONFIRM,SCREEN_TRANSFER,SCREEN_RESULT,SCREEN_CIV_PLACEHOLDER,SCREEN_LIVE,SCREEN_MACRO_DETAILS,SCREEN_MACRO_DELETE};
    const char *names[]={"USB KEYBOARD","TEXT FILES","COMPOSE & SEND","TEXT SENDER","RESULT","CIV EXPLOIT","LIVE KEYBOARD","MACRO DETAILS","DELETE MACRO"};
    for(size_t i=0;i<sizeof(screens)/sizeof(screens[0]);i++) { v.screen=screens[i];header_footer(&v,names[i]); }
    v.screen=SCREEN_KEYBOARD;
    for(int i=0;i<3;i++) { v.selected=i;draw(&v);assert(actual[(150+i*135)*1280+400]==rgb(0xd9aa54)); }
    macro_app_destroy(&macro_app);free(actual);free(expected);ui_shutdown();
    puts("PASS: production routing + raster rendering, menu isolation, hints/actions, focus, touch and shared chrome.");return 0;
}
