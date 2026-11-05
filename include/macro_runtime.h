#pragma once
#include "macro_storage.h"
#include "usb_gamepad.h"

#define MACRO_RECORD_LIMIT_MS 600000u
typedef enum { MR_IDLE,MR_ARMING,MR_RECORDING,MR_STOPPED,MR_LIMIT,MR_INTERRUPTED } MacroRecordState;
typedef struct {
    MacroSequence sequence;
    SwitchGamepadReport current;
    MacroRecordState state;
    uint64_t started,changed,elapsed_ms;
} MacroRecorder;
void macro_record_start(MacroRecorder *recorder);
void macro_record_sample(MacroRecorder *recorder,const SwitchGamepadReport *report,bool connected,bool controls_released,uint64_t now);
void macro_record_stop(MacroRecorder *recorder,uint64_t now);

typedef enum { MP_IDLE,MP_SYNC,MP_SUBMIT,MP_PENDING,MP_HOLD,MP_CLEANUP,MP_BLOCKED,
    MP_FINISHED,MP_CANCELLED,MP_FAILED,MP_INTERRUPTED } MacroPlayState;
typedef struct {
    const MacroDocument *document; /* Borrowed and immutable through cleanup. */
    MacroPlayState state,outcome;
    size_t step;
    uint32_t repeat,repeat_count,duration_ms;
    uint64_t started,completed_at,elapsed_ms;
    bool owned,neutral_confirmed,loop_gap,stop_queued;
} MacroPlayer;
bool macro_play_start(MacroPlayer *player,const MacroDocument *document,uint64_t now);
bool macro_play_once(MacroPlayer *player,const MacroDocument *document,uint64_t now);
void macro_play_tick(MacroPlayer *player,uint64_t now);
void macro_play_cancel(MacroPlayer *player);
bool macro_play_retry_cleanup(MacroPlayer *player);
uint64_t macro_play_minimum_ms(const MacroDocument *document);
