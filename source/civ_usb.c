#include "civ_usb.h"
#include "usb_civ.h"

static CivRunner runner;
static bool initialized,acquiring,shutdown_unresolved;
static bool (*application_can_start)(void);
static uint64_t now_ms;
static CivRunnerStatus acquisition_outcome;

static bool ready(void *ctx) { (void)ctx;return usb_civ_ready(); }
static bool idle(void *ctx) { (void)ctx;return usb_civ_idle(); }
static bool submit(void *ctx,const CivRunnerOutput *out) {
    (void)ctx;return usb_civ_submit(out->ticket,out->keyboard.modifier,out->keyboard.key,&out->gamepad,now_ms);
}
static CivRunnerCompletion poll(void *ctx,uint64_t ticket) {
    (void)ctx;(void)ticket;UsbCivTransfer t=usb_civ_transfer();
    CivTransferResult result=t.result==USB_CIV_OK?CIV_TRANSFER_OK:
        t.result==USB_CIV_INTERRUPTED?CIV_TRANSFER_INTERRUPTED:CIV_TRANSFER_FAILED;
    return (CivRunnerCompletion){t.ticket,t.completed,result};
}
static void abort_transfer(void *ctx,uint64_t ticket) { (void)ctx;usb_civ_abort(ticket); }

bool civ_usb_init(bool (*can_start)(void)) {
    if(initialized || !can_start) return false;
    CivRunnerTransport transport={NULL,ready,idle,submit,poll,abort_transfer};
    if(!civ_runner_init(&runner,&transport)) return false;
    application_can_start=can_start;initialized=true;return true;
}
bool civ_usb_owned(void) { return usb_civ_owned(); }
bool civ_usb_start(void) {
    if(!initialized || shutdown_unresolved || civ_usb_owned() || !application_can_start() || !usb_civ_acquire()) return false;
    acquiring=true;acquisition_outcome=CIV_RUNNER_FINISHED;
    if(usb_civ_transfer().fence_blocked) acquisition_outcome=CIV_RUNNER_FAILED;
    return true;
}
void civ_usb_cancel(void) {
    if(!civ_usb_owned()) return;
    if(acquiring) {
        if(acquisition_outcome==CIV_RUNNER_FINISHED) acquisition_outcome=CIV_RUNNER_CANCELLED;
    } else civ_runner_cancel(&runner);
}
bool civ_usb_retry_cleanup(void) {
    if(!civ_usb_owned()) return false;
    CivRunnerProgress p=civ_runner_progress(&runner);
    if(acquiring || p.status==CIV_RUNNER_CLEANING_UP || p.status==CIV_RUNNER_CLEANUP_BLOCKED) {
        UsbCivTransfer t=usb_civ_transfer();
        if(t.fence_blocked) usb_civ_abort(t.ticket);
        if(p.status==CIV_RUNNER_CLEANUP_BLOCKED && !acquiring) return civ_runner_retry_cleanup(&runner);
        return true;
    }
    return false;
}
void civ_usb_tick(uint64_t now) {
    if(!initialized || !civ_usb_owned()) return;
    now_ms=now;UsbCivTransfer t=usb_civ_transfer();
    if(acquiring) {
        if((!usb_civ_ready() || t.result==USB_CIV_INTERRUPTED) && acquisition_outcome==CIV_RUNNER_FINISHED)
            acquisition_outcome=CIV_RUNNER_INTERRUPTED;
        if(t.result==USB_CIV_FAILED && acquisition_outcome==CIV_RUNNER_FINISHED) acquisition_outcome=CIV_RUNNER_FAILED;
        if(!usb_civ_ready() || !usb_civ_idle()) return;
        if(!civ_runner_start(&runner,civ_payload_embedded())) return;
        acquiring=false;
        if(acquisition_outcome==CIV_RUNNER_CANCELLED) civ_runner_cancel(&runner);
        else if(acquisition_outcome==CIV_RUNNER_INTERRUPTED) civ_runner_interrupt(&runner);
        else if(acquisition_outcome==CIV_RUNNER_FAILED) civ_runner_fail(&runner);
    }
    /* Interruptions are sticky in the USB snapshot, including a reset/resume
       fully observed by USB polling between two scheduler ticks. */
    if(civ_runner_active(&runner) && t.result==USB_CIV_INTERRUPTED) civ_runner_interrupt(&runner);
    civ_runner_tick(&runner,now);
    CivRunnerProgress p=civ_runner_progress(&runner);
    if(!civ_runner_active(&runner) && p.neutral_confirmed) (void)usb_civ_release();
}
CivUsbProgress civ_usb_progress(void) {
    CivUsbProgress p={civ_runner_progress(&runner),civ_usb_owned(),acquiring,usb_civ_transfer().fence_blocked,shutdown_unresolved};
    if(acquiring) {
        p.runner=(CivRunnerProgress){0};p.runner.stage=CIV_STAGE_NEUTRAL;
        p.runner.status=p.fence_blocked?CIV_RUNNER_CLEANUP_BLOCKED:CIV_RUNNER_CLEANING_UP;
    }
    return p;
}
void civ_usb_shutdown(void) {
    if(!civ_usb_owned()) return;
    civ_usb_cancel();usb_civ_shutdown();acquiring=false;shutdown_unresolved=true;
}
