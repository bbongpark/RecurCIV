#pragma once
#include "view.h"

#define CIV_CANCEL_CHORD (MON_L|MON_R|MON_MINUS)
#define CIV_CANCEL_HOLD_MS 1000u
typedef struct {
    uint64_t started_at,chord_at;
    bool on_screen,timing,input_locked,chord_tracking;
    LiveNav navigation;
} CivRoute;

void civ_route_update(CivRoute *route,View *view,uint64_t now_ms);
/* A single input arbitration call per loop; touches are counts (multitouch is
   ignored). Fresh edges/release gating keep transitions and held A local. */
Action civ_route_input(CivRoute *route,View *view,uint32_t down,uint32_t held,
                      int32_t stick_x,int32_t stick_y,unsigned touches,int x,int y,uint64_t now_ms);
bool civ_route_action(CivRoute *route,View *view,Action action,uint64_t now_ms,bool application_available);
/* Uses the existing runner preflight without ticking or submitting any output. */
bool civ_route_payload_valid(CivPayload payload);
