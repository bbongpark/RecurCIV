#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool held,protected,previous_known,previous_disabled,restore_pending,restore_failed;
    uint32_t error;
} CivSleepInfo;
/* Main-thread guard for the existing USB lease, not another runner. Calls IPC
   only on explicit preparation, release, or exit; never waits for an event. */
bool civ_sleep_prepare(void);
void civ_sleep_release(void);
void civ_sleep_update(bool usb_owned);
void civ_sleep_shutdown(void);
CivSleepInfo civ_sleep_info(void);
