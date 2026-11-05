#include "typer.h"
#include "pace.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const Stroke keys[]={{2,11},{0,8},{0,15},{0,15},{0,18}};
    const int order[]={PACE_NORMAL,PACE_FAST,PACE_VERY_FAST,PACE_ULTRA,PACE_TURBO,PACE_SLOW};
    for(unsigned i=0;i<PACE_COUNT;i++) assert(pace_next(order[i])==order[(i+1)%PACE_COUNT]);
    assert(pace_delay(PACE_SLOW)==160 && pace_delay(PACE_NORMAL)==80 && pace_delay(PACE_FAST)==40);
    assert(pace_delay(PACE_VERY_FAST)==10 && pace_delay(PACE_ULTRA)==2 && pace_delay(PACE_TURBO)==0);
    assert(pace_delay(-1)==80 && pace_delay(99)==80);

    Typer t;Stroke next;
    typer_start(&t,keys,5,0,3000);
    assert(!typer_step(&t,2999,false,&next) && t.phase==TX_COUNTDOWN);
    assert(typer_step(&t,3000,false,&next) && next.key==0 && t.phase==TX_INITIAL);
    assert(!typer_step(&t,9000,true,&next)); /* never overwrite an unacknowledged report */
    uint64_t now=9001;
    for(size_t i=0;i<5;i++) {
        assert(typer_step(&t,now,false,&next));
        assert(next.key==keys[i].key && next.modifier==keys[i].modifier && t.index==i);
        assert(!typer_step(&t,now+1,true,&next));
        assert(typer_step(&t,now+2,false,&next) && next.key==0 && next.modifier==0);
        assert(!typer_step(&t,now+3,true,&next)); /* final release must be acknowledged too */
        now+=4;
    }
    assert(!typer_step(&t,now,false,&next) && t.phase==TX_DONE && t.index==5);
    assert(!typer_step(&t,now+1,false,&next)); /* no duplicate final character */

    typer_start(&t,keys,1,80,0);
    assert(typer_step(&t,0,false,&next) && next.key==0);
    assert(typer_step(&t,1,false,&next) && next.key==11);
    assert(!typer_step(&t,80,false,&next));
    assert(typer_step(&t,81,false,&next) && next.key==0);
    assert(!typer_step(&t,160,false,&next) && t.index==0);
    assert(!typer_step(&t,161,false,&next) && t.phase==TX_DONE && t.index==1);
    typer_start(&t,keys,1,2,0);
    assert(typer_step(&t,0,false,&next));assert(typer_step(&t,1,false,&next));
    assert(!typer_step(&t,2,false,&next));assert(typer_step(&t,3,false,&next) && next.key==0);
    assert(!typer_step(&t,4,false,&next));assert(!typer_step(&t,5,false,&next) && t.phase==TX_DONE);
    const Stroke caps={0,0x39};
    typer_start(&t,&caps,1,20,0);
    assert(typer_step(&t,0,false,&next) && next.key==0);
    assert(!typer_step(&t,1,true,&next));
    assert(typer_step(&t,2,false,&next) && next.key==0x39 && next.modifier==0);
    assert(!typer_step(&t,21,false,&next));
    assert(typer_step(&t,22,false,&next) && next.key==0 && next.modifier==0);
    assert(!typer_step(&t,50,true,&next) && t.phase==TX_KEYUP);
    assert(!typer_step(&t,51,false,&next) && t.phase==TX_DONE && t.index==1);
    const Stroke enter_then_letter[]={{0,0x28},{0,4}};
    typer_start(&t,enter_then_letter,2,0,0);t.enter_pause_ms=200;
    assert(typer_step(&t,0,false,&next) && next.key==0);
    assert(!typer_step(&t,1,false,&next) && t.phase==TX_ENTER_WAIT);
    assert(!typer_step(&t,50,false,&next));
    assert(typer_step(&t,51,false,&next) && next.key==0x28 && next.modifier==0);
    assert(!typer_step(&t,500,true,&next)); /* slow USB does not consume the hold */
    assert(!typer_step(&t,501,false,&next));
    assert(!typer_step(&t,550,false,&next));
    assert(typer_step(&t,551,false,&next) && next.key==0);
    assert(!typer_step(&t,1000,true,&next));
    assert(!typer_step(&t,1001,false,&next)); /* pause starts after release ACK */
    assert(!typer_step(&t,1200,false,&next));
    assert(typer_step(&t,1201,false,&next) && next.key==4 && t.index==1);
    /* With Enter pause disabled, Enter follows ordinary zero-delay timing. */
    typer_start(&t,enter_then_letter,2,0,0);
    assert(typer_step(&t,0,false,&next));
    assert(typer_step(&t,1,false,&next) && next.key==0x28);
    assert(typer_step(&t,2,false,&next) && next.key==0);
    assert(typer_step(&t,3,false,&next) && next.key==4);
    const Stroke blank_lines[]={{0,0x28},{0,0x28},{0,0x28}};
    for(unsigned pause=100;pause<=800;pause*=2) {
        typer_start(&t,blank_lines,3,0,0);t.enter_pause_ms=pause;
        unsigned presses=0,releases=0;bool held=false;
        for(unsigned tick=0;tick<5000 && t.phase!=TX_DONE;tick++) {
            if(typer_step(&t,tick,false,&next)) {
                if(next.key) { assert(!held && next.key==0x28);held=true;presses++; }
                else if(held) { held=false;releases++; }
            }
        }
        assert(t.phase==TX_DONE && presses==3 && releases==3 && !held);
    }
    assert(enter_pause_next(800)==0 && enter_pause_next(0)==100 && enter_pause_valid(200) && !enter_pause_valid(7));
    typer_start(&t,keys,0,0,0);assert(!typer_step(&t,0,false,&next) && t.phase==TX_DONE);
    puts("PASS: six speeds, countdown, repeated letters, Caps Lock, final release, Enter settle/hold/post-release pause, slow ACKs, blank lines and disabled Enter pause.");return 0;
}
