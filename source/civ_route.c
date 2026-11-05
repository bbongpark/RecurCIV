#include "civ_route.h"
#include "usb_civ.h"
#include "usb_keyboard.h"

static bool validation_ready(void *ctx) { (void)ctx;return true; }
static bool validation_submit(void *ctx,const CivRunnerOutput *out) { (void)ctx;(void)out;return false; }
static CivRunnerCompletion validation_poll(void *ctx,uint64_t ticket) { (void)ctx;return (CivRunnerCompletion){ticket,0,CIV_TRANSFER_FAILED}; }
static void validation_abort(void *ctx,uint64_t ticket) { (void)ctx;(void)ticket; }
bool civ_route_payload_valid(CivPayload payload) {
    CivRunner probe;
    CivRunnerTransport validation={NULL,validation_ready,validation_ready,validation_submit,validation_poll,validation_abort};
    return civ_runner_init(&probe,&validation) && civ_runner_start(&probe,payload);
}
void civ_route_update(CivRoute *r,View *v,uint64_t now) {
    if(!r || !v) return;
    civ_sleep_update(civ_usb_owned());
    bool on_screen=v->screen==SCREEN_CIV_PLACEHOLDER;
    if(on_screen && !r->on_screen) {
        v->civ=(CivView){0};v->selected=0;r->timing=false;
        r->input_locked=true;r->chord_tracking=false;r->navigation=(LiveNav){0};
    }
    r->on_screen=on_screen;
    if(!on_screen) return;
    bool was_owned=v->civ.progress.owned;
    v->civ.progress=civ_usb_progress();v->civ.ready=usb_civ_ready();
    v->civ.transport_result=(unsigned)usb_civ_transfer().result;
    v->civ.sleep=civ_sleep_info();
    if(r->timing) {
        if(now>=r->started_at) v->civ.elapsed_ms=now-r->started_at;
        if(!v->civ.progress.owned) r->timing=false;
    }
    if(was_owned && !v->civ.progress.owned) r->input_locked=true;
}
static void input_consumed(CivRoute *r) {
    r->input_locked=true;r->chord_tracking=false;
}
static bool can_cancel(const View *v) {
    return v->civ.progress.owned && !view_civ_blocked(v) && !v->civ.cancel_requested &&
        (v->civ.progress.acquiring || v->civ.progress.runner.status==CIV_RUNNER_RUNNING);
}
Action civ_route_input(CivRoute *r,View *v,uint32_t down,uint32_t held,
                      int32_t sx,int32_t sy,unsigned touches,int x,int y,uint64_t now) {
    if(!r || !v || v->screen!=SCREEN_CIV_PLACEHOLDER) return ACTION_NONE;
    if(r->input_locked) {
        if(!held && !touches && sx<12000 && sx> -12000 && sy<12000 && sy> -12000) r->input_locked=false;
        return ACTION_NONE;
    }
    ViewFooter footer=view_footer(v);
    if(can_cancel(v) &&
       (held&CIV_CANCEL_CHORD)==CIV_CANCEL_CHORD) {
        if(!r->chord_tracking) { r->chord_at=now;r->chord_tracking=true; }
        if(now>=r->chord_at && now-r->chord_at>=CIV_CANCEL_HOLD_MS) {
            input_consumed(r);return ACTION_CIV_CANCEL;
        }
    } else r->chord_tracking=false;
    /* Controller actions win the frame; an overlapping touch is consumed too. */
    Action action=ACTION_NONE;
    if(!v->civ.progress.owned && !v->civ.progress.shutdown_unresolved && (down&MON_B)) action=ACTION_BACK;
    else if(down&MON_A) {
        /* A is intentionally NOT a running Cancel shortcut. */
        if(footer.count && v->selected>=0 && (size_t)v->selected<footer.count &&
           footer.buttons[v->selected].action!=ACTION_CIV_CANCEL) action=footer.buttons[v->selected].action;
    }
    if(action==ACTION_NONE && !(down&(MON_A|MON_B)) && touches==1) action=view_touch(v,x,y);
    if(action!=ACTION_NONE) { input_consumed(r);return action; }
    if(touches) { input_consumed(r);return ACTION_NONE; }
    int dx=0,dy=0;view_menu_stick_direction(sx,sy,&dx,&dy);
    bool move=!!(down&(MON_LEFT|MON_RIGHT|MON_UP|MON_DOWN)) || live_nav_step(&r->navigation,dx,dy,now);
    if(move && footer.count>1) v->selected=v->selected?0:1;
    return ACTION_NONE;
}
bool civ_route_action(CivRoute *r,View *v,Action action,uint64_t now,bool available) {
    if(!r || !v || v->screen!=SCREEN_CIV_PLACEHOLDER) return false;
    civ_route_update(r,v,now);
    if(action==ACTION_NONE) return true;
    input_consumed(r);
    if(action==ACTION_CIV_CANCEL && can_cancel(v)) {
        civ_usb_cancel();v->civ.cancel_requested=true;v->civ.retrying=false;
    } else if(action==ACTION_CIV_RETRY && v->civ.progress.owned && view_civ_blocked(v) &&
              !v->civ.progress.shutdown_unresolved) {
        v->civ.retrying=civ_usb_retry_cleanup();
    } else if(action==ACTION_BACK && !v->civ.progress.owned && !v->civ.progress.shutdown_unresolved) {
        v->screen=SCREEN_HOME;v->selected=0;r->on_screen=false;
    } else if(action==ACTION_CIV_START && !v->civ.requested) {
        CivStartError error=CIV_START_OK;
        if(v->civ.progress.shutdown_unresolved) error=CIV_START_RESTART_REQUIRED;
        else if(v->civ.progress.owned || !available || gamepad_exclusive_status()!=GAMEPAD_EXCLUSIVE_OFF || keyboard_status()->transfer_pending)
            error=CIV_START_CONFLICT;
        else if(!civ_route_payload_valid(civ_payload_embedded())) error=CIV_START_INVALID_PAYLOAD;
        else if(!usb_civ_ready()) error=CIV_START_USB_NOT_READY;
        else {
            bool manual_ack=v->civ.start_error==CIV_START_SLEEP_WARNING;
            bool protected=civ_sleep_prepare();
            if(!protected && (!manual_ack || civ_sleep_info().restore_failed)) error=CIV_START_SLEEP_WARNING;
            else if(!civ_usb_start()) error=CIV_START_REJECTED;
            if(error!=CIV_START_OK) civ_sleep_release();
        }
        if(error==CIV_START_OK) {
            v->civ.requested=true;v->civ.cancel_requested=v->civ.retrying=false;
            r->started_at=now;r->timing=true;v->civ.elapsed_ms=0;v->selected=0;
        }
        v->civ.start_error=error;
    }
    civ_route_update(r,v,now);return true; /* All other mode/navigation actions are swallowed. */
}
