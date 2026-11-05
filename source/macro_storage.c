#include "macro_storage.h"
#include <string.h>

static size_t bounded_length(const char *s,size_t cap) {
    size_t n=0;while(n<cap && s[n]) n++;return n;
}
bool macro_name_valid(const char *s) {
    if(!s) return false;
    size_t n=bounded_length(s,MACRO_NAME_BYTES);
    if(!n || n==MACRO_NAME_BYTES || s[0]==' ' || s[n-1]==' ') return false;
    for(size_t i=0;i<n;i++) {
        unsigned char c=(unsigned char)s[i];
        if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c==' ' || c=='_' || c=='-')) return false;
    }
    return true;
}
bool macro_document_valid(const MacroDocument *d) {
    if(!d || !macro_name_valid(d->name) || !macro_sequence_valid(&d->sequence)) return false;
    size_t n=bounded_length(d->description,MACRO_DESCRIPTION_BYTES);
    if(n==MACRO_DESCRIPTION_BYTES) return false;
    for(size_t i=0;i<n;i++) {
        unsigned char c=(unsigned char)d->description[i];
        if((c<32 && c!='\n' && c!='\t') || c>126) return false;
    }
    return (d->speed_percent==25 || d->speed_percent==50 || d->speed_percent==100 || d->speed_percent==150 || d->speed_percent==200) &&
        d->repeat_count<=1000000 && d->loop_delay_ms<=3600000;
}
static void put32(uint8_t *p,uint32_t n) { for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(n>>(8*i)); }
static uint32_t get32(const uint8_t *p) { uint32_t n=0;for(unsigned i=0;i<4;i++) n|=(uint32_t)p[i]<<(8*i);return n; }
static void put64(uint8_t *p,uint64_t n) { for(unsigned i=0;i<8;i++) p[i]=(uint8_t)(n>>(8*i)); }
static uint64_t get64(const uint8_t *p) { uint64_t n=0;for(unsigned i=0;i<8;i++) n|=(uint64_t)p[i]<<(8*i);return n; }
static uint32_t crc32(const uint8_t *p,size_t n) {
    uint32_t c=0xffffffffu;
    while(n--) { c^=*p++;for(unsigned j=0;j<8;j++) c=(c>>1)^(0xedb88320u & (0u-(c&1u))); }
    return ~c;
}
MacroStoreResult macro_encode(const MacroDocument *d,uint8_t *b,size_t cap,size_t *length) {
    if(!macro_document_valid(d) || !length) return MACRO_STORE_INVALID;
    size_t n=364+12*d->sequence.count;*length=n;
    if(!b || cap<n) return MACRO_STORE_BUFFER_SMALL;
    memset(b,0,n);memcpy(b,"RCMS",4);put32(b+4,1);put32(b+8,(uint32_t)d->sequence.count);
    put32(b+12,d->speed_percent);put32(b+16,d->repeat_count);put32(b+20,d->loop_delay_ms);
    put64(b+24,d->created_unix);put64(b+32,d->modified_unix);
    memcpy(b+40,d->name,strlen(d->name));memcpy(b+104,d->description,strlen(d->description));
    for(size_t i=0;i<d->sequence.count;i++) {
        const MacroStep *s=&d->sequence.steps[i];uint8_t *p=b+360+12*i;
        p[0]=(uint8_t)s->report.buttons;p[1]=(uint8_t)(s->report.buttons>>8);p[2]=s->report.hat;
        p[3]=s->report.lx;p[4]=s->report.ly;p[5]=s->report.rx;p[6]=s->report.ry;p[7]=s->report.vendor;
        put32(p+8,s->duration_ms);
    }
    put32(b+n-4,crc32(b,n-4));return MACRO_STORE_OK;
}
static bool canonical_string(const uint8_t *s,size_t n) {
    size_t i=0;while(i<n && s[i]) i++;
    if(i==n) return false;
    while(i<n) if(s[i++]) return false;
    return true;
}
MacroStoreResult macro_decode(const uint8_t *b,size_t n,MacroDocument *out) {
    if(!b || !out || n<364 || n>MACRO_FILE_MAX_BYTES || memcmp(b,"RCMS",4)) return MACRO_STORE_INVALID;
    if(get32(b+4)!=1) return MACRO_STORE_VERSION;
    uint32_t count=get32(b+8);
    if(count>MACRO_MAX_STEPS || n!=364+(size_t)count*12 || get32(b+n-4)!=crc32(b,n-4) ||
       !canonical_string(b+40,64) || !canonical_string(b+104,256)) return MACRO_STORE_INVALID;
    MacroDocument temp;memset(&temp,0,sizeof(temp));
    memcpy(temp.name,b+40,64);memcpy(temp.description,b+104,256);
    temp.created_unix=get64(b+24);temp.modified_unix=get64(b+32);
    temp.speed_percent=get32(b+12);temp.repeat_count=get32(b+16);temp.loop_delay_ms=get32(b+20);
    temp.sequence.count=count;
    for(size_t i=0;i<count;i++) {
        const uint8_t *p=b+360+i*12;MacroStep *s=&temp.sequence.steps[i];
        s->report.buttons=(uint16_t)(p[0]|((uint16_t)p[1]<<8));s->report.hat=p[2];
        s->report.lx=p[3];s->report.ly=p[4];s->report.rx=p[5];s->report.ry=p[6];s->report.vendor=p[7];
        s->duration_ms=get32(p+8);temp.sequence.duration_ms+=s->duration_ms;
    }
    if(!macro_document_valid(&temp)) return MACRO_STORE_INVALID;
    *out=temp;return MACRO_STORE_OK;
}
