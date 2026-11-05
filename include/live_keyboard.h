#pragma once
#include "text.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LIVE_QUEUE_CAPACITY 32
/* Match the hardware-verified Compose & Send default (PACE_NORMAL, 80 ms).
   Each press is held and each all-keys-up report is retained for at least
   this long, so a fast USB acknowledgement cannot collapse a keystroke into
   a press/release pair that the host samples as a single neutral report. */
#define LIVE_PRESS_MS 80u
#define LIVE_NEUTRAL_MS 80u
/* Non-blocking controller/UI timing. */
#define LIVE_CAPS_WINDOW_MS 350u
#define LIVE_PRESS_FLASH_MS 120u
#define LIVE_NAV_DELAY_MS 350u
#define LIVE_NAV_REPEAT_MS 100u
#define LIVE_STICK_DEADZONE 12000
#define LIVE_KEY_ROWS 5
enum { LIVE_SHIFT=256, LIVE_ENTER, LIVE_BACKSPACE, LIVE_SPACE };
/* x,y,w,h,key then the printable pair, optional label, and visual row. */
typedef struct { int x,y,w,h,key; char plain,shifted; const char *label; int row; } LiveKey;
typedef enum { LIVE_SYNC, LIVE_SYNC_WAIT, LIVE_IDLE, LIVE_PRESS_WAIT,
    LIVE_RELEASE, LIVE_RELEASE_WAIT, LIVE_EXIT_SEND, LIVE_EXIT_WAIT, LIVE_DONE } LivePhase;
typedef struct { int dx,dy; uint64_t next_at; } LiveNav;
typedef struct {
    Stroke queue[LIVE_QUEUE_CAPACITY];
    unsigned head,count,dropped;
    LivePhase phase;
    uint64_t due; /* monotonic deadline for the current press or neutral */
    uint32_t failures;
    bool was_ready,shift,caps,touch_armed,exiting,fault,offered,recovery_attempted;
    bool a_held,b_held,x_held,y_held,plus_held;
    int touched;
    unsigned selected;
    int pressed;
    uint64_t pressed_until,last_shift_tap;
    LiveNav nav;
} LiveKeyboard;
/* Controller input is split into held directions and action-button state.
   Action buttons are edge-latched internally, so a held A/B/X/Y/+ fires once.
   The main loop fills this from libnx; tests can drive it directly. */
typedef struct {
    bool up,down,left,right;
    bool a,b,x,y,plus;
    int stick_x,stick_y;
} LiveControls;
typedef enum { LIVE_INPUT_NONE,LIVE_INPUT_DONE } LiveInputResult;
unsigned live_key_count(void);
LiveKey live_key(unsigned index);
int live_key_at(int x,int y);
int live_key_index_at(int x,int y);
int live_key_index_for(int key);
char live_key_character(const LiveKeyboard *live,const LiveKey *key);
const char *live_key_text(const LiveKeyboard *live,const LiveKey *key,char *buffer,size_t size);
void live_init(LiveKeyboard *live,uint32_t failures);
void live_touch(LiveKeyboard *live,unsigned touches,int key,uint64_t now);
void live_exit(LiveKeyboard *live);
bool live_move(LiveKeyboard *live,int dx,int dy);
bool live_activate(LiveKeyboard *live,int key,uint64_t now);
bool live_activate_selected(LiveKeyboard *live,uint64_t now);
bool live_stick_direction(int x,int y,int *dx,int *dy);
bool live_nav_step(LiveNav *nav,int dx,int dy,uint64_t now);
LiveInputResult live_controls(LiveKeyboard *live,const LiveControls *controls,uint64_t now);
/* Caller polls USB first, then submits the offered report and reports success. */
bool live_step(LiveKeyboard *live,bool ready,bool pending,uint32_t failures,uint64_t now,Stroke *report);
void live_submitted(LiveKeyboard *live,bool success);
