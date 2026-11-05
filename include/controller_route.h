#pragma once
#include "controller_mode.h"
#include "view.h"

typedef struct {
    ControllerMode mode;
    Action after_exit;
} ControllerRoute;

/* Handles controller-related screen transitions; false leaves action to main. */
bool controller_route_action(ControllerRoute *route,View *view,Action action);
/* Called for every screen; only Standard Controller can submit live state. */
bool controller_route_tick(ControllerRoute *route,View *view,bool connected,uint32_t held,
                           int32_t lx,int32_t ly,int32_t rx,int32_t ry);
Action controller_route_select(int selected);
/* Route fresh button edges after directional repeat/selection processing. */
bool controller_route_input(const View *view,uint32_t down,bool monitor_exit,Action *action);
/* Shared by main and routing/render regression tests after repeat generation. */
void controller_route_navigate(View *view,uint32_t down);
/* A sampled event cannot be reused on a destination screen or consumed twice. */
bool controller_route_event_current(const View *view,Screen sampled_screen,bool consumed);
