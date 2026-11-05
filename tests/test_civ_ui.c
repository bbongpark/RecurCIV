/* Production routing + renderer + real adapter/runner/USB, using the same
   deterministic usb:ds simulator as Phase 2. No hardware acceptance is assumed. */
#define main civ_usb_regression_main
#include "test_civ_usb.c"
#undef main
#include "civ_route.h"
#include "ui.h"
#include <stdlib.h>

static View app_view;
static CivRoute app_route;
static ControllerRoute app_controller;
static uint32_t *pixels;
static uint32_t rgb(unsigned n) { return 0xff000000u|((n&255)<<16)|(n&0xff00)|((n>>16)&255); }
static Action input(uint32_t down,uint32_t held,unsigned touches,int x,int y) {
    return civ_route_input(&app_route,&app_view,down,held,0,0,touches,x,y,clock_ms);
}
static void action(Action a) {
    assert(civ_route_action(&app_route,&app_view,a,clock_ms,application_idle));
}
static void release_input(void) { assert(input(0,0,0,0,0)==ACTION_NONE); }
static void update(void) { civ_route_update(&app_route,&app_view,clock_ms); }
static void ui_step(unsigned completed,unsigned advance) { step(completed,advance);update(); }
static void open_civ(void) {
    setup_usb();app_view=(View){0};app_route=(CivRoute){0};app_controller=(ControllerRoute){0};
    Action a=ACTION_NONE;
    assert(controller_route_input(&app_view,MON_A,false,&a) && a==ACTION_MENU_CIV);
    assert(controller_route_action(&app_controller,&app_view,a));update();
    assert(app_view.screen==SCREEN_CIV_PLACEHOLDER && !app_view.civ.requested);
    assert(input(0,MON_A,0,0,0)==ACTION_NONE);release_input();
}
static void ui_start(void) {
    Action a=input(MON_A,MON_A,0,0,0);assert(a==ACTION_CIV_START);action(a);
    assert(app_view.civ.requested && app_view.civ.progress.owned);release_input();
    assert(civ_sleep_info().held && civ_sleep_info().protected && auto_sleep_disabled);
    ui_step(0,1);
}
static void ui_reach(CivRunnerStage stage) { reach(stage);update(); }
static void ui_finish(CivRunnerStatus status) {
    finish(status);update();release_input();assert(!civ_sleep_info().held && !auto_sleep_disabled);
}
static void render(void) {
    view_render(&app_view,pixels,1280*4);
    assert(pixels[75*1280+640]==rgb(0x40536b));
    assert(pixels[640*1280+640]==rgb(0xd9aa54));
    assert(pixels[713*1280+640]==rgb(0xd9aa54));
    assert(pixels[100*1280+10]==rgb(0x0b1425));
    ViewFooter f=view_footer(&app_view);unsigned backs=0;
    assert(ui_text_width(20,f.hints)<=(f.count?f.buttons[0].x-82:1140));
    for(size_t i=0;i<f.count;i++) {
        ViewFooterButton b=f.buttons[i];
        assert(ui_text_width(20,b.label)<=b.w-24 && b.x>=48 && b.x+b.w<=1232);
        if(i) assert(f.buttons[i-1].x+f.buttons[i-1].w<b.x);
        assert(view_touch(&app_view,b.x+b.w/2,675)==b.action);
        assert(view_touch(&app_view,b.x+b.w/2,704)==ACTION_NONE);
        backs+=b.action==ACTION_BACK;
        assert(pixels[648*1280+b.x+b.w/2]==rgb(i==(size_t)app_view.selected?0xd9aa54:0x40536b));
    }
    assert(backs<=1);
    for(size_t i=0;i<1280u*720;i++) assert(pixels[i]!=rgb(0x34c5db));
    assert(ui_text_width(32,view_civ_status(&app_view))<=1136);
    if(app_view.civ.requested || app_view.civ.progress.owned) {
        size_t total=app_view.civ.progress.runner.lines_total;
        if(!total) total=87;
        unsigned width=(unsigned)(1136*app_view.civ.progress.runner.lines_completed/total);
        if(width>16) assert(pixels[464*1280+72+width/2]==rgb(0xd9aa54));
        if(width<1120) assert(pixels[464*1280+72+width+8]==rgb(0x40536b));
    }
}
static void start_and_input(void) {
    open_civ();render();
    unsigned keys=key_posts;assert(!app_view.civ.requested);
    bus_state=UsbState_Default;poll(++clock_ms);update();
    action(ACTION_CIV_START);assert(!app_view.civ.requested && !civ_usb_owned());
    assert(app_view.civ.start_error==CIV_START_USB_NOT_READY);render();
    connect();update();application_idle=false;action(ACTION_CIV_START);
    assert(app_view.civ.start_error==CIV_START_CONFLICT && !civ_usb_owned());application_idle=true;
    assert(gamepad_exclusive_begin());action(ACTION_CIV_START);
    assert(app_view.civ.start_error==CIV_START_CONFLICT);
    assert(gamepad_exclusive_stop());step(2,1);step(2,1);update();
    assert(keyboard_send(0,0,clock_ms)==0);action(ACTION_CIV_START);
    assert(app_view.civ.start_error==CIV_START_CONFLICT);step(1,1);update();
    assert(civ_route_payload_valid(civ_payload_embedded()));
    const uint8_t invalid[]={0xff};assert(!civ_route_payload_valid((CivPayload){invalid,1}));
    assert(!civ_route_payload_valid((CivPayload){NULL,0}));
    release_input();
    /* D-pad and left stick both select footer actions without sending USB input. */
    assert(input(MON_DOWN,MON_DOWN,0,0,0)==ACTION_NONE && app_view.selected==1);
    release_input();assert(civ_route_input(&app_route,&app_view,0,0,0,20000,0,0,0,clock_ms)==ACTION_NONE);
    assert(app_view.selected==0);release_input();
    Action a=input(0,0,1,850,675);assert(a==ACTION_CIV_START);action(a);
    assert(app_view.civ.requested && civ_usb_owned() && gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_OFF);
    assert(key_posts==keys+1); /* Only the explicitly completed ordinary neutral above. */
    assert(gamepad_in_report[0]==0); /* Local selection A did not become a controller A. */
    uint64_t ticket=app_view.civ.progress.runner.ticket;
    action(ACTION_CIV_START);assert(app_view.civ.progress.runner.ticket==ticket);
    assert(input(MON_A,MON_A,1,850,675)==ACTION_NONE); /* held/touch cannot double-trigger */
    release_input();ui_step(0,1);render();
    assert(!strcmp(view_civ_status(&app_view),"RUNNING"));
    assert(input(MON_A,MON_A,0,0,0)==ACTION_NONE); /* A is not Cancel */
    assert(input(MON_B,MON_B,0,0,0)==ACTION_NONE);release_input();
    for(Action blocked=ACTION_COMPOSE;blocked<=ACTION_MENU_CONTROLLER;blocked++) action(blocked);
    assert(app_view.screen==SCREEN_CIV_PLACEHOLDER && civ_usb_owned());
    assert(controller_route_action(&app_controller,&app_view,ACTION_BACK));
    assert(app_view.screen==SCREEN_CIV_PLACEHOLDER);
    ui_reach(CIV_STAGE_CHARACTER);assert(!strcmp(view_civ_stage(&app_view),"Typing message"));
    release_input();
    size_t chars=app_view.civ.progress.runner.characters_completed;
    a=input(0,0,1,1100,675);assert(a==ACTION_CIV_CANCEL);action(a);render();
    assert(app_view.civ.cancel_requested && civ_usb_owned());
    assert(civ_sleep_info().held && auto_sleep_disabled);
    action(ACTION_BACK);assert(app_view.screen==SCREEN_CIV_PLACEHOLDER);
    ui_finish(CIV_RUNNER_CANCELLED);assert(app_view.civ.progress.runner.characters_completed==chars);
    assert(!strcmp(view_civ_status(&app_view),"CANCELLATION COMPLETED"));render();
    uint64_t elapsed=app_view.civ.elapsed_ms;clock_ms+=5000;update();assert(app_view.civ.elapsed_ms==elapsed);
    a=input(MON_B,MON_B,1,1100,675);assert(a==ACTION_BACK);action(a);
    assert(app_view.screen==SCREEN_HOME && !civ_usb_owned());
    assert(controller_route_action(&app_controller,&app_view,ACTION_MENU_KEYBOARD));assert(app_view.screen==SCREEN_KEYBOARD);
}
static void shortcut_and_recovery(void) {
    open_civ();ui_start();ui_reach(CIV_STAGE_CHARACTER);release_input();
    assert(input(CIV_CANCEL_CHORD,CIV_CANCEL_CHORD,0,0,0)==ACTION_NONE);
    clock_ms+=999;assert(input(0,CIV_CANCEL_CHORD,0,0,0)==ACTION_NONE);
    clock_ms++;Action a=input(0,CIV_CANCEL_CHORD,0,0,0);assert(a==ACTION_CIV_CANCEL);
    assert(gamepad_in_report[0]==0 && gamepad_in_report[1]==0); /* chord stays local */
    fail_cancel=1;action(a);assert(app_view.civ.progress.fence_blocked && view_civ_blocked(&app_view));render();
    assert(civ_sleep_info().held && auto_sleep_disabled);
    assert(!strcmp(view_civ_status(&app_view),"USB CLEANUP REQUIRED"));
    action(ACTION_BACK);action(ACTION_DIAGNOSTICS);assert(app_view.screen==SCREEN_CIV_PLACEHOLDER);
    release_input();fail_cancel=0;
    a=input(0,0,1,1060,675);assert(a==ACTION_CIV_RETRY);action(a);render();
    assert(app_view.civ.retrying && civ_usb_owned());ui_finish(CIV_RUNNER_CANCELLED);render();
    open_civ();ui_start();ui_reach(CIV_STAGE_CHARACTER);action(ACTION_CIV_CANCEL);ui_step(0,1);
    key_actual=7;ui_step(3,1);key_actual=8;
    assert(app_view.civ.progress.runner.status==CIV_RUNNER_CLEANUP_BLOCKED);render();
    assert(civ_sleep_info().held && auto_sleep_disabled);
    release_input();a=input(MON_A,MON_A,0,0,0);assert(a==ACTION_CIV_RETRY);action(a);
    ui_finish(CIV_RUNNER_CANCELLED);
}
static void faults_and_completion(void) {
    open_civ();ui_start();ui_reach(CIV_STAGE_CHARACTER);
    bus_state=UsbState_Detached;ui_step(0,1);assert(civ_usb_owned());
    assert(strstr(view_civ_status(&app_view),"USB DISCONNECTED"));render();
    connect();ui_finish(CIV_RUNNER_INTERRUPTED);assert(!strcmp(view_civ_status(&app_view),"RUNNER INTERRUPTED"));render();
    for(unsigned timeout=0;timeout<2;timeout++) {
        open_civ();ui_start();ui_reach(CIV_STAGE_CHARACTER);
        if(timeout) ui_step(1,2501);
        else { key_actual=7;ui_step(3,1);key_actual=8; }
        assert(civ_usb_owned());render();ui_finish(CIV_RUNNER_FAILED);
        assert(strstr(view_civ_status(&app_view),"FAILURE / TIMEOUT"));
        assert(app_view.civ.progress.runner.error==CIV_ERROR_COMPLETION);render();
    }
    open_civ();ui_start();unsigned limit=200000;
    while(civ_usb_owned() && --limit) {
        ui_step(3,32);
        if(app_view.civ.progress.runner.lines_completed==42 && app_view.civ.progress.runner.character==0) render();
    }
    assert(limit);update();assert(app_view.civ.progress.runner.lines_completed==87 && app_view.civ.progress.runner.neutral_confirmed);
    assert(!strcmp(view_civ_status(&app_view),"ALL SUBMISSIONS SENT"));render();
    action(ACTION_CIV_START);assert(!civ_usb_owned()); /* explicit re-entry is required */
    assert(pixels[456*1280+1100]==rgb(0xd9aa54));
    release_input();Action a=input(0,0,1,1100,675);assert(a==ACTION_BACK);action(a);assert(app_view.screen==SCREEN_HOME);
}
static void labels_and_restart(void) {
    open_civ();
    const char *labels[]={"Initializing","Initializing","Initializing","Opening chat","Opening chat","Opening chat","Opening chat",
        "Typing message","Typing message","Submitting message","Submitting message","Submitting message","Waiting","Waiting","Waiting","Releasing inputs"};
    for(unsigned stage=0;stage<16;stage++) {
        app_view.civ.progress.runner.stage=(CivRunnerStage)stage;
        assert(!strcmp(view_civ_stage(&app_view),labels[stage]));
    }
    for(int error=CIV_START_USB_NOT_READY;error<=CIV_START_RESTART_REQUIRED;error++) {
        app_view.civ.start_error=(CivStartError)error;assert(*view_civ_start_error((CivStartError)error));render();
    }
    ui_start();ui_reach(CIV_STAGE_CHARACTER);civ_usb_shutdown();civ_sleep_shutdown();update();render();
    assert(!auto_sleep_disabled);
    assert(app_view.civ.progress.shutdown_unresolved && !app_view.civ.progress.runner.neutral_confirmed);
    assert(!strcmp(view_civ_status(&app_view),"APPLICATION RESTART REQUIRED"));
    assert(view_footer(&app_view).count==0);
    action(ACTION_BACK);action(ACTION_CIV_RETRY);action(ACTION_CIV_START);
    assert(app_view.screen==SCREEN_CIV_PLACEHOLDER && !civ_usb_owned());
}
static void sleep_warning(void) {
    open_civ();auto_sleep_disabled=true;action(ACTION_CIV_START);
    assert(civ_sleep_info().protected && civ_sleep_info().previous_disabled);
    action(ACTION_CIV_CANCEL);finish(CIV_RUNNER_CANCELLED);update();release_input();
    assert(auto_sleep_disabled && !civ_sleep_info().held);auto_sleep_disabled=false;
    open_civ();sleep_get_error=99;action(ACTION_CIV_START);render();
    assert(!civ_usb_owned() && app_view.civ.start_error==CIV_START_SLEEP_WARNING);
    assert(!strcmp(view_footer(&app_view).buttons[0].label,"START ANYWAY"));
    assert(input(0,MON_A,0,0,0)==ACTION_NONE); /* No held-button fallback start. */
    release_input();action(ACTION_CIV_START);assert(civ_usb_owned() && !civ_sleep_info().protected);
    assert(civ_sleep_info().error==99);action(ACTION_CIV_CANCEL);ui_finish(CIV_RUNNER_CANCELLED);sleep_get_error=0;
    open_civ();action(ACTION_CIV_START);assert(civ_usb_owned() && civ_sleep_info().protected);
    unsigned g=sleep_gets,s=sleep_sets;
    for(int i=0;i<100;i++) { ui_step(3,1);render(); }
    assert(sleep_gets==g && sleep_sets==s);action(ACTION_CIV_CANCEL);ui_finish(CIV_RUNNER_CANCELLED);
}
int main(void) {
    assert(ui_init());pixels=calloc(1280u*720,4);assert(pixels);
    open_civ();action(ACTION_CIV_START);
    assert(app_view.civ.start_error==CIV_START_REJECTED && !app_view.civ.requested && !civ_usb_owned());
    assert(!auto_sleep_disabled && !civ_sleep_info().held); /* start API rejection rolled back */
    render();assert(civ_usb_init(can_start));
    start_and_input();shortcut_and_recovery();faults_and_completion();sleep_warning();labels_and_restart();
    free(pixels);ui_shutdown();keyboard_exit();
    puts("PASS: production CIV routing/rendering + real USB adapter: explicit start, local input isolation, progress, cancel, faults, neutral cleanup/retry, completion and restart-required UI.");
    return 0;
}
