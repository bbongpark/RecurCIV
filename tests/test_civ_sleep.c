#include "civ_sleep.h"
#include <switch.h>
#include <assert.h>
#include <stdio.h>

static bool supported=true,disabled,ignore_set;
static unsigned gets,sets,fail_get_at;
static Result set_error;
bool hosversionBefore(unsigned a,unsigned b,unsigned c) { (void)a;(void)b;(void)c;return !supported; }
Result appletIsAutoSleepDisabled(bool *out) {
    gets++;if(gets==fail_get_at) return 99;*out=disabled;return 0;
}
Result appletSetAutoSleepDisabled(bool flag) {
    sets++;if(!ignore_set) disabled=flag;return set_error;
}
static void reset(bool previous) {
    set_error=0;fail_get_at=0;ignore_set=false;supported=true;civ_sleep_shutdown();
    disabled=previous;gets=sets=0;
}
int main(void) {
    /* Identical SelfController path in library-applet and application contexts. */
    for(unsigned mode=0;mode<2;mode++) for(unsigned previous=0;previous<2;previous++) {
        (void)mode;reset(previous!=0);assert(civ_sleep_prepare());
        CivSleepInfo s=civ_sleep_info();assert(s.held && s.protected && s.previous_known && s.previous_disabled==(previous!=0));
        assert(disabled);unsigned g=gets,p=sets;
        for(int i=0;i<1000;i++) civ_sleep_update(true); /* running/cancel/blocked cleanup */
        assert(gets==g && sets==p && civ_sleep_info().held && disabled);
        civ_sleep_update(false);assert(disabled==(previous!=0) && !civ_sleep_info().held);
        assert(sets==(previous?0:2));
        g=gets;p=sets;for(int i=0;i<1000;i++) civ_sleep_update(false);
        assert(gets==g && sets==p);
    }
    reset(false);assert(civ_sleep_prepare());civ_sleep_release(); /* rejected start rollback */
    assert(!disabled && !civ_sleep_info().restore_pending);
    reset(false);assert(civ_sleep_prepare());civ_sleep_shutdown(); /* forced exit after USB fence */
    assert(!disabled && !civ_sleep_info().held);
    reset(false);supported=false;assert(!civ_sleep_prepare());
    assert(gets==0 && sets==0 && civ_sleep_info().error);civ_sleep_release();
    reset(false);fail_get_at=1;assert(!civ_sleep_prepare());
    assert(!civ_sleep_info().previous_known && sets==0);civ_sleep_release();assert(sets==0);
    reset(false);set_error=99;assert(!civ_sleep_prepare()); /* ambiguous setter changed state */
    assert(disabled && civ_sleep_info().restore_pending);
    set_error=0;civ_sleep_release();assert(!disabled && !civ_sleep_info().restore_pending);
    reset(false);fail_get_at=2;assert(!civ_sleep_prepare());assert(civ_sleep_info().error==99);
    civ_sleep_release();assert(!disabled && !civ_sleep_info().restore_pending);
    reset(false);ignore_set=true;assert(!civ_sleep_prepare());assert(!civ_sleep_info().protected);
    ignore_set=false;civ_sleep_release();
    reset(false);assert(civ_sleep_prepare());set_error=99;civ_sleep_update(false);
    assert(civ_sleep_info().restore_failed && civ_sleep_info().restore_pending);
    unsigned g=gets,p=sets;for(int i=0;i<100;i++) civ_sleep_update(false);
    assert(gets==g && sets==p);set_error=0;civ_sleep_shutdown();
    assert(!disabled && !civ_sleep_info().restore_pending);
    puts("PASS: auto-sleep guard saves/verifies/restores prior override, preserves cleanup ownership, handles unsupported/query/set/verify/restore failures and exit without per-frame IPC.");
    return 0;
}
