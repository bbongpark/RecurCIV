#include "civ_runner.h"
#include <string.h>

/* PicoSwitch/tools/payload_to_macro.py:23-36; MacroEngine.h:7-10.
   Timings begin AFTER both required report completions, unlike Pico's open-loop
   accumulated deadlines. No catch-up loop or elapsed-time action skipping. */
enum { START_MS=1000, A_MS=50, A_SETTLE_MS=550, KEY_MS=32,
       REPEAT_MS=32, ESC_SETTLE_MS=750, ENTER_MS=32, ENTER_GAP_MS=200,
       LINE_SETTLE_MS=750, PREAMBLE_COUNT=5 };

static bool key_for(uint8_t c,Stroke *stroke) {
    CharacterKeys keys;
    if((c<32 && c!='\t') || c>126 || !character_keys(c,&keys) || keys.count!=1) return false;
    *stroke=keys.keys[0];return true;
}
static size_t end_of_line(CivPayload p,size_t start) {
    while(start<p.size && p.bytes[start]!='\n' && p.bytes[start]!='\r') start++;
    return start;
}
static size_t after_line(CivPayload p,size_t end) {
    if(end<p.size && p.bytes[end]=='\r') {
        end++;if(end<p.size && p.bytes[end]=='\n') end++;
    } else if(end<p.size && p.bytes[end]=='\n') end++;
    return end;
}
static bool preflight(CivPayload p,size_t *lines,size_t *characters) {
    if(!p.bytes || !p.size || p.size>CIV_RUNNER_MAX_BYTES) return false;
    *lines=*characters=0;
    for(size_t start=0;start<p.size;) {
        size_t end=end_of_line(p,start);bool nonspace=false;
        if(end==start || end-start>CIV_RUNNER_MAX_LINE_BYTES || ++*lines>CIV_RUNNER_MAX_LINES) return false;
        for(size_t i=start;i<end;i++) {
            Stroke key;if(!key_for(p.bytes[i],&key)) return false;
            if(p.bytes[i]!=' ' && p.bytes[i]!='\t') nonspace=true;
        }
        if(!nonspace) return false;
        *characters+=end-start;start=after_line(p,end);
    }
    return true;
}
bool civ_runner_init(CivRunner *r,const CivRunnerTransport *transport) {
    if(!r || !transport || !transport->ready || !transport->idle || !transport->submit || !transport->poll || !transport->abort) return false;
    memset(r,0,sizeof(*r));r->transport=*transport;r->initialized=true;return true;
}
bool civ_runner_active(const CivRunner *r) {
    return r && r->initialized && (r->progress.status==CIV_RUNNER_RUNNING ||
        r->progress.status==CIV_RUNNER_CLEANING_UP || r->progress.status==CIV_RUNNER_CLEANUP_BLOCKED);
}
CivRunnerProgress civ_runner_progress(const CivRunner *r) {
    return r?r->progress:(CivRunnerProgress){0};
}
bool civ_runner_start(CivRunner *r,CivPayload payload) {
    size_t lines,characters;
    if(!r || !r->initialized || civ_runner_active(r) || r->next_ticket==UINT64_MAX ||
       !preflight(payload,&lines,&characters) || !r->transport.ready(r->transport.context) ||
       !r->transport.idle(r->transport.context)) return false;
    r->payload=payload;r->progress=(CivRunnerProgress){0};
    r->progress.status=CIV_RUNNER_RUNNING;r->progress.stage=CIV_STAGE_START_DELAY;
    r->progress.line=1;r->progress.lines_total=lines;r->progress.characters_total=characters;
    r->line_start=r->position=0;r->line_end=end_of_line(payload,0);
    r->progress.line_length=r->line_end;r->outcome=CIV_RUNNER_FINISHED;
    r->preamble_count=r->completed_interfaces=0;r->action_live=r->waiting=false;
    return true;
}
static void fence_action(CivRunner *r) {
    if(r->action_live) r->transport.abort(r->transport.context,r->output.ticket);
    r->action_live=r->waiting=false;r->progress.pending=false;r->completed_interfaces=0;
}
static void stop(CivRunner *r,CivRunnerStatus outcome,CivRunnerError error) {
    if(!civ_runner_active(r)) return;
    if(r->progress.status==CIV_RUNNER_CLEANUP_BLOCKED) return;
    /* Do not hide an existing failure with a later Cancel. A cleanup transport
       interruption still revokes any claimed neutral completion. */
    if(r->progress.status==CIV_RUNNER_RUNNING || r->outcome==CIV_RUNNER_FINISHED) {
        r->outcome=outcome;r->progress.error=error;
    }
    fence_action(r);r->progress.neutral_confirmed=false;
    r->progress.stage=CIV_STAGE_NEUTRAL;r->progress.status=CIV_RUNNER_CLEANING_UP;
}
void civ_runner_cancel(CivRunner *r) { stop(r,CIV_RUNNER_CANCELLED,CIV_ERROR_NONE); }
void civ_runner_fail(CivRunner *r) { stop(r,CIV_RUNNER_FAILED,CIV_ERROR_EXTERNAL); }
void civ_runner_interrupt(CivRunner *r) { stop(r,CIV_RUNNER_INTERRUPTED,CIV_ERROR_TRANSPORT); }
bool civ_runner_retry_cleanup(CivRunner *r) {
    if(!r || r->progress.status!=CIV_RUNNER_CLEANUP_BLOCKED || r->next_ticket==UINT64_MAX) return false;
    r->progress.status=CIV_RUNNER_CLEANING_UP;return true;
}
static void blocked(CivRunner *r,CivRunnerError error) {
    fence_action(r);
    if(r->outcome==CIV_RUNNER_FINISHED) r->outcome=CIV_RUNNER_FAILED;
    if(r->progress.error==CIV_ERROR_NONE) r->progress.error=error;
    r->progress.status=CIV_RUNNER_CLEANUP_BLOCKED;r->progress.neutral_confirmed=false;
}
static void advance(CivRunner *r) {
    switch(r->progress.stage) {
    case CIV_STAGE_START_DELAY: r->progress.stage=CIV_STAGE_PREAMBLE_A;break;
    case CIV_STAGE_PREAMBLE_A: r->progress.stage=CIV_STAGE_PREAMBLE_RELEASE;break;
    case CIV_STAGE_PREAMBLE_RELEASE:
        r->progress.stage=++r->preamble_count<PREAMBLE_COUNT?CIV_STAGE_PREAMBLE_A:CIV_STAGE_ESCAPE;break;
    case CIV_STAGE_ESCAPE: r->progress.stage=CIV_STAGE_ESCAPE_SETTLE;break;
    case CIV_STAGE_ESCAPE_SETTLE: r->progress.stage=CIV_STAGE_ACTIVATE_A;break;
    case CIV_STAGE_ACTIVATE_A: r->progress.stage=CIV_STAGE_ACTIVATE_SETTLE;break;
    case CIV_STAGE_ACTIVATE_SETTLE: r->progress.stage=CIV_STAGE_CHARACTER;break;
    case CIV_STAGE_REPEAT_RELEASE: r->progress.stage=CIV_STAGE_CHARACTER;break;
    case CIV_STAGE_CHARACTER: {
        Stroke prev,next;key_for(r->payload.bytes[r->position],&prev);
        r->position++;r->progress.characters_completed++;
        r->progress.character=r->position-r->line_start;
        if(r->position==r->line_end) r->progress.stage=CIV_STAGE_ENTER_1;
        else {
            key_for(r->payload.bytes[r->position],&next);
            r->progress.stage=prev.key==next.key?CIV_STAGE_REPEAT_RELEASE:CIV_STAGE_CHARACTER;
        }
        break;
    }
    case CIV_STAGE_ENTER_1: r->progress.stage=CIV_STAGE_ENTER_GAP;break;
    case CIV_STAGE_ENTER_GAP: r->progress.stage=CIV_STAGE_ENTER_2;break;
    case CIV_STAGE_ENTER_2:
        r->progress.lines_completed++;
        if(r->progress.lines_completed==r->progress.lines_total) {
            r->progress.stage=CIV_STAGE_NEUTRAL;r->progress.status=CIV_RUNNER_CLEANING_UP;
        } else r->progress.stage=CIV_STAGE_LINE_SETTLE;
        break;
    case CIV_STAGE_LINE_SETTLE: r->progress.stage=CIV_STAGE_SEPARATOR_A;break;
    case CIV_STAGE_SEPARATOR_A: r->progress.stage=CIV_STAGE_SEPARATOR_SETTLE;break;
    case CIV_STAGE_SEPARATOR_SETTLE:
        r->line_start=r->position=after_line(r->payload,r->line_end);
        r->line_end=end_of_line(r->payload,r->line_start);
        r->progress.line++;r->progress.character=0;r->progress.line_length=r->line_end-r->line_start;
        r->progress.stage=CIV_STAGE_ESCAPE;break;
    case CIV_STAGE_NEUTRAL:
        r->progress.neutral_confirmed=true;r->progress.status=r->outcome;break;
    }
}
static CivRunnerOutput output_for(CivRunner *r) {
    CivRunnerOutput out={0};out.stage=r->progress.stage;
    out.gamepad.hat=SWITCH_GAMEPAD_HAT_NONE;
    out.gamepad.lx=out.gamepad.ly=out.gamepad.rx=out.gamepad.ry=SWITCH_GAMEPAD_STICK_CENTER;
    switch(out.stage) {
    case CIV_STAGE_START_DELAY: out.minimum_ms=START_MS;break;
    case CIV_STAGE_PREAMBLE_A: case CIV_STAGE_ACTIVATE_A: case CIV_STAGE_SEPARATOR_A:
        out.gamepad.buttons=SWITCH_GAMEPAD_A;out.minimum_ms=A_MS;break;
    case CIV_STAGE_PREAMBLE_RELEASE: case CIV_STAGE_ACTIVATE_SETTLE: case CIV_STAGE_SEPARATOR_SETTLE:
        out.minimum_ms=A_SETTLE_MS;break;
    case CIV_STAGE_ESCAPE: out.keyboard.key=0x29;out.minimum_ms=KEY_MS;break;
    case CIV_STAGE_ESCAPE_SETTLE: out.minimum_ms=ESC_SETTLE_MS;break;
    case CIV_STAGE_CHARACTER:
        key_for(r->payload.bytes[r->position],&out.keyboard);out.minimum_ms=KEY_MS;break;
    case CIV_STAGE_REPEAT_RELEASE: out.minimum_ms=REPEAT_MS;break;
    case CIV_STAGE_ENTER_1: case CIV_STAGE_ENTER_2: out.keyboard.key=0x28;out.minimum_ms=ENTER_MS;break;
    case CIV_STAGE_ENTER_GAP: out.minimum_ms=ENTER_GAP_MS;break;
    case CIV_STAGE_LINE_SETTLE: out.minimum_ms=LINE_SETTLE_MS;break;
    case CIV_STAGE_NEUTRAL: break;
    }
    return out;
}
void civ_runner_tick(CivRunner *r,uint64_t now) {
    if(!civ_runner_active(r) || r->progress.status==CIV_RUNNER_CLEANUP_BLOCKED) return;
    if(!r->transport.ready(r->transport.context)) { civ_runner_interrupt(r);return; }
    if(r->action_live) {
        if(r->waiting) {
            CivRunnerCompletion done=r->transport.poll(r->transport.context,r->output.ticket);
            if(done.ticket!=r->output.ticket) return; /* stale callback, including a stale error */
            if(done.result==CIV_TRANSFER_INTERRUPTED) { civ_runner_interrupt(r);return; }
            if(done.result!=CIV_TRANSFER_OK || (done.completed&~CIV_INTERFACE_BOTH)) {
                if(r->progress.stage==CIV_STAGE_NEUTRAL) blocked(r,CIV_ERROR_COMPLETION);
                else stop(r,CIV_RUNNER_FAILED,CIV_ERROR_COMPLETION);
                return;
            }
            r->completed_interfaces|=done.completed;
            if(r->completed_interfaces!=CIV_INTERFACE_BOTH) return;
            r->waiting=false;r->progress.pending=false;r->completed_at=now;
            return; /* Minimum hold starts now, not when submit was attempted. */
        }
        if(now<r->completed_at || now-r->completed_at<r->output.minimum_ms) return;
        r->action_live=false;advance(r);
        return; /* At most one transition; never catch up multiple steps. */
    }
    if(!r->transport.idle(r->transport.context)) return;
    if(r->next_ticket==UINT64_MAX) { blocked(r,CIV_ERROR_TICKET_EXHAUSTED);return; }
    r->output=output_for(r);r->output.ticket=++r->next_ticket;
    r->progress.ticket=r->output.ticket;r->progress.pending=true;
    r->progress.neutral_confirmed=false;r->action_live=r->waiting=true;r->completed_interfaces=0;
    if(!r->transport.submit(r->transport.context,&r->output)) {
        if(r->progress.stage==CIV_STAGE_NEUTRAL) blocked(r,CIV_ERROR_SUBMISSION);
        else stop(r,CIV_RUNNER_FAILED,CIV_ERROR_SUBMISSION);
    }
}
