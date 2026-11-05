#include "ui.h"
#include "text.h"
#include <stdlib.h>
#include <string.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "fonts.h"

Palette ui_palette;
static uint32_t *pixels;
static int stride;
typedef struct { uint8_t *bitmap; stbtt_packedchar chars[232]; int size; } Font;
static Font fonts[4];
static const int sizes[]={20,24,32,48};
static int codepoints[232];

static uint32_t rgb(unsigned value) {
    return 0xff000000 | ((value&255)<<16) | (value&0x00ff00) | ((value>>16)&255);
}
bool ui_init(void) {
    for(int i=0;i<224;i++) codepoints[i]=i+32;
    const int extra[]={0x152,0x153,0x2013,0x2014,0x2018,0x2019,0x2026,0x20ac};
    memcpy(codepoints+224,extra,sizeof(extra));
    for(int i=0;i<4;i++) {
        fonts[i].size=sizes[i]; fonts[i].bitmap=calloc(1024*512,1);
        if(!fonts[i].bitmap) { ui_shutdown(); return false; }
        stbtt_pack_context context;
        if(!stbtt_PackBegin(&context,fonts[i].bitmap,1024,512,0,1,NULL)) { ui_shutdown(); return false; }
        stbtt_PackSetOversampling(&context,1,1);
        stbtt_pack_range range={.font_size=(float)sizes[i],.array_of_unicode_codepoints=codepoints,
            .num_chars=232,.chardata_for_range=fonts[i].chars};
        bool ok=stbtt_PackFontRanges(&context,i>=2?font_outfit:font_manrope,0,&range,1)!=0;
        stbtt_PackEnd(&context);
        if(!ok) { ui_shutdown(); return false; }
    }
    return true;
}
void ui_shutdown(void) { for(int i=0;i<4;i++) { free(fonts[i].bitmap); fonts[i].bitmap=NULL; } }
void ui_begin(uint32_t *ptr,int stride_bytes) {
    pixels=ptr; stride=stride_bytes/4;
    const unsigned night[]={0x0b1425,0x17273d,0x17273d,0x17273d,0xffffff,0xc2cddd,0xd9aa54,0x40536b,0xe7ba63,0xffa59a,0x0b1425};
    const unsigned *p=night;
    ui_palette=(Palette){rgb(p[0]),rgb(p[1]),rgb(p[2]),rgb(p[3]),rgb(p[4]),rgb(p[5]),rgb(p[6]),rgb(p[7]),rgb(p[8]),rgb(p[9]),rgb(p[10])};
    ui_rect(0,0,1280,720,ui_palette.canvas);
}
void ui_rect(int x,int y,int w,int h,uint32_t color) {
    int x1=x<0?0:x,y1=y<0?0:y,x2=x+w>1280?1280:x+w,y2=y+h>720?720:y+h;
    for(int j=y1;j<y2;j++) for(int i=x1;i<x2;i++) pixels[j*stride+i]=color;
}
void ui_roundrect(int x,int y,int w,int h,int r,uint32_t color) {
    for(int yy=0;yy<h;yy++) {
        int inset=0,dy= yy<r ? r-1-yy : yy>=h-r ? yy-(h-r) : -1;
        if(dy>=0) while(inset<r && (r-inset-1)*(r-inset-1)+dy*dy>r*r) inset++;
        ui_rect(x+inset,y+yy,w-2*inset,1,color);
    }
}
void ui_line(int x,int y,int w,uint32_t c) { ui_rect(x,y,w,1,c); }
static Font *font_for(int size) { for(int i=0;i<4;i++) if(fonts[i].size==size) return &fonts[i]; return &fonts[1]; }
static int glyph_index(uint32_t cp) {
    if(cp>=32 && cp<256) return (int)cp-32;
    for(int i=224;i<232;i++) if((uint32_t)codepoints[i]==cp) return i;
    return '?'-32;
}
int ui_text_width(int size,const char *text) {
    Font *f=font_for(size);size_t pos=0,n=strlen(text);uint32_t cp;float width=0;
    while(pos<n) { size_t old=pos;if(!utf8_next(text,n,&pos,&cp)) { cp='?';pos=old+1; } if(cp<32) cp=' ';width+=f->chars[glyph_index(cp)].xadvance; }
    return (int)(width+0.5f);
}
static void blend(int x,int y,uint32_t color,unsigned alpha) {
    if(x<0 || x>=1280 || y<0 || y>=720) return;
    uint32_t bg=pixels[y*stride+x],out=0xff000000;
    for(unsigned i=0;i<3;i++) {
        unsigned fg=(color>>(i*8))&255,b=(bg>>(i*8))&255;
        out|=((fg*alpha+b*(255-alpha)+127)/255)<<(i*8);
    }
    pixels[y*stride+x]=out;
}
void ui_text(int x,int y,int size,uint32_t color,const char *text,int max_width) {
    Font *f=font_for(size); float pen=(float)x,baseline=y+size*.88f;
    size_t pos=0,n=strlen(text); uint32_t cp;
    while(pos<n) {
        size_t old=pos;
        if(!utf8_next(text,n,&pos,&cp)) { cp='?';pos=old+1; }
        if(cp<32) cp=' ';
        int index=glyph_index(cp);
        if(pen+f->chars[index].xadvance>x+max_width) break;
        stbtt_aligned_quad q;
        float py=baseline;
        stbtt_GetPackedQuad(f->chars,1024,512,index,&pen,&py,&q,1);
        int x0=(int)q.x0,y0=(int)q.y0,x1=(int)q.x1,y1=(int)q.y1;
        int sx=(int)(q.s0*1024),sy=(int)(q.t0*512);
        for(int yy=y0;yy<y1;yy++) for(int xx=x0;xx<x1;xx++) {
            unsigned alpha=f->bitmap[(sy+yy-y0)*1024+sx+xx-x0];
            if(alpha) blend(xx,yy,color,alpha);
        }
    }
}
void ui_wrap(int x,int y,int width,int size,int max_lines,uint32_t color,const char *text) {
    Font *f=font_for(size); size_t pos=0,n=strlen(text); int line=0;
    while(pos<n && line<max_lines) {
        char buffer[1024];size_t out=0,last_break_pos=0,last_break_out=0;float w=0;
        while(pos<n && out<sizeof(buffer)-5) {
            size_t old=pos;uint32_t cp;
            if(!utf8_next(text,n,&pos,&cp)) { cp='?';pos=old+1; }
            if(cp=='\r') continue;
            if(cp=='\n') break;
            if(cp=='\t') cp=' ';
            float advance=f->chars[glyph_index(cp)].xadvance;
            if(w+advance>width) {
                if(last_break_pos) { pos=last_break_pos;out=last_break_out; }
                else pos=old;
                break;
            }
            if(cp==' ') { last_break_pos=pos;last_break_out=out; }
            w+=advance;
            if(cp==' ' && text[old]=='\t') buffer[out++]=' ';
            else { memcpy(buffer+out,text+old,pos-old);out+=pos-old; }
        }
        buffer[out]=0;ui_text(x,y+line*(size+10),size,color,buffer,width);line++;
    }
}
