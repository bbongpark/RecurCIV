#define USB_MOCK_ONLY
#include "test_usb.c"
#include "macro_app.h"
#include "ui.h"
#include <stdlib.h>
#include <direct.h>
#include <windows.h>

static MacroApp app;
static View v;
static uint64_t now=1;
static char root[MAX_PATH];
static uint32_t *pixels;
static void action(int a) { assert(macro_app_action(&app,&v,(Action)a,now,true)); }
static Action input(uint32_t down,uint32_t held,unsigned touches,int x,int y) {
    return macro_app_input(&app,&v,down,held,0,0,touches,x,y,++now);
}
static void tick(unsigned complete,unsigned advance) {
    now+=advance;if(complete && gamepad_status()->transfer_pending) gamepad_in_ep.CompletionEvent.fired=true;
    poll(now);macro_app_tick(&app,&v,true,0,0,0,0,0,0,now);
}
static void connect_usb(void) {
    assert(keyboard_init()==0 && gamepad_init()==0);bus_state=UsbState_Configured;poll(now);
    request(0x81,6,0x2200,1,63,++now);settle(now);now+=301;poll(now);assert(gamepad_ready());
}
static void render(void) {
    view_render(&v,pixels,1280*4);size_t n=view_macro_control_count(&v);
    assert(n);unsigned backs=0;
    for(unsigned i=0;i<n;i++) {
        ViewControl c=view_macro_control(&v,i);char label[96];view_macro_control_label(&v,i,label,sizeof(label));
        assert(c.x>=48 && c.y>=136 && c.x+c.w<=1232 && c.y+c.h<=704);
        assert(view_touch(&v,c.x+c.w/2,c.y+c.h/2)==c.action);backs+=c.action==ACTION_BACK;
        if(*label) assert(ui_text_width(20,label)<=c.w-24);
        for(unsigned j=i+1;j<n;j++) {
            ViewControl d=view_macro_control(&v,j);assert(c.x+c.w<=d.x || d.x+d.w<=c.x || c.y+c.h<=d.y || d.y+d.h<=c.y);
        }
    }
    assert(backs<=1);
}
int main(void) {
    char base[MAX_PATH];assert(GetTempPathA(sizeof(base),base));
    snprintf(root,sizeof(root),"%srecurciv-macro-app-%lu",base,(unsigned long)GetCurrentProcessId());assert(_mkdir(root)==0);
    macro_app_init(&app,root);assert(ui_init());pixels=calloc(1280u*720,4);assert(pixels);connect_usb();
    v.screen=SCREEN_CONTROLLER_MENU;action(ACTION_MACRO_STUDIO);
    assert(input(0,0,1,100,330)==ACTION_NONE && v.screen==SCREEN_MACRO_STUDIO); /* Held menu touch cannot activate Record. */
    assert(input(0,0,0,0,0)==ACTION_NONE);
    action(input(MON_A,MON_A,0,0,0));assert(v.screen==SCREEN_MACRO_EDITOR);render();
    action(ACTION_MS_INSERT);assert(v.screen==SCREEN_MACRO_STEP);render();
    action(ACTION_MS_FIELD0+2);action(ACTION_MS_FIELD0+6);assert(app.draft.report.buttons==0x44);
    action(ACTION_MS_FIELD0+13);macro_app_dialog_result(&app,&v,true,"255");assert(app.draft.report.lx==255);
    action(ACTION_MS_FIELD0+17);macro_app_dialog_result(&app,&v,true,"75");assert(app.draft.duration_ms==75);
    action(ACTION_MS_APPLY);assert(app.editor.current.sequence.count==1);render();
    action(ACTION_MS_DELAY);action(ACTION_MS_APPLY);assert(app.editor.current.sequence.count==2);
    action(ACTION_MS_MOVE_UP);assert(app.cursor==0 && app.editor.current.sequence.steps[0].report.buttons==0);
    action(ACTION_MS_UNDO);assert(app.editor.current.sequence.steps[0].report.buttons==0x44);
    action(ACTION_MS_REDO);assert(app.editor.current.sequence.steps[0].report.buttons==0);
    action(ACTION_MS_MOVE_DOWN);assert(app.cursor==1 && app.editor.current.sequence.steps[0].report.buttons==0x44);
    action(ACTION_MS_REMOVE);assert(app.editor.current.sequence.count==1);action(ACTION_MS_UNDO);assert(app.editor.current.sequence.count==2);
    action(ACTION_MS_EDIT);action(ACTION_MS_FIELD0+17);macro_app_dialog_result(&app,&v,true,"123");action(ACTION_MS_APPLY);
    assert(app.editor.current.sequence.steps[app.cursor].duration_ms==123);
    action(ACTION_MS_REPEATS);macro_app_dialog_result(&app,&v,true,"2");assert(app.editor.current.repeat_count==2);
    action(ACTION_MS_SAVE);macro_app_dialog_result(&app,&v,true,"Macro one");assert(!macro_editor_dirty(&app.editor));
    action(ACTION_BACK);assert(v.screen==SCREEN_MACRO_DETAILS);render();
    action(ACTION_MS_DUPLICATE);macro_app_dialog_result(&app,&v,true,"Macro copy");
    action(ACTION_MS_RENAME);macro_app_dialog_result(&app,&v,true,"Macro renamed");assert(!strcmp(app.editor.current.name,"Macro renamed"));
    action(ACTION_BACK);assert(v.screen==SCREEN_MACRO_LIBRARY && v.file_rows==2);render();
    action(ACTION_ROW0);assert(v.screen==SCREEN_MACRO_DETAILS);
    action(ACTION_MS_PLAY);assert(v.screen==SCREEN_MACRO_PLAY && app.player.owned);render();
    action(ACTION_BACK);assert(v.screen==SCREEN_MACRO_PLAY && app.player.owned);
    unsigned limit=10000;while(app.player.owned && --limit) tick(1,32);
    assert(limit && app.player.state==MP_FINISHED && app.player.neutral_confirmed);render();
    action(ACTION_BACK);action(ACTION_MS_PLAY_ONCE);assert(v.macro_repeat==1 && app.editor.current.repeat_count==2);
    limit=10000;while(app.player.owned && --limit) tick(1,32);
    assert(limit && app.player.state==MP_FINISHED && app.player.repeat==1 && app.player.neutral_confirmed);
    action(ACTION_BACK);action(ACTION_MS_PLAY);
    while(app.player.state!=MP_PENDING) tick(1,1);
    uint32_t old=gamepad_in_urb;gamepad_in_report_override=old+111;tick(1,1);
    assert(app.player.state==MP_PENDING);gamepad_in_report_override=0;
    action(ACTION_MS_STOP);limit=100;while(app.player.owned && --limit) tick(1,1);
    assert(limit && app.player.state==MP_CANCELLED && app.player.neutral_confirmed);
    action(ACTION_BACK);action(ACTION_MS_PLAY);limit=100;
    while(app.player.state!=MP_PENDING && --limit) tick(1,1);assert(limit);
    action(ACTION_MS_STOP);tick(0,2000);tick(0,2000);assert(app.player.owned && app.player.state==MP_BLOCKED);render();
    action(ACTION_BACK);assert(v.screen==SCREEN_MACRO_PLAY);
    action(ACTION_MS_RETRY);limit=100;while(app.player.owned && --limit) tick(1,1);
    assert(limit && app.player.state==MP_CANCELLED && app.player.neutral_confirmed);
    action(ACTION_BACK);action(ACTION_MS_PLAY);tick(1,1);
    bus_state=UsbState_Suspended;tick(0,1);assert(app.player.owned && app.player.state==MP_CLEANUP);
    bus_state=UsbState_Configured;tick(0,301);tick(0,301);
    limit=100;while(app.player.owned && --limit) tick(1,1);
    assert(limit && app.player.state==MP_INTERRUPTED && app.player.neutral_confirmed);
    action(ACTION_BACK);action(ACTION_BACK);action(ACTION_BACK);assert(v.screen==SCREEN_CONTROLLER_MENU);
    action(ACTION_MACRO_STUDIO);
    action(ACTION_STUDIO_RECORD);render();action(ACTION_MS_RECORD_START);
    macro_app_tick(&app,&v,true,MON_A,0,0,0,0,0,++now);assert(app.recorder.state==MR_ARMING);
    macro_app_tick(&app,&v,true,0,0,0,0,0,0,++now);assert(app.recorder.state==MR_RECORDING);render();
    assert(input(0,0,0,0,0)==ACTION_NONE);
    assert(input(MON_B|MON_PLUS,MON_B|MON_PLUS,0,0,0)==ACTION_NONE && app.recorder.state==MR_RECORDING);
    unsigned before=gamepad_in_posts;
    macro_app_tick(&app,&v,true,MON_A|MON_ZR,32767,0,0,0,0,now+50);
    macro_app_tick(&app,&v,true,0,0,0,0,0,0,now+100);now+=150;
    assert(input(0,0,1,100,200)==ACTION_NONE && app.recorder.state==MR_RECORDING);
    action(input(0,0,1,1100,675));assert(app.recorder.sequence.count==3 && gamepad_in_posts==before);
    action(ACTION_MS_DISCARD);assert(v.screen==SCREEN_MACRO_DISCARD && v.selected==1);
    action(ACTION_BACK);assert(v.screen==SCREEN_MACRO_RECORD && app.recorder.sequence.count==3);
    action(ACTION_MS_REVIEW);assert(v.screen==SCREEN_MACRO_EDITOR && app.editor.current.sequence.count==3);
    action(ACTION_BACK);assert(v.screen==SCREEN_MACRO_DISCARD);render();action(ACTION_BACK);assert(v.screen==SCREEN_MACRO_EDITOR);
    action(ACTION_MS_SAVE);macro_app_dialog_result(&app,&v,true,"Recording");assert(!macro_editor_dirty(&app.editor));
    action(ACTION_MS_INSERT);action(ACTION_MS_FIELD0+17);macro_app_dialog_result(&app,&v,true,"0");assert(app.draft.duration_ms==500);
    macro_app_dialog_result(&app,&v,false,NULL);action(ACTION_BACK);
    action(ACTION_MS_INSERT);action(ACTION_MS_APPLY);
    action(ACTION_MS_SAVE);macro_app_dialog_result(&app,&v,true,"Macro copy");assert(macro_editor_dirty(&app.editor));
    assert(!strcmp(app.file_key,"Recording") && !strcmp(app.editor.current.name,"Recording"));
    action(ACTION_BACK);action(ACTION_MS_DISCARD);assert(v.screen==SCREEN_MACRO_STUDIO);
    v.screen=SCREEN_CONTROLLER_MENU;action(ACTION_MACRO_LIBRARY);assert(v.file_rows==3);
    char invalid[MAX_PATH];snprintf(invalid,sizeof(invalid),"%s/Broken.rcm",root);
    FILE *f=fopen(invalid,"wb");assert(f);assert(fwrite("invalid",1,7,f)==7);assert(fclose(f)==0);
    action(ACTION_MACRO_REFRESH);assert(v.file_rows==4);action(ACTION_ROW0);
    assert(v.screen==SCREEN_MACRO_LIBRARY && strstr(v.macro_status,"Invalid"));assert(remove(invalid)==0);
    action(ACTION_MACRO_REFRESH);v.selected=1;action(ACTION_MACRO_DELETE_ASK);assert(v.screen==SCREEN_MACRO_DELETE && v.selected==1);render();
    action(ACTION_BACK);assert(v.screen==SCREEN_MACRO_LIBRARY && v.file_rows==3);
    v.selected=1;action(ACTION_MACRO_DELETE_ASK);action(ACTION_MACRO_DELETE_CONFIRM);assert(v.screen==SCREEN_MACRO_LIBRARY && v.file_rows==2);
    MacroDocument check;assert(macro_store_load(root,"Recording",&check)==MACRO_STORE_OK && check.sequence.count==3);
    /* Tests create/delete only their own generated files, never a user's store. */
    const char *names[]={"Macro copy","Recording"};
    for(unsigned i=0;i<2;i++) assert(macro_store_delete(root,names[i])==MACRO_STORE_OK);
    macro_app_destroy(&app);free(pixels);ui_shutdown();keyboard_exit();assert(_rmdir(root)==0);
    puts("PASS: production Macro Studio editing/dialog validation, save/load/rename/duplicate/library, local recording, real async USB playback, cancellation, stale completion and disconnect cleanup.");return 0;
}
