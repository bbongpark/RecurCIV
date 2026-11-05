#pragma once
#include "view.h"
#include "macro_editor.h"
#include "macro_runtime.h"

typedef enum { MD_NONE,MD_SAVE,MD_RENAME,MD_DUPLICATE,MD_DURATION,MD_AXIS,MD_REPEATS } MacroDialog;
typedef struct {
    MacroEditor editor;
    MacroRecorder recorder;
    MacroPlayer player;
    MacroStep draft;
    size_t cursor,library_page,count;
    char (*names)[MACRO_NAME_BYTES];
    char file_key[MACRO_NAME_BYTES];
    const char *root;
    bool has_file,inserting,input_locked;
    Screen return_screen;
    MacroDialog dialog;
    unsigned axis;
    char dialog_title[80],dialog_initial[MACRO_NAME_BYTES];
    LiveNav navigation;
} MacroApp;
void macro_app_init(MacroApp *app,const char *root);
void macro_app_destroy(MacroApp *app);
bool macro_app_screen(Screen screen);
bool macro_app_busy(const MacroApp *app);
void macro_app_update(MacroApp *app,View *view);
void macro_app_tick(MacroApp *app,View *view,bool connected,uint32_t held,int32_t lx,int32_t ly,int32_t rx,int32_t ry,unsigned touches,uint64_t now);
Action macro_app_input(MacroApp *app,View *view,uint32_t down,uint32_t held,int32_t sx,int32_t sy,unsigned touches,int x,int y,uint64_t now);
bool macro_app_action(MacroApp *app,View *view,Action action,uint64_t now,bool application_available);
void macro_app_dialog_result(MacroApp *app,View *view,bool accepted,const char *text);
