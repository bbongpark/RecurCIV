#include "macro_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static GamepadExclusiveState owner=GAMEPAD_EXCLUSIVE_OFF;
static bool ready=true,reject;
static unsigned submissions,stops;
static SwitchGamepadReport sent;
bool gamepad_ready(void) { return ready; }
GamepadExclusiveState gamepad_exclusive_status(void) { return owner; }
bool gamepad_exclusive_begin(void) { if(!ready || owner!=GAMEPAD_EXCLUSIVE_OFF) return false;owner=GAMEPAD_EXCLUSIVE_SYNC;return true; }
bool gamepad_exclusive_submit(const SwitchGamepadReport *r) {
    if(reject || owner!=GAMEPAD_EXCLUSIVE_READY) return false;
    sent=*r;submissions++;owner=GAMEPAD_EXCLUSIVE_PENDING;return true;
}
bool gamepad_exclusive_stop(void) { if(!ready || reject || owner==GAMEPAD_EXCLUSIVE_OFF) return false;stops++;owner=GAMEPAD_EXCLUSIVE_STOPPING;return true; }
static MacroDocument doc;
static MacroRecorder recorder;
static void setup(MacroPlayer *p) {
    memset(p,0,sizeof(*p));owner=GAMEPAD_EXCLUSIVE_OFF;ready=true;reject=false;submissions=stops=0;
    memset(&doc,0,sizeof(doc));strcpy(doc.name,"Runtime test");doc.speed_percent=100;doc.repeat_count=2;doc.loop_delay_ms=10;
    MacroStep a={{0x84,1,0,255,44,188,0},50},b={{0,8,128,128,128,128,0},20};
    assert(macro_sequence_insert(&doc.sequence,0,&a));assert(macro_sequence_insert(&doc.sequence,1,&b));
}
static void cleanup_done(MacroPlayer *p,MacroPlayState outcome) {
    owner=GAMEPAD_EXCLUSIVE_OFF;macro_play_tick(p,9999999);
    assert(!p->owned && p->neutral_confirmed && p->state==outcome);
}
static void playback(void) {
    MacroPlayer p;setup(&p);assert(macro_play_minimum_ms(&doc)==150);
    assert(macro_play_start(&p,&doc,0));assert(!macro_play_start(&p,&doc,0));
    macro_play_tick(&p,100000);assert(!submissions);owner=GAMEPAD_EXCLUSIVE_READY;
    macro_play_tick(&p,100000);assert(p.state==MP_SUBMIT);macro_play_tick(&p,100000);
    assert(submissions==1 && !memcmp(&sent,&doc.sequence.steps[0].report,8));
    macro_play_tick(&p,1000000);assert(p.state==MP_PENDING && submissions==1);
    owner=GAMEPAD_EXCLUSIVE_READY;macro_play_tick(&p,1000000);assert(p.state==MP_HOLD);
    macro_play_tick(&p,999999);macro_play_tick(&p,1000049);assert(p.state==MP_HOLD);
    macro_play_tick(&p,1000050);assert(p.state==MP_PENDING && p.step==1 && submissions==2);
    uint64_t now=1000050;unsigned limit=100;
    while(p.state!=MP_CLEANUP && --limit) {
        if(owner==GAMEPAD_EXCLUSIVE_PENDING) owner=GAMEPAD_EXCLUSIVE_READY;
        macro_play_tick(&p,now);now+=p.state==MP_HOLD?p.duration_ms:1;
    }
    assert(limit && submissions==5 && p.repeat==2 && stops==1);cleanup_done(&p,MP_FINISHED);
    for(int stage=MP_SYNC;stage<=MP_HOLD;stage++) {
        setup(&p);assert(macro_play_start(&p,&doc,0));
        while(p.state!=(MacroPlayState)stage) {
            owner=GAMEPAD_EXCLUSIVE_READY;macro_play_tick(&p,1);
        }
        unsigned before=submissions;macro_play_cancel(&p);assert(p.owned && p.state==MP_CLEANUP);
        macro_play_tick(&p,100000);assert(p.owned && submissions==before);cleanup_done(&p,MP_CANCELLED);
    }
    setup(&p);assert(macro_play_start(&p,&doc,0));owner=GAMEPAD_EXCLUSIVE_INTERRUPTED;ready=false;
    macro_play_tick(&p,10);assert(p.state==MP_CLEANUP && p.owned && !stops);
    ready=true;macro_play_tick(&p,11);assert(stops==1 && !submissions);
    owner=GAMEPAD_EXCLUSIVE_INTERRUPTED;macro_play_tick(&p,12);assert(p.state==MP_BLOCKED);
    assert(!macro_play_start(&p,&doc,13));assert(macro_play_retry_cleanup(&p));macro_play_tick(&p,14);
    assert(stops==2 && !submissions);cleanup_done(&p,MP_INTERRUPTED);
    setup(&p);assert(macro_play_start(&p,&doc,0));owner=GAMEPAD_EXCLUSIVE_READY;macro_play_tick(&p,1);reject=true;macro_play_tick(&p,2);
    assert(p.state==MP_BLOCKED && p.owned);reject=false;assert(macro_play_retry_cleanup(&p));macro_play_tick(&p,3);cleanup_done(&p,MP_FAILED);
    setup(&p);doc.repeat_count=0;assert(!macro_play_start(&p,&doc,0));doc.repeat_count=1;doc.sequence.count=0;assert(!macro_play_start(&p,&doc,0));
    setup(&p);doc.repeat_count=0;assert(macro_play_once(&p,&doc,0));assert(p.repeat_count==1 && doc.repeat_count==0);
    now=0;limit=100;
    while(p.state!=MP_CLEANUP && --limit) {
        owner=GAMEPAD_EXCLUSIVE_READY;macro_play_tick(&p,now++);
    }
    assert(limit && submissions==2 && p.repeat==1);cleanup_done(&p,MP_FINISHED);
    const unsigned speeds[]={25,50,100,150,200};
    for(unsigned i=0;i<5;i++) {
        setup(&p);doc.speed_percent=speeds[i];doc.repeat_count=1;
        assert(macro_play_start(&p,&doc,0));owner=GAMEPAD_EXCLUSIVE_READY;
        macro_play_tick(&p,0);macro_play_tick(&p,0);
        unsigned hold=(50*100+speeds[i]-1)/speeds[i];assert(p.duration_ms==hold);
        owner=GAMEPAD_EXCLUSIVE_READY;macro_play_tick(&p,100);
        macro_play_tick(&p,100+hold-1);assert(p.state==MP_HOLD && submissions==1);
        macro_play_tick(&p,100+hold);assert(p.state==MP_PENDING && submissions==2);
        macro_play_cancel(&p);cleanup_done(&p,MP_CANCELLED);
    }
    setup(&p);ready=false;assert(!macro_play_start(&p,&doc,0) && !p.owned);
}
static void recording(void) {
    SwitchGamepadReport neutral={0,8,128,128,128,128,0},pressed={0x884,1,0,255,64,192,0};
    macro_record_start(&recorder);macro_record_sample(&recorder,&pressed,true,false,0);assert(recorder.state==MR_ARMING);
    macro_record_sample(&recorder,&neutral,true,true,100);assert(recorder.state==MR_RECORDING);
    macro_record_sample(&recorder,&pressed,true,false,150);macro_record_sample(&recorder,&neutral,true,true,250);macro_record_stop(&recorder,300);
    assert(recorder.state==MR_STOPPED && recorder.sequence.count==3 && recorder.sequence.duration_ms==200);
    assert(recorder.sequence.steps[0].duration_ms==50 && recorder.sequence.steps[1].duration_ms==100 && recorder.sequence.steps[2].duration_ms==50);
    assert(!memcmp(&recorder.sequence.steps[1].report,&pressed,8));
    macro_record_start(&recorder);macro_record_stop(&recorder,0);assert(!recorder.sequence.count && recorder.state==MR_STOPPED);
    macro_record_start(&recorder);macro_record_sample(&recorder,&neutral,true,true,0);
    macro_record_sample(&recorder,&pressed,true,false,0);macro_record_stop(&recorder,5);
    assert(recorder.sequence.count==1 && recorder.sequence.steps[0].report.buttons==pressed.buttons);
    macro_record_start(&recorder);macro_record_sample(&recorder,&neutral,true,true,100);
    macro_record_sample(&recorder,&pressed,false,false,200);assert(recorder.state==MR_INTERRUPTED && recorder.sequence.duration_ms==100);
    macro_record_start(&recorder);macro_record_sample(&recorder,&neutral,true,true,0);
    macro_record_sample(&recorder,&neutral,true,true,MACRO_RECORD_LIMIT_MS+999);
    assert(recorder.state==MR_LIMIT && recorder.sequence.duration_ms==MACRO_RECORD_LIMIT_MS);
    macro_record_start(&recorder);macro_record_sample(&recorder,&neutral,true,true,0);
    for(unsigned i=1;i<=MACRO_MAX_STEPS;i++) macro_record_sample(&recorder,i%2?&pressed:&neutral,true,false,i);
    assert(recorder.state==MR_LIMIT && recorder.sequence.count==MACRO_MAX_STEPS);
    macro_record_start(&recorder);macro_record_sample(&recorder,&neutral,true,true,100);macro_record_sample(&recorder,&pressed,true,false,99);
    assert(recorder.state==MR_INTERRUPTED);
}
int main(void) { playback();recording();puts("PASS: bounded macro recording, combinations/sticks/holds/neutral timing, completion-aware playback, finite repeats, cancellation and interrupted cleanup/retry.");return 0; }
