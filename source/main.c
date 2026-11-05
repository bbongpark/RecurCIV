#include <switch.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "text.h"
#include "ui.h"
#include "view.h"
#include "usb_keyboard.h"
#include "usb_gamepad.h"
#include "gamepad_test.h"
#include "pace.h"
#include "typer.h"
#include "macro_storage.h"
#include "macro_app.h"
#include "controller_route.h"
#include "civ_usb.h"
#include "civ_route.h"

/* Fits hbmenu's applet budget; full application mode is recommended. */
u32 __nx_heap_size=48*1024*1024;
typedef struct { char name[768]; bool directory; } Entry;
static View view;
static Framebuffer framebuffer;
static PadState pad;
static Entry *entries;
static size_t entry_count,browser_index;
static char directory[1024]="sdmc:/";
static char *loaded_text;
static size_t loaded_length;
static TextPlan plan;
static Typer sender;
static Typer caps_toggle;
static const Stroke caps_key={0,0x39};
static uint32_t caps_failure_base;
static uint32_t failure_base;
static bool quit_requested,abort_error;
static bool compose_requested,compose_edit,compose_input_lock;
static Action live_after_exit;
static char abort_message[384];
static Screen previous_screen;
static ControllerRoute controller_route;
static CivRoute civ_route;
static MacroApp macro_app;
#define INPUT_MATCH(a,b) ((uint64_t)(a)==(uint64_t)(b))
_Static_assert(INPUT_MATCH(HidNpadButton_A,MON_A) && INPUT_MATCH(HidNpadButton_B,MON_B) && INPUT_MATCH(HidNpadButton_X,MON_X) && INPUT_MATCH(HidNpadButton_Y,MON_Y) &&
    INPUT_MATCH(HidNpadButton_StickL,MON_L3) && INPUT_MATCH(HidNpadButton_StickR,MON_R3) && INPUT_MATCH(HidNpadButton_L,MON_L) && INPUT_MATCH(HidNpadButton_R,MON_R) &&
    INPUT_MATCH(HidNpadButton_ZL,MON_ZL) && INPUT_MATCH(HidNpadButton_ZR,MON_ZR) && INPUT_MATCH(HidNpadButton_Plus,MON_PLUS) && INPUT_MATCH(HidNpadButton_Minus,MON_MINUS) &&
    INPUT_MATCH(HidNpadButton_Left,MON_LEFT) && INPUT_MATCH(HidNpadButton_Up,MON_UP) && INPUT_MATCH(HidNpadButton_Right,MON_RIGHT) && INPUT_MATCH(HidNpadButton_Down,MON_DOWN),
    "Controller monitor input bits must match libnx");
#undef INPUT_MATCH

static GamepadButtonTest gamepad_button_test;

/* Future CIV UI may call civ_usb_start explicitly. Never preempt another
   writer or open a modal applet while the two-interface lease is held. */
static bool civ_can_start(void) {
    return sender.phase==TX_NONE && caps_toggle.phase==TX_NONE && !compose_requested &&
        !compose_input_lock && !quit_requested && !gamepad_button_test.active &&
        view.screen!=SCREEN_LIVE && view.screen!=SCREEN_CONTROLLER && view.screen!=SCREEN_DIAGNOSTICS;
}

static void gamepad_test_state(uint16_t buttons) {
    SwitchGamepadReport report={0};
    report.buttons=buttons;
    report.hat=SWITCH_GAMEPAD_HAT_NONE;
    report.lx=report.ly=report.rx=report.ry=SWITCH_GAMEPAD_STICK_CENTER;
    gamepad_set_state(&report);
}

static void gamepad_test_release(const char *reason) {
    if(!gamepad_button_test.active) return;
    gamepad_button_test.active=false;
    gamepad_test_state(SWITCH_GAMEPAD_NEUTRAL);
    keyboard_log("Gamepad A test: release requested (%s); delivery not confirmed",reason);
}
static bool mkdir_failed;
static int mkdir_error_number;
static const char *mkdir_error_path;
static char manual_log_result[144]="No manual snapshot yet";

static bool prepare_log_directory(const char **failed_path,int *error_number) {
    const char *paths[]={"sdmc:/switch","sdmc:/switch/RecurCIV"};
    bool ok=true;
    for(size_t i=0;i<2;i++) {
        if(mkdir(paths[i],0777)!=0) {
            int e=errno;
            if(e!=EEXIST && ok) { *failed_path=paths[i];*error_number=e;ok=false; }
        }
    }
    return ok;
}

static uint64_t now_ms(void) { return armTicksToNs(armGetSystemTick())/1000000; }
static void save_settings(void) {
    FILE *file=fopen("sdmc:/switch/RecurCIV/settings.txt","w");
    if(file) { fprintf(file,"pace=%d\nenter_pause_ms=%d\ncancel_autoindent=%d\n",view.pace,view.enter_pause_ms,view.cancel_autoindent);fclose(file); }
}
static void load_settings(void) {
    FILE *file=fopen("sdmc:/switch/RecurCIV/settings.txt","r");
    if(!file) return;
    char line[80];int value;
    while(fgets(line,sizeof(line),file)) {
        if(sscanf(line,"pace=%d",&value)==1 && value>=0 && value<PACE_COUNT) view.pace=value;
        if(sscanf(line,"enter_pause_ms=%d",&value)==1 && enter_pause_valid(value)) view.enter_pause_ms=value;
        if(sscanf(line,"cancel_autoindent=%d",&value)==1) view.cancel_autoindent=value!=0;
    }
    fclose(file);
}
static void message(bool error,const char *text) {
    view.error=error;snprintf(view.message,sizeof(view.message),"%s",text);view.screen=SCREEN_RESULT;
}
static void macro_update_view(void) {
    macro_app_update(&macro_app,&view);
}
static int compare_entries(const void *a,const void *b) {
    const Entry *left=a,*right=b;
    if(strcmp(left->name,"..")==0) return -1;
    if(strcmp(right->name,"..")==0) return 1;
    if(left->directory!=right->directory) return left->directory?-1:1;
    return strcasecmp(left->name,right->name);
}
static bool join_path(char *out,size_t capacity,const char *base,const char *name) {
    size_t len=strlen(base);int n=snprintf(out,capacity,"%s%s%s",base,len&&base[len-1]=='/'?"":"/",name);
    return n>=0 && (size_t)n<capacity;
}
static bool browser_load(void) {
    free(entries);entries=NULL;entry_count=0;browser_index=0;
    DIR *dir=opendir(directory);
    if(!dir) { message(true,"Could not open the SD card folder.");return false; }
    size_t capacity=128;entries=calloc(capacity,sizeof(Entry));
    if(!entries) { closedir(dir);message(true,"Not enough memory to list this folder.");return false; }
    if(strcmp(directory,"sdmc:/")!=0) { strcpy(entries[0].name,"..");entries[0].directory=true;entry_count=1; }
    struct dirent *ent;
    while((ent=readdir(dir))!=NULL) {
        if(strcmp(ent->d_name,".")==0 || strcmp(ent->d_name,"..")==0) continue;
        char full[1024];struct stat st;
        if(strlen(ent->d_name)>=sizeof(entries[0].name) || !join_path(full,sizeof(full),directory,ent->d_name)) continue;
        if(stat(full,&st)!=0) continue;
        bool isdir=S_ISDIR(st.st_mode);const char *extension=strrchr(ent->d_name,'.');
        if(!isdir && (!S_ISREG(st.st_mode) || !extension || strcasecmp(extension,".txt")!=0)) continue;
        if(entry_count==8192) { keyboard_log("Browser limited to 8192 entries");break; }
        if(entry_count==capacity) {
            size_t next=capacity*2;Entry *expanded=realloc(entries,next*sizeof(Entry));
            if(!expanded) { closedir(dir);message(true,"Not enough memory to list this folder.");return false; }
            entries=expanded;capacity=next;
        }
        snprintf(entries[entry_count].name,sizeof(entries[entry_count].name),"%s",ent->d_name);
        entries[entry_count++].directory=isdir;
    }
    closedir(dir);qsort(entries,entry_count,sizeof(Entry),compare_entries);
    view.screen=SCREEN_FILES;return true;
}
static void browser_parent(void) {
    if(strcmp(directory,"sdmc:/")==0) { view.screen=SCREEN_KEYBOARD;view.selected=2;return; }
    char *slash=strrchr(directory,'/');
    if(slash && slash>directory+5) *slash=0;else strcpy(directory,"sdmc:/");
    browser_load();
}
static bool prepare_text(void) {
    text_plan_free(&plan);TextError error;
    view.compose_valid=false;view.compose_error[0]=0;
    if(!text_plan(loaded_text,loaded_length,view.cancel_autoindent,&plan,&error)) {
        char msg[384];
        if(error.invalid_utf8) snprintf(msg,sizeof(msg),"Invalid UTF-8 at byte %zu. Save the file as UTF-8 and retry. Nothing was sent.",error.byte_offset+1);
        else if(error.character) snprintf(msg,sizeof(msg),"Unsupported character U+%04lX in QWERTY US at position %zu. Nothing was sent.",
            (unsigned long)error.codepoint,error.character);
        else snprintf(msg,sizeof(msg),"Could not prepare the text in memory. Nothing was sent.");
        if(view.composing) { snprintf(view.compose_error,sizeof(view.compose_error),"%s",msg);view.screen=SCREEN_CONFIRM; }
        else message(true,msg);
        return false;
    }
    if(!plan.count) { text_plan_free(&plan);message(true,"The text is empty.");return false; }
    view.compose_valid=true;
    view.characters=plan.characters;view.lines=plan.lines;view.tabs=plan.tabs;
    size_t first=loaded_length>=3 && memcmp(loaded_text,"\xef\xbb\xbf",3)==0?3:0;
    size_t n=loaded_length-first;if(n>sizeof(view.preview)-1) n=sizeof(view.preview)-1;
    /* Do not cut a UTF-8 sequence at the end of the preview. */
    while(n && first+n<loaded_length && ((uint8_t)loaded_text[first+n]&0xc0)==0x80) n--;
    memcpy(view.preview,loaded_text+first,n);view.preview[n]=0;view.screen=SCREEN_CONFIRM;view.selected=0;
    return true;
}
static void compose_open(void) {
    SwkbdConfig config;char output[2049]={0};
    Result rc=swkbdCreate(&config,0);
    if(R_FAILED(rc)) { snprintf(view.compose_error,sizeof(view.compose_error),"Software keyboard unavailable (0x%08lX).",(unsigned long)rc);return; }
    swkbdConfigMakePresetDefault(&config);
    swkbdConfigSetHeaderText(&config,"Compose text");
    swkbdConfigSetSubText(&config,"Review and confirm before sending.");
    swkbdConfigSetOkButtonText(&config,"Review");
    swkbdConfigSetStringLenMax(&config,128);
    if(compose_edit && loaded_text) swkbdConfigSetInitialText(&config,loaded_text);
    rc=swkbdShow(&config,output,sizeof(output));swkbdClose(&config);
    /* The modal applet paused our USB pump; refresh before allowing a send. */
    keyboard_poll(now_ms());gamepad_poll(now_ms());
    size_t length;ComposeResult result=compose_validate(R_SUCCEEDED(rc),output,sizeof(output),&length);
    if(result==COMPOSE_CANCELLED) {
        snprintf(view.compose_error,sizeof(view.compose_error),"%s",loaded_length?
            "Keyboard cancelled or unavailable; previous text kept.":"Keyboard cancelled or unavailable. Choose EDIT to enter text.");
        return;
    }
    if(result!=COMPOSE_ACCEPTED) {
        snprintf(view.compose_error,sizeof(view.compose_error),"%s",result==COMPOSE_EMPTY?"Empty text rejected; previous text kept.":"Text exceeds 512 bytes; previous text kept.");
        return;
    }
    char *copy=malloc(length+1);
    if(!copy) { snprintf(view.compose_error,sizeof(view.compose_error),"Not enough memory; nothing was sent.");return; }
    memcpy(copy,output,length+1);free(loaded_text);loaded_text=copy;loaded_length=length;
    view.composing=true;view.compose_page=0;strcpy(view.name,"Compose & Send");
    view.compose_pages=compose_page(loaded_text,loaded_length,view.compose_page,view.compose_rows);
    prepare_text();
}
static void open_file(const char *path,const char *name) {
    FILE *file=fopen(path,"rb");
    if(!file) { message(true,"Could not open the file.");return; }
    if(fseek(file,0,SEEK_END)!=0) { fclose(file);message(true,"Could not determine the file size.");return; }
    long length=ftell(file);
    if(length<0 || length>262144) { fclose(file);message(true,"Unsupported file size. Maximum: 256 KiB.");return; }
    rewind(file);char *data=malloc((size_t)length+1);
    if(!data) { fclose(file);message(true,"Not enough memory to load the file.");return; }
    size_t n=fread(data,1,(size_t)length,file);bool read_error=ferror(file)!=0;fclose(file);
    if(read_error || n!=(size_t)length) { free(data);message(true,"Could not read the file.");return; }
    data[n]=0;free(loaded_text);loaded_text=data;loaded_length=n;view.composing=false;
    snprintf(view.name,sizeof(view.name),"%.255s",name);prepare_text();
}
static void browser_open(void) {
    if(browser_index>=entry_count) return;
    const Entry *entry=&entries[browser_index];
    if(strcmp(entry->name,"..")==0) { browser_parent();return; }
    char full[1024];
    if(!join_path(full,sizeof(full),directory,entry->name)) { message(true,"The file path is too long.");return; }
    if(entry->directory) { snprintf(directory,sizeof(directory),"%s",full);browser_load(); }
    else open_file(full,entry->name);
}
static void abort_send(bool error,const char *reason,uint64_t now) {
    snprintf(abort_message,sizeof(abort_message),"%s",reason);abort_error=error;
    keyboard_log("SEND ABORT after %zu/%zu strokes: %s",sender.index,plan.count,reason);
    if(sender.phase!=TX_COUNTDOWN && keyboard_status()->ready) {
        keyboard_cancel_transfer();
        if(R_SUCCEEDED(keyboard_send(0,0,now))) { sender.phase=TX_ABORT;return; }
    }
    sender.phase=TX_NONE;appletSetMediaPlaybackState(false);message(error,reason);
}
static void caps_toggle_failed(uint64_t now) {
    keyboard_log("CAPS LOCK toggle failed");
    keyboard_cancel_transfer();
    if(keyboard_status()->ready && R_SUCCEEDED(keyboard_send(0,0,now))) {
        caps_toggle.phase=TX_ABORT;
    } else {
        caps_toggle.phase=TX_NONE;
        message(true,"Could not toggle Caps Lock. Check USB diagnostics with -.");
    }
}
static void caps_toggle_tick(uint64_t now) {
    const KeyboardStatus *usb=keyboard_status();
    if(caps_toggle.phase==TX_NONE) return;
    if(caps_toggle.phase==TX_ABORT) {
        if(!usb->transfer_pending || !usb->ready) {
            caps_toggle.phase=TX_NONE;
            message(true,"Could not toggle Caps Lock. Check USB diagnostics with -.");
        }
        return;
    }
    if(!usb->ready || usb->reports_failed!=caps_failure_base) { caps_toggle_failed(now);return; }
    Stroke next;
    if(typer_step(&caps_toggle,now,usb->transfer_pending,&next)) {
        if(R_FAILED(keyboard_send(next.modifier,next.key,now))) { caps_toggle_failed(now);return; }
    }
    if(caps_toggle.phase==TX_DONE) {
        caps_toggle.phase=TX_NONE;
        keyboard_log("CAPS LOCK key sent; host LEDs=%02x known=%u",usb->leds,usb->leds_known);
    }
}
static void begin_caps_toggle(uint64_t now) {
    const KeyboardStatus *usb=keyboard_status();
    if(sender.phase!=TX_NONE || caps_toggle.phase!=TX_NONE) return;
    if(!usb->ready) { message(true,"USB keyboard not ready. Check the connection or press - for diagnostics.");return; }
    keyboard_cancel_transfer();caps_failure_base=usb->reports_failed;
    typer_start(&caps_toggle,&caps_key,1,20,now);
    keyboard_log("CAPS LOCK toggle requested");
}
static void begin_send(uint64_t now) {
    const KeyboardStatus *usb=keyboard_status();
    if(caps_toggle.phase!=TX_NONE) return;
    if(!usb->ready) { message(true,"USB keyboard not ready. Check the connection or press - for diagnostics.");return; }
    if(!plan.count) return;
    keyboard_cancel_transfer();failure_base=usb->reports_failed;
    typer_start(&sender,plan.strokes,plan.count,pace_delay(view.pace),now+3000);view.screen=SCREEN_TRANSFER;
    sender.enter_pause_ms=(unsigned)view.enter_pause_ms;
    view.total=plan.count;view.done=0;view.sending=false;view.seconds=3;
    appletSetMediaPlaybackState(true);
    keyboard_log("SEND queued layout=US chars=%zu strokes=%zu lines=%zu tabs=%zu pace=%d delay_ms=%u enter_pause_ms=%u autoindent=%d",
        plan.characters,plan.count,plan.lines,plan.tabs,view.pace,pace_delay(view.pace),sender.enter_pause_ms,view.cancel_autoindent);
}
static void sending_tick(uint64_t now) {
    const KeyboardStatus *usb=keyboard_status();
    if(sender.phase==TX_NONE) return;
    if(sender.phase==TX_ABORT) {
        if(!usb->transfer_pending || !usb->ready) { sender.phase=TX_NONE;appletSetMediaPlaybackState(false);message(abort_error,abort_message); }
        return;
    }
    if(!usb->ready) { abort_send(true,"USB disconnected. Sending stopped.",now);return; }
    if(usb->reports_failed!=failure_base) { abort_send(true,"USB transfer failed. Sending stopped. Press - for diagnostics.",now);return; }
    if(sender.phase==TX_COUNTDOWN) view.seconds=now<sender.due?(int)((sender.due-now+999)/1000):0;
    Stroke next;
    if(typer_step(&sender,now,usb->transfer_pending,&next)) {
        if(R_FAILED(keyboard_send(next.modifier,next.key,now))) { abort_send(true,"Could not send a keyboard report.",now);return; }
    }
    view.sending=sender.phase!=TX_COUNTDOWN;view.done=sender.index;
    if(sender.phase==TX_DONE) {
        sender.phase=TX_NONE;appletSetMediaPlaybackState(false);
        keyboard_log("SEND COMPLETE %zu strokes, %zu characters",plan.count,plan.characters);
        message(false,"USB transfer complete. Check the text on the target device.");
    }
}
static void update_view(void) {
    civ_route_update(&civ_route,&view,view.now);
    macro_update_view();
    const KeyboardStatus *usb=keyboard_status();
    const KeyboardLogError *log_error=keyboard_log_error();
    view.usb_initialized=usb->initialized;view.usb_ready=usb->ready;
    view.caps_state=usb->leds_known?((usb->leds&2)?1:0):-1;
    view.caps_pending=caps_toggle.phase!=TX_NONE;
    const char *states[]={"USB disconnected","USB attached","USB powered","USB connecting...","USB addressing...","USB keyboard connected","USB suspended"};
    snprintf(view.usb_label,sizeof(view.usb_label),"%s",usb->initialized?states[usb->state<=6?usb->state:0]:"USB unavailable");
    if(usb->initialized && usb->state==UsbState_Configured && !usb->ready)
        snprintf(view.usb_label,sizeof(view.usb_label),"Checking USB keyboard...");
    if(mkdir_failed || log_error->failed)
        snprintf(view.usb_label,sizeof(view.usb_label),"Log error: see diagnostics");
    if(view.screen==SCREEN_FILES) {
        view.file_start=browser_index/8*8;view.selected=(int)(browser_index%8);view.file_count=entry_count;
        snprintf(view.path,sizeof(view.path),"%s",directory);view.file_rows=0;
        for(size_t i=view.file_start;i<entry_count && i<view.file_start+8;i++) {
            size_t row=i-view.file_start;
            snprintf(view.files[row],sizeof(view.files[row]),"%.255s",entries[i].name);
            view.directories[row]=entries[i].directory;view.file_rows++;
        }
    }
    if(view.screen==SCREEN_DIAGNOSTICS) {
        view.error=R_FAILED(usb->error);
        snprintf(view.diagnostics[0],144,"Last error: 0x%08lX  /  %s",(unsigned long)usb->error,usb->stage);
        snprintf(view.diagnostics[1],144,"USB configured: %s  /  keyboard ready: %s  /  state: %u  /  resets: %lu",usb->state==UsbState_Configured?"yes":"no",usb->ready?"yes":"no",usb->state,(unsigned long)usb->bus_resets);
        snprintf(view.diagnostics[2],144,"Requests: %lu  /  descriptor requests: %lu  /  acknowledged: %s",(unsigned long)usb->setup_count,(unsigned long)usb->report_requests,usb->descriptor_valid?"yes":"no");
        snprintf(view.diagnostics[3],144,"Last request: type %02X  /  request %02X  /  value %04X",usb->last_request_type,usb->last_request,usb->last_value);
        snprintf(view.diagnostics[4],144,"Input reports completed: %lu  /  errors: %lu  /  pending: %s",(unsigned long)usb->reports_completed,(unsigned long)usb->reports_failed,usb->transfer_pending?"yes":"no");
        snprintf(view.diagnostics[5],144,"Control IN/OUT: %lu/%lu  /  protocol: %s  /  idle: %u ms  /  LEDs: %02X",(unsigned long)usb->control_in_completed,(unsigned long)usb->control_out_completed,usb->protocol?"Report":"Boot",usb->idle*4,usb->leds);
        snprintf(view.diagnostics[6],144,"Layout: QWERTY US  /  delay: %u ms  /  Enter pause: %d ms",pace_delay(view.pace),view.enter_pause_ms);
        snprintf(view.diagnostics[7],144,"Horizon OS: %u.%u.%u  /  mode: %s",HOSVER_MAJOR(hosversionGet()),HOSVER_MINOR(hosversionGet()),HOSVER_MICRO(hosversionGet()),appletGetAppletType()==AppletType_LibraryApplet?"applet":"application");
        snprintf(view.diagnostics[8],144,"Gamepad interface 0 / EP 81+01  /  keyboard interface 1 / EP 82  /  8-byte reports");
        if(mkdir_failed) snprintf(view.diagnostics[9],144,"Startup mkdir %s: errno=%d",mkdir_error_path,mkdir_error_number);
        else snprintf(view.diagnostics[9],144,"Startup directory: no recorded error");
        if(log_error->failed) snprintf(view.diagnostics[10],144,"Automatic log %s: errno=%d",log_error->operation,log_error->error_number);
        else snprintf(view.diagnostics[10],144,"Automatic log: no recorded error");
        snprintf(view.diagnostics[11],144,"%s",manual_log_result);
    }
}
static void perform(Action action,uint64_t now) {
    if(action==ACTION_NONE) return;
    if(civ_route_action(&civ_route,&view,action,now,civ_can_start())) return;
    if(civ_usb_owned()) return;
    if(view.screen==SCREEN_KEYBOARD && (action==ACTION_PACE || action==ACTION_CAPS_LOCK || action==ACTION_QUIT)) return;
    if(view.screen==SCREEN_CONFIRM && view.composing && action==ACTION_DIAGNOSTICS) return;
    if(view.screen==SCREEN_LIVE) {
        if(action==ACTION_BACK || action==ACTION_QUIT || action==ACTION_DIAGNOSTICS) {
            if(!view.live.exiting) { live_after_exit=action;live_exit(&view.live); }
            return;
        }
        if(action!=ACTION_PACE) return;
    }
    if(caps_toggle.phase!=TX_NONE) {
        if(action==ACTION_QUIT) quit_requested=true;
        return;
    }
    if(sender.phase!=TX_NONE) {
        if(action==ACTION_BACK || action==ACTION_ABORT || action==ACTION_QUIT) {
            if(action==ACTION_QUIT) quit_requested=true;
            if(sender.phase!=TX_ABORT) abort_send(false,"Sending cancelled.",now);
        }
        return;
    }
    if((action==ACTION_MACRO_STUDIO || action==ACTION_MACRO_LIBRARY) && gamepad_button_test.active) {
        snprintf(view.macro_status,sizeof(view.macro_status),"Wait for the diagnostic controller test to finish.");return;
    }
    bool macro_available=!compose_requested && !gamepad_button_test.active && !keyboard_status()->transfer_pending;
    if(macro_app_action(&macro_app,&view,action,now,macro_available)) return;
    if(controller_route_action(&controller_route,&view,action)) return;
    switch(action) {
        case ACTION_QUIT: quit_requested=true;break;
        case ACTION_PACE: case ACTION_ENTER_PAUSE:
            if(view_adjust_typing_setting(&view,action)) save_settings();
            break;
        case ACTION_AUTOINDENT:
            if(!view_adjust_typing_setting(&view,action)) break;
            save_settings();
            if(view.screen==SCREEN_CONFIRM && (!view.composing || loaded_length)) {
                int selected=view.selected;prepare_text();
                if(view.screen==SCREEN_CONFIRM && view.composing) view.selected=selected;
            }
            break;
        case ACTION_CAPS_LOCK: begin_caps_toggle(now);break;
        case ACTION_DIAGNOSTICS:
            previous_screen=view.screen;view.screen=SCREEN_DIAGNOSTICS;break;
        case ACTION_LIVE:
            if(view.screen==SCREEN_KEYBOARD) {
                live_after_exit=ACTION_MENU_KEYBOARD;
                live_init(&view.live,keyboard_status()->reports_failed);
                view.screen=SCREEN_LIVE;
            }
            break;
        case ACTION_COMPOSE:
            if(view.screen==SCREEN_KEYBOARD || (view.screen==SCREEN_CONFIRM && view.composing)) {
                compose_edit=view.composing;
                if(!view.composing) {
                    text_plan_free(&plan);free(loaded_text);loaded_text=NULL;loaded_length=0;
                    view.compose_valid=false;view.compose_page=0;view.compose_pages=1;
                    view.characters=view.lines=view.tabs=0;memset(view.compose_rows,0,sizeof(view.compose_rows));
                }
                view.composing=true;view.screen=SCREEN_CONFIRM;view.selected=1;
                compose_requested=true;view.compose_error[0]=0;
            }
            break;
        case ACTION_COMPOSE_PREV:
            if(view.screen==SCREEN_CONFIRM && view.composing && view.compose_page) view.compose_page--;
            break;
        case ACTION_COMPOSE_NEXT:
            if(view.screen==SCREEN_CONFIRM && view.composing && view.compose_page+1<view.compose_pages) view.compose_page++;
            break;
        case ACTION_FILES: browser_load();break;
        case ACTION_BACK:
            if(view.screen==SCREEN_MACRO_DETAILS || view.screen==SCREEN_MACRO_DELETE) view.screen=SCREEN_MACRO_LIBRARY;
            else if(view.screen==SCREEN_FILES) browser_parent();
            else if(view.screen==SCREEN_DIAGNOSTICS) { if(previous_screen==SCREEN_CONTROLLER) gamepad_test_release("return to controller");view.screen=previous_screen; }
            else if(view.screen==SCREEN_CONFIRM && view.composing) { view.screen=SCREEN_KEYBOARD;view.selected=0;view.compose_error[0]=0; }
            else { view.screen=SCREEN_HOME;view.selected=0; }
            break;
        case ACTION_SEND:
            if(view.screen==SCREEN_CONFIRM) begin_send(now);
            else if(view.screen==SCREEN_DIAGNOSTICS) { gamepad_test_release("USB reset");gamepad_exit();keyboard_exit();keyboard_init();gamepad_init(); }
            break;
        case ACTION_TEST_GAMEPAD_A:
            if(view.screen==SCREEN_DIAGNOSTICS && !gamepad_button_test.active) {
                if(!gamepad_ready()) keyboard_log("Gamepad A test: not ready; no press requested");
                else {
                    gamepad_button_test_start(&gamepad_button_test,now);
                    gamepad_test_state(SWITCH_GAMEPAD_A);
                    keyboard_log("Gamepad A test: press requested buttons=0004 for 500 ms; delivery not confirmed");
                }
            }
            break;
        case ACTION_CREATE_USB_LOG:
            if(view.screen==SCREEN_DIAGNOSTICS) {
                const char *path=NULL;int error_number=0;
                KeyboardLogError result={0};
                if(!prepare_log_directory(&path,&error_number))
                    snprintf(manual_log_result,sizeof(manual_log_result),"Manual mkdir %s: errno=%d",path,error_number);
                else if(!keyboard_create_log_snapshot(now,&result))
                    snprintf(manual_log_result,sizeof(manual_log_result),"Manual log %s: errno=%d",result.operation,result.error_number);
                else snprintf(manual_log_result,sizeof(manual_log_result),"Log snapshot written: /switch/RecurCIV/usb.log");
            }
            break;
        default:
            if(action>=ACTION_ROW0 && action<ACTION_ROW0+8 && view.screen==SCREEN_FILES) {
                browser_index=browser_index/8*8+(size_t)(action-ACTION_ROW0);browser_open();
            }
            break;
    }
}
int main(int argc,char **argv) {
    (void)argc;(void)argv;
    mkdir_failed=!prepare_log_directory(&mkdir_error_path,&mkdir_error_number);
    memset(&view,0,sizeof(view));view.enter_pause_ms=200;load_settings();
    macro_app_init(&macro_app,MACRO_STORAGE_DIRECTORY);
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);hidInitializeTouchScreen();
    Result rc=framebufferCreate(&framebuffer,nwindowGetDefault(),1280,720,PIXEL_FORMAT_RGBA_8888,2);
    if(R_SUCCEEDED(rc)) rc=framebufferMakeLinear(&framebuffer);
    if(R_FAILED(rc)) { framebufferClose(&framebuffer);return 1; }
    if(!ui_init()) { framebufferClose(&framebuffer);return 2; }
    keyboard_init();gamepad_init();civ_usb_init(civ_can_start);
    bool was_touching=false;uint64_t repeat_at=0,next_render=0;LiveNav menu_stick={0},library_stick={0};
    int touch_start_x=0,touch_start_y=0,touch_last_y=0;bool library_touch=false;
    while(appletMainLoop()) {
        uint64_t now=now_ms();view.now=now;keyboard_poll(now);gamepad_poll(now);civ_usb_tick(now);
        civ_route_update(&civ_route,&view,now);
        if(!civ_usb_owned() && view.screen!=SCREEN_CIV_PLACEHOLDER) {
        if(macro_app_screen(view.screen) || macro_app_busy(&macro_app)) {
            padUpdate(&pad);HidTouchScreenState touch={0};hidGetTouchScreenStates(&touch,1);
            HidAnalogStickState left=padGetStickPos(&pad,0),right=padGetStickPos(&pad,1);
            uint32_t held=(uint32_t)padGetButtons(&pad);
            macro_app_tick(&macro_app,&view,padIsConnected(&pad),held,left.x,left.y,right.x,right.y,touch.count,now);
            Action action=macro_app_input(&macro_app,&view,(uint32_t)padGetButtonsDown(&pad),held,left.x,left.y,touch.count,
                touch.count?(int)touch.touches[0].x:0,touch.count?(int)touch.touches[0].y:0,now);
            perform(action,now);
            if(macro_app.dialog!=MD_NONE && !macro_app_busy(&macro_app)) {
                SwkbdConfig config;char text[MACRO_NAME_BYTES]={0};Result rc=swkbdCreate(&config,0);
                if(R_SUCCEEDED(rc)) {
                    swkbdConfigMakePresetDefault(&config);swkbdConfigSetHeaderText(&config,macro_app.dialog_title);
                    swkbdConfigSetStringLenMax(&config,MACRO_NAME_BYTES-1);swkbdConfigSetInitialText(&config,macro_app.dialog_initial);
                    rc=swkbdShow(&config,text,sizeof(text));swkbdClose(&config);
                }
                macro_app_dialog_result(&macro_app,&view,R_SUCCEEDED(rc),text);
                keyboard_poll(now_ms());gamepad_poll(now_ms());
            }
            was_touching=touch.count>0;library_touch=false;
            compose_input_lock=!macro_app_screen(view.screen);
            repeat_at=now+350;menu_stick=(LiveNav){0};library_stick=(LiveNav){0};update_view();
        } else {
        caps_toggle_tick(now);sending_tick(now);
        if(view.screen==SCREEN_LIVE) {
            const KeyboardStatus *usb=keyboard_status();Stroke report;
            if(live_step(&view.live,usb->ready,usb->transfer_pending,usb->reports_failed,now,&report))
                live_submitted(&view.live,R_SUCCEEDED(keyboard_send(report.modifier,report.key,now)));
            if(view.live.phase==LIVE_DONE) {
                Action next=live_after_exit;view.screen=SCREEN_KEYBOARD;view.selected=1;
                if(next==ACTION_QUIT || next==ACTION_DIAGNOSTICS) perform(next,now);
            }
        }
        if(compose_requested && !keyboard_status()->transfer_pending && sender.phase==TX_NONE && caps_toggle.phase==TX_NONE) {
            compose_requested=false;compose_open();compose_input_lock=true;continue;
        }
        if(gamepad_button_test.active) {
            if(!gamepad_ready()) gamepad_test_release("USB not ready");
            else if(gamepad_button_test_expired(&gamepad_button_test,now)) gamepad_test_release("500 ms elapsed");
            else gamepad_test_state(SWITCH_GAMEPAD_A);
        }
        if(quit_requested && sender.phase==TX_NONE && caps_toggle.phase==TX_NONE) break;
        padUpdate(&pad);uint64_t down=padGetButtonsDown(&pad),held=padGetButtons(&pad);
        Screen sampled_screen=view.screen;
        if(compose_input_lock) down=0;
        bool monitor_exit=false,controller_transitioned=false;
        if(view.screen==SCREEN_CONTROLLER) {
            bool connected=padIsConnected(&pad);
            HidAnalogStickState left=padGetStickPos(&pad,0),right=padGetStickPos(&pad,1);
            bool exited=controller_route_tick(&controller_route,&view,connected,(uint32_t)held,left.x,left.y,right.x,right.y);
            if(exited) {
                controller_transitioned=true;
                if(view.screen==SCREEN_DIAGNOSTICS) previous_screen=SCREEN_CONTROLLER_MENU;
            }
            monitor_exit=controller_monitor_exit(&view.monitor,(uint32_t)held,connected,now);
        } else (void)controller_route_tick(&controller_route,&view,false,(uint32_t)held,0,0,0,0);
        if(view.screen==SCREEN_HOME || view.screen==SCREEN_KEYBOARD || view.screen==SCREEN_CONTROLLER_MENU || view.screen==SCREEN_MACRO_STUDIO || view.screen==SCREEN_FILES || (view.screen==SCREEN_CONFIRM && view.composing)) {
            HidAnalogStickState stick=padGetStickPos(&pad,0);int dx=0,dy=0;
            view_menu_stick_direction(stick.x,stick.y,&dx,&dy);
            if(live_nav_step(&menu_stick,dx,dy,now)) {
                if(dx<0) down|=HidNpadButton_Left;else if(dx>0) down|=HidNpadButton_Right;
                if(dy<0) down|=HidNpadButton_Up;else if(dy>0) down|=HidNpadButton_Down;
            }
        } else (void)live_nav_step(&menu_stick,0,0,now);
        uint64_t navigation=HidNpadButton_Up|HidNpadButton_Down;
        if(view.screen==SCREEN_HOME || (view.screen==SCREEN_CONFIRM && view.composing)) navigation|=HidNpadButton_Left|HidNpadButton_Right;
        if(down&navigation) repeat_at=now+350;
        else if(!compose_input_lock && (held&navigation) && now>=repeat_at) { down|=held&navigation;repeat_at=now+100; }
        if(controller_transitioned) down=0; /* also discard synthesized repeat edges */
        if(sender.phase==TX_NONE && caps_toggle.phase==TX_NONE) {
            controller_route_navigate(&view,(uint32_t)down);
            if(view.screen==SCREEN_FILES && entry_count) {
                if(down&HidNpadButton_Down) browser_index=(browser_index+1)%entry_count;
                if(down&HidNpadButton_Up) browser_index=browser_index?browser_index-1:entry_count-1;
                if(down&HidNpadButton_R) { browser_index+=8;if(browser_index>=entry_count) browser_index=entry_count-1; }
                if(down&HidNpadButton_L) browser_index=browser_index>=8?browser_index-8:0;
            }
        }
        if(view.screen==SCREEN_LIVE) {
            /* Live owns A/B/X/Y/+ while it is open. Directions are held so the
               Live repeat timer can pace them; actions are fresh press edges,
               which prevents held A or X from repeating. */
            HidAnalogStickState stick=padGetStickPos(&pad,0);
            LiveControls controls={
                .up=!!(held&HidNpadButton_Up),.down=!!(held&HidNpadButton_Down),
                .left=!!(held&HidNpadButton_Left),.right=!!(held&HidNpadButton_Right),
                .a=!!(down&HidNpadButton_A),.b=!!(down&HidNpadButton_B),
                .x=!!(down&HidNpadButton_X),.y=!!(down&HidNpadButton_Y),
                .plus=!!(down&HidNpadButton_Plus),.stick_x=stick.x,.stick_y=stick.y
            };
            if(live_controls(&view.live,&controls,now)==LIVE_INPUT_DONE && !view.live.exiting) {
                live_after_exit=ACTION_MENU_KEYBOARD;
                live_exit(&view.live);
            }
        }
        Action action=ACTION_NONE;
        if(controller_transitioned) { /* old-screen edges are discarded, including shortcut exit */ }
        else if(view.screen==SCREEN_LIVE) { /* handled above; keep + from reaching global exit */ }
        else if(view.screen==SCREEN_MACRO_LIBRARY || view.screen==SCREEN_MACRO_DETAILS || view.screen==SCREEN_MACRO_DELETE) {
            HidAnalogStickState stick=padGetStickPos(&pad,0);
            int dx=0,dy=0;view_menu_stick_direction(stick.x,stick.y,&dx,&dy);
            if(live_nav_step(&library_stick,dx,dy,now)) {
                if(view.screen==SCREEN_MACRO_LIBRARY && dy) down|=dy<0?HidNpadButton_Up:HidNpadButton_Down;
                else if(view.screen==SCREEN_MACRO_DETAILS && dx) down|=dx<0?HidNpadButton_Left:HidNpadButton_Right;
            }
            (void)controller_route_input(&view,(uint32_t)down,monitor_exit,&action);
        }
        else if(controller_route_input(&view,(uint32_t)down,monitor_exit,&action)) { /* routed locally */ }
        else if(down&HidNpadButton_Plus) action=ACTION_QUIT;
        else if(down&HidNpadButton_B) action=ACTION_BACK;
        else if(down&HidNpadButton_A) {
            if(view.screen==SCREEN_FILES && caps_toggle.phase==TX_NONE) action=(Action)(ACTION_ROW0+browser_index%8);
            else if(view.screen==SCREEN_RESULT) action=ACTION_BACK;
            else if(view.screen!=SCREEN_TRANSFER) action=ACTION_SEND;
        } else if(down&HidNpadButton_R && view.screen==SCREEN_CONFIRM) action=ACTION_ENTER_PAUSE;
        else if(down&HidNpadButton_L && view.screen==SCREEN_CONFIRM) action=ACTION_AUTOINDENT;
        else if(down&HidNpadButton_Minus && view.screen!=SCREEN_DIAGNOSTICS) action=ACTION_DIAGNOSTICS;
        else if(down&HidNpadButton_X && view.screen!=SCREEN_DIAGNOSTICS) action=ACTION_CAPS_LOCK;
        else if(down&HidNpadButton_ZR) action=ACTION_PACE;
        if(controller_route_event_current(&view,sampled_screen,false)) perform(action,now);
        update_view();
        HidTouchScreenState touch={0};hidGetTouchScreenStates(&touch,1);
        bool touching=touch.count>0;
        bool accept_touch=!compose_input_lock && controller_route_event_current(&view,sampled_screen,action!=ACTION_NONE || controller_transitioned);
        if(!accept_touch) library_touch=false;
        if(accept_touch && touching && !was_touching && view.screen==SCREEN_MACRO_LIBRARY) {
            touch_start_x=(int)touch.touches[0].x;touch_start_y=touch_last_y=(int)touch.touches[0].y;library_touch=true;
        } else if(accept_touch && touching && !was_touching) perform(view_touch(&view,(int)touch.touches[0].x,(int)touch.touches[0].y),now);
        if(touching && library_touch) touch_last_y=(int)touch.touches[0].y;
        if(!touching && was_touching && library_touch) {
            if(view.screen==SCREEN_MACRO_LIBRARY) {
                int distance=touch_last_y-touch_start_y;
                if(touch_start_x>=48 && touch_start_x<838 && touch_start_y>=136 && touch_start_y<584 && (distance>30 || distance< -30)) {
                    int moves=(distance<0?-distance:distance)/56;if(moves<1) moves=1;if(moves>8) moves=8;
                    for(int i=0;i<moves;i++) perform(distance<0?ACTION_MACRO_DOWN:ACTION_MACRO_UP,now);
                } else if(distance<=30 && distance>=-30) perform(view_touch(&view,touch_start_x,touch_start_y),now);
            }
            library_touch=false;
        }
        if(view.screen==SCREEN_LIVE && sampled_screen==SCREEN_LIVE && !view.live.exiting) {
            int key=touch.count==1?live_key_at((int)touch.touches[0].x,(int)touch.touches[0].y):-1;
            live_touch(&view.live,touch.count,key,now);
        }
        if(compose_input_lock && !touching && !held) compose_input_lock=false;
        was_touching=touching;
        if(view.screen==SCREEN_CONFIRM && view.composing && loaded_text)
            view.compose_pages=compose_page(loaded_text,loaded_length,view.compose_page,view.compose_rows);
        update_view();
        /* Honour SET_IDLE without allowing a background packet to overwrite a typed key. */
        if(view.screen!=SCREEN_LIVE && !compose_requested && sender.phase==TX_NONE && caps_toggle.phase==TX_NONE && keyboard_idle_due(now)) keyboard_send(0,0,now);
        }
        } else {
            /* CIV owns local input routing, including cancellation/recovery.
               No ordinary sender, passthrough, modal applet or reset runs here. */
            padUpdate(&pad);HidTouchScreenState touch={0};hidGetTouchScreenStates(&touch,1);
            HidAnalogStickState stick=padGetStickPos(&pad,0);
            Action action=civ_route_input(&civ_route,&view,(uint32_t)padGetButtonsDown(&pad),(uint32_t)padGetButtons(&pad),
                stick.x,stick.y,touch.count,touch.count?(int)touch.touches[0].x:0,touch.count?(int)touch.touches[0].y:0,now);
            perform(action,now);
            was_touching=touch.count>0;library_touch=false;
            compose_input_lock=view.screen!=SCREEN_CIV_PLACEHOLDER;
            repeat_at=now+350;menu_stick=(LiveNav){0};library_stick=(LiveNav){0};
            update_view();
        }
        /* Keep the USB pump running between frames. Fast transfers redraw at
           10 Hz instead of blocking every packet on a display refresh. */
        bool fast_stream=civ_usb_owned() || (sender.phase!=TX_NONE && sender.phase!=TX_COUNTDOWN && sender.phase!=TX_ABORT && pace_delay(view.pace)<=10);
        if(now>=next_render) {
            if(macro_app_busy(&macro_app)) fast_stream=true;
            u32 stride;uint32_t *pixels=framebufferBegin(&framebuffer,&stride);
            view_render(&view,pixels,(int)stride);framebufferEnd(&framebuffer);
            next_render=now_ms()+(fast_stream?100:16);
        }
        svcSleepThread((fast_stream || macro_app_busy(&macro_app))?1000000:3000000);
    }
    if(macro_app.player.owned) {
        macro_play_cancel(&macro_app.player);
        keyboard_log("Macro application exit: controller neutral requested; delivery unresolved until service closes");
    }
    /* OS-forced exit cannot wait forever for a disconnected host. Explicitly
       record unresolved cleanup and close the service before ordinary teardown. */
    if(civ_usb_owned()) civ_usb_shutdown();
    civ_sleep_shutdown(); /* After the USB lifetime fence, including forced exit. */
    /* Best effort final all-keys-up before relinquishing the USB controller. */
    if(keyboard_status()->ready) {
        keyboard_cancel_transfer();uint64_t now=now_ms();
        if(R_SUCCEEDED(keyboard_send(0,0,now))) {
            while(keyboard_status()->transfer_pending && now_ms()-now<200) { keyboard_poll(now_ms());svcSleepThread(2000000); }
        }
    }
    if(view.screen==SCREEN_CONTROLLER) { (void)gamepad_exclusive_stop();keyboard_log("Controller exit: neutral requested; completion not confirmed before shutdown"); }
    gamepad_test_release("application exit");
    appletSetMediaPlaybackState(false);gamepad_exit();keyboard_exit();text_plan_free(&plan);free(loaded_text);free(entries);
    macro_app_destroy(&macro_app);ui_shutdown();framebufferClose(&framebuffer);return 0;
}
