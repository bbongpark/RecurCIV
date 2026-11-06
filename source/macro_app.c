#include "macro_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define LIBRARY_CAPACITY 8192u
static const SwitchGamepadReport neutral={0,8,128,128,128,128,0};
bool macro_app_screen(Screen s) {
    return s==SCREEN_MACRO_STUDIO || s==SCREEN_MACRO_LIBRARY || s==SCREEN_MACRO_DETAILS || s==SCREEN_MACRO_DELETE || (s>=SCREEN_MACRO_EDITOR && s<=SCREEN_MACRO_DISCARD);
}
bool macro_app_busy(const MacroApp *a) {
    return a && (a->player.owned || macro_live_busy(&a->live) || a->recorder.state==MR_ARMING || a->recorder.state==MR_RECORDING);
}
void macro_app_init(MacroApp *a,const char *root) { memset(a,0,sizeof(*a));a->root=root; }
void macro_app_destroy(MacroApp *a) { if(a) { free(a->names);a->names=NULL; } }
static void result(View *v,MacroStoreResult r) {
    const char *s=r==MACRO_STORE_EXISTS?"Name already exists; choose another name.":
        r==MACRO_STORE_INVALID?"Invalid or corrupt macro; no changes applied.":
        r==MACRO_STORE_VERSION?"Unsupported macro format version.":
        r==MACRO_STORE_RECOVERY_REQUIRED?"Storage recovery required. Preserve .tmp/.bak files; no automatic overwrite.":
        r==MACRO_STORE_FULL?"SD card full; staged edits may remain.":"Storage operation failed. Original data may remain; refresh the library.";
    if(r==MACRO_STORE_OK) snprintf(v->macro_status,sizeof(v->macro_status),"Saved successfully.");
    else snprintf(v->macro_status,sizeof(v->macro_status),"%s (code %u)",s,(unsigned)r);
}
static void refresh(MacroApp *a,View *v) {
    MacroStoreResult r=MACRO_STORE_OK;
    if(!strcmp(a->root,MACRO_STORAGE_DIRECTORY)) r=macro_store_prepare_default();
    if(!a->names) a->names=calloc(LIBRARY_CAPACITY,sizeof(*a->names));
    if(!a->names) r=MACRO_STORE_IO;
    a->count=0;
    if(r==MACRO_STORE_OK) r=macro_store_list(a->root,a->names,LIBRARY_CAPACITY,&a->count);
    if(r!=MACRO_STORE_OK) { a->count=0;result(v,r); }
    if(a->library_page*8>=a->count) a->library_page=0;
}
static void enter(MacroApp *a,View *v,Screen s) {
    v->screen=s;v->selected=s==SCREEN_MACRO_DISCARD || s==SCREEN_MACRO_DELETE?1:0;
    a->input_locked=true;a->navigation=(LiveNav){0};
}
static void new_document(MacroApp *a) {
    MacroDocument d={0};strcpy(d.name,"Untitled");d.speed_percent=100;d.repeat_count=1;
    (void)macro_editor_open(&a->editor,&d,false);a->has_file=false;a->file_key[0]=0;a->cursor=0;
    a->record_live=false;
}
static void dialog(MacroApp *a,MacroDialog kind,const char *title,const char *initial) {
    a->dialog=kind;snprintf(a->dialog_title,sizeof(a->dialog_title),"%s",title);
    snprintf(a->dialog_initial,sizeof(a->dialog_initial),"%s",initial);
}
static bool loaded(MacroApp *a,View *v,const char *name) {
    MacroDocument d;MacroStoreResult r=macro_store_load(a->root,name,&d);
    if(r!=MACRO_STORE_OK) { result(v,r);return false; }
    macro_editor_open(&a->editor,&d,true);a->has_file=true;a->cursor=0;
    snprintf(a->file_key,sizeof(a->file_key),"%s",name);return true;
}
void macro_app_update(MacroApp *a,View *v) {
    if(!a || !v || !macro_app_screen(v->screen)) return;
    MacroDocument *d=&a->editor.current;
    if(v->screen==SCREEN_MACRO_LIBRARY) {
        v->file_rows=0;v->macro_count=a->count;
        for(size_t i=a->library_page*8;i<a->count && i<a->library_page*8+8;i++)
            snprintf(v->files[v->file_rows++],sizeof(v->files[0]),"%s",a->names[i]);
        v->macro_page=a->library_page;v->macro_pages=(a->count+7)/8;
    } else if(v->screen!=SCREEN_MACRO_DELETE) {
        snprintf(v->name,sizeof(v->name),"%s",d->name);
        snprintf(v->macro_description,sizeof(v->macro_description),"%s",d->description);
        v->macro_step_count=d->sequence.count;v->macro_duration=macro_play_minimum_ms(d);
        v->macro_speed=d->speed_percent;v->macro_repeat=d->repeat_count;v->macro_loop_delay=d->loop_delay_ms;
        v->macro_dirty=macro_editor_dirty(&a->editor);
        if(a->cursor>=d->sequence.count) a->cursor=d->sequence.count?d->sequence.count-1:0;
        v->macro_cursor=a->cursor;v->macro_page=a->cursor/3;v->macro_pages=(d->sequence.count+2)/3;
        if(!v->macro_pages) v->macro_pages=1;
        for(size_t i=0;i<3;i++) {
            size_t n=v->macro_page*3+i;v->macro_steps[i][0]=v->macro_step_details[i][0]=0;
            if(n<d->sequence.count) view_macro_step(&d->sequence.steps[n],n,v->macro_steps[i],sizeof(v->macro_steps[i]),v->macro_step_details[i],sizeof(v->macro_step_details[i]));
        }
    }
    v->macro_draft=a->draft;v->macro_owned=a->player.owned || macro_live_busy(&a->live);
    v->macro_cleanup_blocked=a->player.state==MP_BLOCKED;
    v->macro_neutral=a->player.neutral_confirmed;v->macro_iteration=a->player.repeat;
    v->macro_live=v->screen==SCREEN_MACRO_RECORD && a->record_live;
    v->macro_record_sync=v->macro_live && a->live.state==ML_SYNC;
    v->macro_record_cleanup=v->macro_live && (a->live.state==ML_CLEANUP || a->live.state==ML_BLOCKED);
    v->macro_replace_pending=a->replace_pending;
    v->macro_recording=a->recorder.state==MR_RECORDING;v->macro_arming=a->recorder.state==MR_ARMING;
    if(v->macro_live && a->live.state==ML_ARMING) v->macro_arming=true;
    v->macro_has_recording=a->recorder.state>=MR_STOPPED;
    if(v->macro_live && !a->recorder.sequence.count) v->macro_has_recording=false;
    if(v->screen==SCREEN_MACRO_RECORD) {
        v->macro_elapsed=a->recorder.elapsed_ms;v->macro_step_count=a->recorder.sequence.count;
        if(a->record_live) {
            v->macro_cleanup_blocked=a->live.state==ML_BLOCKED;v->macro_neutral=a->live.neutral_confirmed;
            const char *s=a->live.state==ML_SYNC?"Synchronizing neutral USB output; capture has not started.":
                a->live.state==ML_ARMING?"Release buttons, D-pad, both sticks and touch to begin.":
                a->live.state==ML_READY?(a->recorder.state==MR_LIMIT?"Recording limit reached; capture retained. Passthrough continues.":
                    a->recorder.sequence.count?"Capture retained. Passthrough continues; Exit live before Review/Save.":
                    "Passthrough active. Capture or touch Start to record; Exit live to finish."):
                a->live.state==ML_RECORDING?"Recording sampled inputs while forwarding live controller state.":
                a->live.state==ML_CLEANUP?"Neutral cleanup pending. Keep USB connected; navigation is locked.":
                a->live.state==ML_BLOCKED?"USB CLEANUP REQUIRED. Reconnect, then touch Retry cleanup.":
                a->live.state==ML_STOPPED?(a->recorder.state==MR_LIMIT?"Recording limit reached; data retained. Neutral cleanup confirmed.":
                    a->recorder.state==MR_INTERRUPTED?"Recording interrupted; data retained. Neutral confirmed; no automatic resume.":
                    a->recorder.sequence.count?"Live session exited; neutral confirmed. Review captured states before saving.":
                    "Live session exited; no capture. Return to Macro Studio."):NULL;
            if(s) snprintf(v->macro_status,sizeof(v->macro_status),"%s",s);
        }
    }
    if(v->screen==SCREEN_MACRO_PLAY) {
        v->macro_repeat=a->player.repeat_count;
        v->macro_elapsed=a->player.elapsed_ms;v->macro_cursor=a->player.step;
        const char *s=a->player.state==MP_SYNC?"Synchronizing neutral controller report":
            a->player.state==MP_CLEANUP?"Cleanup pending - keep USB connected":
            a->player.state==MP_BLOCKED?"USB CLEANUP REQUIRED - reconnect, then Retry Cleanup":
            a->player.state==MP_FINISHED?"Playback finished; neutral completion confirmed":
            a->player.state==MP_CANCELLED?"Playback cancelled; neutral completion confirmed":
            a->player.state==MP_FAILED?"Playback failed; neutral completion confirmed":
            a->player.state==MP_INTERRUPTED?"USB interrupted; neutral completion confirmed; no resume":"Playing controller macro";
        snprintf(v->macro_status,sizeof(v->macro_status),"%s",s);
    }
    size_t count=view_macro_control_count(v);
    if(v->selected<0 || (size_t)v->selected>=count) v->selected=0;
}
void macro_app_tick(MacroApp *a,View *v,bool connected,uint32_t held,int32_t lx,int32_t ly,int32_t rx,int32_t ry,unsigned touches,uint64_t now) {
    if(a->player.owned) {
        bool owned=a->player.owned;macro_play_tick(&a->player,now);
        if(owned && !a->player.owned) a->input_locked=true;
    }
    if(macro_live_busy(&a->live)) {
        macro_live_tick(&a->live,connected,held,lx,ly,rx,ry,touches,now);
        if(a->live.state==ML_CLEANUP || a->live.state==ML_BLOCKED) a->replace_pending=false;
        if(!macro_live_busy(&a->live)) a->input_locked=true;
    }
    if(v->screen==SCREEN_MACRO_RECORD && !a->record_live) {
        bool recording=a->recorder.state==MR_ARMING || a->recorder.state==MR_RECORDING;
        SwitchGamepadReport report=controller_monitor_report(held,lx,ly,rx,ry,connected);
        macro_record_sample(&a->recorder,&report,connected,!held && !touches,now);
        if(recording && a->recorder.state!=MR_ARMING && a->recorder.state!=MR_RECORDING) a->input_locked=true;
        if(a->recorder.state==MR_LIMIT) snprintf(v->macro_status,sizeof(v->macro_status),"Recording limit reached; review retained input (1024 steps / 10 minutes max).");
        else if(a->recorder.state==MR_INTERRUPTED) snprintf(v->macro_status,sizeof(v->macro_status),"Recording interrupted. Review or discard retained input.");
    }
    macro_app_update(a,v);
}
Action macro_app_input(MacroApp *a,View *v,uint32_t down,uint32_t held,int32_t sx,int32_t sy,unsigned touches,int x,int y,uint64_t now) {
    if(!macro_app_screen(v->screen)) return ACTION_NONE;
    if(macro_live_busy(&a->live)) {
        /* Physical gameplay buttons never navigate, confirm replacement or exit. */
        Action hit=touches==1?view_touch(v,x,y):ACTION_NONE;
        if(!a->input_locked && (hit==(Action)ACTION_MS_RECORD_STOP || hit==(Action)ACTION_MS_RETRY ||
           hit==(Action)ACTION_MS_RECORD_START || hit==(Action)ACTION_MS_LIVE_EXIT ||
           hit==(Action)ACTION_MS_REPLACE_RECORD || hit==(Action)ACTION_MS_KEEP_RECORD)) {
            a->input_locked=true;return hit;
        }
        if(!touches) a->input_locked=false;
        return ACTION_NONE;
    }
    if(v->screen==SCREEN_MACRO_RECORD && (a->recorder.state==MR_ARMING || a->recorder.state==MR_RECORDING)) {
        /* All controller buttons belong to the recording, including +/- and B. */
        if(touches==1 && !a->input_locked && view_touch(v,x,y)==(Action)ACTION_MS_RECORD_STOP) {
            a->input_locked=true;return (Action)ACTION_MS_RECORD_STOP;
        }
        if(!touches && !held) a->input_locked=false;
        return ACTION_NONE;
    }
    if(a->input_locked) { if(!held && !touches && sx<12000 && sx> -12000 && sy<12000 && sy> -12000) a->input_locked=false;return ACTION_NONE; }
    Action out=ACTION_NONE;
    if(down&MON_B) out=a->player.owned?(Action)ACTION_MS_STOP:ACTION_BACK;
    else if(down&MON_A) out=view_macro_control(v,(unsigned)v->selected).action;
    else if(v->screen==SCREEN_MACRO_LIBRARY && (down&MON_X)) out=ACTION_MACRO_REFRESH;
    else if(v->screen==SCREEN_MACRO_LIBRARY && (down&MON_Y)) out=ACTION_MACRO_DELETE_ASK;
    else if(v->screen==SCREEN_MACRO_LIBRARY && (down&MON_L)) out=ACTION_MACRO_PREV;
    else if(v->screen==SCREEN_MACRO_LIBRARY && (down&MON_R)) out=ACTION_MACRO_NEXT;
    else if(touches==1) out=view_touch(v,x,y);
    if(out!=ACTION_NONE) { a->input_locked=true;return out; }
    if(touches) { a->input_locked=true;return ACTION_NONE; }
    int dx=0,dy=0;view_menu_stick_direction(sx,sy,&dx,&dy);
    if(live_nav_step(&a->navigation,dx,dy,now)) down|=dx<0?MON_LEFT:dx>0?MON_RIGHT:dy<0?MON_UP:dy>0?MON_DOWN:0;
    int stride=v->screen==SCREEN_MACRO_STEP?4:v->screen==SCREEN_MACRO_EDITOR?5:1;
    int delta=(down&MON_LEFT)?-1:(down&MON_RIGHT)?1:(down&MON_UP)?-stride:(down&MON_DOWN)?stride:0;
    size_t n=view_macro_control_count(v);
    if(n && delta) v->selected=(v->selected+delta+(int)n)%(int)n;
    return ACTION_NONE;
}
bool macro_app_action(MacroApp *a,View *v,Action action,uint64_t now,bool available) {
    if(!a || !v) return false;
    if(macro_live_busy(&a->live)) {
        if(action==ACTION_NONE) return true;
        a->input_locked=true;
        if((int)action==ACTION_MS_RECORD_STOP) macro_live_stop(&a->live,now);
        else if((int)action==ACTION_MS_LIVE_EXIT) { a->replace_pending=false;macro_live_exit(&a->live,now); }
        else if((int)action==ACTION_MS_KEEP_RECORD) a->replace_pending=false;
        else if((int)action==ACTION_MS_REPLACE_RECORD && a->replace_pending) {
            a->replace_pending=false;(void)macro_live_record_start(&a->live,true);
        }
        else if((int)action==ACTION_MS_RECORD_START && a->live.state==ML_READY && !a->replace_pending) {
            if(a->recorder.sequence.count) a->replace_pending=true;
            else (void)macro_live_record_start(&a->live,false);
        }
        else if((int)action==ACTION_MS_RETRY) (void)macro_live_retry_cleanup(&a->live);
        macro_app_update(a,v);return true;
    }
    if(action==ACTION_MACRO_STUDIO && v->screen==SCREEN_CONTROLLER_MENU) {
        v->macro_status[0]=0;enter(a,v,SCREEN_MACRO_STUDIO);macro_app_update(a,v);return true;
    }
    if(action==ACTION_MACRO_LIBRARY && v->screen==SCREEN_CONTROLLER_MENU) {
        refresh(a,v);enter(a,v,SCREEN_MACRO_LIBRARY);macro_app_update(a,v);return true;
    }
    if(!macro_app_screen(v->screen)) return false;
    if(action==ACTION_NONE) return true;
    a->input_locked=true;
    if(a->player.owned) {
        if((int)action==ACTION_MS_STOP) macro_play_cancel(&a->player);
        if((int)action==ACTION_MS_RETRY) macro_play_retry_cleanup(&a->player);
        macro_app_update(a,v);return true;
    }
    if(a->recorder.state==MR_ARMING || a->recorder.state==MR_RECORDING) {
        if((int)action==ACTION_MS_RECORD_STOP) macro_record_stop(&a->recorder,now);
        macro_app_update(a,v);return true;
    }
    if((action==ACTION_STUDIO_MANUAL || action==ACTION_STUDIO_RECORD || (int)action==ACTION_MS_LIVE_RECORD) &&
       (a->recorder.sequence.count || (a->editor.current.sequence.count && macro_editor_dirty(&a->editor)))) {
        a->pending_action=action;a->return_screen=v->screen;enter(a,v,SCREEN_MACRO_DISCARD);
        macro_app_update(a,v);return true;
    }
    v->macro_status[0]=0;
    if(action==ACTION_BACK) {
        if(v->screen==SCREEN_MACRO_STEP) enter(a,v,SCREEN_MACRO_EDITOR);
        else if(v->screen==SCREEN_MACRO_DISCARD) { a->pending_action=ACTION_NONE;enter(a,v,a->return_screen); }
        else if(v->screen==SCREEN_MACRO_EDITOR && macro_editor_dirty(&a->editor)) { a->return_screen=SCREEN_MACRO_EDITOR;enter(a,v,SCREEN_MACRO_DISCARD); }
        else if(v->screen==SCREEN_MACRO_EDITOR) enter(a,v,a->has_file?SCREEN_MACRO_DETAILS:SCREEN_MACRO_STUDIO);
        else if(v->screen==SCREEN_MACRO_PLAY) enter(a,v,a->return_screen);
        else if(v->screen==SCREEN_MACRO_DETAILS || v->screen==SCREEN_MACRO_DELETE) { refresh(a,v);enter(a,v,SCREEN_MACRO_LIBRARY); }
        else if(v->screen==SCREEN_MACRO_RECORD && a->recorder.sequence.count) { a->return_screen=SCREEN_MACRO_RECORD;enter(a,v,SCREEN_MACRO_DISCARD); }
        else if(v->screen==SCREEN_MACRO_LIBRARY) enter(a,v,SCREEN_CONTROLLER_MENU);
        else if(v->screen==SCREEN_MACRO_RECORD) enter(a,v,SCREEN_MACRO_STUDIO);
        else enter(a,v,SCREEN_CONTROLLER_MENU);
    } else if(action==ACTION_STUDIO_MANUAL) {
        new_document(a);enter(a,v,SCREEN_MACRO_EDITOR);
    } else if(action==ACTION_STUDIO_RECORD || (int)action==ACTION_MS_LIVE_RECORD) {
        bool live=(int)action==ACTION_MS_LIVE_RECORD;
        if(live && (!available || !macro_live_start(&a->live,&a->recorder))) {
            snprintf(v->macro_status,sizeof(v->macro_status),"Live Record needs ready USB and exclusive ownership. Nothing started.");
            macro_app_update(a,v);return true;
        }
        new_document(a);memset(&a->recorder,0,sizeof(a->recorder));
        if(!live) memset(&a->live,0,sizeof(a->live));
        a->record_live=live;a->replace_pending=false;a->dialog=MD_NONE;enter(a,v,SCREEN_MACRO_RECORD);
    } else if(action==ACTION_MACRO_REFRESH) refresh(a,v);
    else if(action==ACTION_MACRO_PREV || action==ACTION_MACRO_NEXT) {
        if(v->screen==SCREEN_MACRO_LIBRARY) {
            if(action==ACTION_MACRO_PREV && a->library_page) a->library_page--;
            if(action==ACTION_MACRO_NEXT && (a->library_page+1)*8<a->count) a->library_page++;
        } else {
            if(action==ACTION_MACRO_PREV && a->cursor>=3) a->cursor-=3;
            if(action==ACTION_MACRO_NEXT && a->cursor+3<a->editor.current.sequence.count) a->cursor+=3;
        }
        v->selected=0;
    } else if(action>=ACTION_ROW0 && action<ACTION_ROW0+8) {
        size_t row=(size_t)(action-ACTION_ROW0);
        if(v->screen==SCREEN_MACRO_LIBRARY && a->library_page*8+row<a->count) {
            if(loaded(a,v,a->names[a->library_page*8+row])) enter(a,v,SCREEN_MACRO_DETAILS);
        } else if(v->screen==SCREEN_MACRO_EDITOR && v->macro_page*3+row<a->editor.current.sequence.count) a->cursor=v->macro_page*3+row;
    } else if(action==ACTION_MACRO_DELETE_ASK) {
        if(v->screen==SCREEN_MACRO_LIBRARY && v->selected<v->file_rows) {
            snprintf(a->file_key,sizeof(a->file_key),"%s",a->names[a->library_page*8+(size_t)v->selected]);
            snprintf(v->name,sizeof(v->name),"%s",a->file_key);enter(a,v,SCREEN_MACRO_DELETE);
        } else if(v->screen==SCREEN_MACRO_DETAILS) { snprintf(v->name,sizeof(v->name),"%s",a->file_key);enter(a,v,SCREEN_MACRO_DELETE); }
    } else if(action==ACTION_MACRO_DELETE_CONFIRM && v->screen==SCREEN_MACRO_DELETE) {
        MacroStoreResult r=macro_store_delete(a->root,a->file_key);refresh(a,v);enter(a,v,SCREEN_MACRO_LIBRARY);result(v,r);
    } else switch((int)action) {
    case ACTION_MS_RECORD_START:
        if(v->screen!=SCREEN_MACRO_RECORD) break;
        if(a->record_live) break; /* Exited sessions cannot restart implicitly. */
        if(a->recorder.sequence.count) a->replace_pending=true;
        else if(available) macro_record_start(&a->recorder);
        else snprintf(v->macro_status,sizeof(v->macro_status),"Another input mode is active; recording not started.");
        break;
    case ACTION_MS_KEEP_RECORD: a->replace_pending=false;break;
    case ACTION_MS_REPLACE_RECORD:
        if(a->replace_pending && !a->record_live && available) {
            a->replace_pending=false;macro_record_start(&a->recorder);
        }
        break;
    case ACTION_MS_REVIEW: {
        MacroDocument d=a->editor.current;d.sequence=a->recorder.sequence;
        if(macro_editor_change(&a->editor,&d)) {
            memset(&a->recorder,0,sizeof(a->recorder));a->replace_pending=false;enter(a,v,SCREEN_MACRO_EDITOR);
        }
        break;
    }
    case ACTION_MS_PREV_STEP: if(a->cursor) a->cursor--;break;
    case ACTION_MS_NEXT_STEP: if(a->cursor+1<a->editor.current.sequence.count) a->cursor++;break;
    case ACTION_MS_EDIT:
        if(v->screen==SCREEN_MACRO_DETAILS) enter(a,v,SCREEN_MACRO_EDITOR);
        else if(a->editor.current.sequence.count) {
            a->draft=a->editor.current.sequence.steps[a->cursor];a->inserting=false;enter(a,v,SCREEN_MACRO_STEP);
        }
        break;
    case ACTION_MS_INSERT: case ACTION_MS_DELAY:
        a->draft=(MacroStep){neutral,500};a->inserting=true;enter(a,v,SCREEN_MACRO_STEP);break;
    case ACTION_MS_APPLY: {
        size_t index=a->inserting && a->editor.current.sequence.count?a->cursor+1:a->cursor;
        if(a->inserting?macro_editor_insert(&a->editor,index,&a->draft):macro_editor_replace(&a->editor,index,&a->draft)) {
            a->cursor=index;enter(a,v,SCREEN_MACRO_EDITOR);
        }
        else snprintf(v->macro_status,sizeof(v->macro_status),"Step rejected: 1024 steps, 1 hour/step, 24 hours/sequence maximum.");
        break;
    }
    case ACTION_MS_REMOVE: (void)macro_editor_remove(&a->editor,a->cursor);break;
    case ACTION_MS_MOVE_UP: if(a->cursor && macro_editor_move(&a->editor,a->cursor,a->cursor-1)) a->cursor--;break;
    case ACTION_MS_MOVE_DOWN: if(macro_editor_move(&a->editor,a->cursor,a->cursor+1)) a->cursor++;break;
    case ACTION_MS_UNDO: (void)macro_editor_undo(&a->editor);break;
    case ACTION_MS_REDO: (void)macro_editor_redo(&a->editor);break;
    case ACTION_MS_NEUTRAL: a->draft.report=neutral;break;
    case ACTION_MS_SAVE: dialog(a,MD_SAVE,"Save macro - name",a->editor.current.name);break;
    case ACTION_MS_RENAME: dialog(a,MD_RENAME,"Rename macro",a->editor.current.name);break;
    case ACTION_MS_DUPLICATE: dialog(a,MD_DUPLICATE,"Duplicate macro - new name","");break;
    case ACTION_MS_REPEATS: {
        char text[32];snprintf(text,sizeof(text),"%u",a->editor.current.repeat_count);
        dialog(a,MD_REPEATS,"Finite repeat count (1 - 1000000)",text);break;
    }
    case ACTION_MS_SPEED: {
        MacroDocument d=a->editor.current;
        d.speed_percent=d.speed_percent==25?50:d.speed_percent==50?100:d.speed_percent==100?150:d.speed_percent==150?200:25;
        (void)macro_editor_change(&a->editor,&d);break;
    }
    case ACTION_MS_PLAY: case ACTION_MS_PLAY_ONCE:
        if(available && ((int)action==ACTION_MS_PLAY_ONCE?macro_play_once(&a->player,&a->editor.current,now):macro_play_start(&a->player,&a->editor.current,now))) {
            a->return_screen=v->screen;enter(a,v,SCREEN_MACRO_PLAY);
        } else snprintf(v->macro_status,sizeof(v->macro_status),"Cannot play: require nonempty valid macro, finite repeats, ready USB and exclusive controller.");
        break;
    case ACTION_MS_DISCARD:
        if(v->screen==SCREEN_MACRO_RECORD && a->recorder.sequence.count) {
            a->return_screen=SCREEN_MACRO_RECORD;enter(a,v,SCREEN_MACRO_DISCARD);break;
        }
        if(v->screen==SCREEN_MACRO_DISCARD && a->pending_action!=ACTION_NONE) {
            Action next=a->pending_action;a->pending_action=ACTION_NONE;
            new_document(a);memset(&a->recorder,0,sizeof(a->recorder));
            enter(a,v,SCREEN_MACRO_STUDIO);
            return macro_app_action(a,v,next,now,available);
        }
        if(v->screen==SCREEN_MACRO_DISCARD && a->editor.has_saved) { MacroDocument d=a->editor.saved;macro_editor_open(&a->editor,&d,true); }
        else new_document(a);
        memset(&a->recorder,0,sizeof(a->recorder));enter(a,v,SCREEN_MACRO_STUDIO);break;
    default:
        if((int)action>=ACTION_MS_FIELD0 && (int)action<ACTION_MS_FIELD0+18) {
            unsigned field=(unsigned)((int)action-ACTION_MS_FIELD0);
            if(field<12) a->draft.report.buttons^=(uint16_t)(1u<<field);
            else if(field==12) a->draft.report.hat=(uint8_t)((a->draft.report.hat+1)%9);
            else {
                char text[32];a->axis=field-13;
                unsigned value=field==17?a->draft.duration_ms:((uint8_t *)&a->draft.report.lx)[a->axis];
                snprintf(text,sizeof(text),"%u",value);
                dialog(a,field==17?MD_DURATION:MD_AXIS,field==17?"Hold duration (1 - 3600000 ms)":"Stick value (0 - 255, neutral 128)",text);
            }
        }
        break;
    }
    macro_app_update(a,v);return true;
}
void macro_app_capture(MacroApp *a,View *v,uint64_t now) {
    if(!a || !v || v->screen!=SCREEN_MACRO_RECORD || !a->live.owned || a->replace_pending) return;
    if(a->live.state==ML_RECORDING)
        (void)macro_app_action(a,v,(Action)ACTION_MS_RECORD_STOP,now,true);
    else if(a->live.state==ML_READY)
        (void)macro_app_action(a,v,(Action)ACTION_MS_RECORD_START,now,true);
}
void macro_app_dialog_result(MacroApp *a,View *v,bool accepted,const char *text) {
    if(macro_app_busy(a)) { a->dialog=MD_NONE;return; }
    MacroDialog kind=a->dialog;a->dialog=MD_NONE;a->input_locked=true;
    if(!accepted || !text) { snprintf(v->macro_status,sizeof(v->macro_status),"Dialog cancelled; changes kept.");return; }
    if(kind==MD_SAVE || kind==MD_RENAME || kind==MD_DUPLICATE) {
        if(!macro_name_valid(text)) { result(v,MACRO_STORE_INVALID);return; }
        MacroStoreResult r;
        if(kind==MD_SAVE) {
            MacroDocument d=a->editor.current;strcpy(d.name,text);
            time_t stamp=time(NULL);d.modified_unix=stamp==(time_t)-1?0:(uint64_t)stamp;
            if(!d.created_unix) d.created_unix=d.modified_unix;
            if(!strcmp(a->root,MACRO_STORAGE_DIRECTORY)) r=macro_store_prepare_default();else r=MACRO_STORE_OK;
            bool same=a->has_file && !strcmp(a->file_key,d.name);
            if(r==MACRO_STORE_OK) r=macro_store_save(a->root,&d,same);
            if(r==MACRO_STORE_OK) { macro_editor_change(&a->editor,&d);macro_editor_mark_saved(&a->editor);a->has_file=true;strcpy(a->file_key,d.name); }
        } else if(kind==MD_RENAME) {
            r=macro_store_rename(a->root,a->file_key,text);if(r==MACRO_STORE_OK) (void)loaded(a,v,text);
        } else r=macro_store_duplicate(a->root,a->file_key,text);
        result(v,r);
    } else if(kind!=MD_NONE) {
        errno=0;char *end=NULL;unsigned long n=strtoul(text,&end,10);
        bool digits=*text!=0;for(const char *p=text;*p;p++) if(*p<'0' || *p>'9') digits=false;
        if(errno || !digits || !end || *end || (kind==MD_AXIS?n>255:n<1 || n>(kind==MD_REPEATS?1000000u:MACRO_MAX_STEP_MS))) {
            snprintf(v->macro_status,sizeof(v->macro_status),"Invalid numeric value; no changes applied.");return;
        }
        if(kind==MD_DURATION) a->draft.duration_ms=(uint32_t)n;
        else if(kind==MD_AXIS && a->axis<4) ((uint8_t *)&a->draft.report.lx)[a->axis]=(uint8_t)n;
        else if(kind==MD_REPEATS) { MacroDocument d=a->editor.current;d.repeat_count=(uint32_t)n;(void)macro_editor_change(&a->editor,&d); }
    }
    macro_app_update(a,v);
}
