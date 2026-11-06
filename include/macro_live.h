#pragma once
#include "macro_runtime.h"
#include "controller_mode.h"

/* Application coordinator only: no endpoint access, event queue, or new mapping.
   The recorder is borrowed and must outlive ownership, including cleanup. */
typedef enum { ML_IDLE,ML_SYNC,ML_ARMING,ML_READY,ML_RECORDING,ML_CLEANUP,ML_BLOCKED,ML_STOPPED } MacroLiveState;
typedef struct {
    MacroRecorder *recorder;
    ControllerMode controller;
    MacroLiveState state;
    bool owned,stop_queued,neutral_confirmed;
} MacroLive;

bool macro_live_start(MacroLive *live,MacroRecorder *recorder);
bool macro_live_busy(const MacroLive *live);
void macro_live_tick(MacroLive *live,bool connected,uint32_t held,
                     int32_t lx,int32_t ly,int32_t rx,int32_t ry,unsigned touches,uint64_t sampled_at);
void macro_live_stop(MacroLive *live,uint64_t sampled_at);
/* Recording controls do not release passthrough. Replacement must be explicit. */
bool macro_live_record_start(MacroLive *live,bool discard_previous);
void macro_live_exit(MacroLive *live,uint64_t sampled_at);
bool macro_live_retry_cleanup(MacroLive *live);
