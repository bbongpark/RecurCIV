#include "live_keyboard.h"
#include "pace.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Small deterministic model of the main loop. USB completion is controlled
   explicitly so every press/neutral deadline can be checked without sleeping. */
typedef struct {
    LiveKeyboard *s;
    uint64_t now;
    unsigned failures;
    bool pending;
} Harness;

static bool pump(Harness *h,Stroke *out) {
    out->modifier=out->key=99;
    return live_step(h->s,true,h->pending,h->failures,h->now,out);
}
static void expect_pending(Harness *h) {
    Stroke out={99,99};
    assert(!pump(h,&out));
}
static void submit(Harness *h,unsigned modifier,unsigned key) {
    Stroke out;
    assert(pump(h,&out));
    assert(out.modifier==modifier && out.key==key);
    live_submitted(h->s,true);
    h->pending=true;
}
static void tap(Harness *h,int key) {
    live_touch(h->s,1,key,h->now);live_touch(h->s,0,-1,h->now);
}
static void ready(Harness *h,LiveKeyboard *s,unsigned failures) {
    h->s=s;h->now=0;h->failures=failures;h->pending=false;
    live_init(s,failures);
    Stroke out;
    assert(pump(h,&out) && out.modifier==0 && out.key==0); /* neutral sync */
    live_submitted(s,true);h->pending=true;
    expect_pending(h);
    h->pending=false;h->now=1;
    assert(!pump(h,&out)); /* sync acknowledged; queue empty */
    live_touch(s,0,-1,h->now);
}
/* Consume one queued stroke, asserting the complete ordering and both minimum
   intervals. Completion is immediate (one tick) unless a test drives it itself. */
static void consume(Harness *h,unsigned modifier,unsigned key) {
    Stroke out;
    uint64_t start=h->now;
    submit(h,modifier,key);
    h->now=start+1;
    assert(!pump(h,&out));                      /* transfer pending */
    h->now=start+LIVE_PRESS_MS-1;
    h->pending=false;                           /* acknowledged, still held */
    assert(!pump(h,&out));
    h->now=start+LIVE_PRESS_MS;
    submit(h,0,0);                              /* key-up no earlier than min press */
    uint64_t release_at=h->now;
    h->now=release_at+1;
    assert(!pump(h,&out));                      /* neutral transfer pending */
    h->now=release_at+LIVE_NEUTRAL_MS-1;
    h->pending=false;                           /* acknowledged, but next press waits */
    assert(!pump(h,&out));
    h->now=release_at+LIVE_NEUTRAL_MS;
}
static void consume_char(Harness *h,char c) {
    CharacterKeys mapped;
    assert(character_keys((unsigned char)c,&mapped) && mapped.count==1);
    consume(h,mapped.keys[0].modifier,mapped.keys[0].key);
}
static int index_of(int key) {
    int index=live_key_index_for(key);
    assert(index>=0);
    return index;
}
static int selected_key(const LiveKeyboard *s) {
    assert(s->selected<live_key_count());
    return live_key(s->selected).key;
}
static Stroke last_stroke(const LiveKeyboard *s) {
    assert(s->count);
    return s->queue[(s->head+s->count-1)%LIVE_QUEUE_CAPACITY];
}
static void expect_last(const LiveKeyboard *s,char c) {
    CharacterKeys mapped;
    assert(character_keys((unsigned char)c,&mapped) && mapped.count==1);
    Stroke stroke=last_stroke(s);
    assert(stroke.modifier==mapped.keys[0].modifier && stroke.key==mapped.keys[0].key);
}

int main(void) {
    /* Geometry: 47 printable keys plus Shift, Space, Backspace and Enter. */
    assert(live_key_count()==51);
    bool seen[128]={0};
    int row_count[5]={0},row_x[5]={0};
    unsigned printable=0;
    for(unsigned i=0;i<live_key_count();i++) {
        LiveKey k=live_key(i);
        assert(k.w>=70 && k.h>=64);
        assert(k.x>=0 && k.y>=0 && k.x+k.w<=1280 && k.y+k.h<=720);
        assert(k.row>=0 && k.row<5);
        if(!row_count[k.row]) row_x[k.row]=k.x;
        row_count[k.row]++;
        assert(live_key_at(k.x+k.w/2,k.y+k.h/2)==k.key);
        assert(live_key_index_at(k.x+k.w/2,k.y+k.h/2)==(int)i);
        for(unsigned j=0;j<i;j++) {
            LiveKey other=live_key(j);
            assert(k.x+k.w<=other.x || other.x+other.w<=k.x ||
                   k.y+k.h<=other.y || other.y+other.h<=k.y);
        }
        if(i<47) {
            printable++;
            assert(k.label==NULL);
            assert((unsigned char)k.plain>=32 && (unsigned char)k.plain<127);
            assert((unsigned char)k.shifted>=32 && (unsigned char)k.shifted<127);
            seen[(unsigned char)k.plain]=true;
            seen[(unsigned char)k.shifted]=true;
        }
    }
    assert(printable==47);
    for(unsigned c=33;c<127;c++) assert(seen[c]);
    assert(!seen[32]); /* Space is a dedicated key, not a printable pair. */
    assert(row_count[0]==13 && row_count[1]==13 && row_count[2]==11 &&
           row_count[3]==10 && row_count[4]==4);
    assert(row_x[2]>row_x[1] && row_x[3]>row_x[2]); /* staggered letter rows */
    assert(live_key_at(0,0)==-1);
    assert(live_key_at(1279,719)==-1);
    assert(live_key(index_of(LIVE_SPACE)).w>=480);
    assert(live_key(index_of(LIVE_SHIFT)).w>=160);
    assert(live_key(index_of(LIVE_BACKSPACE)).w>=160);
    assert(live_key(index_of(LIVE_ENTER)).w>=160);

    /* Selection starts on a, moves within a row, and uses nearest-centre
       movement between the staggered rows. */
    LiveKeyboard s;
    live_init(&s,0);
    assert(selected_key(&s)=='a');
    assert(live_move(&s,1,0) && selected_key(&s)=='s');
    assert(live_move(&s,-1,0) && selected_key(&s)=='a');
    assert(!live_move(&s,-1,0));
    s.selected=(unsigned)index_of('`');
    assert(!live_move(&s,-1,0));
    s.selected=(unsigned)index_of('a');
    assert(live_move(&s,0,-1) && selected_key(&s)=='w');
    assert(live_move(&s,0,1) && selected_key(&s)=='a');
    assert(live_move(&s,0,1) && selected_key(&s)=='z');
    assert(live_move(&s,0,1) && selected_key(&s)==LIVE_SHIFT);
    assert(live_move(&s,0,-1) && selected_key(&s)=='z');
    s.selected=(unsigned)index_of('p');
    assert(live_move(&s,0,1) && selected_key(&s)=='l');
    s.selected=(unsigned)index_of('q');
    assert(live_move(&s,0,-1) && selected_key(&s)=='`');

    /* Left-stick deadzone and dominant-axis selection. libnx reports +Y up. */
    int dx=0,dy=0;
    assert(!live_stick_direction(LIVE_STICK_DEADZONE,-LIVE_STICK_DEADZONE,&dx,&dy));
    assert(live_stick_direction(LIVE_STICK_DEADZONE+1,0,&dx,&dy) && dx==1 && dy==0);
    assert(live_stick_direction(0,20000,&dx,&dy) && dx==0 && dy==-1);
    assert(live_stick_direction(0,-20000,&dx,&dy) && dx==0 && dy==1);
    assert(live_stick_direction(20000,15000,&dx,&dy) && dx==1 && dy==0);

    /* Controlled repeat: first step immediately, 350 ms delay, then 100 ms.
       Releasing or changing direction resets the timer. */
    LiveNav nav={0};
    assert(live_nav_step(&nav,1,0,100));
    assert(!live_nav_step(&nav,1,0,449));
    assert(live_nav_step(&nav,1,0,450));
    assert(!live_nav_step(&nav,1,0,549));
    assert(live_nav_step(&nav,1,0,550));
    assert(live_nav_step(&nav,-1,0,551));
    assert(!live_nav_step(&nav,0,0,552));
    assert(live_nav_step(&nav,0,-1,553));
    assert(!live_nav_step(&nav,0,-1,554));

    /* D-pad and stick drive controller selection; held direction repeats. */
    Harness h;Stroke out;
    ready(&h,&s,0);
    LiveControls controls={0};
    assert(selected_key(&s)=='a');
    controls.right=true;
    assert(live_controls(&s,&controls,100)==LIVE_INPUT_NONE && selected_key(&s)=='s');
    live_controls(&s,&controls,200);
    assert(selected_key(&s)=='s'); /* waiting for the initial repeat delay */
    live_controls(&s,&controls,450);
    assert(selected_key(&s)=='d');
    controls.right=false;live_controls(&s,&controls,451);
    controls.left=true;live_controls(&s,&controls,500);
    assert(selected_key(&s)=='s'); /* direction change is immediate */
    controls.left=false;live_controls(&s,&controls,501);
    controls.stick_x=13000;controls.stick_y=0;
    live_controls(&s,&controls,600);
    assert(selected_key(&s)=='d');
    live_controls(&s,&controls,601);
    assert(selected_key(&s)=='d');
    live_controls(&s,&controls,950);
    assert(selected_key(&s)=='f');
    controls.stick_x=0;live_controls(&s,&controls,951);

    /* Action edges: A types once even while held, B backspaces, X shifts,
       Y inserts a space, and + reports Done without touching the queue. */
    ready(&h,&s,0);
    memset(&controls,0,sizeof(controls));
    controls.a=true;
    live_controls(&s,&controls,100);
    live_controls(&s,&controls,101);
    assert(s.count==1 && last_stroke(&s).key==0x04);
    controls.a=false;live_controls(&s,&controls,102);
    controls.a=true;live_controls(&s,&controls,103); /* a real second press */
    assert(s.count==2);
    controls.a=false;live_controls(&s,&controls,104);
    consume_char(&h,'a');consume_char(&h,'a');
    controls.b=true;live_controls(&s,&controls,105);
    controls.b=false;live_controls(&s,&controls,106);
    consume(&h,0,0x2a);
    controls.x=true;live_controls(&s,&controls,107);
    controls.x=false;live_controls(&s,&controls,108);
    assert(s.shift && !s.caps);
    controls.a=true;live_controls(&s,&controls,109);
    controls.a=false;live_controls(&s,&controls,110);
    assert(!s.shift);
    consume_char(&h,'A');
    controls.y=true;live_controls(&s,&controls,111);
    controls.y=false;live_controls(&s,&controls,112);
    consume(&h,0,0x2c);
    memset(&controls,0,sizeof(controls));
    controls.plus=true;
    assert(live_controls(&s,&controls,113)==LIVE_INPUT_DONE);
    assert(live_controls(&s,&controls,114)==LIVE_INPUT_NONE); /* held + is one edge */
    assert(s.count==0 && !s.exiting); /* Done is reported before the caller exits */
    controls.plus=false;live_controls(&s,&controls,115);
    controls.plus=true;
    assert(live_controls(&s,&controls,116)==LIVE_INPUT_DONE);
    live_exit(&s);assert(s.exiting);
    assert(live_controls(&s,&controls,117)==LIVE_INPUT_NONE); /* no second Done/exit edge */

    /* A held X cannot produce a false double-tap. Releasing and pressing again
       within 350 ms is a real second tap and enables CAPS. */
    ready(&h,&s,0);
    memset(&controls,0,sizeof(controls));
    controls.x=true;live_controls(&s,&controls,1000);
    live_controls(&s,&controls,1001);
    assert(s.shift && !s.caps);
    controls.x=false;live_controls(&s,&controls,1002);
    controls.x=true;live_controls(&s,&controls,1100);
    assert(!s.shift && s.caps);
    controls.x=false;live_controls(&s,&controls,1101);

    /* Touch and controller share one capitalization state machine. */
    ready(&h,&s,0);
    tap(&h,LIVE_SHIFT);
    assert(s.shift && !s.caps);
    memset(&controls,0,sizeof(controls));
    controls.x=true;live_controls(&s,&controls,200);
    assert(!s.shift && s.caps);
    controls.x=false;live_controls(&s,&controls,201);
    tap(&h,'a');
    assert(s.count==1);expect_last(&s,'A');
    tap(&h,'1');
    assert(s.count==2);expect_last(&s,'1'); /* CAPS leaves digits unshifted */
    tap(&h,LIVE_SHIFT); /* CAPS off */
    assert(!s.caps && !s.shift);
    memset(&controls,0,sizeof(controls));
    controls.x=true;live_controls(&s,&controls,1000);
    controls.x=false;live_controls(&s,&controls,1001);
    assert(s.shift && !s.caps);
    h.now=1100;
    tap(&h,LIVE_SHIFT);
    assert(!s.shift && s.caps); /* X then touch Shift also forms CAPS */
    tap(&h,LIVE_SHIFT);
    assert(!s.caps && !s.shift);

    /* Displayed labels and transmitted characters come from one mapping. */
    live_init(&s,0);
    LiveKey ka=live_key(index_of('a')),k1=live_key(index_of('1')),
        kslash=live_key(index_of('/')),kshift=live_key(index_of(LIVE_SHIFT));
    char label[2];
    assert(strcmp(live_key_text(&s,&ka,label,sizeof(label)),"a")==0);
    s.shift=true;
    assert(strcmp(live_key_text(&s,&ka,label,sizeof(label)),"A")==0);
    assert(strcmp(live_key_text(&s,&k1,label,sizeof(label)),"!")==0);
    assert(strcmp(live_key_text(&s,&kshift,label,sizeof(label)),"Shift")==0);
    s.shift=false;s.caps=true;
    assert(strcmp(live_key_text(&s,&ka,label,sizeof(label)),"A")==0);
    assert(strcmp(live_key_text(&s,&k1,label,sizeof(label)),"1")==0);
    assert(strcmp(live_key_text(&s,&kslash,label,sizeof(label)),"/")==0);
    assert(strcmp(live_key_text(&s,&kshift,label,sizeof(label)),"CAPS")==0);
    for(unsigned i=0;i<47;i++) {
        LiveKey k=live_key(i);
        assert(live_key_character(&s,&k)==k.plain || live_key_character(&s,&k)==k.shifted);
    }

    /* Shift timing and one-shot consumption. */
    ready(&h,&s,0);
    assert(live_activate(&s,LIVE_SHIFT,1000) && s.shift && !s.caps);
    assert(live_activate(&s,'a',1001) && !s.shift);
    expect_last(&s,'A');
    assert(live_activate(&s,'b',1002) && !s.shift);
    expect_last(&s,'b');
    assert(live_activate(&s,LIVE_SHIFT,2000) && live_activate(&s,LIVE_SHIFT,2349));
    assert(!s.shift && s.caps); /* exactly 350 ms is still a double-tap */
    assert(live_activate(&s,'a',2350));
    expect_last(&s,'A');
    assert(live_activate(&s,'1',2351));
    expect_last(&s,'1');
    assert(live_activate(&s,LIVE_SHIFT,3000) && !s.caps && !s.shift);

    assert(live_activate(&s,LIVE_SHIFT,4000) && s.shift);
    assert(live_activate(&s,LIVE_SHIFT,4351) && !s.shift && !s.caps);
    assert(live_activate(&s,'c',4352));
    expect_last(&s,'c');

    assert(live_activate(&s,LIVE_SHIFT,5000) && s.shift);
    assert(live_activate(&s,'d',5100) && !s.shift); /* intervening key invalidates */
    assert(live_activate(&s,LIVE_SHIFT,5200) && s.shift && !s.caps);
    assert(live_activate(&s,LIVE_SPACE,5201) && !s.shift); /* Space consumes Shift */
    assert(live_activate(&s,'e',5202) && !s.shift);
    expect_last(&s,'e');

    assert(live_activate(&s,LIVE_SHIFT,6000) && s.shift);
    assert(live_activate(&s,LIVE_ENTER,6001) && s.shift); /* Enter is not a character */
    assert(live_activate(&s,'f',6002) && !s.shift);
    expect_last(&s,'F');
    assert(s.count<LIVE_QUEUE_CAPACITY);

    /* Saved touch/controller state is reset by a fresh session and by exit. */
    ready(&h,&s,0);
    tap(&h,LIVE_SHIFT);tap(&h,LIVE_SHIFT);
    assert(s.caps);
    live_exit(&s);
    assert(!s.caps && !s.shift && s.exiting);
    live_init(&s,0);
    assert(!s.caps && !s.shift && !s.was_ready && s.phase==LIVE_SYNC);

    /* Rapid different and repeated taps are queued in order and each produces
       a distinct press/neutral pair at the minimum intervals. */
    ready(&h,&s,0);
    assert(LIVE_PRESS_MS==pace_delay(PACE_NORMAL));
    assert(LIVE_NEUTRAL_MS==pace_delay(PACE_NORMAL));
    tap(&h,'a');tap(&h,'a');tap(&h,'b');
    assert(s.count==3);
    consume(&h,0,4);consume(&h,0,4);consume(&h,0,5);
    assert(s.count==0 && s.phase==LIVE_IDLE);
    assert(!pump(&h,&out));

    /* Shift, shifted symbols, punctuation and specials retain HID mappings. */
    tap(&h,LIVE_SHIFT);assert(s.shift);tap(&h,'1');assert(!s.shift);
    tap(&h,LIVE_SHIFT);tap(&h,'a');tap(&h,'/');
    tap(&h,LIVE_ENTER);tap(&h,LIVE_BACKSPACE);tap(&h,LIVE_SPACE);
    assert(s.count==6);
    const Stroke expected[]={{2,0x1e},{2,4},{0,0x38},{0,0x28},{0,0x2a},{0,0x2c}};
    for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);i++)
        consume(&h,expected[i].modifier,expected[i].key);
    assert(!pump(&h,&out));

    /* Sliding to another key queues it; a second finger cancels input until lift. */
    live_touch(&s,1,'q',h.now);live_touch(&s,1,'q',h.now);live_touch(&s,1,'w',h.now);
    live_touch(&s,1,-1,h.now);live_touch(&s,1,'q',h.now);live_touch(&s,0,-1,h.now);
    assert(s.count==3);
    consume(&h,0,0x14);consume(&h,0,0x1a);consume(&h,0,0x14);
    assert(!pump(&h,&out));
    live_touch(&s,2,'a',h.now);live_touch(&s,1,'a',h.now);
    assert(s.count==0);live_touch(&s,0,-1,h.now);

    /* Long sequence exactly at queue capacity, then two dropped taps. */
    for(unsigned i=0;i<LIVE_QUEUE_CAPACITY+2;i++) tap(&h,'x');
    assert(s.count==LIVE_QUEUE_CAPACITY && s.dropped==2);
    for(unsigned i=0;i<LIVE_QUEUE_CAPACITY;i++) consume(&h,0,0x1b);
    assert(s.count==0 && s.dropped==2);
    assert(!pump(&h,&out));

    /* Slow USB completion: the minimum interval has already elapsed, so the
       next report follows the acknowledgement immediately without deadlock. */
    ready(&h,&s,0);
    tap(&h,'z');tap(&h,'y');
    uint64_t slow=h.now;
    submit(&h,0,0x1d);
    h.now=slow+500;h.pending=false;
    assert(pump(&h,&out) && out.modifier==0 && out.key==0); /* press held >=80 ms */
    live_submitted(&s,true);h.pending=true;
    h.now=slow+900;h.pending=false;
    assert(pump(&h,&out) && out.modifier==0 && out.key==0x1c); /* next press, no extra wait */
    live_submitted(&s,true);h.pending=true;
    h.now+=LIVE_PRESS_MS;h.pending=false;
    assert(pump(&h,&out) && out.key==0);
    live_submitted(&s,true);h.pending=true;
    h.now+=LIVE_NEUTRAL_MS;h.pending=false;
    assert(!pump(&h,&out) && s.count==0);

    /* Disconnect while strokes are queued: queue is cleared, input stays off
       until reconnect, and the first report after reconnect is neutral sync. */
    ready(&h,&s,0);
    tap(&h,'q');tap(&h,'w');assert(s.count==2);
    h.now+=10;h.pending=false;
    assert(!live_step(&s,false,false,h.failures,h.now,&out));
    assert(s.count==0 && !s.was_ready && s.phase==LIVE_SYNC);
    tap(&h,'a');assert(s.count==0); /* not armed while disconnected */
    h.now+=100;
    assert(pump(&h,&out) && out.modifier==0 && out.key==0);
    live_submitted(&s,true);h.pending=true;
    expect_pending(&h);
    h.pending=false;h.now+=1;
    assert(!pump(&h,&out));
    live_touch(&s,0,-1,h.now); /* reconnect clears touch arming until a full lift */
    tap(&h,'r');consume(&h,0,0x15);
    assert(!pump(&h,&out));

    /* Transfer failure clears queued input, recovers with neutral sync, and a
       later failure returns to idle instead of getting permanently stuck. */
    ready(&h,&s,0);
    tap(&h,'a');
    assert(pump(&h,&out) && out.key==4);
    live_submitted(&s,false);
    assert(s.fault && !s.count);
    h.pending=false;h.failures=1;h.now+=10;
    assert(pump(&h,&out) && out.modifier==0 && out.key==0);
    assert(s.recovery_attempted);
    live_submitted(&s,false); /* recovery neutral also fails */
    assert(s.fault && !s.count && s.phase==LIVE_IDLE);
    h.failures=2;h.now+=10;
    assert(!pump(&h,&out) && s.phase==LIVE_IDLE && s.fault);
    tap(&h,'b');assert(!s.count); /* input stays disabled while faulted */
    assert(!live_step(&s,false,false,2,h.now,&out));
    h.now+=10;
    assert(pump(&h,&out) && out.modifier==0 && out.key==0);
    live_submitted(&s,true);h.pending=true;
    expect_pending(&h);
    h.pending=false;h.now+=1;
    assert(!pump(&h,&out) && !s.fault);
    live_touch(&s,0,-1,h.now); /* reconnect clears touch arming until a full lift */
    tap(&h,'d');consume(&h,0,7);

    /* Exit while a press is still pending: the press minimum is honoured, then
       a final all-keys-up is sent and acknowledged before LIVE_DONE. */
    ready(&h,&s,0);
    tap(&h,'e');
    uint64_t exit_at=h.now;
    assert(pump(&h,&out) && out.key==8);
    live_submitted(&s,true);h.pending=true;
    live_exit(&s);assert(s.exiting && !s.count);
    assert(!pump(&h,&out));
    h.pending=false;h.now=exit_at+LIVE_PRESS_MS-1;
    assert(!pump(&h,&out));
    h.now=exit_at+LIVE_PRESS_MS;
    assert(pump(&h,&out) && out.modifier==0 && out.key==0);
    live_submitted(&s,true);h.pending=true;
    h.now+=1;
    assert(!pump(&h,&out));
    h.pending=false;h.now+=1;
    assert(!pump(&h,&out) && s.phase==LIVE_DONE);

    /* A failure while waiting for the exit release still ends the session. */
    ready(&h,&s,0);
    live_exit(&s);
    assert(pump(&h,&out) && out.key==0);
    live_submitted(&s,true);h.pending=true;
    expect_pending(&h);
    h.pending=false;h.failures=1;h.now+=1;
    assert(!pump(&h,&out) && s.phase==LIVE_DONE);

    /* Exit while disconnected also ends immediately. */
    ready(&h,&s,0);
    live_exit(&s);
    h.pending=false;
    assert(!live_step(&s,false,false,h.failures,h.now,&out) && s.phase==LIVE_DONE);

    puts("PASS: live geometry, 47-key mapping, navigation/deadzone/repeat, controller edges, "
         "touch/controller CAPS, labels, queue ordering, 80 ms pacing, slow ACKs, overflow, "
         "failures, reconnect, pending exit and exit failure.");
    return 0;
}
