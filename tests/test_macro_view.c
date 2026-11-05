#include "view.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    View v;memset(&v,0,sizeof(v));
    v.screen=SCREEN_CONTROLLER;
    assert(view_touch(&v,620,670)==ACTION_NONE);
    assert(view_touch(&v,850,670)==ACTION_DIAGNOSTICS);
    assert(view_touch(&v,1060,670)==ACTION_BACK);
    v.screen=SCREEN_CONTROLLER_MENU;
    assert(view_touch(&v,100,200)==ACTION_CONTROLLER_STANDARD);
    assert(view_touch(&v,100,330)==ACTION_MACRO_STUDIO);
    assert(view_touch(&v,100,460)==ACTION_MACRO_LIBRARY);
    v.screen=SCREEN_MACRO_STUDIO;v.selected=0;
    assert(view_touch(&v,100,200)==ACTION_STUDIO_MANUAL);
    assert(view_touch(&v,100,380)==ACTION_STUDIO_RECORD);
    assert(view_studio_move(0,1)==1 && view_studio_move(1,-1)==0);
    int dx,dy;LiveNav nav={0};
    view_menu_stick_direction(11999,11999,&dx,&dy);assert(!dx && !dy);
    view_menu_stick_direction(12000,-12000,&dx,&dy);assert(!dx && dy==1);
    assert(live_nav_step(&nav,dx,dy,100));assert(!live_nav_step(&nav,dx,dy,449));
    assert(live_nav_step(&nav,dx,dy,450));
    assert(view_studio_move(0,dy)==1);
    assert(ui_init());uint32_t *pixels=calloc(1280*720,sizeof(*pixels));assert(pixels);
    view_render(&v,pixels,1280*4);assert(pixels[160*1280+640]==0xff54aad9);
    v.selected=1;view_render(&v,pixels,1280*4);assert(pixels[330*1280+640]==0xff54aad9);
    assert(pixels[160*1280+640]!=0xff54aad9);
    const Screen screens[]={SCREEN_HOME,SCREEN_KEYBOARD,SCREEN_LIVE,SCREEN_FILES,SCREEN_CONFIRM,SCREEN_TRANSFER,
        SCREEN_CONTROLLER_MENU,SCREEN_CONTROLLER,SCREEN_MACRO_STUDIO,SCREEN_MACRO_LIBRARY,SCREEN_MACRO_DETAILS,
        SCREEN_MACRO_DELETE,SCREEN_CIV_PLACEHOLDER,SCREEN_DIAGNOSTICS,SCREEN_RESULT,
        SCREEN_MACRO_EDITOR,SCREEN_MACRO_STEP,SCREEN_MACRO_RECORD,SCREEN_MACRO_PLAY,SCREEN_MACRO_DISCARD};
    for(size_t i=0;i<sizeof(screens)/sizeof(screens[0]);i++) {
        v.screen=screens[i];view_render(&v,pixels,1280*4);
        assert(pixels[75*1280+640]==0xff6b5340); /* common divider */
        assert(pixels[640*1280+500]==0xff54aad9); /* common footer border */
    }
    free(pixels);ui_shutdown();v.screen=SCREEN_MACRO_STUDIO;
    assert(view_touch(&v,1100,670)==ACTION_BACK);
    assert(view_touch(&v,500,410)==ACTION_STUDIO_RECORD);
    v.screen=SCREEN_MACRO_LIBRARY;v.file_rows=2;v.macro_count=2;
    assert(view_touch(&v,60,196)==ACTION_ROW0);
    assert(view_touch(&v,60,246)==ACTION_ROW0+1);
    assert(view_touch(&v,60,280)==ACTION_NONE);
    assert(view_touch(&v,890,650)==ACTION_MACRO_REFRESH);
    assert(view_touch(&v,1100,650)==ACTION_BACK);
    v.screen=SCREEN_MACRO_DETAILS;
    assert(view_touch(&v,600,660)==ACTION_MACRO_PREV);
    assert(view_touch(&v,750,660)==ACTION_MACRO_NEXT);
    assert(view_touch(&v,900,660)==(Action)ACTION_MS_PLAY_ONCE);
    assert(view_touch(&v,1100,660)==ACTION_BACK);
    v.screen=SCREEN_MACRO_DELETE;
    assert(view_touch(&v,80,560)==ACTION_MACRO_DELETE_CONFIRM);
    assert(view_touch(&v,1100,670)==ACTION_BACK);
    char label[256],detail[120];
    MacroStep step={{0,8,128,128,128,128,0},500};
    view_macro_step(&step,0,label,sizeof(label),detail,sizeof(detail));
    assert(!strcmp(label,"1. Neutral / delay") && !strcmp(detail,"500 ms"));
    step.report.buttons=0x0004|0x0080;step.report.hat=0;
    view_macro_step(&step,1,label,sizeof(label),detail,sizeof(detail));
    assert(strstr(label,"A + ZR") && strstr(label,"D-pad Up") && strstr(label,"2."));
    step.report.buttons=0;step.report.hat=8;step.report.lx=255;step.report.ry=0;
    view_macro_step(&step,2,label,sizeof(label),detail,sizeof(detail));
    assert(strstr(label,"Left Stick Right") && strstr(label,"Right Stick Up"));
    assert(strstr(detail,"L X:255 Y:128") && strstr(detail,"R X:128 Y:0"));
    step.report.buttons=0x0fff;step.report.hat=7;
    view_macro_step(&step,3,label,sizeof(label),detail,sizeof(detail));
    assert(strstr(label,"L Stick Click") && strstr(label,"R Stick Click") && strstr(label,"D-pad Up-Left"));
    char short_label[10],short_detail[6];memset(short_label,'!',sizeof(short_label));memset(short_detail,'!',sizeof(short_detail));
    view_macro_step(&step,4,short_label,8,short_detail,4);
    assert(short_label[7]==0 && short_label[8]=='!' && short_label[9]=='!');
    assert(short_detail[3]==0 && short_detail[4]=='!' && short_detail[5]=='!');
    puts("PASS: macro library and detail touch routing.");
    return 0;
}
