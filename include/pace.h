#pragma once
#include <stdint.h>

/* Keep indices 0,1,2 compatible with settings saved by earlier releases. */
enum { PACE_NORMAL,PACE_FAST,PACE_SLOW,PACE_VERY_FAST,PACE_ULTRA,PACE_TURBO,PACE_COUNT };
static inline unsigned pace_delay(int pace) {
    const unsigned delays[]={80,40,160,10,2,0};
    return delays[pace>=0 && pace<PACE_COUNT?pace:PACE_NORMAL];
}
static inline const char *pace_label(int pace) {
    const char *labels[]={"80 ms","40 ms","160 ms","10 ms","2 ms","0 ms"};
    return labels[pace>=0 && pace<PACE_COUNT?pace:PACE_NORMAL];
}
static inline int pace_next(int pace) {
    const int next[]={PACE_FAST,PACE_VERY_FAST,PACE_NORMAL,PACE_ULTRA,PACE_TURBO,PACE_SLOW};
    return next[pace>=0 && pace<PACE_COUNT?pace:PACE_NORMAL];
}
static inline int enter_pause_next(int milliseconds) {
    switch(milliseconds) {
        case 0: return 100;case 100: return 200;case 200: return 400;
        case 400: return 800;case 800: return 0;default: return 200;
    }
}
static inline int enter_pause_valid(int milliseconds) {
    return milliseconds==0 || milliseconds==100 || milliseconds==200 || milliseconds==400 || milliseconds==800;
}
