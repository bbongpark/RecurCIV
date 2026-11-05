#include <switch.h>
#include "civ_sleep.h"

static CivSleepInfo info;
CivSleepInfo civ_sleep_info(void) { return info; }
void civ_sleep_release(void) {
    info.held=false;info.protected=false;
    if(!info.restore_pending) return;
    bool actual=false;
    Result rc=appletSetAutoSleepDisabled(info.previous_disabled);
    if(R_SUCCEEDED(rc)) rc=appletIsAutoSleepDisabled(&actual);
    if(R_SUCCEEDED(rc) && actual!=info.previous_disabled) rc=KERNELRESULT(InvalidState);
    info.restore_failed=R_FAILED(rc);
    if(R_SUCCEEDED(rc)) info.restore_pending=false;
    else info.error=rc;
}
bool civ_sleep_prepare(void) {
    if(info.held) return info.protected;
    if(info.restore_pending) {
        civ_sleep_release();
        if(info.restore_pending) return false; /* Preserve the ORIGINAL prior state. */
    }
    info=(CivSleepInfo){0};info.held=true;
    if(hosversionBefore(5,0,0)) {
        info.error=MAKERESULT(Module_Libnx,LibnxError_IncompatSysVer);return false;
    }
    Result rc=appletIsAutoSleepDisabled(&info.previous_disabled);
    if(R_FAILED(rc)) { info.error=rc;return false; } /* Never change an unknown prior state. */
    info.previous_known=true;
    if(info.previous_disabled) { info.protected=true;return true; }
    info.restore_pending=true; /* A failed setter may still have changed state. */
    rc=appletSetAutoSleepDisabled(true);
    bool actual=false;
    if(R_SUCCEEDED(rc)) rc=appletIsAutoSleepDisabled(&actual);
    if(R_SUCCEEDED(rc) && !actual) rc=KERNELRESULT(InvalidState);
    info.error=rc;info.protected=R_SUCCEEDED(rc);return info.protected;
}
void civ_sleep_update(bool usb_owned) {
    if(info.held && !usb_owned) civ_sleep_release();
}
void civ_sleep_shutdown(void) { civ_sleep_release(); }
