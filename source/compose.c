#include "compose.h"
#include "text.h"
#include <string.h>

ComposeResult compose_validate(bool accepted,const char *text,size_t capacity,size_t *length) {
    *length=0;
    if(!accepted) return COMPOSE_CANCELLED;
    const char *end=memchr(text,0,capacity);
    if(!end || (size_t)(end-text)>COMPOSE_MAX_BYTES) return COMPOSE_TOO_LONG;
    *length=(size_t)(end-text);
    return *length?COMPOSE_ACCEPTED:COMPOSE_EMPTY;
}

/* Fixed codepoint-width rows give every character a place in the review.
   UTF-8 is not split. Newlines advance to the next row. */
size_t compose_page(const char *text,size_t length,size_t page,char rows[COMPOSE_ROWS][COMPOSE_ROW_BYTES]) {
    memset(rows,0,COMPOSE_ROWS*COMPOSE_ROW_BYTES);
    size_t pos=0,line=0,column=0,out=0;
    while(pos<length) {
        size_t start=pos;uint32_t cp;
        if(!utf8_next(text,length,&pos,&cp)) { pos=start+1;cp='?'; }
        if(cp=='\r' || cp=='\n') {
            if(cp=='\r' && pos<length && text[pos]=='\n') pos++;
            line++;column=out=0;continue;
        }
        if(column==32) { line++;column=out=0; }
        if(line/COMPOSE_ROWS==page) {
            size_t bytes=pos-start;
            if(cp=='\t' || cp<32) { rows[line%COMPOSE_ROWS][out++]=' '; }
            else if(out+bytes<COMPOSE_ROW_BYTES) { memcpy(rows[line%COMPOSE_ROWS]+out,text+start,bytes);out+=bytes; }
            rows[line%COMPOSE_ROWS][out]=0;
        }
        column++;
    }
    return line/COMPOSE_ROWS+1;
}
