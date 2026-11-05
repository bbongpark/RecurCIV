#pragma once
#include "text.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { TX_NONE,TX_COUNTDOWN,TX_INITIAL,TX_PRESS,TX_KEYDOWN,TX_KEYUP,TX_ENTER_WAIT,TX_ENTER_DOWN,TX_ENTER_UP,TX_DONE,TX_ABORT } TxPhase;
typedef struct {
    TxPhase phase;
    const Stroke *strokes;
    size_t count,index;
    uint64_t due;
    unsigned delay_ms;
    unsigned enter_pause_ms; /* 0 uses ordinary key timing; otherwise settle/hold Enter for >=50 ms. */
} Typer;

void typer_start(Typer *typer,const Stroke *strokes,size_t count,unsigned delay_ms,uint64_t countdown_until);
/* pending means the preceding report has not yet been acknowledged. The caller
   must stop on a transport failure before calling this function again. */
bool typer_step(Typer *typer,uint64_t now,bool pending,Stroke *next);
