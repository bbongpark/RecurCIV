#include "macro_live.h"
#include "controller_monitor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* A completion-driven endpoint. Capture timestamps never wait for completion. */
static GamepadExclusiveState owner;
static bool ready,reject_begin,reject_stop;
static unsigned begins,submits,stops;
static SwitchGamepadReport sent;
static MacroLive live;
static MacroRecorder recorder;
static const SwitchGamepadReport neutral={0,8,128,128,128,128,0};
bool gamepad_ready(void) { return ready; }
GamepadExclusiveState gamepad_exclusive_status(void) { return owner; }
bool gamepad_exclusive_begin(void) {
    if(!ready || reject_begin || owner!=GAMEPAD_EXCLUSIVE_OFF) return false;
    begins++;owner=GAMEPAD_EXCLUSIVE_SYNC;sent=neutral;return true;
}
bool gamepad_exclusive_submit(const SwitchGamepadReport *r) {
    assert(ready && owner==GAMEPAD_EXCLUSIVE_READY);
    sent=*r;submits++;owner=GAMEPAD_EXCLUSIVE_PENDING;return true;
}
bool gamepad_exclusive_stop(void) {
    if(!ready || reject_stop || owner==GAMEPAD_EXCLUSIVE_OFF) return false;
    stops++;sent=neutral;owner=GAMEPAD_EXCLUSIVE_STOPPING;return true;
}
static void reset(void) {
    memset(&live,0,sizeof(live));memset(&recorder,0,sizeof(recorder));
    owner=GAMEPAD_EXCLUSIVE_OFF;ready=true;reject_begin=reject_stop=false;begins=submits=stops=0;sent=neutral;
}
static void tick(uint64_t now,uint32_t held) { macro_live_tick(&live,true,held,0,0,0,0,0,now); }
static void complete(void) {
    if(owner==GAMEPAD_EXCLUSIVE_SYNC || owner==GAMEPAD_EXCLUSIVE_PENDING) owner=GAMEPAD_EXCLUSIVE_READY;
    else if(owner==GAMEPAD_EXCLUSIVE_STOPPING) owner=GAMEPAD_EXCLUSIVE_OFF;
}
static void start(void) {
    reset();assert(macro_live_start(&live,&recorder));complete();tick(100,0);
    assert(live.state==ML_READY && recorder.state==MR_IDLE);
    assert(macro_live_record_start(&live,false));tick(100,0);
    assert(live.state==ML_RECORDING && recorder.started==100);
}
static void finish(uint64_t now) {
    macro_live_exit(&live,now);assert(live.owned && live.state==ML_CLEANUP);
    unsigned saved=stops;tick(now+1,MON_A);assert(live.owned && stops==saved);
    complete();tick(now+2,MON_A);
    assert(!macro_live_busy(&live) && live.neutral_confirmed && live.state==ML_STOPPED);
}
static void synchronization(void) {
    reset();assert(macro_live_start(&live,&recorder));assert(begins==1 && live.state==ML_SYNC);
    assert(!macro_live_start(&live,&recorder));tick(1,MON_A);tick(10,0);
    assert(recorder.state==MR_IDLE && !recorder.sequence.count && !submits);
    assert(!macro_live_record_start(&live,false));
    complete();tick(20,MON_A|MON_UP);assert(live.state==ML_ARMING && !submits);
    macro_live_tick(&live,true,0,32767,0,0,0,0,30);
    macro_live_tick(&live,true,0,0,0,0,-32768,0,40);
    macro_live_tick(&live,true,0,0,0,0,0,1,50);
    assert(recorder.state==MR_IDLE && !submits && !memcmp(&sent,&neutral,8));
    tick(60,0);assert(live.state==ML_READY && recorder.state==MR_IDLE && submits==1);
    complete();tick(65,MON_A);assert(sent.buttons && !recorder.sequence.count && recorder.state==MR_IDLE);
    assert(macro_live_record_start(&live,false));tick(65,MON_A);finish(75);
    assert(recorder.sequence.count==1 && recorder.sequence.duration_ms==10);
    /* Stopping during synchronization never records the activating control. */
    reset();assert(macro_live_start(&live,&recorder));macro_live_exit(&live,7);
    assert(recorder.state==MR_IDLE && !recorder.sequence.count);complete();tick(8,MON_A);
    assert(live.neutral_confirmed && !live.owned && !submits);
}
static void sampled_timing(void) {
    start();unsigned before=submits;
    uint32_t combo=MON_A|MON_B|MON_ZR|MON_UP|MON_RIGHT;
    tick(125,combo);tick(175,0); /* Neither state can be submitted while pending. */
    assert(submits==before && recorder.sequence.count==2);
    assert(recorder.sequence.steps[0].duration_ms==25 && recorder.sequence.steps[1].duration_ms==50);
    SwitchGamepadReport expected=controller_monitor_report(combo,0,0,0,0,true);
    assert(!memcmp(&recorder.sequence.steps[1].report,&expected,8));
    complete();
    macro_live_tick(&live,true,MON_Y|MON_L3,-32768,32767,32767,-32768,0,200);
    expected=controller_monitor_report(MON_Y|MON_L3,-32768,32767,32767,-32768,true);
    assert(submits==before+1 && !memcmp(&sent,&expected,8) && !memcmp(&recorder.current,&expected,8));
    /* USB completion at a much later instant cannot stretch the recording. */
    complete();tick(225,MON_PLUS|MON_MINUS|MON_B);complete();tick(2300,MON_PLUS|MON_MINUS|MON_B);
    assert(live.state==ML_RECORDING); /* No Standard Controller exit chord. */
    finish(2325);assert(recorder.sequence.duration_ms==2225 && recorder.state==MR_STOPPED);
    assert(recorder.sequence.steps[2].duration_ms==25 && recorder.sequence.steps[3].duration_ms==25);
    assert(recorder.sequence.steps[4].duration_ms==2100);
    assert(macro_sequence_valid(&recorder.sequence));
}
static void conflicts(void) {
    reset();recorder.state=MR_STOPPED;
    MacroStep step={neutral,25};assert(macro_sequence_insert(&recorder.sequence,0,&step));
    MacroRecorder saved=recorder;
    ready=false;assert(!macro_live_start(&live,&recorder));ready=true;
    owner=GAMEPAD_EXCLUSIVE_PENDING;assert(!macro_live_start(&live,&recorder));
    owner=GAMEPAD_EXCLUSIVE_OFF;reject_begin=true;assert(!macro_live_start(&live,&recorder));
    assert(!begins && !live.owned && !memcmp(&saved,&recorder,sizeof(saved)));
    assert(!macro_live_retry_cleanup(&live));
    reject_begin=false;recorder.state=MR_RECORDING;assert(!macro_live_start(&live,&recorder));
}
static void disconnect_and_cleanup(void) {
    start();complete();tick(125,MON_A);
    unsigned sent_before=submits;
    ready=false;owner=GAMEPAD_EXCLUSIVE_INTERRUPTED;tick(175,MON_A);
    assert(recorder.state==MR_INTERRUPTED && recorder.sequence.duration_ms==75 && live.state==ML_CLEANUP);
    tick(200,MON_B);assert(submits==sent_before && !stops);
    ready=true;tick(225,MON_B);assert(stops==1 && !memcmp(&sent,&neutral,8));
    owner=GAMEPAD_EXCLUSIVE_INTERRUPTED;tick(250,MON_B);
    assert(live.state==ML_BLOCKED && macro_live_busy(&live));
    tick(300,MON_B);macro_live_stop(&live,300);assert(stops==1);
    reject_stop=true;assert(macro_live_retry_cleanup(&live) && live.state==ML_BLOCKED);
    assert(recorder.sequence.duration_ms==75);
    reject_stop=false;assert(macro_live_retry_cleanup(&live));complete();tick(350,MON_B);
    assert(!live.owned && live.neutral_confirmed && recorder.state==MR_INTERRUPTED);
    assert(recorder.sequence.duration_ms==75 && submits==sent_before);
    /* Physical controller disconnect also neutralizes a still-connected USB host. */
    start();complete();tick(125,MON_A);
    macro_live_tick(&live,false,0,0,0,0,0,0,150);
    assert(recorder.state==MR_INTERRUPTED && stops==1 && live.owned);
    complete();tick(160,MON_B);assert(!live.owned && recorder.sequence.duration_ms==50);
    /* Immediate stop rejection is recoverable; retry while offline waits. */
    start();reject_stop=true;macro_live_exit(&live,150);assert(live.state==ML_BLOCKED);
    ready=false;owner=GAMEPAD_EXCLUSIVE_INTERRUPTED;
    assert(macro_live_retry_cleanup(&live));tick(175,0);assert(live.owned && !stops);
    reject_stop=false;ready=true;tick(200,MON_A);complete();tick(225,MON_A);
    assert(!live.owned && live.neutral_confirmed && recorder.sequence.duration_ms==50);
    /* A transport failure while synchronizing never starts capture. */
    reset();assert(macro_live_start(&live,&recorder));owner=GAMEPAD_EXCLUSIVE_INTERRUPTED;tick(10,MON_A);
    assert(recorder.state==MR_IDLE && !recorder.sequence.count && live.state==ML_CLEANUP);
    complete();tick(11,0);assert(live.neutral_confirmed);
}
static void limits(void) {
    start();
    for(unsigned i=1;i<=MACRO_MAX_STEPS;i++) {
        complete();tick(100+i,i%2?MON_A:0);
    }
    assert(recorder.state==MR_LIMIT && recorder.sequence.count==MACRO_MAX_STEPS);
    assert(recorder.sequence.duration_ms==MACRO_MAX_STEPS && live.state==ML_READY && !stops);
    complete();tick(2000,MON_B);assert(live.owned && sent.buttons && recorder.state==MR_LIMIT);
    finish(2001);
    start();tick(100+MACRO_RECORD_LIMIT_MS+999,0);
    assert(recorder.state==MR_LIMIT && recorder.sequence.duration_ms==MACRO_RECORD_LIMIT_MS && !stops);
    finish(100+MACRO_RECORD_LIMIT_MS+1000);
    start();tick(99,MON_A);assert(recorder.state==MR_INTERRUPTED && live.state==ML_READY);
}
static void persistent(void) {
    start();complete();tick(125,MON_A);macro_live_stop(&live,175);
    assert(live.state==ML_READY && live.owned && !stops && recorder.sequence.duration_ms==75);
    MacroRecorder saved=recorder;
    complete();tick(200,MON_B);assert(sent.buttons && !memcmp(&saved,&recorder,sizeof(saved)));
    assert(!macro_live_record_start(&live,false));
    assert(macro_live_record_start(&live,true));tick(225,MON_B);
    assert(recorder.state==MR_RECORDING && recorder.started==225 && !recorder.sequence.count);
    macro_live_stop(&live,250);assert(recorder.sequence.duration_ms==25 && !stops && live.owned);
    finish(275);
    /* A disconnect after Stop preserves the already finalized capture exactly. */
    start();complete();tick(125,MON_L3|MON_R3);macro_live_stop(&live,150);
    saved=recorder;assert(live.state==ML_READY && recorder.sequence.steps[1].report.buttons==0xc00);
    ready=false;owner=GAMEPAD_EXCLUSIVE_INTERRUPTED;tick(175,MON_A);
    assert(live.state==ML_CLEANUP && !memcmp(&saved,&recorder,sizeof(saved)));
    ready=true;tick(200,0);complete();tick(201,0);
    assert(!live.owned && live.neutral_confirmed && !memcmp(&saved,&recorder,sizeof(saved)));
}
int main(void) {
    synchronization();sampled_timing();conflicts();disconnect_and_cleanup();limits();persistent();
    puts("PASS: live recording synchronization, full neutral gating, sampled combinations/sticks/timing, backpressure, ownership, limits and neutral cleanup/retry.");return 0;
}
