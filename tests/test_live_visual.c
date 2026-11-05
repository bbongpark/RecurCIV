#include "ui.h"
#include "view.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W 1280
#define SCREEN_H 720

static uint32_t rgb(unsigned value) {
    return 0xff000000u|((value&255u)<<16)|(value&0xff00u)|((value>>16)&255u);
}
static uint32_t *render(const View *view) {
    uint32_t *pixels=calloc((size_t)SCREEN_W*SCREEN_H,sizeof(uint32_t));
    assert(pixels);
    view_render(view,pixels,SCREEN_W*4);
    return pixels;
}
static uint32_t pixel(const uint32_t *pixels,int x,int y) {
    assert(x>=0 && x<SCREEN_W && y>=0 && y<SCREEN_H);
    return pixels[(size_t)y*SCREEN_W+x];
}
static int count_white(const uint32_t *pixels,const LiveKey *key) {
    int count=0;
    for(int y=key->y;y<key->y+key->h;y++) for(int x=key->x;x<key->x+key->w;x++) {
        uint32_t c=pixel(pixels,x,y);
        if((c&255u)>220 && ((c>>8)&255u)>220 && ((c>>16)&255u)>220) count++;
    }
    return count;
}
static int border_thickness(const uint32_t *pixels,const LiveKey *key,uint32_t expected) {
    int thickness=0,cx=key->x+key->w/2;
    while(thickness<key->h && pixel(pixels,cx,key->y+thickness)==expected) thickness++;
    return thickness;
}
static int count_gold(const uint32_t *pixels,int x0,int y0,int x1,int y1,uint32_t gold) {
    int count=0;
    for(int y=y0;y<y1;y++) for(int x=x0;x<x1;x++) if(pixel(pixels,x,y)==gold) count++;
    return count;
}
static void prepare(View *view) {
    memset(view,0,sizeof(*view));
    view->screen=SCREEN_LIVE;
    view->usb_ready=true;
    view->usb_initialized=true;
    view->now=1000;
    strcpy(view->usb_label,"USB keyboard connected");
    live_init(&view->live,0);
    view->live.phase=LIVE_IDLE;
    view->live.was_ready=true;
}
int main(void) {
    assert(ui_init());
    const uint32_t gold=rgb(0xd9aa54),soft=rgb(0x17273d),line=rgb(0x40536b),
        accent=rgb(0x34c5db),gold_tint=rgb(0x3b2f1b),
        caps_tint=rgb(0x2d2414);
    View view;prepare(&view);
    const LiveKey a=live_key((unsigned)live_key_index_for('a'));
    const LiveKey b=live_key((unsigned)live_key_index_for('b'));
    const LiveKey shift=live_key((unsigned)live_key_index_for(LIVE_SHIFT));

    /* Selected key: gold border only, dark fill, white label, no cyan outline. */
    view.live.selected=(unsigned)live_key_index_for('a');
    uint32_t *pixels=render(&view);
    assert(border_thickness(pixels,&a,gold)>=3);
    assert(pixel(pixels,a.x+a.w/2,a.y+8)==soft);
    assert(count_gold(pixels,a.x+10,a.y+8,a.x+a.w-10,a.y+30,gold)==0);
    assert(count_white(pixels,&a)>0);
    assert(pixel(pixels,a.x+a.w/2,a.y)!=accent);
    assert(border_thickness(pixels,&b,line)>=2);
    assert(pixel(pixels,b.x+b.w/2,b.y+8)==soft);
    for(int y=176;y<528;y++) for(int x=0;x<SCREEN_W;x++) assert(pixel(pixels,x,y)!=accent);
    free(pixels);

    /* Pressed: gold-tinted fill and gold border, with the label still visible. */
    view.live.pressed='a';
    view.live.pressed_until=view.now+100;
    pixels=render(&view);
    assert(border_thickness(pixels,&a,gold)>=3);
    assert(pixel(pixels,a.x+a.w/2,a.y+8)==gold_tint);
    assert(count_white(pixels,&a)>0);
    free(pixels);
    view.live.pressed=-1;

    /* One-shot Shift: gold border, dark fill, white "Shift" label. */
    view.live.shift=true;
    view.live.caps=false;
    pixels=render(&view);
    assert(border_thickness(pixels,&shift,gold)>=3);
    assert(pixel(pixels,shift.x+shift.w/2,shift.y+8)==soft);
    assert(count_white(pixels,&shift)>0);
    free(pixels);

    /* CAPS: thicker gold border, dark-gold tint, white "CAPS" label. */
    view.live.shift=false;
    view.live.caps=true;
    pixels=render(&view);
    assert(border_thickness(pixels,&shift,gold)>=4);
    assert(pixel(pixels,shift.x+shift.w/2,shift.y+8)==caps_tint);
    assert(count_white(pixels,&shift)>0);
    free(pixels);

    /* Every label must fit inside its key in every capitalization state. */
    for(int state=0;state<3;state++) {
        view.live.shift=state==1;
        view.live.caps=state==2;
        for(unsigned i=0;i<live_key_count();i++) {
            LiveKey key=live_key(i);
            char label[2];
            const char *text=live_key_text(&view.live,&key,label,sizeof(label));
            int size=24,width=ui_text_width(size,text);
            if(width>key.w-16) { size=20;width=ui_text_width(size,text); }
            assert(width<=key.w-12);
            assert(size<=key.h);
            assert(text[0]!=0);
        }
    }
    ui_shutdown();
    puts("PASS: gold-border selection, dark fills, white labels, pressed tint, "
         "Shift/CAPS visuals, no cyan outline, label fit at 1280x720.");
    return 0;
}
