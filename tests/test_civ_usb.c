/* Reuse the real usb:ds simulator and descriptor assertions, not fake adapter
   counters. This binary links production usb_keyboard + civ_usb + CivRunner. */
#define USB_MOCK_ONLY
#include "test_usb.c"
#include "civ_usb.h"
#include "usb_civ.h"

static bool application_idle=true;
static bool can_start(void) { return application_idle; }
static uint64_t clock_ms;
static void step(unsigned complete,unsigned advance) {
    clock_ms+=advance;
    if((complete&1) && keyboard_status()->transfer_pending) keyboard_ep.CompletionEvent.fired=true;
    if((complete&2) && gamepad_status()->transfer_pending) gamepad_in_ep.CompletionEvent.fired=true;
    poll(clock_ms);civ_usb_tick(clock_ms);
}
static void connect(void) {
    bus_state=UsbState_Configured;poll(++clock_ms);
    request(0x81,6,0x2200,1,63,++clock_ms);settle(clock_ms);
    request(0x81,6,0x2200,0,86,++clock_ms);settle(clock_ms);
    clock_ms+=301;poll(clock_ms);assert(usb_civ_ready());
}
static void setup_usb(void) {
    assert(!civ_usb_owned());
    keyboard_exit();bus_state=UsbState_Detached;
    fail_key_post=fail_key_read=fail_gamepad_read=fail_cancel=fail_state=0;
    fail_gamepad_in_post=0;key_report_override=gamepad_in_report_override=0;
    key_actual=gamepad_in_actual=8;completed_status=3;
    assert(keyboard_init()==0 && gamepad_init()==0);poll(++clock_ms);connect();
}
static void start(void) {
    assert(civ_usb_start() && civ_usb_owned());
    assert(!civ_usb_start());civ_usb_tick(clock_ms);
}
static void reach(CivRunnerStage stage) {
    unsigned limit=200000;
    while(--limit) {
        CivUsbProgress p=civ_usb_progress();
        if(p.runner.stage==stage && p.runner.pending) return;
        step(3,32);
    }
    assert(!"stage not reached");
}
static void finish(CivRunnerStatus outcome) {
    unsigned limit=200000;
    while(civ_usb_owned() && --limit) step(3,32);
    CivUsbProgress p=civ_usb_progress();
    if(!limit || p.owned || p.runner.status!=outcome || !p.runner.neutral_confirmed)
        fprintf(stderr,"finish: expected=%u actual=%u stage=%u error=%u owned=%u acquiring=%u fence=%u ticket=%llu low=%u pending=%u/%u\n",
            outcome,p.runner.status,p.runner.stage,p.runner.error,p.owned,p.acquiring,p.fence_blocked,
            (unsigned long long)p.runner.ticket,usb_civ_transfer().result,keyboard_status()->transfer_pending,gamepad_status()->transfer_pending);
    assert(limit && !p.owned && p.runner.status==outcome && p.runner.neutral_confirmed);
    const uint8_t neutral[8]={0,0,8,128,128,128,128,0},released[8]={0};
    assert(!memcmp(key_report,released,8) && !memcmp(gamepad_in_report,neutral,8));
}
static void inactive_and_ownership(void) {
    assert(!civ_usb_start());assert(!civ_usb_init(NULL));assert(civ_usb_init(can_start));assert(!civ_usb_init(can_start));
    setup_usb();unsigned keys=key_posts;
    for(int i=0;i<8;i++) step(3,32);
    assert(!civ_usb_owned() && key_posts==keys && civ_usb_progress().runner.status==CIV_RUNNER_IDLE);
    application_idle=false;assert(!civ_usb_start());application_idle=true;
    assert(gamepad_exclusive_begin());assert(!civ_usb_start());
    assert(gamepad_exclusive_stop());step(2,1);step(2,1);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_OFF);
    assert(keyboard_send(0,4,clock_ms)==0);assert(!civ_usb_start());step(1,1);
    start();reach(CIV_STAGE_PREAMBLE_A);
    assert(!usb_civ_release());
    SwitchGamepadReport hostile={SWITCH_GAMEPAD_A,0,0,0,0,0,0};
    keys=key_posts;unsigned pads=gamepad_in_posts;
    uint32_t key_id=keyboard_urb,pad_id=gamepad_in_urb;
    assert(keyboard_send(2,4,clock_ms)!=0 && !keyboard_idle_due(clock_ms+100000));
    keyboard_cancel_transfer();gamepad_set_state(&hostile);
    assert(gamepad_send(&hostile,clock_ms)!=0);
    assert(!gamepad_exclusive_begin() && !gamepad_exclusive_submit(&hostile) && !gamepad_exclusive_stop());
    assert(keyboard_init()!=0 && gamepad_init()!=0);keyboard_exit();gamepad_exit();
    assert(keyboard_status()->initialized && usb_civ_owned());
    assert(key_posts==keys && gamepad_in_posts==pads && keyboard_urb==key_id && gamepad_in_urb==pad_id);
    assert(keyboard_status()->transfer_pending && gamepad_status()->transfer_pending);
    /* Host output echo cannot steal ownership through either OUT path. */
    request(0x21,9,0x0200,0,8,clock_ms);memset(output,0xff,8);settle(clock_ms);
    memset(gamepad_out_ptr,0xff,8);gamepad_out_actual=8;
    gamepad_out_ep.CompletionEvent.fired=true;step(0,1);gamepad_out_actual=64;
    assert(gamepad_in_posts==pads && gamepad_in_report[0]==4);
    /* One interface finishing cannot retire the pair or start the A dwell. */
    step(1,1);assert(civ_usb_progress().runner.pending);
    for(int i=0;i<5;i++) step(0,400);
    assert(key_posts==keys && gamepad_in_posts==pads);
    step(2,1);assert(!civ_usb_progress().runner.pending);
    step(0,49);assert(civ_usb_progress().runner.stage==CIV_STAGE_PREAMBLE_A);
    step(0,1);assert(civ_usb_progress().runner.stage==CIV_STAGE_PREAMBLE_RELEASE);
    assert(gamepad_in_posts==pads); /* No automatic controller repost. */
    civ_usb_cancel();finish(CIV_RUNNER_CANCELLED);
    assert(keyboard_send(0,0,clock_ms)==0);step(1,1); /* ordinary writers restored */
}
static void full_report_copy(void) {
    setup_usb();assert(usb_civ_acquire() && usb_civ_idle());
    SwitchGamepadReport pad_state={0x1234,3,11,22,33,44,55};
    const SwitchGamepadReport saved=pad_state;
    assert(usb_civ_submit(1,2,55,&pad_state,clock_ms));
    memset(&pad_state,0xff,sizeof(pad_state));
    const uint8_t keyboard[8]={2,0,55,0,0,0,0,0};
    assert(!memcmp(key_report,keyboard,8) && !memcmp(gamepad_in_report,&saved,8));
    step(2,1);assert(usb_civ_transfer().ticket==1 && usb_civ_transfer().completed==2);
    assert(!usb_civ_idle() && !usb_civ_submit(2,0,0,&saved,clock_ms));
    step(1,1);assert(usb_civ_transfer().completed==3 && !usb_civ_release());
    pad_state=(SwitchGamepadReport){0,8,128,128,128,128,0};
    assert(usb_civ_submit(2,0,0,&pad_state,clock_ms));
    step(3,1);assert(usb_civ_release());
}
static void failures(void) {
    /* Every transport failure must stop progression and send only cleanup. */
    for(unsigned kind=0;kind<9;kind++) {
        setup_usb();start();reach(CIV_STAGE_CHARACTER);
        uint64_t ticket=civ_usb_progress().runner.ticket;
        if(kind==0 || kind==1) {
            /* Fail the NEXT transaction (after the current character's dwell). */
            step(3,1);step(0,32);
            if(kind==0) fail_key_post=1;else fail_gamepad_in_post=1;
            step(0,1);
        } else if(kind==2 || kind==3) {
            if(kind==2) key_actual=7;else gamepad_in_actual=7;
            step(3,1);key_actual=gamepad_in_actual=8;
        } else if(kind==4) {
            completed_status=5;step(3,1);completed_status=3;
        } else if(kind==5 || kind==6) {
            if(kind==5) fail_key_read=1;else fail_gamepad_read=1;
            step(3,1);fail_key_read=fail_gamepad_read=0;
            assert(civ_usb_progress().fence_blocked);
            assert(civ_usb_retry_cleanup());
        } else if(kind==7) step(1,2501); /* pad timeout */
        else step(2,2501); /* keyboard timeout */
        assert(civ_usb_owned() && civ_usb_progress().runner.status!=CIV_RUNNER_RUNNING);
        assert(!civ_usb_start());
        finish(CIV_RUNNER_FAILED);
        assert(civ_usb_progress().runner.ticket>ticket);
    }
}
static void stale_and_cancel(void) {
    setup_usb();start();reach(CIV_STAGE_CHARACTER);
    uint32_t old_key=keyboard_urb,old_pad=gamepad_in_urb;
    civ_usb_cancel();step(0,1); /* submit neutral after cancelling both URBs */
    assert(keyboard_urb!=old_key && gamepad_in_urb!=old_pad);
    key_report_override=old_key;gamepad_in_report_override=old_pad;
    step(3,1);
    assert(civ_usb_owned() && civ_usb_progress().runner.pending);
    assert(keyboard_status()->transfer_pending && gamepad_status()->transfer_pending);
    key_report_override=gamepad_in_report_override=0;
    finish(CIV_RUNNER_CANCELLED);
    start();reach(CIV_STAGE_START_DELAY);
    key_report_override=old_key;gamepad_in_report_override=old_pad;step(3,1);
    assert(civ_usb_progress().runner.pending);key_report_override=gamepad_in_report_override=0;
    civ_usb_cancel();finish(CIV_RUNNER_CANCELLED);
}
static void interruption(void) {
    const UsbState interruptions[]={UsbState_Detached,UsbState_Default,UsbState_Suspended};
    for(unsigned i=0;i<3;i++) {
        setup_usb();start();reach(CIV_STAGE_CHARACTER);
        size_t chars=civ_usb_progress().runner.characters_completed;
        bus_state=interruptions[i];step(0,1);
        assert(civ_usb_owned() && !civ_usb_progress().runner.neutral_confirmed);
        unsigned keys=key_posts,pads=gamepad_in_posts;
        for(int j=0;j<3;j++) step(0,1000);
        assert(key_posts==keys && gamepad_in_posts==pads);
        connect();finish(CIV_RUNNER_INTERRUPTED);
        assert(civ_usb_progress().runner.characters_completed==chars);
    }
    setup_usb();start();reach(CIV_STAGE_CHARACTER);
    /* Reset + reconnection observed by transport BEFORE the next runner tick. */
    bus_state=UsbState_Default;poll(++clock_ms);connect();
    civ_usb_tick(clock_ms);finish(CIV_RUNNER_INTERRUPTED);
    setup_usb();start();reach(CIV_STAGE_CHARACTER);fail_state=1;step(0,1);fail_state=0;
    step(0,1);finish(CIV_RUNNER_INTERRUPTED);
    setup_usb();start();civ_usb_cancel();step(0,1);step(3,1);
    assert(!civ_usb_progress().runner.pending && !civ_usb_progress().runner.neutral_confirmed);
    /* Invalidate completed cleanup before its final scheduler transition. */
    bus_state=UsbState_Default;poll(++clock_ms);connect();civ_usb_tick(clock_ms);
    assert(civ_usb_owned() && civ_usb_progress().runner.pending);
    finish(CIV_RUNNER_CANCELLED);
}
static void blocked_cleanup(void) {
    setup_usb();start();reach(CIV_STAGE_CHARACTER);fail_cancel=1;civ_usb_cancel();
    assert(civ_usb_progress().fence_blocked && civ_usb_owned());
    unsigned keys=key_posts,pads=gamepad_in_posts;
    for(int i=0;i<4;i++) step(0,1000);
    assert(key_posts==keys && gamepad_in_posts==pads && !civ_usb_start());
    fail_cancel=0;assert(civ_usb_retry_cleanup());finish(CIV_RUNNER_CANCELLED);
    setup_usb();start();civ_usb_cancel();step(0,1); /* neutral submission */
    assert(civ_usb_progress().runner.stage==CIV_STAGE_NEUTRAL);
    key_actual=7;step(3,1);key_actual=8;
    assert(civ_usb_progress().runner.status==CIV_RUNNER_CLEANUP_BLOCKED && civ_usb_owned());
    keys=key_posts;pads=gamepad_in_posts;
    for(int i=0;i<4;i++) step(0,1000);
    civ_usb_cancel();assert(!civ_usb_start());assert(key_posts==keys && gamepad_in_posts==pads);
    assert(civ_usb_retry_cleanup());finish(CIV_RUNNER_CANCELLED);
    /* Failed fence during acquisition is owned, never a partially started run. */
    setup_usb();fail_cancel=1;assert(civ_usb_start());civ_usb_tick(clock_ms);
    assert(civ_usb_progress().acquiring && civ_usb_progress().fence_blocked);
    fail_cancel=0;assert(civ_usb_retry_cleanup());finish(CIV_RUNNER_FAILED);
}
static void cancellation_stages(void) {
    for(int stage=CIV_STAGE_START_DELAY;stage<=CIV_STAGE_NEUTRAL;stage++) {
        setup_usb();start();reach((CivRunnerStage)stage);
        step(1,1); /* Cancel with only keyboard completed. */
        size_t chars=civ_usb_progress().runner.characters_completed;
        civ_usb_cancel();finish(CIV_RUNNER_CANCELLED);
        assert(civ_usb_progress().runner.characters_completed==chars);
    }
    setup_usb();assert(civ_usb_start());civ_usb_cancel();finish(CIV_RUNNER_CANCELLED);
}
static void cleanup_faults(void) {
    for(unsigned kind=0;kind<8;kind++) {
        setup_usb();start();reach(CIV_STAGE_CHARACTER);civ_usb_cancel();
        if(kind==0) fail_key_post=1;
        if(kind==1) fail_gamepad_in_post=1;
        step(0,1); /* Neutral post, never another payload action. */
        if(kind==2) key_actual=7;
        if(kind==3) gamepad_in_actual=7;
        if(kind==4) fail_key_read=1;
        if(kind==5) fail_gamepad_read=1;
        if(kind>=2 && kind<=5) step(3,1);
        if(kind==6) step(1,2501);
        if(kind==7) step(2,2501);
        key_actual=gamepad_in_actual=8;fail_key_read=fail_gamepad_read=0;
        assert(civ_usb_progress().runner.status==CIV_RUNNER_CLEANUP_BLOCKED && civ_usb_owned());
        unsigned keys=key_posts,pads=gamepad_in_posts;
        for(int j=0;j<3;j++) step(0,3000);
        assert(key_posts==keys && gamepad_in_posts==pads && !civ_usb_start());
        assert(civ_usb_retry_cleanup());finish(CIV_RUNNER_CANCELLED);
    }
}
static void ambiguous_submission_fences(void) {
    for(unsigned which=1;which<=2;which++) {
        setup_usb();start();reach(CIV_STAGE_CHARACTER);
        step(3,1);step(0,32); /* Next action not submitted yet. */
        if(which==1) fail_key_post=1;else fail_gamepad_in_post=1;
        fail_cancel=1;step(0,1);
        assert(civ_usb_progress().fence_blocked && civ_usb_owned());
        unsigned keys=key_posts,pads=gamepad_in_posts;
        /* Failed post left the prior URB ID unchanged. Even an apparent matching
           completion for that ID cannot fence the ambiguous new submission. */
        for(int i=0;i<4;i++) step(3,1000);
        assert(civ_usb_owned() && civ_usb_progress().fence_blocked);
        assert(key_posts==keys && gamepad_in_posts==pads && !usb_civ_idle());
        fail_cancel=0;assert(civ_usb_retry_cleanup());finish(CIV_RUNNER_FAILED);
    }
}
static void full_payload(bool trace) {
    setup_usb();start();unsigned esc=0,enter=0,a=0;uint32_t key=0,pad_id=0;
    uint64_t ticket=0;
    const unsigned minimum_ms[]={1000,50,550,32,750,50,550,32,32,32,200,32,750,50,550,0};
    unsigned limit=200000;
    while(civ_usb_owned() && --limit) {
        CivRunnerProgress p=civ_usb_progress().runner;
        if(p.ticket!=ticket && p.pending) {
            ticket=p.ticket;
            assert(key_report[1]==0);
            for(unsigned i=3;i<8;i++) assert(key_report[i]==0);
            assert(gamepad_in_report[1]==0 && gamepad_in_report[2]==8 && gamepad_in_report[7]==0);
            for(unsigned i=3;i<7;i++) assert(gamepad_in_report[i]==128);
            if(trace) printf("%u %u %u %u\n",minimum_ms[p.stage],key_report[0],key_report[2],gamepad_in_report[0]);
        }
        if(key!=keyboard_urb) { key=keyboard_urb;esc+=key_report[2]==41;enter+=key_report[2]==40; }
        if(pad_id!=gamepad_in_urb) { pad_id=gamepad_in_urb;a+=gamepad_in_report[0]==4; }
        step(3,32);
    }
    assert(limit && esc==87 && enter==174 && a==178);
    assert(civ_usb_progress().runner.status==CIV_RUNNER_FINISHED && civ_usb_progress().runner.neutral_confirmed);
    assert(civ_usb_progress().runner.lines_completed==87 && civ_usb_progress().runner.characters_completed==21675);
}
static void forced_exit(void) {
    setup_usb();start();reach(CIV_STAGE_CHARACTER);fail_cancel=1;civ_usb_shutdown();fail_cancel=0;
    assert(!civ_usb_owned() && !keyboard_status()->initialized && !civ_usb_progress().runner.neutral_confirmed);
    assert(!keyboard_status()->ready && !gamepad_ready() && keyboard_send(0,0,clock_ms)!=0);
    assert(civ_usb_progress().shutdown_unresolved && !civ_usb_start());
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--trace")) {
        assert(civ_usb_init(can_start));full_payload(true);keyboard_exit();return 0;
    }
    full_report_copy();inactive_and_ownership();failures();stale_and_cancel();interruption();
    blocked_cleanup();cancellation_stages();cleanup_faults();ambiguous_submission_fences();
    full_payload(false);forced_exit();
    keyboard_exit();
    puts("PASS: production CIV USB adapter: exclusive pair, actual URBs, timing, failures/timeouts, stale completions, reset/disconnect, cleanup fencing/retry, re-entry, full payload and forced shutdown.");
    return 0;
}
