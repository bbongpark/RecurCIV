#define USB_MOCK_ONLY
#include "test_usb.c"
#include "macro_runtime.h"
#include "macro_live.h"
#include <inttypes.h>
#include <stdlib.h>

/* Production USB pump + production player. The fake host consumes a posted URB
   at its due time, independently of when the application next polls completion.
   Completion is an observable delivery proxy, NOT a game-processing timestamp. */
typedef struct { uint64_t at;SwitchGamepadReport report; } Arrival;
static Arrival arrivals[2048];
static size_t arrived;
static SwitchGamepadReport active,inflight;
static uint32_t tracked_urb;
static unsigned tracked_cancels;
static uint64_t due,clock_ms;
static bool delivered,freeze;
static unsigned delay_ms;
static bool jitter;
static unsigned traced_posts,commands;
static uint64_t command_times[2048],cleanup_at;
static MacroDocument document;
static MacroPlayer player;
static FILE *trace;
static const SwitchGamepadReport idle={0,8,128,128,128,128,0};

static void track_post(void) {
    if(tracked_cancels!=gamepad_in_cancels) { delivered=true;tracked_cancels=gamepad_in_cancels; }
    if(!gamepad_status()->transfer_pending) return;
    if(tracked_urb==gamepad_in_urb) return;
    if(tracked_urb && !delivered) fprintf(stderr,"pending write at %" PRIu64 " old=%u new=%u due=%" PRIu64 " state=%d\n",clock_ms,tracked_urb,gamepad_in_urb,due,(int)player.state);
    assert(!tracked_urb || delivered); /* No competing writes to a pending URB. */
    tracked_urb=gamepad_in_urb;memcpy(&inflight,gamepad_in_report,8);
    const unsigned delays[]={1,2,5,10};
    due=clock_ms+(jitter?delays[traced_posts++%4]:delay_ms);delivered=false;
}
static void host_tick(void) {
    if(!tracked_urb || delivered || freeze || clock_ms<due) return;
    delivered=true;gamepad_in_ep.CompletionEvent.fired=true;
    if(memcmp(&active,&inflight,8)) {
        assert(arrived<sizeof(arrivals)/sizeof(arrivals[0]));
        arrivals[arrived++]=(Arrival){clock_ms,inflight};active=inflight;
    }
}
static void app_tick(void) {
    uint64_t before=player.submitted_at;
    poll(clock_ms);macro_play_tick(&player,clock_ms);track_post();
    if(player.submitted_at!=before) {
        assert(commands<2048);command_times[commands++]=player.submitted_at;
    }
    if(player.state==MP_CLEANUP && !cleanup_at) cleanup_at=clock_ms;
}
static void open_usb(unsigned latency) {
    assert(keyboard_init()==0 && gamepad_init()==0);
    clock_ms=1;bus_state=UsbState_Configured;poll(clock_ms);
    request(0x81,6,0x2200,1,63,++clock_ms);settle(clock_ms);
    clock_ms+=301;poll(clock_ms);assert(gamepad_ready());
    arrived=0;active=idle;tracked_urb=0;delay_ms=latency;delivered=true;freeze=jitter=false;
    traced_posts=commands=0;cleanup_at=0;
    memset(&player,0,sizeof(player));track_post();
}
static void make_document(unsigned count,unsigned hold,bool analog,bool square) {
    memset(&document,0,sizeof(document));strcpy(document.name,"Measured timeline");
    document.speed_percent=100;document.repeat_count=1;
    for(unsigned i=0;i<count;i++) {
        unsigned dir=square?i/15:i%4;
        int32_t x=0,y=0,amount=32767-(int32_t)(i%15)*256;
        if(dir==0) y=amount;else if(dir==1) x=amount;else if(dir==2) y=-amount;else x=-amount;
        SwitchGamepadReport report=controller_monitor_report(analog?(i%2?MON_A:0):0,x,y,
            analog?(int32_t)((i*719)%65536)-32768:0,analog?(int32_t)((i*1291)%65536)-32768:0,true);
        MacroStep step={report,square?(i%15==14?138u:133u):hold};
        assert(macro_sequence_insert(&document.sequence,i,&step));
    }
}
static void measure(const char *name,unsigned count,unsigned latency,unsigned hold,unsigned tick_ms,bool analog,bool square,bool stall) {
    make_document(count,hold,analog,square);open_usb(latency);
    jitter=!strcmp(name,"usb-jitter");
    uint64_t started=clock_ms;assert(macro_play_start(&player,&document,started));
    for(;player.owned && clock_ms<started+100000;clock_ms++) {
        host_tick();
        /* A 40 ms render/scheduler blackout once per second is independent of
           USB delivery. A seven-ms tick models a consistently slower loop. */
        if((clock_ms-started)%tick_ms==0 && (!stall || (clock_ms-started)%1000<400 || (clock_ms-started)%1000>=440)) app_tick();
    }
    assert(!player.owned && player.state==MP_FINISHED && player.neutral_confirmed);
    assert(arrived==count+1 && !memcmp(&arrivals[count].report,&idle,8));
    uint64_t intended=0,minimum=UINT64_MAX,maximum=0;
    int64_t cumulative=0;
    for(unsigned i=0;i<count;i++) {
        assert(!memcmp(&arrivals[i].report,&document.sequence.steps[i].report,8));
        uint64_t actual=arrivals[i+1].at-arrivals[i].at;
        unsigned want=document.sequence.steps[i].duration_ms;
        intended+=want;cumulative+=(int64_t)actual-want;
        if(actual<minimum) minimum=actual;if(actual>maximum) maximum=actual;
        fprintf(trace,"%s,%u,%u,%u,%u,%" PRIu64 ",%" PRId64 "\n",name,count,latency,i,want,actual,cumulative);
    }
    uint64_t actual=arrivals[count].at-arrivals[0].at;
    printf("%-12s n=%u usb=%2u tick=%u intended=%5" PRIu64 " active=%5" PRIu64 " drift=%5" PRId64
        " state_min/max=%" PRIu64 "/%" PRIu64 " app_elapsed=%" PRIu64 "\n",
        name,count,latency,tick_ms,intended,actual,cumulative,minimum,maximum,player.elapsed_ms);
#ifndef TIMING_BASELINE
    assert(commands==count);
    for(unsigned i=1;i<count;i++) assert(command_times[i]-command_times[i-1]>=document.sequence.steps[i-1].duration_ms);
    assert(cleanup_at-command_times[count-1]>=document.sequence.steps[count-1].duration_ms);
    /* Fixed latency cancels between state arrivals. The final cleanup delivery
       may add at most two URBs of tail latency, not n URBs of accumulated dwell. */
    if(tick_ms==1 && !stall && hold>=100) assert(actual<=intended+2*latency+2);
    if(tick_ms==1 && !stall && square) assert(actual<=intended+2*latency+2);
    if(tick_ms==1 && !stall && !square && hold==100 && !jitter) assert(actual==intended);
#endif
    keyboard_exit();
}
static void recording_ticks(unsigned tick_ms,bool stall) {
    static MacroRecorder recorder;
    uint64_t observed[61]={0},first=UINT64_MAX,minimum=UINT64_MAX,maximum=0,max_late=0;
    make_document(60,100,true,false);
    macro_record_start(&recorder);macro_record_sample(&recorder,&idle,true,true,0);
    unsigned next=0;uint64_t t=0;
    for(;;t++) {
        if(t%tick_ms || (stall && t%1000>=400 && t%1000<440)) continue;
        if(t>=6000) { macro_record_stop(&recorder,t);observed[60]=t;break; }
        unsigned index=(unsigned)(t/100);
        if(next<=index) { assert(next==index);observed[next++]=t; }
        macro_record_sample(&recorder,&document.sequence.steps[index].report,true,false,t);
        if(first==UINT64_MAX) first=t;
    }
    assert(recorder.state==MR_STOPPED && recorder.sequence.count==60 && recorder.sequence.duration_ms==t-first);
    for(unsigned i=0;i<60;i++) {
        uint64_t actual=recorder.sequence.steps[i].duration_ms;
        assert(actual==observed[i+1]-observed[i]);
        assert(!memcmp(&recorder.sequence.steps[i].report,&document.sequence.steps[i].report,8));
        uint64_t late=observed[i]-100*i;if(late>max_late) max_late=late;
        if(actual<minimum) minimum=actual;if(actual>maximum) maximum=actual;
    }
    printf("recording tick=%u stall=%u intended=6000 sampled=%" PRIu64 " state_min/max=%" PRIu64 "/%" PRIu64 " max_detection_lag=%" PRIu64 "\n",
        tick_ms,stall?40:0,recorder.sequence.duration_ms,minimum,maximum,max_late);
}
static void live_skips(void) {
    static MacroRecorder recorder;
    MacroLive live={0};memset(&recorder,0,sizeof(recorder));open_usb(10);
    assert(macro_live_start(&live,&recorder));unsigned limit=1000;
    while(live.state!=ML_READY && --limit) {
        clock_ms++;host_tick();poll(clock_ms);macro_live_tick(&live,true,0,0,0,0,0,0,clock_ms);track_post();
    }
    assert(limit && macro_live_record_start(&live,false));
    uint64_t start=clock_ms+1;
    for(unsigned t=0;t<120;t++) {
        clock_ms=start+t;host_tick();poll(clock_ms);
        unsigned i=t/2;
        macro_live_tick(&live,true,i%2?MON_A:MON_B,(int32_t)i*512,0,0,0,0,clock_ms);track_post();
    }
    macro_live_stop(&live,start+120);
    for(unsigned t=0;t<30;t++) {
        clock_ms++;host_tick();poll(clock_ms);
        macro_live_tick(&live,true,MON_A,59*512,0,0,0,0,clock_ms);track_post();
    }
    assert(recorder.sequence.count==60 && recorder.sequence.duration_ms==120 && arrived<60);
    printf("live-sampling usb=10 physical_states=60 captured=%zu delivered_distinct=%zu duration=120\n",recorder.sequence.count,arrived);
    macro_live_exit(&live,clock_ms);limit=1000;
    while(live.owned && --limit) {
        clock_ms++;host_tick();poll(clock_ms);macro_live_tick(&live,true,0,0,0,0,0,0,clock_ms);track_post();
    }
    assert(limit && live.neutral_confirmed);keyboard_exit();
}
static void faults(void) {
    for(unsigned fault=0;fault<4;fault++) {
        make_document(60,100,false,false);open_usb(5);
        assert(macro_play_start(&player,&document,clock_ms));
        unsigned limit=1000;
        while(!arrived && --limit) { clock_ms++;host_tick();app_tick(); }
        assert(limit);size_t actions=arrived;
        if(fault==0) { /* Pending forever: transport timeout, then neutral only. */
            freeze=true;
            while(player.state!=MP_CLEANUP && player.state!=MP_BLOCKED) {
                assert(--limit);clock_ms+=10;app_tick();
            }
            delivered=true;tracked_urb=0;freeze=false;
        } else if(fault==1) { /* Actual endpoint submission failure. */
            fail_gamepad_in_post=1;
            while(player.state!=MP_CLEANUP && player.state!=MP_BLOCKED) {
                assert(--limit);clock_ms++;host_tick();app_tick();
            }
        } else if(fault==2) { /* Disconnect while active, no automatic resume. */
            bus_state=UsbState_Suspended;clock_ms++;poll(clock_ms);macro_play_tick(&player,clock_ms);
            assert(player.owned && player.state==MP_CLEANUP);
            delivered=true;tracked_urb=0;gamepad_in_ep.CompletionEvent.fired=false;
            bus_state=UsbState_Configured;clock_ms+=301;poll(clock_ms);macro_play_tick(&player,clock_ms);track_post();
            clock_ms+=301;host_tick();app_tick();
        } else macro_play_cancel(&player);
        if(player.state==MP_BLOCKED) assert(macro_play_retry_cleanup(&player));
        limit=10000;
        while(player.owned && --limit) { clock_ms++;host_tick();app_tick(); }
        assert(limit && player.neutral_confirmed);
        assert(player.state==(fault==3?MP_CANCELLED:MP_INTERRUPTED));
        for(size_t i=actions;i<arrived;i++) assert(!memcmp(&arrivals[i].report,&idle,8));
        keyboard_exit();
    }
    puts("PASS: pending timeout, failed USB post, disconnect/reconnect and cancellation retain neutral cleanup and never resume steps.");
}
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
#ifdef TIMING_BASELINE
    trace=fopen("build/macro-timing-before.csv","w");
#else
    trace=fopen("build/macro-timing-after.csv","w");
#endif
    assert(trace);fprintf(trace,"scenario,steps,usb_ms,step,intended_ms,actual_ms,cumulative_drift_ms\n");
    const unsigned delays[]={1,2,5,10},counts[]={50,60,75};
    for(unsigned n=0;n<3;n++) for(unsigned d=0;d<4;d++) measure("directions",counts[n],delays[d],100,1,false,false,false);
    for(unsigned d=0;d<4;d++) {
        measure("square",60,delays[d],100,1,false,true,false);
        measure("rapid-analog",60,delays[d],2,1,true,false,false);
    }
    measure("slow-ticks",60,5,100,7,false,false,false);
    measure("render-stall",60,5,100,1,false,false,true);
    measure("usb-jitter",60,10,100,1,true,false,false);
    recording_ticks(1,false);recording_ticks(7,false);recording_ticks(1,true);live_skips();
    assert(fclose(trace)==0);faults();
    puts("PASS: production playback timing trace, byte-exact ordered states and no added neutral between steps.");return 0;
}
