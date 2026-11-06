#include "macro_runtime.h"
#include <string.h>

static const SwitchGamepadReport neutral={0,8,128,128,128,128,0};
void macro_record_start(MacroRecorder *r) {
    if(!r) return;
    memset(r,0,sizeof(*r));r->state=MR_ARMING;r->current=neutral;
}
static bool record_commit(MacroRecorder *r,uint64_t now) {
    if(now<r->changed || now<r->started) { r->state=MR_INTERRUPTED;return false; }
    uint64_t end=now;
    if(end-r->started>=MACRO_RECORD_LIMIT_MS) { end=r->started+MACRO_RECORD_LIMIT_MS;r->state=MR_LIMIT; }
    uint64_t duration=end-r->changed;
    if(duration) {
        MacroStep s={r->current,(uint32_t)duration};
        if(!macro_sequence_insert(&r->sequence,r->sequence.count,&s)) { r->state=MR_LIMIT;return false; }
    }
    r->changed=end;r->elapsed_ms=end-r->started;
    if(r->sequence.count==MACRO_MAX_STEPS) r->state=MR_LIMIT;
    return r->state==MR_RECORDING;
}
void macro_record_stop(MacroRecorder *r,uint64_t now) {
    if(!r) return;
    if(r->state==MR_RECORDING) { if(record_commit(r,now)) r->state=MR_STOPPED; }
    else if(r->state==MR_ARMING) r->state=MR_STOPPED;
}
void macro_record_sample(MacroRecorder *r,const SwitchGamepadReport *report,bool connected,bool released,uint64_t now) {
    if(!r || !report || (r->state!=MR_ARMING && r->state!=MR_RECORDING)) return;
    MacroStep valid={*report,1};
    if(!connected || !macro_step_valid(&valid)) {
        macro_record_stop(r,now);r->state=MR_INTERRUPTED;return;
    }
    if(r->state==MR_ARMING) {
        if(released && !memcmp(report,&neutral,sizeof(neutral))) {
            r->state=MR_RECORDING;r->started=r->changed=now;r->current=*report;
        }
        return;
    }
    if(now<r->changed || now<r->started) { r->state=MR_INTERRUPTED;return; }
    r->elapsed_ms=now-r->started;
    if(r->elapsed_ms>=MACRO_RECORD_LIMIT_MS || memcmp(report,&r->current,sizeof(*report))) {
        if(record_commit(r,now)) r->current=*report;
    }
}
static uint32_t duration(const MacroDocument *d,size_t i) {
    return (uint32_t)(((uint64_t)d->sequence.steps[i].duration_ms*100+d->speed_percent-1)/d->speed_percent);
}
uint64_t macro_play_minimum_ms(const MacroDocument *d) {
    if(!macro_document_valid(d) || !d->repeat_count) return 0;
    uint64_t total=0;for(size_t i=0;i<d->sequence.count;i++) total+=duration(d,i);
    return total*d->repeat_count+(uint64_t)d->loop_delay_ms*(d->repeat_count-1);
}
static bool play_start(MacroPlayer *p,const MacroDocument *d,uint32_t repeats,uint64_t now) {
    if(!p || p->owned || !macro_document_valid(d) || !d->sequence.count || !repeats || !gamepad_exclusive_begin()) return false;
    memset(p,0,sizeof(*p));p->document=d;p->state=MP_SYNC;p->outcome=MP_FINISHED;
    p->repeat=1;p->repeat_count=repeats;p->started=now;p->owned=true;return true;
}
bool macro_play_start(MacroPlayer *p,const MacroDocument *d,uint64_t now) { return play_start(p,d,d?d->repeat_count:0,now); }
bool macro_play_once(MacroPlayer *p,const MacroDocument *d,uint64_t now) { return play_start(p,d,1,now); }
static void cleanup(MacroPlayer *p,MacroPlayState outcome) {
    if(!p || !p->owned || p->state==MP_BLOCKED || p->state==MP_CLEANUP) return;
    p->outcome=outcome;
    p->state=MP_CLEANUP;p->neutral_confirmed=false;
    if(gamepad_ready()) {
        p->stop_queued=gamepad_exclusive_stop();
        if(!p->stop_queued) p->state=MP_BLOCKED;
    } else p->stop_queued=false;
}
void macro_play_cancel(MacroPlayer *p) { cleanup(p,MP_CANCELLED); }
bool macro_play_retry_cleanup(MacroPlayer *p) {
    if(!p || !p->owned || p->state!=MP_BLOCKED) return false;
    p->state=MP_CLEANUP;p->stop_queued=false;return true;
}
static void submit(MacroPlayer *p,uint64_t now) {
    if(gamepad_exclusive_status()!=GAMEPAD_EXCLUSIVE_READY) return;
    const SwitchGamepadReport *r=p->loop_gap?&neutral:&p->document->sequence.steps[p->step].report;
    p->duration_ms=p->loop_gap?p->document->loop_delay_ms:duration(p->document,p->step);
    if(!gamepad_exclusive_submit(r)) { cleanup(p,MP_FAILED);return; }
    /* This is acceptance by the exclusive session, not a host delivery time.
       Start the full logical hold here so waiting for this report's completion
       does not get added to every recorded interval. The pending gate below
       still preserves every transition and prevents competing submissions. */
    p->submitted_at=now;
    p->state=MP_PENDING;
}
void macro_play_tick(MacroPlayer *p,uint64_t now) {
    if(!p || !p->owned) return;
    if(now>=p->started) p->elapsed_ms=now-p->started;
    GamepadExclusiveState owner=gamepad_exclusive_status();
    if(p->state==MP_BLOCKED) return;
    if(p->state==MP_CLEANUP) {
        if(owner==GAMEPAD_EXCLUSIVE_OFF && p->stop_queued) {
            p->neutral_confirmed=true;p->owned=false;p->state=p->outcome;return;
        }
        if(!gamepad_ready()) { p->stop_queued=false;return; }
        if(owner==GAMEPAD_EXCLUSIVE_INTERRUPTED && p->stop_queued) { p->state=MP_BLOCKED;return; }
        if(!p->stop_queued) {
            p->stop_queued=gamepad_exclusive_stop();if(!p->stop_queued) p->state=MP_BLOCKED;
        }
        return;
    }
    if(!gamepad_ready() || owner==GAMEPAD_EXCLUSIVE_INTERRUPTED) { cleanup(p,MP_INTERRUPTED);return; }
    if(owner==GAMEPAD_EXCLUSIVE_OFF) { p->outcome=MP_INTERRUPTED;p->state=MP_BLOCKED;return; }
    switch(p->state) {
    case MP_SYNC:
        if(owner==GAMEPAD_EXCLUSIVE_READY) p->state=MP_SUBMIT;
        break;
    case MP_SUBMIT: submit(p,now);break;
    case MP_PENDING:
        if(owner!=GAMEPAD_EXCLUSIVE_READY) return;
        p->completed_at=now;p->state=MP_HOLD;
        /* Completion may be observed after the deadline. Do not add another
           loop iteration, or skip ahead to a later report, in that case. */
        /* fall through */
    case MP_HOLD:
        if(now<p->submitted_at || now-p->submitted_at<p->duration_ms) return;
        if(p->loop_gap) { p->loop_gap=false;p->step=0;p->repeat++; }
        else if(++p->step==p->document->sequence.count) {
            if(p->repeat==p->repeat_count) { cleanup(p,MP_FINISHED);return; }
            p->loop_gap=true;
        }
        /* Each newly accepted state gets its FULL scaled duration from now.
           No catch-up compression after late ticks/completions, no skipped
           button edges, and no assumed/hardcoded transport latency. */
        p->state=MP_SUBMIT;submit(p,now);break;
    default: break;
    }
}
