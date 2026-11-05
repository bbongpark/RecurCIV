#include "controller_monitor.h"
#include <string.h>

void controller_monitor_init(ControllerMonitor *m) { memset(m,0,sizeof(*m)); }
bool controller_monitor_can_arm(uint32_t held,bool connected,bool usb_ready,bool synchronized) {
    return connected && usb_ready && synchronized && !(held&0xffffu);
}
void controller_monitor_update(ControllerMonitor *m,uint32_t held,bool connected) {
    held=connected?(held&0xffffu):0;
    if(!m->observed || m->held!=held) {
        m->held=held;m->observed=true;
        m->history[m->next]=held;m->next=(m->next+1)%MON_HISTORY;
        if(m->count<MON_HISTORY) m->count++;
    }
}
uint32_t controller_monitor_history(const ControllerMonitor *m,unsigned index) {
    return index<m->count?m->history[(m->next+MON_HISTORY-1-index)%MON_HISTORY]:0;
}
bool controller_monitor_exit(ControllerMonitor *m,uint32_t held,bool connected,uint64_t now) {
    if(!connected || (held&(MON_PLUS|MON_MINUS))!=(MON_PLUS|MON_MINUS)) {
        m->shortcut_active=false;return false;
    }
    if(!m->shortcut_active) { m->shortcut_active=true;m->shortcut_since=now;return false; }
    return now-m->shortcut_since>=2000;
}
static uint8_t axis(int32_t n,bool invert) {
    if(n>32767) n=32767;
    if(n< -32768) n=-32768;
    if(n>-1024 && n<1024) n=0;
    if(invert) n=n==-32768?32767:-n;
    return (uint8_t)((n+32768)>>8);
}
SwitchGamepadReport controller_monitor_report(uint32_t h,int32_t lx,int32_t ly,int32_t rx,int32_t ry,bool available) {
    SwitchGamepadReport r={0,8,128,128,128,128,0};
    if(!available) return r;
    const uint32_t src[]={MON_Y,MON_B,MON_A,MON_X,MON_L,MON_R,MON_ZL,MON_ZR,MON_MINUS,MON_PLUS,MON_L3,MON_R3};
    for(unsigned i=0;i<12;i++) if(h&src[i]) r.buttons|=(uint16_t)(1u<<i);
    int x=!!(h&MON_RIGHT)-!!(h&MON_LEFT),y=!!(h&MON_DOWN)-!!(h&MON_UP);
    if(y<0) r.hat=x<0?7:x>0?1:0;
    else if(y>0) r.hat=x<0?5:x>0?3:4;
    else if(x) r.hat=x<0?6:2;
    r.lx=axis(lx,false);r.ly=axis(ly,true);r.rx=axis(rx,false);r.ry=axis(ry,true);
    return r;
}
