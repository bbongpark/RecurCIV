#pragma once
#include "switch_gamepad.h"
#include <stdbool.h>
#include <stdint.h>

/* Bits follow libnx HidNpadButton A through Down; verify in the platform adapter. */
enum { MON_A=1u<<0,MON_B=1u<<1,MON_X=1u<<2,MON_Y=1u<<3,
       MON_L3=1u<<4,MON_R3=1u<<5,MON_L=1u<<6,MON_R=1u<<7,
       MON_ZL=1u<<8,MON_ZR=1u<<9,MON_PLUS=1u<<10,MON_MINUS=1u<<11,
       MON_LEFT=1u<<12,MON_UP=1u<<13,MON_RIGHT=1u<<14,MON_DOWN=1u<<15 };
#define MON_HISTORY 5
typedef struct {
    uint32_t held, history[MON_HISTORY];
    unsigned count, next;
    bool observed, shortcut_active;
    uint64_t shortcut_since;
} ControllerMonitor;
void controller_monitor_init(ControllerMonitor *m);
void controller_monitor_update(ControllerMonitor *m,uint32_t held,bool connected);
uint32_t controller_monitor_history(const ControllerMonitor *m,unsigned index);
bool controller_monitor_exit(ControllerMonitor *m,uint32_t held,bool connected,uint64_t now);
bool controller_monitor_can_arm(uint32_t held,bool connected,bool usb_ready,bool synchronized);
SwitchGamepadReport controller_monitor_report(uint32_t held,int32_t lx,int32_t ly,int32_t rx,int32_t ry,bool available);
