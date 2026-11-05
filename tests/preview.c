#include "ui.h"
#include "view.h"
#include "pace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void save(View *view,const char *name) {
    uint32_t *pixels=calloc(1280*720,4);
    view_render(view,pixels,1280*4);
    FILE *out=fopen(name,"wb");
    fprintf(out,"P6\n1280 720\n255\n");
    for(int i=0;i<1280*720;i++) { unsigned char rgb[]={pixels[i]&255,(pixels[i]>>8)&255,(pixels[i]>>16)&255};fwrite(rgb,1,3,out); }
    fclose(out);free(pixels);
}
int main(void) {
    if(!ui_init()) return 1;
    View view={0};view.enter_pause_ms=200;view.caps_state=-1;strcpy(view.usb_label,"USB keyboard connected");view.usb_ready=true;view.usb_initialized=true;
    save(&view,"home-dark.ppm");
    view.selected=1;save(&view,"home-keyboard-focus.ppm");view.selected=2;save(&view,"home-controller-focus.ppm");view.selected=0;
    view.screen=SCREEN_CIV_PLACEHOLDER;view.civ.ready=true;save(&view,"civ-initial.ppm");
    view.civ.requested=true;view.civ.progress.owned=true;
    view.civ.progress.runner.status=CIV_RUNNER_RUNNING;view.civ.progress.runner.lines_total=87;
    view.civ.progress.runner.line=1;view.civ.progress.runner.stage=CIV_STAGE_START_DELAY;
    save(&view,"civ-running.ppm");
    view.civ.progress.runner.line=43;view.civ.progress.runner.lines_completed=42;
    view.civ.progress.runner.stage=CIV_STAGE_CHARACTER;view.civ.progress.runner.character=72;view.civ.progress.runner.line_length=253;
    view.civ.elapsed_ms=496000;save(&view,"civ-mid-progress.ppm");
    view.civ.cancel_requested=true;view.civ.progress.runner.status=CIV_RUNNER_CLEANING_UP;
    view.civ.progress.runner.stage=CIV_STAGE_NEUTRAL;save(&view,"civ-cancellation-pending.ppm");
    view.civ.progress.owned=false;view.civ.progress.runner.neutral_confirmed=true;
    view.civ.progress.runner.status=CIV_RUNNER_CANCELLED;save(&view,"civ-cancelled.ppm");
    view.civ.cancel_requested=false;view.civ.progress.runner.status=CIV_RUNNER_FINISHED;
    view.civ.progress.runner.line=87;view.civ.progress.runner.lines_completed=87;
    view.civ.elapsed_ms=1050000;save(&view,"civ-completed.ppm");
    view.civ.progress.runner.status=CIV_RUNNER_FAILED;view.civ.progress.runner.lines_completed=42;
    view.civ.progress.runner.line=43;view.civ.progress.runner.error=CIV_ERROR_COMPLETION;
    view.civ.transport_result=1;save(&view,"civ-transfer-failure.ppm");
    view.civ.progress.owned=true;view.civ.progress.runner.neutral_confirmed=false;
    view.civ.progress.runner.status=CIV_RUNNER_CLEANUP_BLOCKED;view.civ.progress.fence_blocked=true;
    save(&view,"civ-cleanup-unresolved.ppm");
    view.civ.progress.fence_blocked=false;view.civ.progress.runner.status=CIV_RUNNER_CLEANING_UP;
    view.civ.retrying=true;view.civ.transport_result=0;save(&view,"civ-retry-cleanup.ppm");
    view.civ=(CivView){0};view.civ.start_error=CIV_START_USB_NOT_READY;save(&view,"civ-usb-not-ready.ppm");
    view.civ=(CivView){0};
    view.screen=SCREEN_KEYBOARD;view.selected=0;save(&view,"keyboard-menu.ppm");
    view.screen=SCREEN_CONTROLLER_MENU;view.selected=0;save(&view,"controller-menu.ppm");
    view.screen=SCREEN_CONTROLLER;view.controller_connected=true;view.controller_armed=true;view.controller_usb_ready=true;
    view.controller_report=controller_monitor_report(0,0,0,0,0,true);save(&view,"standard-controller.ppm");
    view.controller_armed=false;view.controller_synchronizing=true;save(&view,"controller-sync.ppm");
    view.controller_synchronizing=false;save(&view,"controller-release-gate.ppm");
    view.controller_exiting=true;save(&view,"controller-neutralizing.ppm");
    view.controller_usb_ready=false;view.controller_interrupted=true;save(&view,"controller-disconnected.ppm");
    view.controller_usb_ready=true;save(&view,"controller-interrupted.ppm");
    view.controller_interrupted=false;view.controller_exiting=false;view.controller_armed=true;
    controller_monitor_init(&view.monitor);
    for(int i=0;i<5;i++) controller_monitor_update(&view.monitor,(1u<<i)|MON_ZR,true);
    view.monitor.held=0xffff;view.controller_report=controller_monitor_report(0xffff,32767,-32768,0,0,true);save(&view,"controller-history.ppm");
    view.screen=SCREEN_MACRO_STUDIO;save(&view,"macro-studio.ppm");
    view.selected=1;save(&view,"macro-studio-record-focus.ppm");
    view.selected=0;
    view.screen=SCREEN_MACRO_LIBRARY;view.file_rows=0;view.macro_count=0;save(&view,"macro-library-empty.ppm");
    view.screen=SCREEN_MACRO_LIBRARY;view.file_rows=2;view.macro_count=2;view.selected=0;
    strcpy(view.files[0],"example macro");strcpy(view.files[1],"second macro");save(&view,"macro-library.ppm");
    view.screen=SCREEN_MACRO_DETAILS;strcpy(view.name,"Example macro");strcpy(view.macro_description,"Example saved controller input.");
    view.macro_step_count=2;view.macro_duration=600;view.macro_pages=1;view.macro_speed=100;view.macro_repeat=1;
    strcpy(view.macro_steps[0],"1. A + ZR");strcpy(view.macro_step_details[0],"100 ms");
    strcpy(view.macro_steps[1],"2. Neutral / delay");strcpy(view.macro_step_details[1],"500 ms");save(&view,"macro-details.ppm");
    view.screen=SCREEN_MACRO_EDITOR;view.selected=3;view.macro_dirty=true;save(&view,"macro-editor.ppm");
    view.screen=SCREEN_MACRO_STEP;view.selected=2;
    view.macro_draft=(MacroStep){{0x84,1,255,128,128,0,0},100};save(&view,"macro-action.ppm");
    view.screen=SCREEN_MACRO_RECORD;view.selected=0;save(&view,"macro-record-ready.ppm");
    view.macro_arming=true;save(&view,"macro-record-arming.ppm");
    view.macro_arming=false;view.macro_recording=true;view.macro_elapsed=12123;view.macro_step_count=42;save(&view,"macro-record-active.ppm");
    view.macro_recording=false;view.macro_has_recording=true;save(&view,"macro-record-stopped.ppm");
    view.screen=SCREEN_MACRO_PLAY;view.macro_owned=true;view.macro_step_count=42;view.macro_cursor=21;view.macro_repeat=3;view.macro_iteration=2;
    strcpy(view.macro_status,"Playing controller macro");save(&view,"macro-play.ppm");
    view.macro_cleanup_blocked=true;strcpy(view.macro_status,"USB CLEANUP REQUIRED - reconnect, then Retry Cleanup");save(&view,"macro-play-cleanup.ppm");
    view.macro_owned=false;view.macro_cleanup_blocked=false;view.macro_neutral=true;
    strcpy(view.macro_status,"Playback finished; neutral completion confirmed");save(&view,"macro-play-finished.ppm");
    view.macro_status[0]=0;view.screen=SCREEN_MACRO_DISCARD;view.selected=1;save(&view,"macro-discard.ppm");
    view.screen=SCREEN_MACRO_DELETE;save(&view,"macro-delete.ppm");
    view.screen=SCREEN_HOME;
    view.caps_state=1;view.pace=PACE_VERY_FAST;save(&view,"home-veryfast.ppm");
    view.caps_pending=true;view.pace=PACE_TURBO;save(&view,"home-turbo.ppm");view.pace=PACE_NORMAL;
    view.caps_pending=false;view.caps_state=0;
    view.screen=SCREEN_FILES;view.file_rows=8;view.file_count=36;view.selected=3;
    strcpy(view.path,"sdmc:/texts/");
    for(int i=0;i<8;i++) { snprintf(view.files[i],256,"%s %d - sample.txt",i<2?"Folder":"File",i+1);view.directories[i]=i<2; }
    save(&view,"files-dark.ppm");
    view.screen=SCREEN_CONFIRM;view.pace=PACE_TURBO;strcpy(view.name,"switch2-test.txt");view.characters=157;view.lines=2;
    strcpy(view.preview,"Hello.\nUSB keyboard test: 0123456789.\nAccent test: é è à ç ù.");
    save(&view,"confirm-dark.ppm");
    view.composing=true;view.compose_valid=true;view.compose_pages=1;view.caps_state=-1;
    strcpy(view.name,"Compose & Send");strcpy(view.compose_rows[0],"Hello from RecurCIV.");
    strcpy(view.compose_rows[1],"Review text before sending.");view.selected=0;save(&view,"compose-review.ppm");
    save(&view,"compose-send-confirmation.ppm");
    view.selected=3;save(&view,"compose-settings-focus.ppm");
    view.compose_pages=3;view.compose_page=1;
    for(int i=0;i<COMPOSE_ROWS;i++) snprintf(view.compose_rows[i],COMPOSE_ROW_BYTES,"Page 2 - example line %d",i+1);
    view.selected=8;save(&view,"compose-pages.ppm");
    view.characters=view.lines=view.tabs=0;view.compose_valid=false;view.compose_page=0;view.compose_pages=1;
    memset(view.compose_rows,0,sizeof(view.compose_rows));view.selected=1;save(&view,"compose-empty.ppm");
    view.composing=false;view.selected=0;
    view.screen=SCREEN_TRANSFER;view.sending=true;view.done=51;view.total=157;save(&view,"sending-light.ppm");
    view.screen=SCREEN_DIAGNOSTICS;
    for(int i=0;i<12;i++) snprintf(view.diagnostics[i],144,"USB status %d / result: 0x00000000",i+1);
    save(&view,"diagnostics-light.ppm");
    view.screen=SCREEN_RESULT;view.error=true;
    strcpy(view.message,"Unsupported character U+1F600 in QWERTY US at position 25. Nothing was sent.");
    save(&view,"error-light.ppm");
    view.screen=SCREEN_LIVE;view.error=false;view.usb_ready=true;view.usb_initialized=true;
    view.now=1000;strcpy(view.usb_label,"USB keyboard connected");
    live_init(&view.live,0);view.live.phase=LIVE_IDLE;view.live.was_ready=true;
    view.live.selected=(unsigned)live_key_index_for('a');
    save(&view,"live-normal.ppm");
    view.live.pressed='a';view.live.pressed_until=view.now+100;
    save(&view,"live-pressed.ppm");view.live.pressed=-1;
    view.live.shift=true;save(&view,"live-shift.ppm");
    view.live.shift=false;view.live.caps=true;save(&view,"live-caps.ppm");
    ui_shutdown();return 0;
}
