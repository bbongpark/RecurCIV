#include "capture_control.h"
#include <switch.h>
#include <stdio.h>
#include <string.h>

#define QUIET_MS 300u
#define STALE_MS 500u
static struct {
    AppletHookCookie hook;
    uint64_t (*clock_ms)(void);
    uint64_t queued_at,last_at,blocked_until,retry_at;
    bool session,hooked,enabled,restore_pending,accepting,pending,seen;
    char status[120];
} capture;
void capture_control_discard(uint64_t now) {
    capture.pending=false;capture.accepting=false;capture.blocked_until=now+QUIET_MS;
}
static void notification(AppletHookType type,void *param) {
    (void)param;
    if(type==AppletHookType_OnFocusState || type==AppletHookType_OnExitRequest) {
        capture_control_discard(capture.clock_ms());return;
    }
    if(type!=AppletHookType_OnCaptureButtonShortPressed) return;
    uint64_t now=capture.clock_ms();
    bool fresh=!capture.seen || (now>=capture.last_at && now-capture.last_at>=QUIET_MS);
    capture.seen=true;capture.last_at=now;
    if(capture.accepting && fresh && !capture.pending) {
        capture.pending=true;capture.queued_at=now;
    }
}
void capture_control_init(uint64_t (*clock_ms)(void)) {
    memset(&capture,0,sizeof(capture));capture.clock_ms=clock_ms;
}
static void restore(uint64_t now) {
    Result rc=appletSetRequiresCaptureButtonShortPressedMessage(false);
    capture.restore_pending=R_FAILED(rc);capture.retry_at=now+1000;
    if(R_FAILED(rc)) snprintf(capture.status,sizeof(capture.status),"Capture restore failed (0x%08lX); retrying. Restart if unresolved.",(unsigned long)rc);
    else { capture.enabled=false;capture.status[0]=0; }
}
void capture_control_update(bool live_session,bool eligible,uint64_t now) {
    if(!live_session) {
        if(capture.session) {
            capture_control_discard(now);capture.session=false;
            if(capture.enabled) restore(now);
            else if(!capture.restore_pending) capture.status[0]=0;
            if(capture.hooked) { appletUnhook(&capture.hook);capture.hooked=false; }
        } else if(capture.restore_pending && now>=capture.retry_at) restore(now);
        return;
    }
    if(!capture.session) {
        capture.session=true;capture_control_discard(now);capture.seen=false;
        if(capture.restore_pending) {
            snprintf(capture.status,sizeof(capture.status),"Capture restore unresolved; use touchscreen. Exit live to retry.");return;
        }
        int type=appletGetAppletType();
        if(!capture.clock_ms || hosversionBefore(3,0,0) ||
           (type!=AppletType_Application && type!=AppletType_LibraryApplet)) {
            snprintf(capture.status,sizeof(capture.status),"Capture unavailable in this environment; use touchscreen Start/Stop.");return;
        }
        appletHook(&capture.hook,notification,NULL);capture.hooked=true;
        Result rc=appletSetRequiresCaptureButtonShortPressedMessage(true);
        if(R_FAILED(rc)) {
            appletUnhook(&capture.hook);capture.hooked=false;
            snprintf(capture.status,sizeof(capture.status),"Capture registration failed (0x%08lX); use touchscreen Start/Stop.",(unsigned long)rc);
            return;
        }
        capture.enabled=true;
        snprintf(capture.status,sizeof(capture.status),"Capture short press: Start/Stop. Short-press screenshots reserved during live.");
    }
    bool ready=capture.enabled && capture.hooked && eligible &&
        appletGetFocusState()==AppletFocusState_InFocus;
    if(!ready) { capture.pending=false;capture.accepting=false;capture.blocked_until=now+QUIET_MS; }
    else capture.accepting=now>=capture.blocked_until;
}
bool capture_control_take(uint64_t now) {
    bool take=capture.pending && capture.accepting && now>=capture.queued_at && now-capture.queued_at<=STALE_MS;
    capture.pending=false;
    if(take) capture_control_discard(now);
    return take;
}
void capture_control_shutdown(void) {
    capture_control_update(false,false,capture.clock_ms?capture.clock_ms():0);
    if(capture.restore_pending) restore(capture.clock_ms?capture.clock_ms():0);
}
const char *capture_control_status(void) { return capture.status; }
