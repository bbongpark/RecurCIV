#include "civ_runner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(CivRunner)<=512,"Runner must remain a small cursor, not an expanded script");
typedef struct {
    bool ready,active,draining,hold_abort,reject,fail,interrupt,stale,trace;
    unsigned completed,delivered,submissions,aborts,escapes,enters,a_pulses;
    uint64_t stale_ticket;
    CivRunnerOutput output;
    Stroke keyboard;
    SwitchGamepadReport gamepad;
} Fake;
static const uint8_t small[]="aa\nB";
static const CivPayload sample={small,sizeof(small)-1};
static bool ready(void *ctx) { return ((Fake *)ctx)->ready; }
static bool idle(void *ctx) { Fake *f=ctx;return !f->active && !f->draining; }
static bool submit(void *ctx,const CivRunnerOutput *output) {
    Fake *f=ctx;assert(idle(ctx));f->output=*output;f->active=true;f->completed=f->delivered=0;f->submissions++;
    assert(output->gamepad.hat==8 && output->gamepad.lx==128 && output->gamepad.ly==128 && output->gamepad.rx==128 && output->gamepad.ry==128 && !output->gamepad.vendor);
    assert(!output->gamepad.buttons || output->gamepad.buttons==4);
    assert(!output->gamepad.buttons || (!output->keyboard.key && !output->keyboard.modifier));
    if(output->keyboard.key==0x29) f->escapes++;
    if(output->keyboard.key==0x28) f->enters++;
    if(output->gamepad.buttons==4) f->a_pulses++;
    if(f->trace) printf("%u %u %u %u\n",output->minimum_ms,output->keyboard.modifier,output->keyboard.key,output->gamepad.buttons);
    return !f->reject;
}
static CivRunnerCompletion poll(void *ctx,uint64_t ticket) {
    Fake *f=ctx;
    if(f->stale) { f->stale=false;return (CivRunnerCompletion){f->stale_ticket,CIV_INTERFACE_BOTH,CIV_TRANSFER_FAILED}; }
    assert(ticket==f->output.ticket && f->active);
    if(f->fail) return (CivRunnerCompletion){ticket,0,CIV_TRANSFER_FAILED};
    if(f->interrupt) return (CivRunnerCompletion){ticket,0,CIV_TRANSFER_INTERRUPTED};
    if(f->completed&CIV_INTERFACE_KEYBOARD) f->keyboard=f->output.keyboard;
    if(f->completed&CIV_INTERFACE_GAMEPAD) f->gamepad=f->output.gamepad;
    f->delivered|=f->completed;
    if(f->delivered==CIV_INTERFACE_BOTH) f->active=false;
    return (CivRunnerCompletion){ticket,f->completed,CIV_TRANSFER_OK};
}
static void abort_transfer(void *ctx,uint64_t ticket) {
    Fake *f=ctx;assert(ticket==f->output.ticket);f->aborts++;f->active=false;
    f->draining=f->hold_abort;f->completed=f->delivered=0;
}
static void setup(CivRunner *r,Fake *f,CivPayload payload) {
    memset(f,0,sizeof(*f));f->ready=true;
    CivRunnerTransport t={f,ready,idle,submit,poll,abort_transfer};
    assert(civ_runner_init(r,&t));assert(civ_runner_start(r,payload));
}
static void pump(CivRunner *r,Fake *f,uint64_t *now) {
    civ_runner_tick(r,*now);
    if(f->active) { f->completed=CIV_INTERFACE_BOTH;civ_runner_tick(r,*now); }
    if(r->action_live && !r->waiting) {
        *now+=r->output.minimum_ms;civ_runner_tick(r,*now);
    }
}
static void reach(CivRunner *r,Fake *f,CivRunnerStage stage,uint64_t *now) {
    unsigned limit=1000;
    while((r->progress.stage!=stage || r->action_live) && --limit) pump(r,f,now);
    assert(limit && r->progress.stage==stage && !r->action_live);
}
static void finish(CivRunner *r,Fake *f,uint64_t *now,CivRunnerStatus status) {
    unsigned limit=100000;
    while(civ_runner_active(r) && --limit) pump(r,f,now);
    assert(limit && r->progress.status==status && r->progress.neutral_confirmed);
    assert(!f->keyboard.key && !f->keyboard.modifier && !f->gamepad.buttons && f->gamepad.hat==8);
    unsigned count=f->submissions;civ_runner_tick(r,*now+1000000);assert(f->submissions==count);
}
static void approved_payload(bool trace) {
    CivPayload payload=civ_payload_embedded();assert(payload.size==21762);
    FILE *input=fopen("payload.txt","rb");assert(input);
    for(size_t i=0;i<payload.size;i++) assert(fgetc(input)==payload.bytes[i]);
    assert(fgetc(input)==EOF);fclose(input);
    CivRunner r;Fake f;setup(&r,&f,payload);f.trace=trace;uint64_t now=0;
    assert(r.progress.lines_total==87 && r.progress.characters_total==21675);
    finish(&r,&f,&now,CIV_RUNNER_FINISHED);
    assert(f.escapes==87 && f.enters==174 && f.a_pulses==178);
    assert(r.progress.lines_completed==87 && r.progress.characters_completed==21675);
    assert(r.progress.line==87 && r.progress.stage==CIV_STAGE_NEUTRAL);
    /* Includes startup wait; excludes USB latency (the fake completes instantly). */
    assert(now==993190);
}
static void timing(void) {
    const unsigned ms[]={1000,50,550,32,750,50,550,32,32,32,200,32,750,50,550,0};
    for(int stage=CIV_STAGE_START_DELAY;stage<=CIV_STAGE_NEUTRAL;stage++) {
        CivRunner r;Fake f;uint64_t now=0;setup(&r,&f,sample);reach(&r,&f,(CivRunnerStage)stage,&now);
        civ_runner_tick(&r,now);assert(f.output.minimum_ms==ms[stage] && r.waiting);
        unsigned count=f.submissions;
        now+=1000000;civ_runner_tick(&r,now);assert(r.waiting && f.submissions==count);
        f.completed=CIV_INTERFACE_KEYBOARD;civ_runner_tick(&r,now);assert(r.waiting);
        f.completed=0;now+=1000000;civ_runner_tick(&r,now);assert(r.waiting);
        f.completed=CIV_INTERFACE_GAMEPAD;civ_runner_tick(&r,now);assert(!r.waiting);
        assert(r.action_live && f.submissions==count);
        /* Duplicate and backward times cannot consume the dwell. */
        civ_runner_tick(&r,now-1);assert(r.action_live);
        if(ms[stage]) { civ_runner_tick(&r,now+ms[stage]-1);assert(r.action_live && f.submissions==count); }
        now+=ms[stage];civ_runner_tick(&r,now);assert(!r.action_live && f.submissions==count);
    }
    CivRunner r;Fake f;uint64_t now=0;setup(&r,&f,sample);
    pump(&r,&f,&now);assert(r.progress.stage==CIV_STAGE_PREAMBLE_A);
    civ_runner_tick(&r,UINT64_MAX);assert(f.output.gamepad.buttons==4);
    f.completed=CIV_INTERFACE_BOTH;civ_runner_tick(&r,UINT64_MAX);
    unsigned count=f.submissions;
    civ_runner_tick(&r,UINT64_MAX);civ_runner_tick(&r,0);
    assert(r.action_live && f.submissions==count); /* no overflowing deadline */
}
static void cancellation(void) {
    for(int stage=CIV_STAGE_START_DELAY;stage<=CIV_STAGE_NEUTRAL;stage++) for(int phase=0;phase<4;phase++) {
        CivRunner r;Fake f;uint64_t now=0;setup(&r,&f,sample);reach(&r,&f,(CivRunnerStage)stage,&now);
        if(phase) civ_runner_tick(&r,now);
        if(phase>=2) { f.completed=phase==2?CIV_INTERFACE_KEYBOARD:CIV_INTERFACE_BOTH;civ_runner_tick(&r,now); }
        unsigned before=f.submissions,esc=f.escapes,enter=f.enters,a=f.a_pulses;
        size_t chars=r.progress.characters_completed,lines=r.progress.lines_completed;
        f.hold_abort=true;civ_runner_cancel(&r);
        assert(civ_runner_active(&r) && r.progress.status==CIV_RUNNER_CLEANING_UP);
        assert(!civ_runner_start(&r,sample));
        if(f.draining) { civ_runner_tick(&r,now+1000000);assert(f.submissions==before); }
        f.draining=false;f.hold_abort=false;
        finish(&r,&f,&now,CIV_RUNNER_CANCELLED);
        assert(f.submissions==before+1 && f.output.stage==CIV_STAGE_NEUTRAL);
        assert(f.escapes==esc && f.enters==enter && f.a_pulses==a);
        assert(r.progress.characters_completed==chars && r.progress.lines_completed==lines);
    }
}
static void failures(void) {
    for(int stage=CIV_STAGE_START_DELAY;stage<=CIV_STAGE_NEUTRAL;stage++) for(int kind=0;kind<2;kind++) {
        CivRunner r;Fake f;uint64_t now=0;setup(&r,&f,sample);reach(&r,&f,(CivRunnerStage)stage,&now);
        f.reject=kind==0;civ_runner_tick(&r,now);
        if(kind==1) { f.fail=true;civ_runner_tick(&r,now); }
        f.reject=f.fail=false;assert(f.aborts);
        if(stage==CIV_STAGE_NEUTRAL) {
            assert(r.progress.status==CIV_RUNNER_CLEANUP_BLOCKED && !r.progress.neutral_confirmed);
            unsigned count=f.submissions;civ_runner_tick(&r,now+1000000);civ_runner_cancel(&r);
            assert(f.submissions==count && r.progress.status==CIV_RUNNER_CLEANUP_BLOCKED);
            assert(!civ_runner_start(&r,sample));assert(civ_runner_retry_cleanup(&r));
        }
        unsigned before=f.submissions;
        finish(&r,&f,&now,CIV_RUNNER_FAILED);assert(f.submissions==before+1);
        assert(r.progress.error==(kind==0?CIV_ERROR_SUBMISSION:CIV_ERROR_COMPLETION));
    }
    CivRunner r;Fake f;uint64_t now=0;setup(&r,&f,sample);
    civ_runner_fail(&r);finish(&r,&f,&now,CIV_RUNNER_FAILED);assert(r.progress.error==CIV_ERROR_EXTERNAL);
}
static void interruptions(void) {
    for(int stage=CIV_STAGE_START_DELAY;stage<=CIV_STAGE_NEUTRAL;stage++) {
        CivRunner r;Fake f;uint64_t now=0;setup(&r,&f,sample);reach(&r,&f,(CivRunnerStage)stage,&now);
        civ_runner_tick(&r,now);f.ready=false;civ_runner_tick(&r,now);
        unsigned count=f.submissions;
        for(int i=0;i<3;i++) civ_runner_tick(&r,now+1000000);
        assert(f.submissions==count && !r.progress.neutral_confirmed);
        f.ready=true;finish(&r,&f,&now,CIV_RUNNER_INTERRUPTED);
        assert(f.submissions==count+1 && r.progress.error==CIV_ERROR_TRANSPORT);
        uint64_t old=f.output.ticket;assert(civ_runner_start(&r,sample));
        civ_runner_tick(&r,now);assert(f.output.ticket>old);
        f.stale=true;f.stale_ticket=old;civ_runner_tick(&r,now+1000000);
        assert(r.waiting && r.progress.status==CIV_RUNNER_RUNNING);
        civ_runner_cancel(&r);finish(&r,&f,&now,CIV_RUNNER_CANCELLED);
    }
    CivRunner r;Fake f;uint64_t now=0;setup(&r,&f,sample);civ_runner_tick(&r,now);
    f.interrupt=true;civ_runner_tick(&r,now);f.interrupt=false;
    finish(&r,&f,&now,CIV_RUNNER_INTERRUPTED);
}
static void invalid_inputs(void) {
    CivRunner r={0};Fake f={0};assert(!civ_runner_start(&r,sample));
    assert(!civ_runner_init(&r,NULL));CivRunnerTransport bad={0};assert(!civ_runner_init(&r,&bad));
    setup(&r,&f,sample);assert(!civ_runner_start(&r,sample));
    civ_runner_cancel(&r);uint64_t now=0;finish(&r,&f,&now,CIV_RUNNER_CANCELLED);
    const uint8_t *invalid[]={NULL,(const uint8_t *)"",(const uint8_t *)"\n",(const uint8_t *)" \t\n",(const uint8_t *)"a\n\nb",(const uint8_t *)"\xef\xbb\xbf" "a",(const uint8_t *)"a\x08",(const uint8_t *)"a\x7f",(const uint8_t *)"a\xc3\xa9"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        size_t length=invalid[i]?strlen((const char *)invalid[i]):1;
        unsigned count=f.submissions;assert(!civ_runner_start(&r,(CivPayload){invalid[i],length}));assert(count==f.submissions);
    }
    const uint8_t nul[]={'a',0,'b'};assert(!civ_runner_start(&r,(CivPayload){nul,sizeof(nul)}));
    uint8_t long_line[CIV_RUNNER_MAX_LINE_BYTES+1];memset(long_line,'a',sizeof(long_line));
    assert(!civ_runner_start(&r,(CivPayload){long_line,sizeof(long_line)}));
    assert(!civ_runner_start(&r,(CivPayload){long_line,CIV_RUNNER_MAX_BYTES+1}));
    uint8_t lines[(CIV_RUNNER_MAX_LINES+1)*2];
    for(size_t i=0;i<sizeof(lines);i++) lines[i]=i%2?'\n':'a';
    assert(!civ_runner_start(&r,(CivPayload){lines,sizeof(lines)}));
    f.ready=false;assert(!civ_runner_start(&r,sample));f.ready=true;f.draining=true;
    assert(!civ_runner_start(&r,sample));f.draining=false;
    const uint8_t crlf[]=" a\t\r\nb\rc\n";
    assert(civ_runner_start(&r,(CivPayload){crlf,sizeof(crlf)-1}));
    assert(r.progress.lines_total==3 && r.progress.characters_total==5);
    finish(&r,&f,&now,CIV_RUNNER_FINISHED);
    assert(!civ_runner_retry_cleanup(&r));
    r.next_ticket=UINT64_MAX;assert(!civ_runner_start(&r,sample));
}
static void shift_edges(void) {
    const uint8_t text[]="aAbC";CivRunner r;Fake f;uint64_t now=0;
    setup(&r,&f,(CivPayload){text,sizeof(text)-1});reach(&r,&f,CIV_STAGE_CHARACTER,&now);
    pump(&r,&f,&now);assert(r.progress.stage==CIV_STAGE_REPEAT_RELEASE && r.progress.characters_completed==1);
    pump(&r,&f,&now);assert(r.progress.stage==CIV_STAGE_CHARACTER);
    civ_runner_tick(&r,now);assert(f.output.keyboard.key==4 && f.output.keyboard.modifier==2);
    pump(&r,&f,&now);assert(r.progress.stage==CIV_STAGE_CHARACTER && r.progress.characters_completed==2);
    civ_runner_tick(&r,now);assert(f.output.keyboard.key==5 && !f.output.keyboard.modifier);
    pump(&r,&f,&now);assert(r.progress.stage==CIV_STAGE_CHARACTER && r.progress.characters_completed==3);
    civ_runner_tick(&r,now);assert(f.output.keyboard.key==6 && f.output.keyboard.modifier==2);
    finish(&r,&f,&now,CIV_RUNNER_FINISHED);
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--trace")) { approved_payload(true);return 0; }
    approved_payload(false);timing();cancellation();failures();interruptions();invalid_inputs();shift_edges();
    puts("PASS: CIV runner payload/counts, completion/dwell gating, cancellation/failure/interruption, stale tickets and neutral cleanup.");
    return 0;
}
