#pragma once
#include <stdbool.h>
#include <stdint.h>

/* appletMainLoop dispatch and these calls all run on the application thread.
   One coalescing slot, never HID bits or USB work inside the callback. */
void capture_control_init(uint64_t (*clock_ms)(void));
void capture_control_update(bool live_session,bool eligible,uint64_t now);
bool capture_control_take(uint64_t now);
void capture_control_discard(uint64_t now);
void capture_control_shutdown(void);
const char *capture_control_status(void);
