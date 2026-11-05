#include "typer.h"

void typer_start(Typer *t,const Stroke *keys,size_t count,unsigned delay_ms,uint64_t countdown_until) {
    *t=(Typer){.phase=count?TX_COUNTDOWN:TX_DONE,.strokes=keys,.count=count,.due=countdown_until,.delay_ms=delay_ms};
}
bool typer_step(Typer *t,uint64_t now,bool pending,Stroke *next) {
    if(pending) return false;
    if(t->phase==TX_COUNTDOWN) {
        if(now<t->due) return false;
        *next=(Stroke){0,0};t->phase=TX_INITIAL;return true;
    }
    if(t->phase==TX_INITIAL) t->phase=TX_PRESS;
    if(t->phase==TX_ENTER_DOWN || t->phase==TX_ENTER_UP) {
        /* Start these timers after USB completion, so a slow acknowledgement
           cannot consume the pause intended for the target application. */
        if(!t->due) {
            unsigned delay=t->phase==TX_ENTER_DOWN?50:t->enter_pause_ms;
            if(delay<t->delay_ms) delay=t->delay_ms;
            t->due=now+delay;return false;
        }
        if(now<t->due) return false;
        if(t->phase==TX_ENTER_DOWN) {
            *next=(Stroke){0,0};t->phase=TX_ENTER_UP;t->due=0;return true;
        }
        t->index++;
        if(t->index>=t->count) { t->phase=TX_DONE;return false; }
        t->phase=TX_PRESS;
    }
    if(t->phase==TX_KEYUP) {
        if(now<t->due) return false;
        t->index++;
        if(t->index>=t->count) { t->phase=TX_DONE;return false; }
        t->phase=TX_PRESS;
    }
    if(t->phase==TX_PRESS) {
        if(t->enter_pause_ms && t->strokes[t->index].key==0x28) {
            /* The preceding all-keys-up was acknowledged. Keep modifiers
               released before Enter as well as after it. */
            t->phase=TX_ENTER_WAIT;t->due=now+50;return false;
        }
        *next=t->strokes[t->index];t->phase=TX_KEYDOWN;t->due=now+t->delay_ms;return true;
    }
    if(t->phase==TX_ENTER_WAIT && now>=t->due) {
        *next=t->strokes[t->index];t->phase=TX_ENTER_DOWN;t->due=0;return true;
    }
    if(t->phase==TX_KEYDOWN && now>=t->due) {
        *next=(Stroke){0,0};t->phase=TX_KEYUP;t->due=now+t->delay_ms;return true;
    }
    return false;
}
