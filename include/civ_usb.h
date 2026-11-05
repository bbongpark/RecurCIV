#pragma once
#include "civ_runner.h"

typedef struct {
    CivRunnerProgress runner;
    bool owned,acquiring,fence_blocked,shutdown_unresolved;
} CivUsbProgress;
/* Singleton, main-thread only. Initialize once; can_start must reject all active
   application writers/modal work. init/tick NEVER start automation themselves. */
bool civ_usb_init(bool (*can_start)(void));
/* True accepts the lease/request, not game acceptance. Acquisition fences the
   previous controller stream before starting; failure enters cleanup only. */
bool civ_usb_start(void); /* Explicit request; embedded approved payload only. */
void civ_usb_tick(uint64_t now_ms); /* After keyboard_poll + gamepad_poll. */
void civ_usb_cancel(void);
bool civ_usb_retry_cleanup(void); /* Neutral/fencing only; never payload replay. */
bool civ_usb_owned(void);
CivUsbProgress civ_usb_progress(void);
/* Terminal until process restart; no same-process restart after forced teardown. */
void civ_usb_shutdown(void);
