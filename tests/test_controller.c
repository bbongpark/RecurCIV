#include "controller_monitor.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(!controller_monitor_can_arm(MON_A,true,true,true));
    assert(!controller_monitor_can_arm(0,false,true,true));
    assert(!controller_monitor_can_arm(0,true,false,true));
    assert(!controller_monitor_can_arm(0,true,true,false));
    assert(controller_monitor_can_arm(0,true,true,true));
    SwitchGamepadReport r=controller_monitor_report(MON_A|MON_Y|MON_ZR|MON_PLUS|MON_L3|MON_UP|MON_RIGHT,32767,-32768,0,32767,true);
    assert(r.buttons==(0x0004|0x0001|0x0080|0x0200|0x0400));
    assert(r.hat==1 && r.lx==255 && r.ly==255 && r.rx==128 && r.ry==0);
    const uint32_t inputs[]={MON_Y,MON_B,MON_A,MON_X,MON_L,MON_R,MON_ZL,MON_ZR,MON_MINUS,MON_PLUS,MON_L3,MON_R3};
    for(unsigned i=0;i<12;i++) {
        r=controller_monitor_report(inputs[i],0,0,0,0,true);
        assert(r.buttons==(1u<<i));
    }
    const uint32_t hats[]={MON_UP,MON_UP|MON_RIGHT,MON_RIGHT,MON_DOWN|MON_RIGHT,MON_DOWN,MON_DOWN|MON_LEFT,MON_LEFT,MON_UP|MON_LEFT};
    for(unsigned i=0;i<8;i++) assert(controller_monitor_report(hats[i],0,0,0,0,true).hat==i);
    r=controller_monitor_report(0,-32768,32767,32767,-32768,true);
    assert(r.lx==0 && r.ly==0 && r.rx==255 && r.ry==255);
    r=controller_monitor_report(MON_LEFT|MON_RIGHT|MON_UP|MON_DOWN,0,0,0,0,true);
    assert(r.hat==8);
    r=controller_monitor_report(0xffff,10000,10000,10000,10000,false);
    assert(r.buttons==0 && r.hat==8 && r.lx==128 && r.ly==128 && r.rx==128 && r.ry==128);
    ControllerMonitor m;controller_monitor_init(&m);
    controller_monitor_update(&m,MON_A,true);
    controller_monitor_update(&m,MON_A,true);assert(m.count==1);
    controller_monitor_update(&m,MON_A|MON_B,true);
    controller_monitor_update(&m,0,true);
    controller_monitor_update(&m,MON_L,true);
    controller_monitor_update(&m,MON_R,true);
    controller_monitor_update(&m,MON_ZL,true);
    assert(m.count==5 && controller_monitor_history(&m,0)==MON_ZL && controller_monitor_history(&m,4)==(MON_A|MON_B));
    controller_monitor_update(&m,MON_A,false);assert(m.held==0 && controller_monitor_history(&m,0)==0);
    assert(!controller_monitor_exit(&m,MON_PLUS,true,10));
    assert(!controller_monitor_exit(&m,MON_PLUS|MON_MINUS,true,100));
    assert(!controller_monitor_exit(&m,MON_PLUS|MON_MINUS,true,2099));
    assert(!controller_monitor_exit(&m,MON_PLUS,true,2099));
    assert(!controller_monitor_exit(&m,MON_PLUS|MON_MINUS,true,2100));
    assert(controller_monitor_exit(&m,MON_PLUS|MON_MINUS,true,4100));
    assert(!controller_monitor_exit(&m,MON_PLUS,false,4101));
    assert(!controller_monitor_exit(&m,MON_PLUS|MON_MINUS,true,4200));
    puts("PASS: controller mapping, axes, history, disconnect, and exit timing.");
    return 0;
}
