#include <switch.h>
#include "capture_control.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static AppletHookCookie *hooks;
static uint64_t now;
static unsigned enables,disables,other_calls;
static bool old_os,fail_enable,fail_disable;
static int applet_type;
static AppletFocusState focus=AppletFocusState_InFocus;
static uint64_t clock_ms(void) { return now; }
bool hosversionBefore(unsigned a,unsigned b,unsigned c) { (void)a;(void)b;(void)c;return old_os; }
int appletGetAppletType(void) { return applet_type; }
AppletFocusState appletGetFocusState(void) { return focus; }
void appletHook(AppletHookCookie *cookie,void (*fn)(AppletHookType,void *),void *param) {
    cookie->next=hooks;cookie->callback=fn;cookie->param=param;hooks=cookie;
}
void appletUnhook(AppletHookCookie *cookie) {
    AppletHookCookie **p=&hooks;
    while(*p && *p!=cookie) p=&(*p)->next;
    assert(*p==cookie);*p=cookie->next;
}
Result appletSetRequiresCaptureButtonShortPressedMessage(bool enable) {
    if(enable) { enables++;return fail_enable?123:0; }
    disables++;return fail_disable?456:0;
}
static void other(AppletHookType type,void *param) { (void)type;assert(param==&other_calls);other_calls++; }
static void emit(AppletHookType type) {
    for(AppletHookCookie *p=hooks;p;p=p->next) p->callback(type,p->param);
}
static void update(bool session,bool eligible,uint64_t t) { now=t;capture_control_update(session,eligible,now); }
static void press(uint64_t t) { now=t;emit(AppletHookType_OnCaptureButtonShortPressed); }
int main(void) {
    AppletHookCookie existing={0};appletHook(&existing,other,&other_calls);
    capture_control_init(clock_ms);
    press(1);assert(!capture_control_take(now));
    update(true,false,10);assert(enables==1 && hooks!=&existing);
    press(20);assert(!capture_control_take(now)); /* synchronization */
    update(true,true,100);press(110);assert(!capture_control_take(now)); /* entry quiet gate */
    update(true,true,500);press(600);press(600);press(601);
    assert(capture_control_take(601));assert(!capture_control_take(601)); /* coalesced */
    update(true,true,650);press(660);assert(!capture_control_take(660));
    update(true,true,1000);press(1100);assert(capture_control_take(1100));
    update(true,true,1500);press(1600);assert(!capture_control_take(2200)); /* stale */
    press(2300);capture_control_discard(now);assert(!capture_control_take(now)); /* touch wins */
    update(true,true,2700);press(2800);update(true,false,2801);assert(!capture_control_take(2801)); /* confirmation/exit */
    update(true,true,3200);press(3300);focus=AppletFocusState_Background;emit(AppletHookType_OnFocusState);
    assert(!capture_control_take(3300));update(true,true,3400);
    focus=AppletFocusState_InFocus;update(true,true,3800);press(3900);assert(capture_control_take(3900));
    emit(AppletHookType_OnExitRequest);assert(other_calls>0); /* other hooks remain alive */
    update(false,false,4000);assert(disables==1 && hooks==&existing);
    press(4100);assert(!capture_control_take(4100));
    update(true,true,4200);press(4210);assert(!capture_control_take(4210)); /* new generation */
    update(true,true,4600);press(4700);update(false,false,4701);assert(!capture_control_take(4701));
    old_os=true;update(true,true,5000);assert(strstr(capture_control_status(),"unavailable"));
    assert(enables==2);update(false,false,5100);old_os=false;
    applet_type=-2;update(true,true,5200);assert(strstr(capture_control_status(),"touchscreen"));
    update(false,false,5300);applet_type=2;fail_enable=true;
    update(true,true,5400);assert(enables==3 && hooks==&existing && strstr(capture_control_status(),"registration failed"));
    press(6000);assert(!capture_control_take(now));update(false,false,6100);
    fail_enable=false;update(true,true,6200);assert(hooks!=&existing);
    fail_disable=true;update(false,false,6300);assert(hooks==&existing && strstr(capture_control_status(),"restore failed"));
    unsigned saved=disables;update(false,false,6400);assert(disables==saved);
    fail_disable=false;update(false,false,7300);assert(disables==saved+1 && !*capture_control_status());
    update(true,true,7400);capture_control_shutdown();assert(hooks==&existing);
    appletUnhook(&existing);assert(!hooks);
    puts("PASS: Capture applet registration, bounded notifications, debounce/stale/session/focus gates, fallback, restoration/retry and hook isolation.");return 0;
}
