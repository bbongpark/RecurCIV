#include "text.h"
#include "view.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check_key(uint32_t cp,int mod,int key) {
    CharacterKeys got;assert(character_keys(cp,&got));assert(got.count==1);
    assert(got.keys[0].modifier==mod);assert(got.keys[0].key==key);
}
int main(void) {
    TextPlan plan;TextError error;
    {
        assert(text_plan("Hello",5,false,&plan,&error));
        assert(plan.count==5 && plan.characters==5 && plan.lines==0);
        const int keys[]={11,8,15,15,18};
        for(int i=0;i<5;i++) { assert(plan.strokes[i].key==keys[i]);assert(plan.strokes[i].modifier==(i==0?2:0)); }
        text_plan_free(&plan);
        for(unsigned c=32;c<127;c++) { CharacterKeys out;assert(character_keys(c,&out)); }
    }
    check_key('a',0,4);check_key('q',0,0x14);check_key('M',2,0x10);
    check_key('z',0,0x1d);check_key('w',0,0x1a);check_key('@',2,0x1f);check_key('0',0,0x27);
    const char crlf[]="\xef\xbb\xbf" "a\r\nb\rc\n\t";
    assert(text_plan(crlf,sizeof(crlf)-1,false,&plan,&error));
    assert(plan.characters==7 && plan.count==7 && plan.lines==3 && plan.tabs==1);text_plan_free(&plan);
    assert(!text_plan("ok\xf0\x9f\x98\x80",6,false,&plan,&error));
    assert(!plan.strokes && plan.count==0 && error.codepoint==0x1f600 && error.character==3 && error.byte_offset==2);
    const char *bad[]={"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82","\x80"};
    for(int i=0;i<5;i++) { assert(!text_plan(bad[i],strlen(bad[i]),false,&plan,&error));assert(error.invalid_utf8 && !plan.strokes); }
    assert(!text_plan("a\0b",3,false,&plan,&error));assert(error.character==2);
    assert(text_plan("",0,false,&plan,&error));assert(!plan.count);text_plan_free(&plan);
    assert(!text_plan("\xc3\xa9",2,false,&plan,&error));assert(error.codepoint==0xe9);
    View v={0};
    assert(view_home_action(SCREEN_HOME,false,false,true,false,0)==ACTION_DIAGNOSTICS);
    assert(view_home_action(SCREEN_CONTROLLER,false,false,true,false,0)==ACTION_NONE);
    assert(view_home_action(SCREEN_HOME,true,false,true,true,0)==ACTION_QUIT);
    assert(view_home_action(SCREEN_HOME,false,true,false,false,0)==ACTION_BACK);
    assert(view_home_action(SCREEN_HOME,false,false,false,true,0)==ACTION_MENU_CIV);
    assert(view_home_action(SCREEN_HOME,false,false,false,true,1)==ACTION_MENU_KEYBOARD);
    assert(view_home_action(SCREEN_HOME,false,false,false,true,2)==ACTION_MENU_CONTROLLER);
    assert(view_home_move(0,-1)==2 && view_home_move(1,1)==2 && view_home_move(0,1)==1);
    assert(ui_init());
    const char *titles[]={"CIV Exploit","USB Keyboard","USB Controller"};
    const char *sub1[]={"Future Features","Text & Live Input","Control & Macros"};
    for(int i=0;i<3;i++) {
        int widths[]={ui_text_width(32,titles[i]),ui_text_width(20,sub1[i])};
        for(int j=0;j<2;j++) {
            int left=58+i*400, x=view_home_text_x(i,widths[j]);
            assert(widths[j]>0 && widths[j]<=368 && x>=left+3 && x+widths[j]<=left+371);
            assert((x-left)-(left+374-(x+widths[j]))>=-1 && (x-left)-(left+374-(x+widths[j]))<=1);
        }
    }
    ui_shutdown();
    assert(view_touch(&v,100,400)==ACTION_MENU_CIV);
    assert(view_touch(&v,500,400)==ACTION_MENU_KEYBOARD);
    assert(view_touch(&v,900,400)==ACTION_MENU_CONTROLLER);
    assert(view_touch(&v,440,400)==ACTION_NONE);
    v.screen=SCREEN_CONTROLLER;
    assert(view_touch(&v,900,680)==ACTION_DIAGNOSTICS);
    assert(view_touch(&v,1100,680)==ACTION_BACK);
    assert(view_touch(&v,700,680)==ACTION_NONE);
    assert(view_touch(&v,590,680)==ACTION_NONE);
    v.screen=SCREEN_CIV_PLACEHOLDER;
    assert(view_touch(&v,1100,670)==ACTION_BACK);
    v.screen=SCREEN_KEYBOARD;
    assert(view_touch(&v,100,200)==ACTION_COMPOSE);assert(view_touch(&v,100,330)==ACTION_LIVE);assert(view_touch(&v,100,470)==ACTION_FILES);
    assert(view_keyboard_select(0)==ACTION_COMPOSE && view_keyboard_select(1)==ACTION_LIVE && view_keyboard_select(2)==ACTION_FILES);
    assert(view_keyboard_move(0,-1)==2 && view_keyboard_move(2,1)==0);
    assert(view_touch(&v,350,610)==ACTION_NONE);
    assert(view_touch(&v,900,678)==ACTION_NONE);
    v.screen=SCREEN_CONFIRM;
    v.composing=true;
    v.usb_ready=v.compose_valid=true;v.compose_page=1;v.compose_pages=3;
    assert(view_touch(&v,500,580)==ACTION_COMPOSE);
    assert(view_touch(&v,950,410)==ACTION_COMPOSE_PREV);
    assert(view_touch(&v,1100,410)==ACTION_COMPOSE_NEXT);
    assert(view_touch(&v,100,600)==ACTION_SEND);
    assert(view_touch(&v,1000,600)==ACTION_BACK);
    v.composing=false;
    assert(view_touch(&v,100,678)==ACTION_CAPS_LOCK);
    assert(view_touch(&v,450,678)==ACTION_AUTOINDENT);
    assert(view_touch(&v,720,678)==ACTION_DIAGNOSTICS);
    assert(view_touch(&v,900,678)==ACTION_ENTER_PAUSE);
    v.screen=SCREEN_TRANSFER;assert(view_touch(&v,1100,678)==ACTION_ABORT);assert(view_touch(&v,100,300)==ACTION_NONE);
    v.screen=SCREEN_FILES;v.file_rows=2;assert(view_touch(&v,200,275)==ACTION_ROW0+1);assert(view_touch(&v,200,350)==ACTION_NONE);
    v.screen=SCREEN_DIAGNOSTICS;
    assert(view_touch(&v,1100,670)==ACTION_BACK);
    assert(view_touch(&v,650,670)==ACTION_SEND);
    assert(view_touch(&v,850,670)==ACTION_CREATE_USB_LOG);
    assert(view_touch(&v,600,648)==ACTION_SEND);
    assert(view_touch(&v,1029,703)==ACTION_CREATE_USB_LOG);
    assert(view_touch(&v,1030,670)==ACTION_NONE);
    assert(view_touch(&v,1012,617)==ACTION_NONE);
    assert(view_touch(&v,1231,672)==ACTION_NONE);
    assert(view_touch(&v,1232,640)==ACTION_NONE);
    assert(view_touch(&v,599,670)==ACTION_NONE);
    puts("PASS: US text encoding, UTF-8 rejection, preflight, CRLF, keyboard navigation and touch routing.");return 0;
}
