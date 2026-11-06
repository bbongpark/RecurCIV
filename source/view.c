#include "view.h"
#include "ui.h"
#include "pace.h"
#include <stdio.h>
#include <string.h>

static void button(int x,int y,int w,const char *label,bool active) {
    ui_roundrect(x,y,w,56,10,active?0xff54aad9:ui_palette.line);
    ui_roundrect(x+2,y+2,w-4,52,8,ui_palette.surface);
    ui_text(x+16,y+14,24,ui_palette.ink,label,w-32);
}
static void chip(int x,int y,int w,const char *label) {
    ui_roundrect(x,y,w,40,12,ui_palette.soft);
    ui_text(x+12,y+9,20,0xff54aad9,label,w-24);
}
static void caps_chip(const View *v,int x,int y,int width) {
    char label[64];
    const char *state=v->caps_pending?"...":v->caps_state<0?"not reported":v->caps_state?"on":"off";
    snprintf(label,sizeof(label),v->screen==SCREEN_CONFIRM && v->composing?"Caps Lock: %s":"X  Caps Lock: %s",state);
    chip(x,y,width,label);
}
/* Vector artwork is rendered live; no screenshot or proprietary art is used. */
static uint32_t color(unsigned rgb) {
    return 0xff000000|((rgb&255)<<16)|(rgb&0xff00)|((rgb>>16)&255);
}
static void footer_frame(void) {
    ui_roundrect(48,640,1184,74,10,color(0xd9aa54));
    ui_roundrect(50,642,1180,70,8,color(0x0b1425));
}
const char *view_screen_name(Screen screen) {
    switch(screen) {
    case SCREEN_HOME: return "HOME";
    case SCREEN_FILES: return "TEXT FILES";
    case SCREEN_CONFIRM: return "COMPOSE & SEND";
    case SCREEN_TRANSFER: return "TEXT SENDER";
    case SCREEN_RESULT: return "RESULT";
    case SCREEN_DIAGNOSTICS: return "USB DIAGNOSTICS";
    case SCREEN_KEYBOARD: return "USB KEYBOARD";
    case SCREEN_CIV_PLACEHOLDER: return "CIV EXPLOIT";
    case SCREEN_CONTROLLER: return "STANDARD CONTROLLER";
    case SCREEN_LIVE: return "LIVE KEYBOARD";
    case SCREEN_MACRO_LIBRARY: return "MACRO LIBRARY";
    case SCREEN_MACRO_DETAILS: return "MACRO DETAILS";
    case SCREEN_MACRO_DELETE: return "DELETE MACRO";
    case SCREEN_CONTROLLER_MENU: return "USB CONTROLLER";
    case SCREEN_MACRO_STUDIO: return "MACRO STUDIO";
    case SCREEN_MACRO_EDITOR: return "MACRO EDITOR";
    case SCREEN_MACRO_STEP: return "CONTROLLER ACTION";
    case SCREEN_MACRO_RECORD: return "MACRO RECORDING";
    case SCREEN_MACRO_PLAY: return "MACRO PLAYBACK";
    case SCREEN_MACRO_DISCARD: return "UNSAVED MACRO";
    }
    return "UNKNOWN SCREEN";
}
static void shared_header(Screen screen) {
    const char *name=view_screen_name(screen);int width=ui_text_width(20,name);
    ui_text(48,20,32,color(0xffffff),"Recur",200);
    ui_text(48+ui_text_width(32,"Recur"),20,32,color(0xd9aa54),"CIV",100);
    ui_text(1232-width,27,20,color(0xc2cddd),name,width+2);
    ui_line(48,75,1184,color(0x40536b));
}
/* The renderer and touch router consume the same footer specification. */
ViewFooter view_footer(const View *v) {
    ViewFooter f={0};
    f.hints="B  Back";
    f.buttons[0]=(ViewFooterButton){1030,180,"Back",ACTION_BACK};f.count=1;
    switch(v->screen) {
    case SCREEN_HOME:
        f.hints="D-pad / L-stick  Navigate    A  Select    Y  Diagnostics    +  Exit";f.count=0;break;
    case SCREEN_CONTROLLER_MENU:
        f.hints="D-pad / L-stick  Navigate    A  Select    B  Back";break;
    case SCREEN_MACRO_STUDIO:
        f.hints="D-pad / L-stick  Navigate    A  Select    B  Back";
        break;
    case SCREEN_MACRO_LIBRARY:
        f.hints="D-pad Move   A Open   Y Delete";f.count=4;
        f.buttons[0]=(ViewFooterButton){560,150,"Previous",ACTION_MACRO_PREV};
        f.buttons[1]=(ViewFooterButton){720,150,"Next",ACTION_MACRO_NEXT};
        f.buttons[2]=(ViewFooterButton){880,160,"Refresh",ACTION_MACRO_REFRESH};
        f.buttons[3]=(ViewFooterButton){1050,178,"Back",ACTION_BACK};break;
    case SCREEN_MACRO_DETAILS:
        f.hints="D-pad Move   A Select";f.count=4;
        f.buttons[0]=(ViewFooterButton){560,150,"Previous",ACTION_MACRO_PREV};
        f.buttons[1]=(ViewFooterButton){720,150,"Next",ACTION_MACRO_NEXT};
        f.buttons[2]=(ViewFooterButton){880,160,"Play once",(Action)ACTION_MS_PLAY_ONCE};
        f.buttons[3]=(ViewFooterButton){1050,178,"Back",ACTION_BACK};break;
    case SCREEN_MACRO_DELETE:
        f.hints="D-pad Move   A Select   B Cancel";f.buttons[0].label="Cancel";break;
    case SCREEN_MACRO_EDITOR:
        f.hints="D-pad / L-stick Move    A Select    B Back";
        f.buttons[0]=(ViewFooterButton){1030,180,"Play once",(Action)ACTION_MS_PLAY_ONCE};break;
    case SCREEN_MACRO_STEP:
        f.hints="D-pad / L-stick Move    A Select    B Back    Touch controls supported";f.count=0;break;
    case SCREEN_MACRO_DISCARD:
        f.hints="A Select    B Keep editing";f.count=2;
        f.buttons[0]=(ViewFooterButton){760,220,"Discard edits",(Action)ACTION_MS_DISCARD};
        f.buttons[1]=(ViewFooterButton){996,214,"Keep editing",ACTION_BACK};break;
    case SCREEN_MACRO_RECORD:
        if(v->macro_record_cleanup) {
            f.hints=v->macro_cleanup_blocked?"Touch Retry cleanup; captured data is retained.":"Waiting for neutral USB completion. Keep USB connected.";
            f.count=v->macro_cleanup_blocked?1:0;
            if(f.count) f.buttons[0]=(ViewFooterButton){930,280,"Retry cleanup",(Action)ACTION_MS_RETRY};
        } else if(v->macro_replace_pending) {
            f.hints="Replace unsaved capture? Touch only.";f.count=v->macro_owned?3:2;
            f.buttons[0]=(ViewFooterButton){510,270,"Discard + record",(Action)ACTION_MS_REPLACE_RECORD};
            f.buttons[1]=(ViewFooterButton){790,210,"Keep capture",(Action)ACTION_MS_KEEP_RECORD};
            if(v->macro_owned) f.buttons[2]=(ViewFooterButton){1010,200,"Exit live",(Action)ACTION_MS_LIVE_EXIT};
        } else if(v->macro_live && v->macro_owned) {
            f.hints="Capture: Start/Stop. Touch controls available.";
            f.count=2;
            f.buttons[0]=(ViewFooterButton){700,300,v->macro_recording?"Stop recording":"Start recording",
                (Action)(v->macro_recording?ACTION_MS_RECORD_STOP:ACTION_MS_RECORD_START)};
            if(v->macro_record_sync || v->macro_arming) f.buttons[0]=(ViewFooterButton){700,300,"Release controls",ACTION_NONE};
            f.buttons[1]=(ViewFooterButton){1010,200,"Exit live",(Action)ACTION_MS_LIVE_EXIT};
        } else if(v->macro_recording || v->macro_arming || v->macro_record_sync) {
            f.hints=v->macro_live?"Live forwarding + local capture. Touch Stop to finish.":"All controller input is recorded locally. Touch Stop to finish.";
            f.buttons[0]=(ViewFooterButton){1020,190,"Stop",(Action)ACTION_MS_RECORD_STOP};
        } else if(v->macro_has_recording) {
            f.hints="Review captured input before saving.";f.count=3;
            f.buttons[0]=(ViewFooterButton){640,180,"Review",(Action)ACTION_MS_REVIEW};
            f.buttons[1]=(ViewFooterButton){836,180,"Discard",(Action)ACTION_MS_DISCARD};
            f.buttons[2]=(ViewFooterButton){1030,180,"Back",ACTION_BACK};
        } else if(v->macro_live) {
            f.hints="Live session ended. Return to Macro Studio to enter again.";
        } else {
            f.hints=v->macro_live?"A Select    B Back    Live USB recording":"A Select    B Back    No live forwarding";f.count=2;
            f.buttons[0]=(ViewFooterButton){740,280,"Start recording",(Action)ACTION_MS_RECORD_START};
            f.buttons[1]=(ViewFooterButton){1036,174,"Back",ACTION_BACK};
        }
        break;
    case SCREEN_MACRO_PLAY:
        f.hints=v->macro_cleanup_blocked?"A Retry cleanup - navigation locked":v->macro_owned?"A / B Stop - navigation locked during cleanup":"A / B Back";
        if(v->macro_owned) f.buttons[0]=(ViewFooterButton){930,280,v->macro_cleanup_blocked?"Retry cleanup":"Stop",(Action)(v->macro_cleanup_blocked?ACTION_MS_RETRY:ACTION_MS_STOP)};
        break;
    case SCREEN_CONTROLLER:
        f.hints="Hold + and - for 2 s to exit (forwarded)";
        f.buttons[0]=(ViewFooterButton){846,190,"Diagnostics",ACTION_DIAGNOSTICS};
        f.buttons[1]=(ViewFooterButton){1050,178,"Exit",ACTION_BACK};f.count=2;break;
    case SCREEN_KEYBOARD: case SCREEN_FILES:
        if(v->screen==SCREEN_KEYBOARD) {
            f.hints="D-pad / L-stick  Navigate    A  Select    B  Back    -  Diagnostics";break;
        }
        f.hints=v->screen==SCREEN_FILES?"D-pad / L-stick  Move   A  Open   L / R  Page   B  Back":"D-pad / L-stick  Navigate    A  Select    B  Back";
        f.buttons[0]=(ViewFooterButton){846,190,"Diagnostics",ACTION_DIAGNOSTICS};
        f.buttons[1]=(ViewFooterButton){1050,178,"Back",ACTION_BACK};f.count=2;break;
    case SCREEN_LIVE:
        f.hints="D-pad / L-stick  Move   A  Type   B  Backspace   X  Shift   Y  Space   +  Done";
        f.buttons[0]=(ViewFooterButton){1050,178,"Done",ACTION_BACK};break;
    case SCREEN_CONFIRM:
        if(v->composing) {
            f.hints="D-pad / L-stick  Move    A  Select    B  Back    X  Edit    |    Send: 3 s countdown";
            f.count=0;break;
        }
        f.hints="";f.count=4;
        f.buttons[0]=(ViewFooterButton){54,242,"Caps Lock",ACTION_CAPS_LOCK};
        f.buttons[1]=(ViewFooterButton){308,308,"L  Auto-indent",ACTION_AUTOINDENT};
        f.buttons[2]=(ViewFooterButton){628,196,"Diagnostics",ACTION_DIAGNOSTICS};
        f.buttons[3]=(ViewFooterButton){836,390,"R  Enter pause",ACTION_ENTER_PAUSE};break;
    case SCREEN_TRANSFER:
        f.hints="B  Cancel";f.buttons[0]=(ViewFooterButton){1030,180,"Cancel",ACTION_ABORT};break;
    case SCREEN_RESULT:
        f.hints="A / B  Back    -  Diagnostics    +  Exit";
        f.buttons[0]=(ViewFooterButton){846,190,"Diagnostics",ACTION_DIAGNOSTICS};
        f.buttons[1]=(ViewFooterButton){1050,178,"Back",ACTION_BACK};f.count=2;break;
    case SCREEN_DIAGNOSTICS:
        f.hints="B  Back    A  Reset    X  Create log";f.count=3;
        f.buttons[0]=(ViewFooterButton){600,200,"Reset USB",ACTION_SEND};
        f.buttons[1]=(ViewFooterButton){810,220,"Create log",ACTION_CREATE_USB_LOG};
        f.buttons[2]=(ViewFooterButton){1050,160,"Back",ACTION_BACK};break;
    case SCREEN_CIV_PLACEHOLDER:
        if(v->civ.progress.shutdown_unresolved) {
            f.hints="Close and restart RecurCIV. Cleanup was not confirmed.";f.count=0;
        } else if(v->civ.progress.owned) {
            if(view_civ_blocked(v)) {
                f.hints="A  Retry cleanup    Menu navigation locked";
                f.buttons[0]=(ViewFooterButton){936,274,"RETRY CLEANUP",ACTION_CIV_RETRY};
            } else if(v->civ.cancel_requested || (!v->civ.progress.acquiring && v->civ.progress.runner.status!=CIV_RUNNER_RUNNING)) {
                f.hints="Cleanup pending. Keep USB connected; menu navigation is locked.";f.count=0;
            } else {
                f.hints="Hold L + R + Minus for 1 s to cancel (local only)";
                f.buttons[0]=(ViewFooterButton){990,220,"CANCEL",ACTION_CIV_CANCEL};
            }
        } else if(v->civ.requested) {
            f.hints="A / B  Back";
        } else {
            f.hints="D-pad / L-stick  Select    A  Confirm    B  Back";
            const char *label=v->civ.start_error==CIV_START_SLEEP_WARNING?
                (v->civ.sleep.restore_failed?"RETRY START":"START ANYWAY"):"START PAYLOAD";
            f.buttons[0]=(ViewFooterButton){730,294,label,ACTION_CIV_START};
            f.buttons[1]=(ViewFooterButton){1036,174,"BACK",ACTION_BACK};f.count=2;
        }
        break;
    }
    return f;
}
static void shared_footer(const View *v) {
    ViewFooter f=view_footer(v);footer_frame();
    int width=f.count?f.buttons[0].x-82:1140;
    ui_text(66,666,20,color(0xd9aa54),f.hints,width);
    for(size_t i=0;i<f.count;i++) {
        const ViewFooterButton *b=&f.buttons[i];char label[96];const char *text=b->label;
        if(v->screen==SCREEN_CONFIRM) {
            if(i==0) snprintf(label,sizeof(label),"Caps Lock: %s",v->caps_pending?"...":v->caps_state<0?"not reported":v->caps_state?"on":"off");
            else if(i==1) snprintf(label,sizeof(label),"L  Auto-indent: %s",v->cancel_autoindent?"on":"off");
            else if(i==3) snprintf(label,sizeof(label),"R  Enter pause: %d ms",v->enter_pause_ms);
            else snprintf(label,sizeof(label),"-  Diagnostics");
            text=label;
        }
        size_t offset=v->screen==SCREEN_MACRO_STUDIO?3:v->screen==SCREEN_MACRO_LIBRARY?(size_t)v->file_rows:
            v->screen==SCREEN_MACRO_DETAILS?5:v->screen==SCREEN_MACRO_DELETE?1:v->screen==SCREEN_MACRO_EDITOR?15:0;
        bool macro_focus=(v->screen==SCREEN_MACRO_STUDIO || v->screen==SCREEN_MACRO_LIBRARY || v->screen==SCREEN_MACRO_DETAILS ||
            v->screen==SCREEN_MACRO_DELETE || v->screen>=SCREEN_MACRO_EDITOR) && (size_t)v->selected==i+offset;
        bool focus=(v->screen==SCREEN_CIV_PLACEHOLDER && (size_t)v->selected==i) || macro_focus;
        ui_roundrect(b->x,648,b->w,56,8,color(focus?0xd9aa54:0x40536b));
        ui_roundrect(b->x+1,649,b->w-2,54,7,color(0x17273d));
        ui_text(b->x+12,666,20,color(0xd9aa54),text,b->w-24);
    }
}
static void card_frame(int x,int y,int w,int h,bool selected) {
    ui_roundrect(x,y,w,h,14,color(selected?0xd9aa54:0x40536b));
    ui_roundrect(x+3,y+3,w-6,h-6,11,color(0x17273d));
}
bool view_civ_blocked(const View *v) {
    return v->civ.progress.shutdown_unresolved || v->civ.progress.fence_blocked ||
        v->civ.progress.runner.status==CIV_RUNNER_CLEANUP_BLOCKED;
}
const char *view_civ_start_error(CivStartError error) {
    switch(error) {
    case CIV_START_OK: return "";
    case CIV_START_USB_NOT_READY: return "USB not ready. Connect USB and wait for both HID interfaces.";
    case CIV_START_CONFLICT: return "Conflicting active mode or pending USB report. Nothing started.";
    case CIV_START_INVALID_PAYLOAD: return "Invalid embedded payload: runner preflight rejected it. Nothing sent.";
    case CIV_START_REJECTED: return "Start rejected by the USB adapter. Nothing started; check USB readiness.";
    case CIV_START_RESTART_REQUIRED: return "Application restart required. Cleanup was not confirmed in this session.";
    case CIV_START_SLEEP_WARNING: return "Auto-sleep not confirmed. Set Switch sleep settings manually, then select START ANYWAY.";
    }
    return "Unknown start error. Nothing started.";
}
const char *view_civ_status(const View *v) {
    const CivView *c=&v->civ;const CivRunnerProgress *p=&c->progress.runner;
    if(c->progress.shutdown_unresolved) return "APPLICATION RESTART REQUIRED";
    if(view_civ_blocked(v)) return "USB CLEANUP REQUIRED";
    if(c->progress.owned) {
        if(!c->ready) return "USB DISCONNECTED / NOT READY";
        if(c->retrying && (c->progress.acquiring || p->status!=CIV_RUNNER_RUNNING)) return "RETRYING USB CLEANUP";
        if(c->cancel_requested) return "CANCELLING - CLEANUP PENDING";
        if(c->progress.acquiring) return "PREPARING USB";
        if(p->status==CIV_RUNNER_RUNNING) return "RUNNING";
        return c->retrying?"RETRYING USB CLEANUP":"CLEANUP PENDING";
    }
    if(!c->requested) return c->ready?"READY TO START":"USB NOT READY";
    if(!p->neutral_confirmed) return "CLEANUP NOT CONFIRMED";
    switch(p->status) {
    case CIV_RUNNER_FINISHED: return "ALL SUBMISSIONS SENT";
    case CIV_RUNNER_CANCELLED: return "CANCELLATION COMPLETED";
    case CIV_RUNNER_INTERRUPTED: return "RUNNER INTERRUPTED";
    case CIV_RUNNER_FAILED: return "USB TRANSFER FAILURE / TIMEOUT";
    default: return "AUTOMATION STOPPED";
    }
}
const char *view_civ_stage(const View *v) {
    if(v->civ.progress.acquiring) return "Preparing USB";
    if(v->civ.requested && !v->civ.progress.owned && v->civ.progress.runner.neutral_confirmed) return "Inputs released";
    if(v->civ.progress.owned && v->civ.progress.runner.status!=CIV_RUNNER_RUNNING) return "Releasing inputs";
    switch(v->civ.progress.runner.stage) {
    case CIV_STAGE_START_DELAY: case CIV_STAGE_PREAMBLE_A: case CIV_STAGE_PREAMBLE_RELEASE: return "Initializing";
    case CIV_STAGE_ESCAPE: case CIV_STAGE_ESCAPE_SETTLE: case CIV_STAGE_ACTIVATE_A: case CIV_STAGE_ACTIVATE_SETTLE: return "Opening chat";
    case CIV_STAGE_CHARACTER: case CIV_STAGE_REPEAT_RELEASE: return "Typing message";
    case CIV_STAGE_ENTER_1: case CIV_STAGE_ENTER_2: case CIV_STAGE_ENTER_GAP: return "Submitting message";
    case CIV_STAGE_LINE_SETTLE: case CIV_STAGE_SEPARATOR_A: case CIV_STAGE_SEPARATOR_SETTLE: return "Waiting";
    case CIV_STAGE_NEUTRAL: return "Releasing inputs";
    }
    return "Waiting";
}
static void render_civ(const View *v) {
    Palette p=ui_palette;const CivView *c=&v->civ;const CivRunnerProgress *r=&c->progress.runner;
    uint32_t gold=color(0xd9aa54);char text[192];
    ui_text(48,101,32,p.ink,"CIV VII AUTOMATION",1184);
    if(!c->requested && !c->progress.owned && !c->progress.shutdown_unresolved) {
        ui_text(48,155,24,p.muted,"Automatically sends the approved 87-message payload",1184);
        ui_text(48,189,24,p.muted,"using USB keyboard and controller input.",1184);
        card_frame(48,245,760,310,false);card_frame(836,245,396,310,false);
        ui_text(72,268,24,gold,"PREPARE SWITCH 2",700);
        ui_text(72,313,20,p.ink,"Civilization VII running",700);
        ui_text(72,347,20,p.ink,"Multiplayer > Wireless > Create Game > Host Lobby",700);
        ui_text(72,381,20,p.ink,"Chat field focused",700);
        ui_text(72,415,20,p.ink,"Keyboard configuration: English-US",700);
        ui_wrap(72,465,706,20,3,p.muted,"Prepare the wireless lobby manually and focus chat. RecurCIV does not navigate these menus.");
        ui_text(860,268,24,gold,"USB CONNECTION",348);
        ui_text(860,313,20,p.ink,c->ready?"Both HID interfaces ready":"USB not ready",348);
        ui_wrap(860,360,348,20,3,p.muted,"Minimum schedule: about 16m 33s. USB waits can make this longer.");
        ui_wrap(860,454,348,20,3,p.muted,"Select START PAYLOAD only after preparing the game.");
        if(c->start_error) {
            if(c->start_error==CIV_START_SLEEP_WARNING) {
                snprintf(text,sizeof(text),"%s (0x%08lX)",c->sleep.restore_failed?
                    "Sleep override restore failed. Retry Start or exit, and check sleep settings.":view_civ_start_error(c->start_error),
                    (unsigned long)c->sleep.error);
                ui_wrap(48,575,1184,20,2,gold,text);
            } else ui_wrap(48,575,1184,20,2,gold,view_civ_start_error(c->start_error));
        }
        return;
    }
    card_frame(48,160,1184,430,false);
    ui_text(72,184,32,gold,view_civ_status(v),1136);
    uint64_t seconds=c->elapsed_ms/1000;
    snprintf(text,sizeof(text),"Elapsed %02llu:%02llu",(unsigned long long)(seconds/60),(unsigned long long)(seconds%60));
    ui_text(72,234,24,p.muted,text,1136);
    size_t total=r->lines_total?r->lines_total:87,completed=r->lines_completed;
    if(completed>total) completed=total;
    if(c->progress.acquiring) snprintf(text,sizeof(text),"Preparing USB - waiting for a safe handoff");
    else if(c->progress.owned && r->line) snprintf(text,sizeof(text),"Message %zu of %zu",r->line,total);
    else snprintf(text,sizeof(text),"%zu of %zu messages transmitted.",completed,total);
    ui_text(72,282,32,p.ink,text,1136);
    ui_text(72,334,24,p.muted,view_civ_stage(v),1136);
    const char *detail;
    if(c->progress.shutdown_unresolved) detail="Forced shutdown left cleanup unresolved. Close and restart RecurCIV; neutral delivery is not confirmed.";
    else if(view_civ_blocked(v)) detail="New runs and other modes are blocked until cleanup succeeds. Reconnect USB if needed, then select RETRY CLEANUP.";
    else if(c->progress.owned && !c->ready) detail="USB disconnected or not ready. Payload scheduling has stopped. Reconnect USB for neutral cleanup only; automation will not resume.";
    else if(c->progress.owned && (c->cancel_requested || r->status!=CIV_RUNNER_RUNNING) && !c->progress.acquiring)
        detail="Releasing keyboard and controller inputs. Keep USB connected; ownership is retained until cleanup completes.";
    else if(!c->progress.owned && r->status==CIV_RUNNER_FINISHED && r->neutral_confirmed)
        detail="Verify that Switch 2 displays the expected receiver/waiting screen. USB transmission does not confirm in-game acceptance.";
    else if(!c->progress.owned && r->neutral_confirmed)
        detail=r->status==CIV_RUNNER_FAILED?"USB transfer failed or timed out. Neutral cleanup confirmed. No automatic restart or resume.":"Automation stopped. Neutral cleanup confirmed. No automatic restart or resume.";
    else detail="Input is automatic. Keep USB connected. Physical controller input stays local.";
    ui_wrap(72,380,1136,20,2,p.muted,detail);
    ui_roundrect(72,456,1136,16,8,color(0x40536b));
    int width=(int)(1136*completed/total);if(width) ui_roundrect(72,456,width,16,8,gold);
    snprintf(text,sizeof(text),"Completed messages: %zu / %zu",completed,total);ui_text(72,490,20,gold,text,1136);
    if(r->line_length && c->progress.owned) {
        snprintf(text,sizeof(text),"Characters completed in message: %zu / %zu",r->character,r->line_length);
        ui_text(72,522,20,p.muted,text,1136);
    }
    if(r->error || c->transport_result) {
        snprintf(text,sizeof(text),"Runner error: %u   |   USB transfer result: %u",(unsigned)r->error,c->transport_result);
        ui_text(72,554,20,gold,text,1136);
    }
    if(c->sleep.error) {
        snprintf(text,sizeof(text),c->sleep.restore_failed?
            "Sleep override restore failed (0x%08lX). Check Switch sleep settings; exit retries restoration.":
            "Auto-sleep NOT confirmed (0x%08lX). Configure Switch sleep settings manually.",(unsigned long)c->sleep.error);
        ui_text(48,606,20,gold,text,1184);
    } else if(c->sleep.held && c->sleep.protected) {
        ui_text(48,606,20,p.muted,"Auto-sleep prevention confirmed. Physical Power-button sleep is not blocked.",1184);
    }
}
static void disc(int x,int y,int r,uint32_t c) {
    for(int dy=-r;dy<=r;dy++) for(int dx=-r;dx<=r;dx++)
        if(dx*dx+dy*dy<=r*r) ui_rect(x+dx,y+dy,1,1,c);
}
static const char *const control_names[]={"A","B","X","Y","L3","R3","L","R","ZL","ZR","PLUS","MINUS","LEFT","UP","RIGHT","DOWN"};
static void held_text(uint32_t held,char *out,size_t size) {
    size_t used=0;out[0]=0;
    for(unsigned i=0;i<16;i++) if(held&(1u<<i)) {
        int n=snprintf(out+used,size-used,"%s%s",used?" + ":"",control_names[i]);
        if(n<0 || (size_t)n>=size-used) break;
        used+=(size_t)n;
    }
    if(!used) snprintf(out,size,"None");
}
static void menu_icon(int which,int x,int y) {
    uint32_t white=color(0xd9aa54),dark=color(0x17273d),gold=color(0xd9aa54);
    if(which==0) {
        for(int i=0;i<70;i++) { ui_rect(x-54+i/3,y-35+i,7,1,gold);ui_rect(x-8-i/3,y-35+i,7,1,gold); }
        ui_rect(x+14,y-35,8,70,gold);ui_rect(x+38,y-35,8,70,gold);
        ui_rect(x+8,y-35,20,4,gold);ui_rect(x+32,y-35,20,4,gold);
        ui_rect(x+8,y+31,20,4,gold);ui_rect(x+32,y+31,20,4,gold);return;
    }
    if(which==1) {
        ui_roundrect(x-58,y-31,116,62,7,white);
        ui_roundrect(x-54,y-27,108,54,5,dark);
        for(int row=0;row<3;row++) for(int col=0;col<8;col++)
            ui_rect(x-47+col*12,y-19+row*12,8,7,white);
        ui_rect(x-30,y+17,60,5,white);
        return;
    }
    /* Slim, flat gamepad with one D-pad, two sticks and four face buttons. */
    ui_roundrect(x-64,y-29,128,58,20,white);
    ui_roundrect(x-60,y-25,120,50,17,dark);
    ui_rect(x-44,y-3,22,6,white);ui_rect(x-36,y-11,6,22,white);
    disc(x-12,y+12,5,white);disc(x+15,y+12,5,white);
    disc(x+40,y-12,3,white);disc(x+40,y+4,3,white);
    disc(x+32,y-4,3,white);disc(x+48,y-4,3,white);
}
int view_home_text_x(int card,int text_width) { return 58+card*400+(374-text_width)/2; }
int view_home_move(int selected,int direction) { return (selected+(direction>0?1:2))%3; }
int view_keyboard_move(int selected,int direction) { return (selected+(direction>0?1:2))%3; }
Action view_keyboard_select(int selected) { return selected==0?ACTION_COMPOSE:selected==1?ACTION_LIVE:ACTION_FILES; }
ViewControl view_compose_control(unsigned i) {
    static const ViewControl controls[COMPOSE_CONTROL_COUNT]={
        {48,556,376,56,ACTION_SEND},{452,556,376,56,ACTION_COMPOSE},{856,556,376,56,ACTION_BACK},
        {48,466,280,64,ACTION_PACE},{344,466,280,64,ACTION_ENTER_PAUSE},
        {640,466,280,64,ACTION_AUTOINDENT},{936,466,296,64,ACTION_CAPS_LOCK},
        {900,392,146,40,ACTION_COMPOSE_PREV},{1062,392,146,40,ACTION_COMPOSE_NEXT}
    };
    return i<COMPOSE_CONTROL_COUNT?controls[i]:(ViewControl){0};
}
Action view_compose_action(const View *v,unsigned i) {
    if(i>=COMPOSE_CONTROL_COUNT || v->caps_pending || v->civ.progress.owned) return ACTION_NONE;
    if(i==0 && (!v->usb_ready || !v->compose_valid)) return ACTION_NONE;
    if(i==6 && !v->usb_ready) return ACTION_NONE;
    if(i==7 && !v->compose_page) return ACTION_NONE;
    if(i==8 && v->compose_page+1>=v->compose_pages) return ACTION_NONE;
    return view_compose_control(i).action;
}
void view_compose_label(const View *v,unsigned i,char *out,size_t size) {
    switch(i) {
    case 0: snprintf(out,size,"SEND TEXT");break;
    case 1: snprintf(out,size,"EDIT");break;
    case 2: snprintf(out,size,"BACK");break;
    case 3: snprintf(out,size,"ZR  Delay: %s",pace_label(v->pace));break;
    case 4: snprintf(out,size,"R  Enter pause: %d ms",v->enter_pause_ms);break;
    case 5: snprintf(out,size,"L  Auto-indent: %s",v->cancel_autoindent?"on":"off");break;
    case 6: snprintf(out,size,v->caps_pending?"Caps: sending key...":v->caps_state<0?
        "Toggle Caps (no host LED)":v->caps_state?"Caps Lock: on (host)":"Caps Lock: off (host)");break;
    case 7: snprintf(out,size,"Previous");break;
    case 8: snprintf(out,size,"Next");break;
    default: if(size) out[0]=0;break;
    }
}
void view_compose_navigate(View *v,uint32_t down) {
    static const unsigned next[COMPOSE_CONTROL_COUNT][4]={
        {2,1,3,7},{0,2,4,7},{1,0,6,8},{6,4,7,0},{3,5,7,1},
        {4,6,8,1},{5,3,8,2},{8,8,0,3},{7,7,2,6}
    };
    if(v->selected<0 || v->selected>=COMPOSE_CONTROL_COUNT) v->selected=0;
    int direction=(down&MON_LEFT)?0:(down&MON_RIGHT)?1:(down&MON_UP)?2:(down&MON_DOWN)?3:-1;
    if(direction>=0) v->selected=(int)next[v->selected][direction];
}
bool view_adjust_typing_setting(View *v,Action action) {
    if(v->civ.progress.owned) return false;
    if(action==ACTION_PACE) v->pace=pace_next(v->pace);
    else if(action==ACTION_ENTER_PAUSE) v->enter_pause_ms=enter_pause_next(v->enter_pause_ms);
    else if(action==ACTION_AUTOINDENT) v->cancel_autoindent=!v->cancel_autoindent;
    else return false;
    return true;
}
int view_studio_move(int selected,int direction) { return (selected+(direction>0?1:2))%3; }
void view_menu_stick_direction(int32_t x,int32_t y,int *dx,int *dy) {
    if(!dx || !dy) return;
    *dx=*dy=0;
    int64_t ax=x<0?-(int64_t)x:x,ay=y<0?-(int64_t)y:y;
    if(ax<12000 && ay<12000) return;
    if(ax>ay) *dx=x<0?-1:1;else *dy=y>0?-1:1;
}
static void macro_append(char *out,size_t capacity,const char *text) {
    size_t used=strlen(out);
    if(used<capacity) snprintf(out+used,capacity-used,"%s%s",used?" + ":"",text);
}
void view_macro_step(const MacroStep *step,size_t index,char *label,size_t label_size,char *detail,size_t detail_size) {
    if(!step || !label || !label_size || !detail || !detail_size) return;
    static const char *const buttons[]={"Y","B","A","X","L","R","ZL","ZR","Minus","Plus","L Stick Click","R Stick Click"};
    static const char *const hats[]={"D-pad Up","D-pad Up-Right","D-pad Right","D-pad Down-Right","D-pad Down","D-pad Down-Left","D-pad Left","D-pad Up-Left"};
    char inputs[256]={0};
    for(unsigned i=0;i<12;i++) if(step->report.buttons&(1u<<i)) macro_append(inputs,sizeof(inputs),buttons[i]);
    if(step->report.hat<8) macro_append(inputs,sizeof(inputs),hats[step->report.hat]);
    if(step->report.lx!=128) macro_append(inputs,sizeof(inputs),step->report.lx<128?"Left Stick Left":"Left Stick Right");
    if(step->report.ly!=128) macro_append(inputs,sizeof(inputs),step->report.ly<128?"Left Stick Up":"Left Stick Down");
    if(step->report.rx!=128) macro_append(inputs,sizeof(inputs),step->report.rx<128?"Right Stick Left":"Right Stick Right");
    if(step->report.ry!=128) macro_append(inputs,sizeof(inputs),step->report.ry<128?"Right Stick Up":"Right Stick Down");
    snprintf(label,label_size,"%zu. %s",index+1,inputs[0]?inputs:"Neutral / delay");
    if(step->report.lx!=128 || step->report.ly!=128 || step->report.rx!=128 || step->report.ry!=128)
        snprintf(detail,detail_size,"%lu ms   |   L X:%u Y:%u   R X:%u Y:%u",(unsigned long)step->duration_ms,step->report.lx,step->report.ly,step->report.rx,step->report.ry);
    else snprintf(detail,detail_size,"%lu ms",(unsigned long)step->duration_ms);
}
size_t view_macro_control_count(const View *v) {
    switch(v->screen) {
    case SCREEN_MACRO_STUDIO: return 4;
    case SCREEN_MACRO_LIBRARY: return (size_t)v->file_rows+4;
    case SCREEN_MACRO_DETAILS: return 9;
    case SCREEN_MACRO_DELETE: return 2;
    case SCREEN_MACRO_EDITOR: return 16;
    case SCREEN_MACRO_STEP: return 21;
    case SCREEN_MACRO_RECORD: return view_footer(v).count;
    case SCREEN_MACRO_PLAY: return 1;
    case SCREEN_MACRO_DISCARD: return 2;
    default: return 0;
    }
}
ViewControl view_macro_control(const View *v,unsigned i) {
    if(i>=view_macro_control_count(v)) return (ViewControl){0};
    if(v->screen==SCREEN_MACRO_EDITOR && i<15) {
        const int actions[]={ACTION_MS_PREV_STEP,ACTION_MS_NEXT_STEP,ACTION_MS_INSERT,ACTION_MS_EDIT,ACTION_MS_REMOVE,
            ACTION_MS_MOVE_UP,ACTION_MS_MOVE_DOWN,ACTION_MS_DELAY,ACTION_MS_UNDO,ACTION_MS_REDO,
            ACTION_MS_REPEATS,ACTION_MS_SPEED,ACTION_MS_SAVE,ACTION_MS_PLAY,ACTION_BACK};
        return (ViewControl){48+(int)(i%5)*238,450+(int)(i/5)*55,220,46,(Action)actions[i]};
    }
    if(v->screen==SCREEN_MACRO_STEP) {
        if(i<12) return (ViewControl){48+(int)(i%4)*300,175+(int)(i/4)*75,280,60,(Action)(ACTION_MS_FIELD0+i)};
        if(i<17) return (ViewControl){48+(int)(i-12)*238,400,220,56,(Action)(ACTION_MS_FIELD0+i)};
        if(i<19) return (ViewControl){48+(int)(i-17)*378,480,360,56,(Action)(i==17?ACTION_MS_FIELD0+17:ACTION_MS_NEUTRAL)};
        return (ViewControl){i==19?48:856,556,376,56,(Action)(i==19?ACTION_MS_APPLY:ACTION_BACK)};
    }
    unsigned footer=0;
    if(v->screen==SCREEN_MACRO_STUDIO) {
        if(i<3) return (ViewControl){48,160+(int)i*155,1184,140,i==0?ACTION_STUDIO_MANUAL:i==1?ACTION_STUDIO_RECORD:(Action)ACTION_MS_LIVE_RECORD};
        footer=i-3;
    } else if(v->screen==SCREEN_MACRO_LIBRARY) {
        if(i<(unsigned)v->file_rows) return (ViewControl){48,180+(int)i*50,1184,46,(Action)(ACTION_ROW0+i)};
        footer=i-(unsigned)v->file_rows;
    } else if(v->screen==SCREEN_MACRO_DETAILS) {
        const int actions[]={ACTION_MS_PLAY,ACTION_MS_EDIT,ACTION_MS_RENAME,ACTION_MS_DUPLICATE,ACTION_MACRO_DELETE_ASK};
        if(i<5) return (ViewControl){48+(int)i*238,545,220,50,(Action)actions[i]};
        footer=i-5;
    } else if(v->screen==SCREEN_MACRO_DELETE) {
        if(!i) return (ViewControl){48,545,260,56,ACTION_MACRO_DELETE_CONFIRM};
        footer=0;
    } else footer=v->screen==SCREEN_MACRO_EDITOR?i-15:i;
    ViewFooter f=view_footer(v);if(footer>=f.count) return (ViewControl){0};
    return (ViewControl){f.buttons[footer].x,648,f.buttons[footer].w,56,f.buttons[footer].action};
}
void view_macro_control_label(const View *v,unsigned i,char *out,size_t cap) {
    if(v->screen==SCREEN_MACRO_EDITOR && i<15) {
        const char *labels[]={"Previous step","Next step","Insert action","Edit action","Remove action","Move up","Move down","Insert delay","Undo","Redo"};
        if(i<10) snprintf(out,cap,"%s",labels[i]);
        else if(i==10) snprintf(out,cap,"Repeats: %u",v->macro_repeat);
        else if(i==11) snprintf(out,cap,"Speed: %u%%",v->macro_speed);
        else snprintf(out,cap,"%s",i==12?"Save":i==13?"Play repeats":"Back");
    } else if(v->screen==SCREEN_MACRO_STEP) {
        const char *names[]={"Y","B","A","X","L","R","ZL","ZR","Minus","Plus","L3","R3"};
        if(i<12) snprintf(out,cap,"%s: %s",names[i],v->macro_draft.report.buttons&(1u<<i)?"held":"released");
        else if(i==12) snprintf(out,cap,"D-pad: %u (8=none)",v->macro_draft.report.hat);
        else if(i<17) {
            const char *axes[]={"LX","LY","RX","RY"};
            snprintf(out,cap,"%s: %u",axes[i-13],((const uint8_t *)&v->macro_draft.report.lx)[i-13]);
        } else if(i==17) snprintf(out,cap,"Hold: %lu ms",(unsigned long)v->macro_draft.duration_ms);
        else snprintf(out,cap,"%s",i==18?"Reset to neutral":i==19?"Apply action":"Cancel action");
    } else if(v->screen==SCREEN_MACRO_DETAILS && i<5) {
        const char *labels[]={"Play repeats","Edit","Rename","Duplicate","Delete"};snprintf(out,cap,"%s",labels[i]);
    } else out[0]=0;
}
static void macro_controls(const View *v) {
    for(unsigned i=0;i<view_macro_control_count(v);i++) {
        ViewControl c=view_macro_control(v,i);if(c.y>=640) continue;
        char label[96];view_macro_control_label(v,i,label,sizeof(label));if(!*label) continue;
        card_frame(c.x,c.y,c.w,c.h,(int)i==v->selected);
        ui_text(c.x+12,c.y+(c.h-20)/2-2,20,ui_palette.ink,label,c.w-24);
    }
}
static void macro_sequence_cards(const View *v) {
    for(unsigned i=0;i<3;i++) if(v->macro_steps[i][0]) {
        int y=170+(int)i*90;card_frame(48,y,1184,84,v->macro_cursor%3==i);
        ui_wrap(64,y+3,1148,20,2,ui_palette.ink,v->macro_steps[i]);
        ui_text(64,y+61,20,ui_palette.muted,v->macro_step_details[i],1148);
    }
}
static void render_macro_workspace(const View *v) {
    Palette p=ui_palette;char text[256];
    bool sequence=v->screen==SCREEN_MACRO_EDITOR || v->screen==SCREEN_MACRO_DETAILS;
    ui_text(48,101,32,p.ink,sequence && v->name[0]?v->name:view_screen_name(v->screen),1184);
    if(v->screen==SCREEN_MACRO_EDITOR || v->screen==SCREEN_MACRO_DETAILS) {
        snprintf(text,sizeof(text),"%zu steps   |   minimum %llu ms   |   repeats %u   |   speed %u%%   |   %s",v->macro_step_count,
            (unsigned long long)v->macro_duration,v->macro_repeat,v->macro_speed,v->macro_dirty?"UNSAVED":"saved");
        ui_text(48,140,20,p.muted,text,1184);macro_sequence_cards(v);
        if(!v->macro_step_count) ui_text(72,240,24,p.muted,"No actions yet. Choose Insert action or Insert delay.",1100);
        if(v->screen==SCREEN_MACRO_DETAILS) {
            ui_text(48,455,20,p.muted,v->macro_description[0]?v->macro_description:"No description.",1184);
            snprintf(text,sizeof(text),"Page %zu / %zu   |   loop delay %u ms",v->macro_page+1,v->macro_pages?v->macro_pages:1,v->macro_loop_delay);
            ui_text(48,495,20,p.muted,text,1184);
        }
        macro_controls(v);
    } else if(v->screen==SCREEN_MACRO_STEP) {
        ui_text(48,140,20,p.muted,"Toggle combinations. Hat: 0=Up, clockwise to 7; 8=neutral. Stick range 0-255.",1184);macro_controls(v);
    } else if(v->screen==SCREEN_MACRO_RECORD) {
        card_frame(48,175,1184,390,false);
        const char *title=v->macro_record_cleanup?(v->macro_cleanup_blocked?"USB CLEANUP REQUIRED":"NEUTRALIZING USB OUTPUT"):
            v->macro_replace_pending?"REPLACE UNSAVED CAPTURE?":
            v->macro_record_sync?"SYNCHRONIZING NEUTRAL OUTPUT":v->macro_arming?"RELEASE CONTROLS TO BEGIN":
            v->macro_recording?(v->macro_live?"LIVE RECORDING":"RECORDING - LOCAL ONLY"):
            v->macro_live && v->macro_owned?"LIVE CONTROLLER - NOT RECORDING":
            v->macro_has_recording?"RECORDING STOPPED":v->macro_live?"LIVE SESSION ENDED":"READY TO RECORD";
        ui_text(72,210,32,color(0xd9aa54),title,1120);
        snprintf(text,sizeof(text),"Elapsed %02llu:%02llu   |   %zu completed states",(unsigned long long)(v->macro_elapsed/60000),
            (unsigned long long)(v->macro_elapsed/1000%60),v->macro_step_count);ui_text(72,280,24,p.ink,text,1120);
        ui_wrap(72,350,1100,24,4,p.muted,v->macro_live?
            "Passthrough continues before, during and after recording. Capture short press or touch Start/Stop. Exit live and wait for neutral cleanup before Review/Save. Capture is never a macro input. Limits: 1024 states / 10 minutes.":
            "All supported controller buttons and sticks are captured after release-to-arm. Nothing is forwarded. Touch Stop to finish, then review and edit before saving. Limits: 1024 states / 10 minutes.");
        if(v->macro_status[0]) ui_wrap(72,475,1100,20,2,color(0xd9aa54),v->macro_status);
        if(v->macro_live && v->macro_capture_status[0]) ui_wrap(72,577,1100,20,2,color(0xd9aa54),v->macro_capture_status);
    } else if(v->screen==SCREEN_MACRO_PLAY) {
        card_frame(48,175,1184,390,false);
        ui_wrap(72,210,1100,24,2,color(0xd9aa54),v->macro_status);
        snprintf(text,sizeof(text),"Step %zu / %zu   |   repeat %u / %u",v->macro_cursor<v->macro_step_count?v->macro_cursor+1:v->macro_step_count,
            v->macro_step_count,v->macro_iteration,v->macro_repeat);ui_text(72,310,32,p.ink,text,1120);
        snprintf(text,sizeof(text),"Elapsed %02llu:%02llu",(unsigned long long)(v->macro_elapsed/60000),(unsigned long long)(v->macro_elapsed/1000%60));
        ui_text(72,370,24,p.muted,text,1120);
        ui_wrap(72,430,1100,20,3,p.muted,"Holds are timed from state submission; completion still gates the next state. Late USB/ticks can stretch timing. Stop requests neutral cleanup; navigation stays locked until completion. Reconnection never resumes playback.");
    } else if(v->screen==SCREEN_MACRO_DISCARD) {
        card_frame(48,175,1184,350,true);ui_text(72,220,32,p.ink,"Discard unsaved macro changes?",1120);
        ui_wrap(72,300,1100,24,3,p.muted,"The saved file is not changed. Choose Keep editing to return. Discard requires an explicit confirmation.");
    } else if(v->screen==SCREEN_MACRO_LIBRARY) {
        snprintf(text,sizeof(text),"%zu macros   |   page %zu / %zu",v->macro_count,v->macro_page+1,v->macro_pages?v->macro_pages:1);ui_text(48,140,20,p.muted,text,1184);
        for(int i=0;i<v->file_rows;i++) { ViewControl c=view_macro_control(v,(unsigned)i);card_frame(c.x,c.y,c.w,c.h,i==v->selected);ui_text(c.x+16,c.y+11,24,p.ink,v->files[i],c.w-32); }
        if(!v->file_rows) ui_text(72,240,24,p.muted,"No macros. Create one in Macro Studio, then Save.",1100);
    }
    if(v->screen!=SCREEN_MACRO_PLAY && v->screen!=SCREEN_MACRO_RECORD && v->macro_status[0]) ui_text(48,616,20,color(0xd9aa54),v->macro_status,1184);
}
Action view_home_action(Screen screen,bool plus,bool back,bool y,bool a,int selected) {
    if(screen!=SCREEN_HOME) return ACTION_NONE;
    if(plus) return ACTION_QUIT;
    if(back) return ACTION_BACK;
    if(y) return ACTION_DIAGNOSTICS;
    if(a) return selected==0?ACTION_MENU_CIV:selected==1?ACTION_MENU_KEYBOARD:ACTION_MENU_CONTROLLER;
    return ACTION_NONE;
}
void view_render(const View *v,uint32_t *pixels,int stride) {
    ui_begin(pixels,stride); Palette p=ui_palette;
    shared_header(v->screen);
    shared_footer(v);
    if(!v->macro_live && v->macro_capture_status[0])
        ui_text(48,616,20,color(0xd9aa54),v->macro_capture_status,1184);
    if(v->screen==SCREEN_MACRO_LIBRARY || v->screen==SCREEN_MACRO_DETAILS || v->screen>=SCREEN_MACRO_EDITOR) { render_macro_workspace(v);return; }
    if(v->screen==SCREEN_KEYBOARD || v->screen==SCREEN_FILES || v->screen==SCREEN_CONFIRM || v->screen==SCREEN_TRANSFER || v->screen==SCREEN_RESULT || v->screen==SCREEN_DIAGNOSTICS || v->screen==SCREEN_LIVE)
        ui_text(810,87,20,p.muted,v->usb_label,422);
    char buffer[256];
    if(v->screen==SCREEN_HOME) {
        ui_text(58,120,24,p.muted,"Select a USB input mode",800);
        const char *titles[]={"CIV Exploit","USB Keyboard","USB Controller"};
        const char *subtitles[]={"CIV VII Automation","Text & Live Input","Control & Macros"};
        for(int i=0;i<3;i++) {
            int x=58+i*400;
            card_frame(x,188,374,306,i==v->selected);
            menu_icon(i,x+187,292);
            int title_width=ui_text_width(32,titles[i]);
            int sub_width=ui_text_width(20,subtitles[i]);
            ui_text(view_home_text_x(i,title_width),374,32,p.ink,titles[i],title_width+2);
            ui_text(view_home_text_x(i,sub_width),424,20,p.muted,subtitles[i],sub_width+2);
        }
        return;
    } else if(v->screen==SCREEN_CONTROLLER_MENU) {
        ui_text(48,82,32,p.ink,"USB CONTROLLER",900);
        ui_text(48,120,20,p.muted,"Choose a controller mode. Menu input stays local.",1100);
        const char *titles[]={"Standard Controller","Macro Studio","Macro Library"};
        const char *descriptions[]={"Live controller passthrough","Manual builder and recording (in development)","Browse saved macros"};
        for(int i=0;i<3;i++) {
            int y=150+i*135;card_frame(48,y,1184,112,i==v->selected);
            ui_text(75,y+17,32,p.ink,titles[i],1080);
            ui_text(75,y+66,20,p.muted,descriptions[i],1080);
        }
        if(v->macro_status[0]) ui_text(48,575,20,color(0xd9aa54),v->macro_status,1100);
        return;
    } else if(v->screen==SCREEN_MACRO_STUDIO) {
        ui_text(48,88,32,p.ink,"MACRO STUDIO",1000);
        const char *titles[]={"MANUAL BUILDER","LOCAL RECORD","LIVE RECORD"};
        const char *descriptions[]={"Create controller combinations, holds and neutral delays.","Record locally without USB forwarding; review, edit and save.","Enter live passthrough; Capture or touch Start/Stop to record."};
        for(int i=0;i<3;i++) {
            int y=160+i*155;card_frame(48,y,1184,140,i==v->selected);
            ui_text(75,y+25,32,p.ink,titles[i],1050);
            ui_text(75,y+80,20,p.muted,descriptions[i],1050);
        }
        if(v->macro_status[0]) ui_text(48,616,20,color(0xd9aa54),v->macro_status,1184);
        return;
    } else if(v->screen==SCREEN_MACRO_DELETE) {
        ui_text(48,82,32,p.ink,"Delete macro?",1000);
        card_frame(48,160,1184,320,false);
        ui_text(75,190,32,p.ink,v->name,1100);
        ui_wrap(75,260,1000,24,3,p.muted,"This permanently deletes the selected saved macro. This cannot be undone.");
        button(48,545,260,"Delete",v->selected==0);return;
    } else if(v->screen==SCREEN_CONTROLLER) {
        uint32_t panel=color(0x17273d),gold=color(0xd9aa54);
        uint32_t white=p.ink,muted=p.muted,line=p.line;
        ui_text(48,82,32,white,"STANDARD CONTROLLER",750);
        ui_text(48,116,20,muted,"Physical Controller Passthrough",430);
        const char *status=!v->controller_usb_ready?"USB disconnected - waiting for reconnection":
            v->controller_interrupted?"USB interrupted - touch Exit to neutralize":
            v->controller_exiting?"Neutralizing - waiting for USB completion":
            v->controller_synchronizing?"Synchronizing neutral report...":
            !v->controller_connected?"Controller disconnected - reconnect to activate":
            !v->controller_armed?"Release buttons to activate":"Controller active";
        ui_text(500,116,20,gold,status,700);
        ui_text(52,150,20,muted,"CURRENTLY HELD (LOCAL)",700);
        int x=52,y=188;
        for(unsigned i=0;i<16;i++) if(v->monitor.held&(1u<<i)) {
            int w=ui_text_width(20,control_names[i])+24;if(w<60) w=60;
            if(x+w>1228) { x=52;y+=56; }
            ui_roundrect(x,y,w,45,8,gold);ui_roundrect(x+2,y+2,w-4,41,6,panel);
            ui_text(x+12,y+12,20,white,control_names[i],w-20);x+=w+5;
        }
        if(!v->monitor.held) ui_text(52,201,20,muted,v->controller_connected?"None":"Controller disconnected",500);
        snprintf(buffer,sizeof(buffer),"L X:%3u Y:%3u    R X:%3u Y:%3u  |  Local state, delivery not confirmed",
            v->controller_report.lx,v->controller_report.ly,v->controller_report.rx,v->controller_report.ry);
        ui_text(52,260,20,muted,buffer,1176);ui_line(44,302,1192,line);
        ui_text(52,328,20,muted,"INPUT HISTORY (LOCAL)",800);
        for(unsigned i=0;i<v->monitor.count && i<MON_HISTORY;i++) {
            int row_y=365+(int)i*55;char held[144];held_text(controller_monitor_history(&v->monitor,i),held,sizeof(held));
            ui_roundrect(52,row_y,1176,49,8,i==0?color(0x20364a):panel);
            ui_text(70,row_y+13,20,i==0?gold:white,held,1120);
        }
        return;
    } else if(v->screen==SCREEN_KEYBOARD) {
        ui_text(48,82,32,p.ink,"USB KEYBOARD",1184);
        ui_text(48,120,20,p.muted,"Choose how to send keyboard input.",1184);
        const char *titles[]={"Compose & Send","Live Keyboard","Send Text File"};
        const char *descriptions[]={"Write, review, and send complete text.","Send individual keystrokes in real time.","Select a .txt file from your SD card to send."};
        for(int i=0;i<3;i++) {
            int y=150+i*135;bool active=v->selected==i;
            card_frame(48,y,1184,112,active);
            ui_text(75,y+17,32,p.ink,titles[i],1080);
            ui_text(75,y+66,20,p.muted,descriptions[i],1080);
        }
        if(v->compose_error[0]) ui_text(48,584,20,p.negative,v->compose_error,1184);
    } else if(v->screen==SCREEN_LIVE) {
        uint32_t gold=color(0xd9aa54),gold_tint=color(0x3b2f1b),caps_tint=color(0x2d2414);
        ui_text(48,118,32,p.ink,"Live Keyboard",500);
        ui_text(48,151,20,p.muted,"US QWERTY  /  Touch or controller",900);
        for(unsigned i=0;i<live_key_count();i++) {
            LiveKey k=live_key(i);
            bool shift_key=k.key==LIVE_SHIFT;
            bool selected=i==v->live.selected;
            bool pressed=v->live.touched==k.key ||
                (v->live.pressed==k.key && v->now<v->live.pressed_until);
            bool one_shot=shift_key && v->live.shift;
            bool caps=shift_key && v->live.caps;
            uint32_t background=pressed?gold_tint:caps?caps_tint:p.soft;
            int border=caps?4:(selected||one_shot||pressed)?3:2;
            uint32_t border_color=(caps||selected||one_shot||pressed)?gold:p.line;
            ui_roundrect(k.x,k.y,k.w,k.h,10,border_color);
            int inner_radius=10-border;
            if(inner_radius<2) inner_radius=2;
            ui_roundrect(k.x+border,k.y+border,k.w-2*border,k.h-2*border,inner_radius,background);
            char label[2];
            const char *text=live_key_text(&v->live,&k,label,sizeof(label));
            int size=24,w=ui_text_width(size,text);
            if(w>k.w-16) { size=20;w=ui_text_width(size,text); }
            ui_text(k.x+(k.w-w)/2,k.y+(k.h-28)/2,size,p.ink,text,k.w-12);
        }
        const char *status=v->live.exiting?"Closing keyboard...":v->live.fault?"USB transfer failed. Reconnect USB or close.":!v->usb_ready?"USB disconnected; input disabled.":v->live.phase==LIVE_SYNC || v->live.phase==LIVE_SYNC_WAIT?"Preparing keyboard...":"Touch a key or use the controller. Shift twice for CAPS.";
        ui_text(48,546,20,v->live.fault?p.negative:p.muted,status,1184);
        if(v->live.dropped) ui_text(48,574,20,p.negative,"Input queue full; some taps were not sent.",1184);
        ui_text(48,600,20,p.muted,"Double-tap X or touch Shift within 350 ms for CAPS.",1184);
    } else if(v->screen==SCREEN_CIV_PLACEHOLDER) {
        render_civ(v);return;
    } else if(v->screen==SCREEN_FILES) {
        ui_text(48,132,32,p.ink,"Select a file",900);
        ui_text(48,176,20,p.muted,v->path,900);
        for(int i=0;i<v->file_rows;i++) {
            int y=218+i*44;bool active=v->selected==i;
            card_frame(48,y,1184,42,active);
            ui_text(64,y+10,20,active?p.ink:p.muted,v->directories[i]?"FOLDER":"TXT",110);
            ui_text(198,y+8,24,p.ink,v->files[i],1000);
        }
        if(!v->file_rows) ui_wrap(52,268,1000,24,3,p.muted,"No .txt files in this folder.");
        snprintf(buffer,sizeof(buffer),"%zu items",v->file_count);
        ui_text(1030,176,20,p.muted,buffer,200);
    } else if(v->screen==SCREEN_CONFIRM) {
        if(v->composing) {
            ui_text(48,101,32,p.ink,"COMPOSE & SEND",1184);
            snprintf(buffer,sizeof(buffer),"%zu characters   /   %zu line breaks   /   %zu tabs",v->characters,v->lines,v->tabs);
            ui_text(48,147,20,p.muted,buffer,1184);
            card_frame(48,190,1184,252,false);
            if(!v->characters) ui_text(72,216,24,p.muted,"No text yet. Choose EDIT to write a message.",1100);
            else for(int i=0;i<COMPOSE_ROWS;i++) ui_text(72,208+i*30,24,p.ink,v->compose_rows[i],1136);
            snprintf(buffer,sizeof(buffer),"Page %zu / %zu",v->compose_page+1,v->compose_pages?v->compose_pages:1);
            ui_text(72,402,20,p.muted,buffer,700);
            ui_text(48,444,20,p.muted,"TYPING SETTINGS",1184);
            for(unsigned i=0;i<COMPOSE_CONTROL_COUNT;i++) {
                ViewControl c=view_compose_control(i);char label[96];view_compose_label(v,i,label,sizeof(label));
                bool focus=(int)i==v->selected;
                ui_roundrect(c.x,c.y,c.w,c.h,8,focus?color(0xd9aa54):p.line);
                ui_roundrect(c.x+2,c.y+2,c.w-4,c.h-4,6,p.surface);
                int size=i<3?24:20;
                ui_text(c.x+16,c.y+(c.h-size)/2-2,size,view_compose_action(v,i)==ACTION_NONE?p.muted:p.ink,label,c.w-32);
            }
            const char *notice=v->compose_error[0]?v->compose_error:!v->usb_ready?"USB not ready. Connect the target before sending.":
                "Review before sending. Caps is target-side; LED feedback depends on the host.";
            ui_text(48,616,20,v->compose_error[0]?p.negative:p.muted,notice,1184);
            return;
        }
        ui_text(48,134,32,p.ink,v->name,1150);
        snprintf(buffer,sizeof(buffer),"%zu characters   /   %zu line breaks   /   %zu tabs",v->characters,v->lines,v->tabs);
        ui_text(48,182,20,p.muted,buffer,1184);
        ui_roundrect(48,230,1184,254,20,p.surface);
        ui_wrap(72,251,1136,24,6,p.ink,v->preview);
        if(v->lines || v->tabs) ui_text(48,507,20,p.muted,"Line breaks press Enter. Tabs press Tab.",1184);
        else ui_text(48,507,20,p.muted,"No Enter key is added at the end.",1184);
        ui_text(48,540,20,p.muted,"The keyboard layout must match the target device.",650);
        ui_text(740,540,20,color(0xd9aa54),v->composing?"A  Send    B  Back    X  Edit":"A  Send    B  Back    X  Caps Lock",492);
        button(48,580,375,!v->usb_ready?"USB not ready":v->composing && !v->compose_valid?"Edit text to send":"Send (3 s delay)",v->usb_ready && (!v->composing || v->compose_valid));
        button(441,580,235,"Back",false);
        snprintf(buffer,sizeof(buffer),"ZR  Delay: %s",pace_label(v->pace));
        chip(722,588,350,buffer);
    } else if(v->screen==SCREEN_TRANSFER) {
        ui_text(48,159,48,p.ink,v->sending?"Sending":"Starting",1150);
        ui_text(50,229,24,p.muted,v->name,1150);
        snprintf(buffer,sizeof(buffer),"Delay: %s  /  Enter pause: %d ms",pace_label(v->pace),v->enter_pause_ms);
        ui_text(50,271,20,p.muted,buffer,1150);
        if(!v->sending) {
            snprintf(buffer,sizeof(buffer),"%d",v->seconds);
            ui_text(52,311,48,p.ink,buffer,200);
            ui_text(150,327,24,p.muted,"Select a text field on the target device.",1000);
        } else {
            snprintf(buffer,sizeof(buffer),"%zu / %zu keystrokes",v->done,v->total);
            ui_text(48,320,32,p.ink,buffer,1100);
            ui_roundrect(48,388,1184,16,8,p.soft);
            if(v->total) ui_roundrect(48,388,(int)(1184*v->done/v->total),16,8,p.accent);
            ui_text(48,437,24,p.muted,"Sending to target device...",1150);
        }
    } else if(v->screen==SCREEN_RESULT) {
        ui_text(48,159,48,v->error?p.negative:p.ink,v->error?"Error":"Finished",1150);
        ui_wrap(48,247,1100,24,6,p.ink,v->message);
    } else if(v->screen==SCREEN_DIAGNOSTICS) {
        ui_text(48,130,32,p.ink,"USB diagnostics",1180);
        for(int i=0;i<12;i++) ui_text(48,181+i*31,20,i==0&&v->error?p.negative:p.ink,v->diagnostics[i],1184);
        ui_text(48,581,20,p.muted,"Log: sdmc:/switch/RecurCIV/usb.log",1184);
    } else {
        ui_text(48,150,32,p.negative,"Unknown screen - return Home",1100);
    }
    if(v->screen==SCREEN_FILES) {
        snprintf(buffer,sizeof(buffer),"ZR  %s",pace_label(v->pace));chip(48,590,220,buffer);
        caps_chip(v,288,590,270);chip(578,590,180,"+  Exit");
    }
}
Action view_touch(const View *v,int x,int y) {
    if(y>=648 && y<704) {
        ViewFooter f=view_footer(v);
        for(size_t i=0;i<f.count;i++)
            if(x>=f.buttons[i].x && x<f.buttons[i].x+f.buttons[i].w) return f.buttons[i].action;
    }
    if(v->screen==SCREEN_LIVE) return ACTION_NONE; /* keys are routed through live_touch */
    if(view_macro_control_count(v)) {
        for(unsigned i=0;i<view_macro_control_count(v);i++) {
            ViewControl c=view_macro_control(v,i);
            if(x>=c.x && x<c.x+c.w && y>=c.y && y<c.y+c.h) return c.action;
        }
        if(v->screen==SCREEN_MACRO_EDITOR && x>=48 && x<1232 && y>=170 && y<434) {
            int row=(y-170)/90;if(row<3 && v->macro_steps[row][0]) return (Action)(ACTION_ROW0+row);
        }
        return ACTION_NONE;
    }
    if(v->screen==SCREEN_CONTROLLER_MENU) {
        if(x>=48 && x<1232) {
            if(y>=150 && y<262) return ACTION_CONTROLLER_STANDARD;
            if(y>=285 && y<397) return ACTION_MACRO_STUDIO;
            if(y>=420 && y<532) return ACTION_MACRO_LIBRARY;
        }
        return ACTION_NONE;
    }
    if(v->screen==SCREEN_CONFIRM && v->composing) {
        for(unsigned i=0;i<COMPOSE_CONTROL_COUNT;i++) {
            ViewControl c=view_compose_control(i);
            if(x>=c.x && x<c.x+c.w && y>=c.y && y<c.y+c.h) return view_compose_action(v,i);
        }
        return ACTION_NONE;
    }
    if(v->screen==SCREEN_HOME && y>=188 && y<494) {
        if(x>=58 && x<432) return ACTION_MENU_CIV;
        if(x>=458 && x<832) return ACTION_MENU_KEYBOARD;
        if(x>=858 && x<1232) return ACTION_MENU_CONTROLLER;
    }
    if(v->screen==SCREEN_KEYBOARD) {
        if(x>=48 && x<1232 && y>=150 && y<532 && (y-150)%135<112) {
            int card=(y-150)/135;if(card<3) return view_keyboard_select(card);
        }
        return ACTION_NONE;
    }
    if(v->screen==SCREEN_FILES && x>=48 && x<1232 && y>=218 && y<570) {
        int row=(y-218)/44;if(row<v->file_rows && (y-218)%44<42) return (Action)(ACTION_ROW0+row);
    }
    if(v->screen==SCREEN_CONFIRM && y>=580 && y<636) {
        if(x>=48 && x<423) return ACTION_SEND;
        if(x>=441 && x<676) return ACTION_BACK;
        if(x>=722 && x<1072 && y>=588 && y<628) return ACTION_PACE;
    }
    if(v->screen==SCREEN_FILES) {
        if(y>=590 && y<630) {
            if(x>=48 && x<268) return ACTION_PACE;
            if(x>=288 && x<558) return ACTION_CAPS_LOCK;
            if(x>=578 && x<758) return ACTION_QUIT;
        }
    }
    return ACTION_NONE;
}
