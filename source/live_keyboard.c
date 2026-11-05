#include "live_keyboard.h"
#include <string.h>

#define LIVE_KEY_W 70
#define LIVE_KEY_H 64
#define LIVE_KEY_GAP 6
#define LIVE_ROW_GAP 8
#define LIVE_START_Y 176
#define LIVE_SPECIAL_Y (LIVE_START_Y+4*(LIVE_KEY_H+LIVE_ROW_GAP))
#define LIVE_PRINTABLE 47

static const char *const plain_rows[]={
    "`1234567890-=",
    "qwertyuiop[]\\",
    "asdfghjkl;'",
    "zxcvbnm,./"
};
static const char *const shifted_rows[]={
    "~!@#$%^&*()_+",
    "QWERTYUIOP{}|",
    "ASDFGHJKL:\"",
    "ZXCVBNM<>?"
};
static const LiveKey special_keys[]={
    {148,LIVE_SPECIAL_Y,160,LIVE_KEY_H,LIVE_SHIFT,0,0,"Shift",4},
    {316,LIVE_SPECIAL_Y,480,LIVE_KEY_H,LIVE_SPACE,' ',' ',"Space",4},
    {804,LIVE_SPECIAL_Y,160,LIVE_KEY_H,LIVE_BACKSPACE,0,0,"Backspace",4},
    {972,LIVE_SPECIAL_Y,160,LIVE_KEY_H,LIVE_ENTER,0,0,"Enter",4}
};

unsigned live_key_count(void) { return LIVE_PRINTABLE+4; }
LiveKey live_key(unsigned index) {
    for(int row=0;row<4;row++) {
        unsigned n=(unsigned)strlen(plain_rows[row]);
        if(index<n) {
            int total=(int)n*LIVE_KEY_W+((int)n-1)*LIVE_KEY_GAP;
            int x=(1280-total)/2;
            return (LiveKey){
                x+(int)index*(LIVE_KEY_W+LIVE_KEY_GAP),
                LIVE_START_Y+row*(LIVE_KEY_H+LIVE_ROW_GAP),
                LIVE_KEY_W,LIVE_KEY_H,
                (int)(unsigned char)plain_rows[row][index],
                plain_rows[row][index],shifted_rows[row][index],NULL,row
            };
        }
        index-=n;
    }
    return index<4?special_keys[index]:(LiveKey){0};
}
int live_key_at(int x,int y) {
    for(unsigned i=0;i<live_key_count();i++) {
        LiveKey k=live_key(i);
        if(x>=k.x && x<k.x+k.w && y>=k.y && y<k.y+k.h) return k.key;
    }
    return -1;
}
int live_key_index_at(int x,int y) {
    for(unsigned i=0;i<live_key_count();i++) {
        LiveKey k=live_key(i);
        if(x>=k.x && x<k.x+k.w && y>=k.y && y<k.y+k.h) return (int)i;
    }
    return -1;
}
int live_key_index_for(int key) {
    for(unsigned i=0;i<live_key_count();i++) if(live_key(i).key==key) return (int)i;
    return -1;
}
static int abs_int(int value) { return value<0?-value:value; }
static bool is_lower_letter(char c) { return c>='a' && c<='z'; }
char live_key_character(const LiveKeyboard *live,const LiveKey *key) {
    if(!key) return 0;
    if(key->key==LIVE_SPACE) return ' ';
    if(key->key==LIVE_SHIFT || key->key==LIVE_ENTER || key->key==LIVE_BACKSPACE) return 0;
    char c=key->plain;
    if(is_lower_letter(c)) {
        if(live->caps || live->shift) c=key->shifted;
    } else if(live->shift) c=key->shifted;
    return c;
}
const char *live_key_text(const LiveKeyboard *live,const LiveKey *key,char *buffer,size_t size) {
    if(!key) return "";
    if(key->key==LIVE_SHIFT) return live->caps?"CAPS":"Shift";
    if(key->label) return key->label;
    if(buffer && size) {
        buffer[0]=live_key_character(live,key);
        buffer[1]=0;
        return buffer;
    }
    return "";
}
void live_init(LiveKeyboard *s,uint32_t failures) {
    memset(s,0,sizeof(*s));s->phase=LIVE_SYNC;s->failures=failures;s->touched=-1;s->pressed=-1;
    int a=live_key_index_for('a');
    s->selected=a>=0?(unsigned)a:0;
}
static void clear_input(LiveKeyboard *s) {
    s->head=s->count=0;s->shift=false;s->touch_armed=false;s->touched=-1;s->pressed=-1;
    s->last_shift_tap=0;s->nav=(LiveNav){0};
    s->a_held=s->b_held=s->x_held=s->y_held=s->plus_held=false;
}
static bool accepts_input(const LiveKeyboard *s) {
    return s->was_ready && !s->exiting && !s->fault &&
        s->phase!=LIVE_SYNC && s->phase!=LIVE_SYNC_WAIT;
}
static bool enqueue(LiveKeyboard *s,Stroke stroke) {
    if(s->count==LIVE_QUEUE_CAPACITY) { s->dropped++;return false; }
    s->queue[(s->head+s->count)%LIVE_QUEUE_CAPACITY]=stroke;s->count++;
    return true;
}
static void shift_press(LiveKeyboard *s,uint64_t now) {
    if(s->caps) {
        s->caps=false;s->shift=false;s->last_shift_tap=0;
    } else if(s->shift) {
        if(now>=s->last_shift_tap && now-s->last_shift_tap<=LIVE_CAPS_WINDOW_MS) {
            s->shift=false;s->caps=true;s->last_shift_tap=0;
        } else {
            s->shift=false;s->last_shift_tap=0;
        }
    } else {
        s->shift=true;s->last_shift_tap=now;
    }
    s->pressed=LIVE_SHIFT;s->pressed_until=now+LIVE_PRESS_FLASH_MS;
}
bool live_activate(LiveKeyboard *s,int key,uint64_t now) {
    if(!accepts_input(s)) return false;
    if(key==LIVE_SHIFT) { shift_press(s,now);return true; }
    s->pressed=key;s->pressed_until=now+LIVE_PRESS_FLASH_MS;
    s->last_shift_tap=0; /* any other key invalidates a pending double-tap */
    if(key==LIVE_ENTER) return enqueue(s,(Stroke){0,0x28});
    if(key==LIVE_BACKSPACE) return enqueue(s,(Stroke){0,0x2a});
    if(key==LIVE_SPACE) {
        bool queued=enqueue(s,(Stroke){0,0x2c});
        if(queued && s->shift) s->shift=false;
        return queued;
    }
    int index=live_key_index_for(key);
    if(index<0) return false;
    LiveKey k=live_key((unsigned)index);
    char character=live_key_character(s,&k);
    CharacterKeys mapped;
    if(!character || !character_keys((unsigned char)character,&mapped) || mapped.count!=1) return false;
    bool queued=enqueue(s,mapped.keys[0]);
    if(queued && s->shift) s->shift=false;
    return queued;
}
bool live_activate_selected(LiveKeyboard *s,uint64_t now) {
    if(s->selected>=live_key_count()) return false;
    return live_activate(s,live_key(s->selected).key,now);
}
bool live_move(LiveKeyboard *s,int dx,int dy) {
    if(!dx && !dy) return false;
    LiveKey current=live_key(s->selected);
    int best=-1,best_primary=0,best_secondary=0;
    for(unsigned i=0;i<live_key_count();i++) {
        LiveKey candidate=live_key(i);
        int primary,secondary;
        if(dx) {
            if(candidate.row!=current.row) continue;
            if(dx<0 && candidate.x>=current.x) continue;
            if(dx>0 && candidate.x<=current.x) continue;
            primary=abs_int((candidate.x*2+candidate.w)-(current.x*2+current.w));
            secondary=abs_int(candidate.x-current.x);
        } else {
            if(candidate.row!=current.row+dy) continue;
            int left=candidate.x>current.x?candidate.x:current.x;
            int right=(candidate.x+candidate.w)<(current.x+current.w)?
                candidate.x+candidate.w:current.x+current.w;
            primary=left<right?0:(candidate.x>current.x?
                candidate.x-(current.x+current.w):current.x-(candidate.x+candidate.w));
            secondary=abs_int((candidate.x*2+candidate.w)-(current.x*2+current.w));
        }
        if(best<0 || primary<best_primary || (primary==best_primary && secondary<best_secondary)) {
            best=(int)i;best_primary=primary;best_secondary=secondary;
        }
    }
    if(best<0 || (unsigned)best==s->selected) return false;
    s->selected=(unsigned)best;s->pressed=-1;
    return true;
}
bool live_stick_direction(int x,int y,int *dx,int *dy) {
    if(dx) *dx=0;
    if(dy) *dy=0;
    int ax=abs_int(x),ay=abs_int(y);
    if(ax<=LIVE_STICK_DEADZONE && ay<=LIVE_STICK_DEADZONE) return false;
    if(ax>=ay) { if(dx) *dx=x>0?1:-1; }
    else { if(dy) *dy=y>0?-1:1; } /* libnx stick Y is positive upwards */
    return true;
}
bool live_nav_step(LiveNav *nav,int dx,int dy,uint64_t now) {
    if(!dx && !dy) { nav->dx=nav->dy=0;nav->next_at=0;return false; }
    if(dx!=nav->dx || dy!=nav->dy) {
        nav->dx=dx;nav->dy=dy;nav->next_at=now+LIVE_NAV_DELAY_MS;return true;
    }
    if(now>=nav->next_at) { nav->next_at=now+LIVE_NAV_REPEAT_MS;return true; }
    return false;
}
LiveInputResult live_controls(LiveKeyboard *s,const LiveControls *controls,uint64_t now) {
    if(!controls || s->exiting) return LIVE_INPUT_NONE;
    /* Latch action buttons so a held button still produces exactly one edge,
       even if a caller passes held state instead of freshly pressed state. */
    bool plus_edge=controls->plus && !s->plus_held;
    bool a_edge=controls->a && !s->a_held;
    bool b_edge=controls->b && !s->b_held;
    bool x_edge=controls->x && !s->x_held;
    bool y_edge=controls->y && !s->y_held;
    s->plus_held=controls->plus;s->a_held=controls->a;s->b_held=controls->b;
    s->x_held=controls->x;s->y_held=controls->y;
    if(plus_edge) return LIVE_INPUT_DONE;
    int dx=0,dy=0;
    if(controls->left) dx=-1;
    else if(controls->right) dx=1;
    else if(controls->up) dy=-1;
    else if(controls->down) dy=1;
    else live_stick_direction(controls->stick_x,controls->stick_y,&dx,&dy);
    if(live_nav_step(&s->nav,dx,dy,now)) live_move(s,dx,dy);
    if(a_edge) live_activate_selected(s,now);
    if(b_edge) live_activate(s,LIVE_BACKSPACE,now);
    if(x_edge) live_activate(s,LIVE_SHIFT,now);
    if(y_edge) live_activate(s,LIVE_SPACE,now);
    return LIVE_INPUT_NONE;
}
void live_touch(LiveKeyboard *s,unsigned touches,int key,uint64_t now) {
    if(touches!=1) { s->touch_armed=touches==0;s->touched=-1;return; }
    if(!s->touch_armed || !s->was_ready || s->phase==LIVE_SYNC || s->phase==LIVE_SYNC_WAIT || s->exiting || s->fault) return;
    if(key==s->touched) return;
    s->touched=key;
    if(key<0) return;
    int index=live_key_index_for(key);
    if(index>=0) s->selected=(unsigned)index;
    live_activate(s,key,now);
}
void live_exit(LiveKeyboard *s) {
    s->exiting=true;s->caps=false;s->last_shift_tap=0;
    clear_input(s);
}
bool live_step(LiveKeyboard *s,bool ready,bool pending,uint32_t failures,uint64_t now,Stroke *report) {
    s->offered=false;
    if(!ready) {
        if(s->was_ready) clear_input(s);
        s->was_ready=false;s->failures=failures;s->due=0;
        s->phase=s->exiting?LIVE_DONE:LIVE_SYNC;
        return false;
    }
    if(!s->was_ready) { clear_input(s);s->was_ready=true;s->fault=false;s->recovery_attempted=false;s->failures=failures;s->due=0;s->phase=LIVE_SYNC; }
    if(failures!=s->failures) {
        bool exit_release_failed=s->exiting && s->phase==LIVE_EXIT_WAIT;
        s->failures=failures;clear_input(s);s->fault=true;s->due=0;
        s->phase=exit_release_failed?LIVE_DONE:s->exiting?LIVE_EXIT_SEND:s->recovery_attempted?LIVE_IDLE:LIVE_SYNC;
    }
    if(pending) return false;
    if(s->exiting && s->phase!=LIVE_EXIT_WAIT && s->phase!=LIVE_DONE) {
        /* Do not shorten a press that is already in flight: the host may drop
           the key if the following all-keys-up arrives too soon. */
        if(s->phase==LIVE_PRESS_WAIT && now<s->due) return false;
        s->phase=LIVE_EXIT_SEND;
    }
    if(s->phase==LIVE_SYNC_WAIT) { s->phase=LIVE_IDLE;return false; }
    if(s->phase==LIVE_PRESS_WAIT) {
        if(now<s->due) return false;
        s->phase=LIVE_RELEASE;
    }
    if(s->phase==LIVE_RELEASE_WAIT) { s->head=(s->head+1)%LIVE_QUEUE_CAPACITY;if(s->count) s->count--;s->phase=LIVE_IDLE; }
    if(s->phase==LIVE_EXIT_WAIT) { s->phase=LIVE_DONE;return false; }
    if(s->phase==LIVE_IDLE && (s->fault || !s->count)) return false;
    if(s->phase==LIVE_IDLE && now<s->due) return false;
    if(s->phase==LIVE_SYNC || s->phase==LIVE_RELEASE || s->phase==LIVE_EXIT_SEND) *report=(Stroke){0,0};
    else if(s->phase==LIVE_IDLE) *report=s->queue[s->head];
    else return false;
    if(s->phase==LIVE_IDLE) s->due=now+LIVE_PRESS_MS;
    else if(s->phase==LIVE_RELEASE) s->due=now+LIVE_NEUTRAL_MS;
    if(s->fault && s->phase==LIVE_SYNC) s->recovery_attempted=true;
    s->offered=true;return true;
}
void live_submitted(LiveKeyboard *s,bool success) {
    if(!s->offered) return;
    s->offered=false;
    if(!success) { clear_input(s);s->fault=true;s->due=0;s->phase=s->exiting?LIVE_DONE:s->recovery_attempted?LIVE_IDLE:LIVE_SYNC;return; }
    if(s->phase==LIVE_SYNC) s->phase=LIVE_SYNC_WAIT;
    else if(s->phase==LIVE_IDLE) s->phase=LIVE_PRESS_WAIT;
    else if(s->phase==LIVE_RELEASE) s->phase=LIVE_RELEASE_WAIT;
    else if(s->phase==LIVE_EXIT_SEND) s->phase=LIVE_EXIT_WAIT;
}
