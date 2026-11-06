#define USB_MOCK_ONLY
#include "test_usb.c"
#include "macro_app.h"
#include "capture_control.h"
#include "ui.h"
#include <stdlib.h>
#include <direct.h>
#include <windows.h>

static MacroApp app;
static View v;
static uint64_t now=1;
static char root[MAX_PATH];
static uint32_t *pixels;
static AppletHookCookie *capture_hook;
static bool capture_fail;
static uint64_t capture_clock(void) { return now; }
void appletHook(AppletHookCookie *cookie,void (*fn)(AppletHookType,void *),void *param) {
    assert(!capture_hook);capture_hook=cookie;cookie->callback=fn;cookie->param=param;
}
void appletUnhook(AppletHookCookie *cookie) { assert(cookie==capture_hook);capture_hook=NULL; }
AppletFocusState appletGetFocusState(void) { return AppletFocusState_InFocus; }
Result appletSetRequiresCaptureButtonShortPressedMessage(bool enabled) { return enabled && capture_fail?123:0; }
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
    assert(n || (v.screen==SCREEN_MACRO_RECORD && v.macro_record_cleanup));unsigned backs=0;
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
/* Match main's live-input ordering: touchscreen Stop precedes the sampled
   frame's capture/output. Other macro modes retain their original ordering. */
static void live_frame(unsigned complete,unsigned advance,uint32_t held,int32_t lx,int32_t ry,unsigned touches,int x,int y) {
    now+=advance;if(complete && gamepad_status()->transfer_pending) gamepad_in_ep.CompletionEvent.fired=true;
    poll(now);bool active=macro_live_busy(&app.live);
    bool session=active && app.live.state!=ML_CLEANUP && app.live.state!=ML_BLOCKED;
    capture_control_update(session,session && !touches && !app.replace_pending &&
        (app.live.state==ML_READY || app.live.state==ML_RECORDING),now);
    if(active) {
        Action a=macro_app_input(&app,&v,held,held,lx,0,touches,x,y,now);action(a);
        if(a!=ACTION_NONE) capture_control_discard(now);
        else if(capture_control_take(now)) macro_app_capture(&app,&v,now);
    }
    macro_app_tick(&app,&v,true,held,lx,0,0,ry,touches,now);
    if(!active) action(macro_app_input(&app,&v,held,held,lx,0,touches,x,y,now));
}
static void capture_press(void) {
    now+=400;capture_control_update(true,true,now);
    MacroRecordState before=app.recorder.state;
    assert(capture_hook);capture_hook->callback(AppletHookType_OnCaptureButtonShortPressed,capture_hook->param);
    capture_hook->callback(AppletHookType_OnCaptureButtonShortPressed,capture_hook->param);
    assert(app.recorder.state==before); /* callback only enqueues */
    live_frame(1,1,0,0,0,0,0,0);
}
static void blocked_actions(void) {
    const int actions[]={ACTION_BACK,ACTION_QUIT,ACTION_MS_REVIEW,ACTION_MS_EDIT,ACTION_MS_SAVE,
        ACTION_MS_PLAY,ACTION_MS_PLAY_ONCE,ACTION_MS_DISCARD,ACTION_MACRO_LIBRARY,ACTION_MENU_CIV,
        ACTION_DIAGNOSTICS,ACTION_MS_FIELD0+2};
    size_t count=app.recorder.sequence.count;uint64_t duration=app.recorder.sequence.duration_ms;
    for(size_t i=0;i<sizeof(actions)/sizeof(actions[0]);i++) {
        action(actions[i]);assert(v.screen==SCREEN_MACRO_RECORD && macro_app_busy(&app));
        assert(app.dialog==MD_NONE && !app.player.owned && !app.editor.current.sequence.count);
    }
    app.dialog=MD_SAVE;macro_app_dialog_result(&app,&v,true,"Should not save");
    assert(app.dialog==MD_NONE && !app.has_file && app.recorder.sequence.count==count && app.recorder.sequence.duration_ms==duration);
}
static void live_flow(void) {
    v.screen=SCREEN_CONTROLLER_MENU;action(ACTION_MACRO_STUDIO);
    assert(macro_app_action(&app,&v,(Action)ACTION_MS_LIVE_RECORD,now,false));
    assert(v.screen==SCREEN_MACRO_STUDIO && !app.live.owned);
    /* Another exclusive writer owns USB: reject entry without preemption. */
    assert(gamepad_exclusive_begin());action(ACTION_MS_LIVE_RECORD);
    assert(!app.live.owned && v.screen==SCREEN_MACRO_STUDIO);
    assert(gamepad_exclusive_stop());unsigned limit=100;
    while(gamepad_exclusive_status()!=GAMEPAD_EXCLUSIVE_OFF && --limit) tick(1,1);assert(limit);
    v.selected=2;assert(input(0,0,0,0,0)==ACTION_NONE);
    action(input(MON_A,MON_A,0,0,0));assert(v.screen==SCREEN_MACRO_RECORD && app.record_live);render();
    assert(app.live.state==ML_SYNC && macro_app_busy(&app) && app.recorder.state==MR_IDLE);blocked_actions();
    macro_app_capture(&app,&v,now);assert(app.recorder.state==MR_IDLE);
    limit=100;
    while(app.live.state==ML_SYNC && --limit) live_frame(1,1,MON_A,0,0,0,0,0);
    assert(limit && app.live.state==ML_ARMING && !app.recorder.sequence.count);render();
    /* An activation touch held over Stop cannot end or arm the session. */
    live_frame(1,1,0,0,0,1,100,200);assert(app.live.state==ML_ARMING);
    live_frame(1,1,0,32767,0,0,0,0);assert(app.live.state==ML_ARMING);
    live_frame(1,1,0,0,-32768,0,0,0);assert(app.live.state==ML_ARMING);
    live_frame(1,1,0,0,0,1,100,200);assert(app.live.state==ML_ARMING);
    live_frame(1,1,0,0,0,0,0,0);assert(app.live.state==ML_READY && app.recorder.state==MR_IDLE);render();
    live_frame(1,1,MON_B,0,0,0,0,0);
    limit=10;while(!((SwitchGamepadReport *)gamepad_in_report)->buttons && --limit) live_frame(1,1,MON_B,0,0,0,0,0);
    assert(limit && app.recorder.state==MR_IDLE); /* forwarding BEFORE capture */
    capture_press();assert(app.live.state==ML_RECORDING);
    uint64_t start=now;
    uint32_t buttons=MON_A|MON_ZR|MON_PLUS|MON_MINUS;
    live_frame(1,25,buttons,32767,-32768,0,0,0);
    live_frame(1,5,buttons,32767,-32768,0,0,0);
    SwitchGamepadReport expected=controller_monitor_report(buttons,32767,0,0,-32768,true);
    assert(!memcmp(&app.recorder.current,&expected,8));
    limit=10;while(memcmp(gamepad_in_report,&expected,8) && --limit) live_frame(1,1,buttons,32767,-32768,0,0,0);
    assert(limit && !memcmp(gamepad_in_report,&expected,8));
    assert(input(MON_B|MON_PLUS|MON_MINUS,MON_B|MON_PLUS|MON_MINUS,0,0,0)==ACTION_NONE);
    live_frame(1,50,0,0,0,0,0,0);
    assert(app.recorder.sequence.count==2 && app.recorder.sequence.steps[0].duration_ms==25);
    /* Capture notifications stop recording, never enter USB/macro reports. */
    capture_press();
    assert(app.recorder.state==MR_STOPPED && app.live.state==ML_READY && app.live.owned);render();
    size_t count=app.recorder.sequence.count;uint64_t duration=app.recorder.sequence.duration_ms;
    assert(count==3 && duration==now-start && !app.recorder.sequence.steps[2].report.buttons);
    blocked_actions();
    live_frame(1,1,MON_X,0,0,0,0,0);
    macro_app_capture(&app,&v,now);assert(app.replace_pending);render();
    macro_app_capture(&app,&v,++now);assert(app.replace_pending && app.recorder.sequence.duration_ms==duration);
    /* Physical A cannot confirm replacement. Capture never confirms it either. */
    assert(input(MON_A,MON_A,0,0,0)==ACTION_NONE);
    action(ACTION_MS_KEEP_RECORD);assert(!app.replace_pending && app.recorder.sequence.count==count);
    limit=10;SwitchGamepadReport after=controller_monitor_report(MON_X,0,0,0,0,true);
    while(memcmp(gamepad_in_report,&after,8) && --limit) live_frame(1,1,MON_X,0,0,0,0,0);
    assert(limit && app.recorder.sequence.duration_ms==duration); /* forwarding AFTER capture */
    action(ACTION_MS_LIVE_EXIT);assert(app.live.state==ML_CLEANUP);
    /* A stale completion cannot release cleanup ownership. */
    gamepad_in_report_override=gamepad_in_urb+111;tick(1,1);assert(app.live.owned);gamepad_in_report_override=0;
    tick(0,2600);assert(app.live.state==ML_BLOCKED && app.live.owned);render();blocked_actions();
    assert(input(0,0,0,0,0)==ACTION_NONE);
    assert(input(MON_A,MON_A,0,0,0)==ACTION_NONE && app.live.state==ML_BLOCKED);
    assert(input(0,0,0,0,0)==ACTION_NONE);
    action(input(0,0,1,1100,675));assert(app.live.state==ML_CLEANUP);
    limit=100;while(app.live.owned && --limit) tick(1,1);
    assert(limit && app.live.neutral_confirmed && !macro_app_busy(&app));render();
    assert(app.recorder.sequence.count==count && app.recorder.sequence.duration_ms==duration);
    action(ACTION_MS_REVIEW);assert(v.screen==SCREEN_MACRO_EDITOR && app.editor.current.sequence.count==count);
    action(ACTION_MS_EDIT);action(ACTION_MS_FIELD0+17);macro_app_dialog_result(&app,&v,true,"40");action(ACTION_MS_APPLY);
    assert(app.editor.current.sequence.steps[0].duration_ms==40);
    action(ACTION_MS_SAVE);macro_app_dialog_result(&app,&v,true,"Live capture");assert(!macro_editor_dirty(&app.editor));
    action(ACTION_BACK);action(ACTION_BACK);assert(v.screen==SCREEN_MACRO_LIBRARY);
    size_t row=0;while(row<app.count && strcmp(app.names[row],"live capture")) row++;
    assert(row<app.count && row<8);action(ACTION_ROW0+(int)row);
    assert(v.screen==SCREEN_MACRO_DETAILS && app.editor.current.sequence.count==count && app.editor.current.sequence.steps[0].duration_ms==40);
    action(ACTION_MS_PLAY_ONCE);limit=10000;while(app.player.owned && --limit) tick(1,10);
    assert(limit && app.player.state==MP_FINISHED && app.player.neutral_confirmed);
    assert(macro_store_delete(root,"Live capture")==MACRO_STORE_OK);
    /* Actual usb:ds suspension interrupts capture and never auto-resumes it. */
    action(ACTION_BACK);action(ACTION_BACK);action(ACTION_BACK);assert(v.screen==SCREEN_CONTROLLER_MENU);
    capture_control_update(false,false,now);capture_fail=true;
    action(ACTION_MACRO_STUDIO);action(ACTION_MS_LIVE_RECORD);
    limit=100;while(app.live.state!=ML_READY && --limit) tick(1,1);assert(limit);
    live_frame(1,1,0,0,0,0,0,0);
    assert(!capture_hook && strstr(capture_control_status(),"registration failed"));
    assert(input(0,0,0,0,0)==ACTION_NONE);
    action(input(0,0,1,850,675));tick(1,1);assert(app.live.state==ML_RECORDING);
    live_frame(1,50,MON_A,0,0,0,0,0);
    live_frame(1,25,MON_A,0,0,1,850,675);assert(app.recorder.state==MR_STOPPED && app.live.owned);
    duration=app.recorder.sequence.duration_ms;
    live_frame(1,1,MON_A,0,0,0,0,0);
    live_frame(1,1,MON_A,0,0,1,850,675);assert(app.replace_pending && app.recorder.sequence.duration_ms==duration);
    live_frame(1,1,MON_A,0,0,0,0,0);
    live_frame(1,1,MON_A,0,0,1,600,675);assert(!app.replace_pending && app.live.state==ML_RECORDING);
    assert(!app.recorder.sequence.count && app.recorder.state==MR_RECORDING);
    live_frame(1,50,MON_A,0,0,0,0,0);
    bus_state=UsbState_Suspended;tick(0,25);
    assert(app.recorder.state==MR_INTERRUPTED && app.live.owned && app.live.state==ML_CLEANUP);
    duration=app.recorder.sequence.duration_ms;count=app.recorder.sequence.count;
    bus_state=UsbState_Configured;tick(0,301);tick(0,301);
    limit=100;while(app.live.owned && --limit) tick(1,1);
    assert(limit && app.live.neutral_confirmed && app.recorder.state==MR_INTERRUPTED);
    assert(app.recorder.sequence.duration_ms==duration && app.recorder.sequence.count==count);
    action(ACTION_MS_REVIEW);assert(app.editor.current.sequence.count==count);
    action(ACTION_BACK);action(ACTION_MS_DISCARD);assert(v.screen==SCREEN_MACRO_STUDIO);
    capture_control_shutdown();capture_fail=false;
}
int main(void) {
    char base[MAX_PATH];assert(GetTempPathA(sizeof(base),base));
    snprintf(root,sizeof(root),"%srecurciv-macro-app-%lu",base,(unsigned long)GetCurrentProcessId());assert(_mkdir(root)==0);
    macro_app_init(&app,root);capture_control_init(capture_clock);
    assert(ui_init());pixels=calloc(1280u*720,4);assert(pixels);connect_usb();
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
    action(ACTION_MS_LIVE_RECORD);assert(v.screen==SCREEN_MACRO_DISCARD && !app.live.owned && app.recorder.sequence.count==3);
    action(ACTION_BACK);assert(v.screen==SCREEN_MACRO_RECORD && app.pending_action==ACTION_NONE && app.recorder.sequence.count==3);
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
    live_flow();
    macro_app_destroy(&app);free(pixels);ui_shutdown();keyboard_exit();assert(_rmdir(root)==0);
    puts("PASS: Macro Studio local/live capture, ownership gates, live Stop/Review/Edit/Save/Load/Replay, modal isolation, async USB, stale completions and interrupted cleanup/retry.");return 0;
}
