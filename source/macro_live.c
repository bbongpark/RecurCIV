#include "macro_live.h"
#include "controller_monitor.h"
#include <string.h>

bool macro_live_busy(const MacroLive *l) { return l && l->owned; }
bool macro_live_start(MacroLive *l,MacroRecorder *r) {
    if(!l || !r || l->owned || r->state==MR_ARMING || r->state==MR_RECORDING) return false;
    ControllerMode mode;
    /* A failed acquisition must not erase a previously captured recording. */
    if(!controller_mode_enter(&mode)) return false;
    memset(l,0,sizeof(*l));l->controller=mode;l->recorder=r;l->owned=true;l->state=ML_SYNC;
    return true;
}
static void request_neutral(MacroLive *l) {
    /* Re-enter the existing exit request only for initial cleanup/retry. Never
       re-enter passthrough or release ownership on a failed cleanup attempt. */
    l->controller.exiting=false;
    controller_mode_request_exit(&l->controller);
    l->stop_queued=gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_STOPPING;
    if(gamepad_ready() && !l->stop_queued) l->state=ML_BLOCKED;
}
static void cleanup(MacroLive *l) {
    l->state=ML_CLEANUP;l->neutral_confirmed=false;request_neutral(l);
}
void macro_live_stop(MacroLive *l,uint64_t now) {
    if(!macro_live_busy(l) || l->state==ML_CLEANUP || l->state==ML_BLOCKED) return;
    macro_record_stop(l->recorder,now);
    if(l->state==ML_RECORDING) l->state=ML_READY;
}
bool macro_live_record_start(MacroLive *l,bool discard_previous) {
    if(!macro_live_busy(l) || l->state!=ML_READY ||
       (l->recorder->sequence.count && !discard_previous)) return false;
    macro_record_start(l->recorder);l->state=ML_RECORDING;return true;
}
void macro_live_exit(MacroLive *l,uint64_t now) {
    if(!macro_live_busy(l) || l->state==ML_CLEANUP || l->state==ML_BLOCKED) return;
    macro_live_stop(l,now);cleanup(l);
}
bool macro_live_retry_cleanup(MacroLive *l) {
    if(!macro_live_busy(l) || l->state!=ML_BLOCKED) return false;
    l->state=ML_CLEANUP;l->stop_queued=false;request_neutral(l);return true;
}
void macro_live_tick(MacroLive *l,bool connected,uint32_t held,
                     int32_t lx,int32_t ly,int32_t rx,int32_t ry,unsigned touches,uint64_t now) {
    if(!macro_live_busy(l)) return;
    GamepadExclusiveState owner=gamepad_exclusive_status();
    if(l->state==ML_BLOCKED) return;
    if(l->state==ML_CLEANUP) {
        if(owner==GAMEPAD_EXCLUSIVE_OFF && l->stop_queued &&
           controller_mode_tick(&l->controller,false,0,0,0,0,0)) {
            l->neutral_confirmed=true;l->owned=false;l->state=ML_STOPPED;return;
        }
        if(!gamepad_ready()) { l->stop_queued=false;return; }
        if(owner==GAMEPAD_EXCLUSIVE_INTERRUPTED && l->stop_queued) { l->state=ML_BLOCKED;return; }
        if(!l->stop_queued) request_neutral(l);
        return;
    }
    SwitchGamepadReport report=controller_monitor_report(held,lx,ly,rx,ry,connected);
    if(!connected || !gamepad_ready() || owner==GAMEPAD_EXCLUSIVE_INTERRUPTED || owner==GAMEPAD_EXCLUSIVE_OFF) {
        macro_record_sample(l->recorder,&report,false,false,now);cleanup(l);return;
    }
    if(l->state==ML_SYNC) {
        if(owner!=GAMEPAD_EXCLUSIVE_READY) return;
        l->state=ML_ARMING;
    }
    /* This enforces mapped neutral for BOTH sticks and the hat, in addition to
       physical button/touch release. No controller tick can leak entry inputs. */
    if(l->state==ML_ARMING) {
        static const SwitchGamepadReport neutral={0,8,128,128,128,128,0};
        if(held || touches || memcmp(&report,&neutral,sizeof(report))) return;
        l->state=ML_READY;
    }
    if(l->state==ML_RECORDING) {
        /* Passthrough has already armed. Start capture at this physical sample,
           even if a gameplay button/stick is held; do not pause forwarding. */
        if(l->recorder->state==MR_ARMING) {
            static const SwitchGamepadReport neutral={0,8,128,128,128,128,0};
            macro_record_sample(l->recorder,&neutral,true,true,now);
        }
        macro_record_sample(l->recorder,&report,true,false,now);
        if(l->recorder->state!=MR_RECORDING) l->state=ML_READY;
    }
    /* Capture above uses the physical sampling timestamp even while USB is
       pending. Passthrough remains latest-state, not a delivery log or queue. */
    (void)controller_mode_tick(&l->controller,connected,held,lx,ly,rx,ry);
}
