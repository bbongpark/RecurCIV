#pragma once
#include "text.h"
#include "controller_monitor.h"
#include "compose.h"
#include "live_keyboard.h"
#include "macro_model.h"
#include "civ_usb.h"
#include "civ_sleep.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { SCREEN_HOME,SCREEN_FILES,SCREEN_CONFIRM,SCREEN_TRANSFER,SCREEN_RESULT,SCREEN_DIAGNOSTICS,SCREEN_KEYBOARD,SCREEN_CIV_PLACEHOLDER,SCREEN_CONTROLLER,SCREEN_LIVE,SCREEN_MACRO_LIBRARY,SCREEN_MACRO_DETAILS,SCREEN_MACRO_DELETE,SCREEN_CONTROLLER_MENU,SCREEN_MACRO_STUDIO,SCREEN_MACRO_EDITOR,SCREEN_MACRO_STEP,SCREEN_MACRO_RECORD,SCREEN_MACRO_PLAY,SCREEN_MACRO_DISCARD } Screen;
/* CIV uses the existing Screen router; these are presentation data, not another
   screen state machine. Ownership/status always come from the USB service. */
typedef enum { CIV_START_OK,CIV_START_USB_NOT_READY,CIV_START_CONFLICT,
    CIV_START_INVALID_PAYLOAD,CIV_START_REJECTED,CIV_START_RESTART_REQUIRED,CIV_START_SLEEP_WARNING } CivStartError;
typedef struct {
    CivUsbProgress progress;
    bool requested,cancel_requested,retrying,ready;
    CivStartError start_error;
    uint64_t elapsed_ms;
    unsigned transport_result;
    CivSleepInfo sleep;
} CivView;
typedef struct {
    Screen screen;
    bool usb_initialized,usb_ready,error,sending;
    bool caps_pending,cancel_autoindent;
    uint64_t now;
    int caps_state; /* -1 unknown, 0 off, 1 on; reported by the USB host */
    int selected,pace,seconds,enter_pause_ms;
    size_t characters,lines,tabs,done,total,file_count,file_start;
    char path[1024],name[256],message[384],preview[1024],usb_label[128];
    char files[8][256];bool directories[8];int file_rows;
    char diagnostics[12][144];
    ControllerMonitor monitor;
    SwitchGamepadReport controller_report;
    bool controller_connected,controller_armed,controller_exiting,controller_interrupted,controller_synchronizing,controller_usb_ready;
    bool composing,compose_valid;
    size_t compose_page,compose_pages;
    char compose_rows[COMPOSE_ROWS][COMPOSE_ROW_BYTES],compose_error[384];
    LiveKeyboard live;
    char macro_status[160],macro_description[256],macro_steps[3][256],macro_step_details[3][120];
    size_t macro_count,macro_index,macro_step_count,macro_page,macro_pages;
    uint64_t macro_duration;
    unsigned macro_speed,macro_repeat,macro_loop_delay;
    MacroStep macro_draft;
    size_t macro_cursor;
    uint64_t macro_elapsed;
    uint32_t macro_iteration;
    bool macro_dirty,macro_recording,macro_arming,macro_has_recording,macro_owned,macro_cleanup_blocked,macro_neutral;
    bool macro_live,macro_record_sync,macro_record_cleanup,macro_replace_pending;
    char macro_capture_status[120];
    CivView civ;
} View;
typedef enum { ACTION_NONE,ACTION_COMPOSE,ACTION_FILES,ACTION_BACK,ACTION_SEND,ACTION_PACE,ACTION_DIAGNOSTICS,ACTION_QUIT,ACTION_ABORT,ACTION_CAPS_LOCK,ACTION_ENTER_PAUSE,ACTION_AUTOINDENT,ACTION_CREATE_USB_LOG,ACTION_TEST_GAMEPAD_A,ACTION_MENU_CIV,ACTION_MENU_KEYBOARD,ACTION_MENU_CONTROLLER,ACTION_COMPOSE_PREV,ACTION_COMPOSE_NEXT,ACTION_LIVE,ACTION_MACRO_LIBRARY,ACTION_MACRO_OPEN,ACTION_MACRO_UP,ACTION_MACRO_DOWN,ACTION_MACRO_PREV,ACTION_MACRO_NEXT,ACTION_MACRO_REFRESH,ACTION_MACRO_DELETE_ASK,ACTION_MACRO_DELETE_CONFIRM,ACTION_CONTROLLER_STANDARD,ACTION_MACRO_STUDIO,ACTION_STUDIO_MANUAL,ACTION_STUDIO_RECORD,ACTION_STUDIO_DISMISS,ACTION_CIV_START,ACTION_CIV_CANCEL,ACTION_CIV_RETRY,ACTION_ROW0=100 } Action;
typedef struct { int x,w;const char *label;Action action; } ViewFooterButton;
/* Separate action range from file rows; stored RCMS format is unchanged. */
enum { ACTION_MS_NEW=200,ACTION_MS_RECORD,ACTION_MS_RECORD_START,ACTION_MS_RECORD_STOP,ACTION_MS_REVIEW,
    ACTION_MS_PREV_STEP,ACTION_MS_NEXT_STEP,ACTION_MS_INSERT,ACTION_MS_EDIT,ACTION_MS_REMOVE,
    ACTION_MS_MOVE_UP,ACTION_MS_MOVE_DOWN,ACTION_MS_DELAY,ACTION_MS_UNDO,ACTION_MS_REDO,
    ACTION_MS_REPEATS,ACTION_MS_SPEED,ACTION_MS_SAVE,ACTION_MS_PLAY,ACTION_MS_STOP,
    ACTION_MS_RETRY,ACTION_MS_RENAME,ACTION_MS_DUPLICATE,ACTION_MS_APPLY,ACTION_MS_NEUTRAL,
    ACTION_MS_DISCARD,ACTION_MS_PLAY_ONCE,ACTION_MS_LIVE_RECORD,ACTION_MS_LIVE_EXIT,
    ACTION_MS_REPLACE_RECORD,ACTION_MS_KEEP_RECORD,ACTION_MS_FIELD0=250 };
typedef struct { const char *hints;ViewFooterButton buttons[4];size_t count; } ViewFooter;
const char *view_screen_name(Screen screen);
ViewFooter view_footer(const View *view);
void view_render(const View *view,uint32_t *pixels,int stride);
Action view_touch(const View *view,int x,int y);
Action view_home_action(Screen screen,bool plus,bool back,bool y,bool a,int selected);
int view_home_move(int selected,int direction);
int view_home_text_x(int card,int text_width);
int view_keyboard_move(int selected,int direction);
int view_studio_move(int selected,int direction);
void view_menu_stick_direction(int32_t x,int32_t y,int *dx,int *dy);
Action view_keyboard_select(int selected);
void view_macro_step(const MacroStep *step,size_t index,char *label,size_t label_size,char *detail,size_t detail_size);
bool view_civ_blocked(const View *view);
const char *view_civ_status(const View *view);
const char *view_civ_stage(const View *view);
const char *view_civ_start_error(CivStartError error);
typedef struct { int x,y,w,h;Action action; } ViewControl;
#define COMPOSE_CONTROL_COUNT 9
ViewControl view_compose_control(unsigned index);
Action view_compose_action(const View *view,unsigned index);
void view_compose_label(const View *view,unsigned index,char *label,size_t size);
void view_compose_navigate(View *view,uint32_t down);
bool view_adjust_typing_setting(View *view,Action action);
size_t view_macro_control_count(const View *view);
ViewControl view_macro_control(const View *view,unsigned index);
void view_macro_control_label(const View *view,unsigned index,char *label,size_t size);
