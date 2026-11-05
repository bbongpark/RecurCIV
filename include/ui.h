#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct { uint32_t canvas,surface,soft,tint,ink,muted,accent,line,positive,negative,nav; } Palette;
extern Palette ui_palette;
bool ui_init(void);
void ui_shutdown(void);
void ui_begin(uint32_t *pixels,int stride_bytes);
void ui_rect(int x,int y,int w,int h,uint32_t color);
void ui_roundrect(int x,int y,int w,int h,int radius,uint32_t color);
void ui_text(int x,int y,int size,uint32_t color,const char *text,int max_width);
int ui_text_width(int size,const char *text);
void ui_wrap(int x,int y,int width,int size,int max_lines,uint32_t color,const char *text);
void ui_line(int x,int y,int width,uint32_t color);
