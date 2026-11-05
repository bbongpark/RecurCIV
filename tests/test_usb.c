#include "usb_keyboard.h"
#include "usb_gamepad.h"
#include "gamepad_test.h"
#include "controller_route.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Composite host simulator: interface 0 is the gamepad (0x81/0x01) and
   interface 1 is Plume's keyboard (0x82, IN only). */
static UsbDsInterface gamepad_iface,keyboard_iface;
static UsbDsEndpoint keyboard_ep,gamepad_in_ep,gamepad_out_ep;
static UsbState bus_state=UsbState_Detached;
static struct usb_control_setup setup;
static uint32_t sequence;
static uint32_t ctrl_in_urb,ctrl_out_urb,keyboard_urb,gamepad_in_urb,gamepad_out_urb;
static uint32_t ctrl_out_report_override;
static UsbDsInterface *ctrl_in_owner,*ctrl_out_owner;
static uint8_t ctrl_in_data[4096];
static size_t ctrl_in_size;
static unsigned stalls,closed,last_stall_gamepad;
static uint8_t config[2][3][128];
static size_t config_size[2][3];
static unsigned interfaces_registered,endpoints_registered,interfaces_enabled;
static unsigned fail_append;
static unsigned completed_status=3;

static uint8_t key_report[8];
static unsigned key_actual=8,keyboard_urb_size=8;
static uint8_t gamepad_in_report[8];
static unsigned gamepad_in_actual=8;
static uint32_t gamepad_in_report_override;
static uint32_t key_report_override;
static unsigned fail_key_post,fail_key_read,fail_gamepad_read,fail_cancel,fail_state;
static uint8_t *gamepad_out_ptr;
static size_t gamepad_out_posted;
static unsigned gamepad_out_actual=64;
static uint8_t next_in_address,next_out_address;
static unsigned key_cancels,gamepad_in_cancels,gamepad_out_cancels;
static unsigned key_posts,gamepad_in_posts,gamepad_out_posts;
static unsigned fail_gamepad_in_post;
static bool in_queued,out_queued,in_data_completed;
static void *output;
static size_t output_size;

uint32_t hosversionGet(void) { return 22<<16; }
bool hosversionBefore(unsigned a,unsigned b,unsigned c) { (void)b;(void)c;return a>22; }
int appletGetAppletType(void) { return 0; }
static bool auto_sleep_disabled;
static unsigned sleep_gets,sleep_sets;
static Result sleep_get_error,sleep_set_error;
Result appletSetAutoSleepDisabled(bool flag) {
    sleep_sets++;if(sleep_set_error) return sleep_set_error;auto_sleep_disabled=flag;return 0;
}
Result appletIsAutoSleepDisabled(bool *out) {
    sleep_gets++;if(sleep_get_error) return sleep_get_error;*out=auto_sleep_disabled;return 0;
}
void armDCacheFlush(void *p,size_t n) { (void)p;(void)n; }
Result eventWait(Event *e,uint64_t ns) { (void)ns;return e->fired?0:1; }
Result eventClear(Event *e) { e->fired=false;return 0; }
Result usbDsInitialize(void) {
    memset(&gamepad_iface,0,sizeof(gamepad_iface));
    memset(&keyboard_iface,0,sizeof(keyboard_iface));
    memset(&keyboard_ep,0,sizeof(keyboard_ep));
    memset(&gamepad_in_ep,0,sizeof(gamepad_in_ep));
    memset(&gamepad_out_ep,0,sizeof(gamepad_out_ep));
    memset(config_size,0,sizeof(config_size));
    ctrl_in_owner=ctrl_out_owner=NULL;
    in_queued=out_queued=in_data_completed=false;
    output=NULL;output_size=0;ctrl_in_size=0;
    gamepad_in_report_override=0;
    ctrl_out_report_override=0;
    interfaces_registered=endpoints_registered=interfaces_enabled=0;
    next_in_address=0x81;next_out_address=0x01;
    return 0;
}
void usbDsExit(void) { closed++; }
Result usbDsClearDeviceData(void) { return 0; }
Result usbDsAddUsbLanguageStringDescriptor(uint8_t *idx,const uint16_t *s,uint16_t n) { (void)s;assert(n==1);*idx=0;return 0; }
Result usbDsAddUsbStringDescriptor(uint8_t *idx,const char *s) {
    static unsigned next;
    assert(strcmp(s,(next++%2)==0?"PICO SWITCH PRO.":"J24 CONTROLLER")==0);
    *idx=(uint8_t)next;return 0;
}
Result usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed speed,struct usb_device_descriptor *d) {
    assert(d->bDeviceClass==0 && d->bNumConfigurations==1 && d->bLength==18);
    assert(d->idVendor==0x0f0d && d->idProduct==0x0092 && d->bcdDevice==0x0100);
    assert(d->iSerialNumber==0);
    assert(d->bMaxPacketSize0==(speed==UsbDeviceSpeed_Super?9:64));return 0;
}
Result usbDsSetBinaryObjectStore(const void *p,size_t n) { const uint8_t *b=p;assert(n==22 && b[2]==22);return 0; }
Result usbDsRegisterInterfaceEx(UsbDsInterface **out,uint8_t number) {
    assert(number<=1);assert(interfaces_registered<2);
    *out=number==0?&gamepad_iface:&keyboard_iface;
    interfaces_registered++;return 0;
}
static int interface_index(UsbDsInterface *i) {
    if(i==&gamepad_iface) return 0;
    if(i==&keyboard_iface) return 1;
    assert(0);return -1;
}
Result usbDsInterface_AppendConfigurationData(UsbDsInterface *i,UsbDeviceSpeed s,const void *p,size_t n) {
    int index=interface_index(i);
    unsigned speed=s-UsbDeviceSpeed_Full;
    if(fail_append) return 99;
    assert(speed<3 && config_size[index][speed]+n<=128);
    memcpy(config[index][speed]+config_size[index][speed],p,n);
    config_size[index][speed]+=n;return 0;
}
Result usbDsInterface_RegisterEndpoint(UsbDsInterface *i,UsbDsEndpoint **out,uint8_t address) {
    /* usb:ds uses only the direction bit and allocates the number itself.
       libnx expects interface_index+1 per direction; model both the automatic
       allocation and that ordering rule so a descriptor mismatch cannot pass. */
    bool in=(address&0x80)!=0;
    uint8_t allocated=in?next_in_address++:next_out_address++;
    assert(allocated==(uint8_t)((in?0x80:0x00)|(interface_index(i)+1)));
    if(i==&gamepad_iface) *out=in?&gamepad_in_ep:&gamepad_out_ep;
    else if(i==&keyboard_iface) { assert(in);*out=&keyboard_ep; }
    else assert(0);
    (*out)->address=allocated;
    endpoints_registered++;return 0;
}
Result usbDsInterface_EnableInterface(UsbDsInterface *i) { assert(interface_index(i)>=0);interfaces_enabled++;return 0; }
Result usbDsEnable(void) { return 0; }
Result usbDsInterface_StallCtrl(UsbDsInterface *i) {
    assert(interface_index(i)>=0);stalls++;last_stall_gamepad=i==&gamepad_iface;return 0;
}
Result usbDsInterface_CtrlInPostBufferAsync(UsbDsInterface *i,void *p,size_t n,uint32_t *id) {
    assert(interface_index(i)>=0 && ((uintptr_t)p%4096)==0);assert(n<=4096);
    /* Regression from the hardware log: OUT-before-IN rejects the IN request. */
    if(out_queued && output_size==0) return 0x00025c8c;
    memcpy(ctrl_in_data,p,n);ctrl_in_size=n;ctrl_in_urb=*id=++sequence;ctrl_in_owner=i;in_queued=true;return 0;
}
Result usbDsInterface_CtrlOutPostBufferAsync(UsbDsInterface *i,void *p,size_t n,uint32_t *id) {
    assert(interface_index(i)>=0 && ((uintptr_t)p%4096)==0 && n<=8);
    if(n==0 && (!in_data_completed || in_queued)) return 0x00025c8c;
    output=p;output_size=n;ctrl_out_urb=*id=++sequence;ctrl_out_owner=i;out_queued=true;return 0;
}
Result usbDsInterface_GetSetupPacket(UsbDsInterface *i,void *p,size_t n) {
    assert(interface_index(i)>=0 && n==8);
    in_data_completed=false;memcpy(p,&setup,n);return 0;
}
Result usbDsEndpoint_Cancel(UsbDsEndpoint *e) {
    if(fail_cancel) return 99;
    if(e==&keyboard_ep) key_cancels++;
    else if(e==&gamepad_in_ep) gamepad_in_cancels++;
    else if(e==&gamepad_out_ep) gamepad_out_cancels++;
    else assert(0);
    return 0;
}
static void make_report(UsbDsReportData *r,uint32_t id,unsigned requested,unsigned transferred) {
    memset(r,0,sizeof(*r));r->report_count=1;
    r->report[0]=(UsbDsReportEntry){id,requested,transferred,completed_status};
}
Result usbDsEndpoint_GetReportData(UsbDsEndpoint *e,UsbDsReportData *r) {
    if(e==&keyboard_ep) {
        if(fail_key_read) return 99;
        make_report(r,key_report_override?key_report_override:keyboard_urb,keyboard_urb_size,key_actual);
    }
    else if(e==&gamepad_in_ep) {
        if(fail_gamepad_read) return 99;
        make_report(r,gamepad_in_report_override?gamepad_in_report_override:gamepad_in_urb,8,gamepad_in_actual);
    }
    else if(e==&gamepad_out_ep) make_report(r,gamepad_out_urb,(unsigned)gamepad_out_posted,gamepad_out_actual);
    else assert(0);
    return 0;
}
Result usbDsGetState(UsbState *s) { if(fail_state) return 99;*s=bus_state;return 0; }
Result usbDsInterface_GetCtrlInReportData(UsbDsInterface *i,UsbDsReportData *r) {
    assert(i==ctrl_in_owner);in_queued=false;in_data_completed=ctrl_in_size>0;
    make_report(r,ctrl_in_urb,(unsigned)ctrl_in_size,(unsigned)ctrl_in_size);return 0;
}
Result usbDsInterface_GetCtrlOutReportData(UsbDsInterface *i,UsbDsReportData *r) {
    assert(i==ctrl_out_owner);
    uint32_t report_id=ctrl_out_report_override?ctrl_out_report_override:ctrl_out_urb;
    if(report_id==ctrl_out_urb) out_queued=false;
    make_report(r,report_id,(unsigned)output_size,(unsigned)output_size);return 0;
}
Result usbDsParseReportData(UsbDsReportData *r,uint32_t id,uint32_t *requested,uint32_t *transferred) {
    if(r->report_count!=1 || r->report[0].id!=id || r->report[0].urb_status!=3) return 7;
    if(requested) *requested=r->report[0].requestedSize;
    if(transferred) *transferred=r->report[0].transferredSize;return 0;
}
Result usbDsEndpoint_PostBufferAsync(UsbDsEndpoint *e,void *p,size_t n,uint32_t *id) {
    assert(((uintptr_t)p%4096)==0);
    if(e==&keyboard_ep) {
        if(fail_key_post) { fail_key_post=0;return 99; }
        assert(n==8);memcpy(key_report,p,8);keyboard_urb_size=8;
        keyboard_urb=*id=++sequence;key_posts++;return 0;
    }
    if(e==&gamepad_in_ep) {
        assert(n==8);
        if(fail_gamepad_in_post) { fail_gamepad_in_post=0;return 99; }
        memcpy(gamepad_in_report,p,8);gamepad_in_urb=*id=++sequence;gamepad_in_posts++;return 0;
    }
    if(e==&gamepad_out_ep) {
        assert(n==64);gamepad_out_ptr=p;gamepad_out_posted=n;
        gamepad_out_urb=*id=++sequence;gamepad_out_posts++;return 0;
    }
    assert(0);return 1;
}

static void settle(uint64_t now) {
    for(unsigned stage=0;stage<8 && (in_queued || out_queued);stage++) {
        if(in_queued) { assert(ctrl_in_owner);ctrl_in_owner->CtrlInCompletionEvent.fired=true; }
        else { assert(ctrl_out_owner);ctrl_out_owner->CtrlOutCompletionEvent.fired=true; }
        keyboard_poll(now);
    }
    assert(!in_queued && !out_queued);
}
static void request_on(UsbDsInterface *source,unsigned type,unsigned req,unsigned value,unsigned index,unsigned length,uint64_t now) {
    settle(now);
    gamepad_iface.SetupEvent.fired=false;keyboard_iface.SetupEvent.fired=false;
    setup=(struct usb_control_setup){(uint8_t)type,(uint8_t)req,(uint16_t)value,(uint16_t)index,(uint16_t)length};
    source->SetupEvent.fired=true;keyboard_poll(now);
}
static void request(unsigned type,unsigned req,unsigned value,unsigned index,unsigned length,uint64_t now) {
    request_on(index==0?&gamepad_iface:&keyboard_iface,type,req,value,index,length,now);
}
static void poll(uint64_t now) { keyboard_poll(now);gamepad_poll(now); }

#ifndef USB_MOCK_ONLY
int main(void) {
    GamepadButtonTest button_test={0};
    assert(!gamepad_button_test_expired(&button_test,500));
    gamepad_button_test_start(&button_test,1000);
    assert(!gamepad_button_test_expired(&button_test,1499));
    assert(gamepad_button_test_expired(&button_test,1500));
    button_test.active=false;
    assert(!gamepad_button_test_expired(&button_test,1501));
    const uint8_t neutral[8]={0x00,0x00,0x08,0x80,0x80,0x80,0x80,0x00};
    SwitchGamepadReport idle_report;
    memset(&idle_report,0,sizeof(idle_report));
    idle_report.hat=SWITCH_GAMEPAD_HAT_NONE;
    idle_report.lx=idle_report.ly=idle_report.rx=idle_report.ry=SWITCH_GAMEPAD_STICK_CENTER;

    assert(keyboard_init()==0 && keyboard_status()->initialized);
#ifdef _MSC_VER
    /* sdmc: is not a valid Windows drive: verify failures remain observable
       without touching the USB transfer state or overwriting automatic errors. */
    assert(keyboard_log_error()->failed);
    assert(strcmp(keyboard_log_error()->operation,"fopen")==0);
    KeyboardLogError automatic=*keyboard_log_error(), manual={0};
    assert(!keyboard_create_log_snapshot(0,&manual));
    assert(manual.failed && strcmp(manual.operation,"fopen")==0);
    assert(keyboard_log_error()->failed && keyboard_log_error()->error_number==automatic.error_number);
    assert(strcmp(keyboard_log_error()->operation,automatic.operation)==0);
    assert(keyboard_status()->initialized);
#endif
    assert(!keyboard_status()->leds_known && keyboard_status()->state==UsbState_Detached);
    assert(gamepad_init()==0 && !gamepad_ready());
    assert(interfaces_registered==2 && endpoints_registered==3 && interfaces_enabled==2);
    assert(gamepad_in_ep.address==0x81 && gamepad_out_ep.address==0x01 && keyboard_ep.address==0x82);
    assert(keyboard_send(0,4,0)!=0);
    assert(gamepad_send(&idle_report,0)!=0);

    /* Composite configuration: gamepad interface 0 first, keyboard interface 1. */
    for(unsigned s=0;s<3;s++) {
        size_t gamepad_size=s==2?44:32,keyboard_size=s==2?31:25;
        assert(config_size[0][s]==gamepad_size && config_size[1][s]==keyboard_size);
        const uint8_t *g=config[0][s],*k=config[1][s];
        assert(g[0]==9 && g[1]==USB_DT_INTERFACE && g[2]==0 && g[3]==0 && g[4]==2);
        assert(g[5]==USB_CLASS_HID && g[6]==0 && g[7]==0 && g[8]==0);
        assert(g[9]==9 && g[10]==USB_DT_HID && g[11]==0x11 && g[12]==0x01 && g[13]==0);
        assert(g[14]==1 && g[15]==USB_DT_REPORT && (g[16]|(g[17]<<8))==86);
        assert(g[18]==7 && g[19]==USB_DT_ENDPOINT && g[20]==0x01 && g[20]==gamepad_out_ep.address && g[21]==USB_TRANSFER_TYPE_INTERRUPT);
        assert((g[22]|(g[23]<<8))==64 && g[24]==(s==0?1:4));
        size_t in_offset=s==2?31:25;
        if(s==2) { /* SuperSpeed companion follows the 64-byte OUT endpoint. */
            assert(g[25]==6 && g[26]==48 && (g[27]|(g[28]<<8))==0 && (g[29]|(g[30]<<8))==64);
        }
        assert(g[in_offset]==7 && g[in_offset+1]==USB_DT_ENDPOINT);
        assert(g[in_offset+2]==0x81 && g[in_offset+2]==gamepad_in_ep.address && g[in_offset+3]==USB_TRANSFER_TYPE_INTERRUPT);
        assert((g[in_offset+4]|(g[in_offset+5]<<8))==64 && g[in_offset+6]==(s==0?1:4));
        if(s==2) { /* Second companion follows the 64-byte IN endpoint. */
            assert(g[38]==6 && g[39]==48 && (g[40]|(g[41]<<8))==0 && (g[42]|(g[43]<<8))==64);
        }
        assert(k[0]==9 && k[1]==USB_DT_INTERFACE && k[2]==1 && k[3]==0 && k[4]==1);
        assert(k[5]==USB_CLASS_HID && k[6]==1 && k[7]==1 && k[8]==0);
        assert(k[9]==9 && k[10]==USB_DT_HID && k[11]==0x11 && k[12]==0x01 && k[13]==0);
        assert(k[14]==1 && k[15]==USB_DT_REPORT && (k[16]|(k[17]<<8))==63);
        assert(k[18]==7 && k[19]==USB_DT_ENDPOINT && k[20]==0x82 && k[20]==keyboard_ep.address && k[21]==USB_TRANSFER_TYPE_INTERRUPT);
        assert((k[22]|(k[23]<<8))==8 && k[24]==(s==0?1:4));
        if(s==2) { /* Keyboard keeps its 8-byte SuperSpeed interval value. */
            assert(k[25]==6 && k[26]==48 && (k[27]|(k[28]<<8))==0 && (k[29]|(k[30]<<8))==8);
        }
    }

    bus_state=UsbState_Configured;poll(1);
    assert(!keyboard_status()->ready && !gamepad_ready() && gamepad_in_posts==0);

    /* Keyboard stays IN-only: request, then complete data before OUT status. */
    request(0x81,6,0x2200,1,255,2);
    assert(ctrl_in_size==63 && ctrl_in_data[0]==5 && keyboard_status()->report_requests==1);
    assert(in_queued && !out_queued);
    unsigned descriptor_size=(unsigned)ctrl_in_size;
    unsigned bits[3]={0},size=0,count=0;
    for(size_t p=0;p<ctrl_in_size;) {
        unsigned tag=ctrl_in_data[p++],n=tag&3;if(n==3)n=4;unsigned value=0;
        for(unsigned j=0;j<n;j++) value|=(unsigned)ctrl_in_data[p++]<<(j*8);
        if(tag==0x75) size=value;if(tag==0x95) count=value;
        assert(tag!=0x85);
        if((tag&0xfc)==0x80) bits[0]+=size*count;
        if((tag&0xfc)==0x90) bits[1]+=size*count;
    }
    assert(bits[0]==64 && bits[1]==8);
    keyboard_iface.CtrlInCompletionEvent.fired=true;keyboard_poll(3);
    assert(!in_queued && out_queued && output_size==0);
    assert(!keyboard_status()->descriptor_valid && !keyboard_status()->ready);
    keyboard_iface.CtrlOutCompletionEvent.fired=true;keyboard_poll(4);
    assert(keyboard_status()->descriptor_valid && !keyboard_status()->ready);

    /* wIndex, not the signalling interface, selects the responder. */
    request_on(&keyboard_iface,0x81,6,0x2200,0,255,5);
    assert(ctrl_in_size==86 && memcmp(ctrl_in_data,switch_gamepad_report_descriptor,86)==0);
    assert(in_queued && !out_queued);
    gamepad_iface.CtrlInCompletionEvent.fired=true;keyboard_poll(5);
    assert(out_queued && !gamepad_status()->descriptor_valid);
    uint32_t expected_out=ctrl_out_urb;
    unsigned completed_before=keyboard_status()->control_out_completed;
    ctrl_out_report_override=expected_out+32;
    gamepad_iface.CtrlOutCompletionEvent.fired=true;keyboard_poll(5);
    assert(out_queued && !gamepad_status()->descriptor_valid);
    assert(keyboard_status()->control_out_completed==completed_before);
    ctrl_out_report_override=0;
    gamepad_iface.CtrlOutCompletionEvent.fired=true;keyboard_poll(5);
    assert(!out_queued && keyboard_status()->control_out_completed==completed_before+1);
    assert(gamepad_status()->descriptor_valid && !keyboard_status()->ready);

    request_on(&gamepad_iface,0x81,6,0x2200,1,63,6);
    assert(ctrl_in_size==63 && keyboard_status()->report_requests==2);
    settle(6);
    assert(keyboard_status()->descriptor_valid);

    /* Both setup events fired at once must answer the wIndex interface once,
       and both events must be consumed. */
    gamepad_iface.SetupEvent.fired=true;keyboard_iface.SetupEvent.fired=true;
    setup=(struct usb_control_setup){0x81,6,0x2200,1,63};
    unsigned setups=keyboard_status()->setup_count;
    keyboard_poll(7);
    assert(in_queued && ctrl_in_owner==&keyboard_iface && ctrl_in_size==63);
    assert(keyboard_status()->setup_count==setups+1);
    assert(!gamepad_iface.SetupEvent.fired && !keyboard_iface.SetupEvent.fired);

    /* The subsequent polling cycle must not process the same SETUP again. */
    keyboard_poll(8);
    assert(keyboard_status()->setup_count==setups+1);
    assert(in_queued && ctrl_in_owner==&keyboard_iface && ctrl_in_size==63);
    settle(8);

    request_on(&keyboard_iface,0x81,6,0x2100,0,9,8);
    assert(ctrl_in_size==9 && ctrl_in_data[7]==86);
    settle(8);
    request_on(&gamepad_iface,0x81,6,0x2100,1,9,9);
    assert(ctrl_in_size==9 && ctrl_in_data[7]==63);
    settle(9);
    request_on(&keyboard_iface,0x81,10,0,1,1,10);
    assert(ctrl_in_size==1 && ctrl_in_data[0]==0);
    settle(10);
    request_on(&keyboard_iface,0x81,0,0,0,2,11);
    assert(ctrl_in_size==2 && ctrl_in_data[0]==0 && ctrl_in_data[1]==0);
    settle(11);
    unsigned before_stalls=stalls;
    request_on(&keyboard_iface,0x81,6,0x2200,2,63,12);
    assert(stalls==before_stalls+1 && last_stall_gamepad==0);

    /* Device-level configuration descriptor is left to usb:ds. */
    before_stalls=stalls;
    request_on(&gamepad_iface,0x80,6,0x0200,0,9,13);
    assert(stalls==before_stalls && !in_queued && !out_queued);

    keyboard_poll(302);assert(keyboard_status()->ready && gamepad_ready());
    assert(gamepad_in_posts==0); /* gamepad_poll has not run yet */
    gamepad_poll(302);
    assert(gamepad_ready() && gamepad_in_posts==1 && gamepad_out_posts==1);
    assert(memcmp(gamepad_in_report,neutral,8)==0);
    assert(gamepad_status()->transfer_pending);

    /* Continuous neutral stream: each completion is replaced immediately. */
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(303);
    assert(gamepad_in_posts==2 && memcmp(gamepad_in_report,neutral,8)==0);
    assert(gamepad_status()->transfer_pending);

    SwitchGamepadReport pressed=idle_report;
    pressed.buttons=SWITCH_GAMEPAD_A;pressed.hat=0;
    const uint8_t pressed_bytes[8]={0x04,0x00,0x00,0x80,0x80,0x80,0x80,0x00};
    gamepad_set_state(&pressed);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(304);
    assert(memcmp(gamepad_in_report,pressed_bytes,8)==0);
    assert(gamepad_in_posts==3);

    request(0xa1,1,0x0100,0,8,305);
    assert(ctrl_in_size==8 && memcmp(ctrl_in_data,pressed_bytes,8)==0);
    settle(305);

    /* Control SET_REPORT on interface 0 echoes into the next input report. */
    const uint8_t control_out[8]={0x08,0x00,0x04,0x40,0x40,0xc0,0xc0,0x00};
    request(0x21,9,0x0200,0,8,306);
    assert(out_queued && output_size==8 && ctrl_out_owner==&gamepad_iface);
    memcpy(output,control_out,8);
    gamepad_iface.CtrlOutCompletionEvent.fired=true;keyboard_poll(307);
    assert(in_queued && ctrl_in_owner==&gamepad_iface && ctrl_in_size==0);
    gamepad_iface.CtrlInCompletionEvent.fired=true;keyboard_poll(308);
    assert(!in_queued && !out_queued);
    request(0xa1,1,0x0100,0,8,309);
    assert(ctrl_in_size==8 && memcmp(ctrl_in_data,control_out,8)==0);
    settle(309);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(310);
    assert(memcmp(gamepad_in_report,control_out,8)==0);

    /* Interrupt OUT 0x01 is echoed as well. */
    const uint8_t interrupt_out[8]={0x02,0x00,0x08,0x00,0xff,0x80,0x80,0x00};
    assert(gamepad_out_ptr && gamepad_out_posted==64);
    memcpy(gamepad_out_ptr,interrupt_out,8);
    gamepad_out_actual=8;
    unsigned out_posts=gamepad_out_posts;
    gamepad_out_ep.CompletionEvent.fired=true;gamepad_poll(311);
    assert(gamepad_out_posts==out_posts+1);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(312);
    assert(memcmp(gamepad_in_report,interrupt_out,8)==0);
    gamepad_out_actual=64;

    /* gamepad_send is rejected while a report is pending, accepted when idle. */
    assert(gamepad_send(&pressed,313)!=0);
    gamepad_exit();
    assert(gamepad_in_cancels==1 && gamepad_out_cancels==1);
    assert(gamepad_send(&pressed,314)==0);
    assert(memcmp(gamepad_in_report,pressed_bytes,8)==0);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(315);
    assert(memcmp(gamepad_in_report,pressed_bytes,8)==0);

    unsigned cancels=gamepad_in_cancels;
    uint32_t timed_out_urb=gamepad_in_urb;
    gamepad_poll(315+2501);
    assert(gamepad_in_cancels==cancels+1);
    assert(gamepad_status()->transfer_pending);
    assert(gamepad_in_urb!=timed_out_urb);

    /* A stale completion from the cancelled URB must not retire the new URB
       or trigger another post onto the shared DMA buffer. */
    unsigned stale_posts=gamepad_in_posts;
    uint32_t current_urb=gamepad_in_urb;
    gamepad_in_report_override=timed_out_urb;
    gamepad_in_ep.CompletionEvent.fired=true;
    gamepad_poll(315+2502);
    assert(gamepad_status()->transfer_pending);
    assert(gamepad_in_posts==stale_posts);
    assert(gamepad_in_urb==current_urb);

    /* The real completion still retires the current URB and streams again. */
    gamepad_in_report_override=0;
    gamepad_in_ep.CompletionEvent.fired=true;
    gamepad_poll(315+2503);
    assert(gamepad_status()->transfer_pending);
    assert(gamepad_in_posts==stale_posts+1);
    assert(gamepad_in_urb!=current_urb);

    /* Exclusive output waits for the actual neutral/state completions, ignores
       competing writers, and does not mistake the old in-flight report for sync. */
    assert(gamepad_exclusive_begin());
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_SYNC);
    assert(!gamepad_exclusive_submit(&pressed));
    gamepad_set_state(&pressed);
    assert(gamepad_send(&pressed,3200)!=0);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3200);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_SYNC);
    assert(memcmp(gamepad_in_report,neutral,8)==0);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3201);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_READY);
    assert(gamepad_exclusive_submit(&pressed));
    assert(!gamepad_exclusive_submit(&idle_report));
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3202);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_PENDING);
    assert(memcmp(gamepad_in_report,pressed_bytes,8)==0);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3203);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_READY);
    request(0x21,9,0x0200,0,8,3203);
    memcpy(output,control_out,8);
    gamepad_iface.CtrlOutCompletionEvent.fired=true;keyboard_poll(3203);
    settle(3203);
    memcpy(gamepad_out_ptr,interrupt_out,8);gamepad_out_actual=8;
    gamepad_out_ep.CompletionEvent.fired=true;
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3203);
    assert(memcmp(gamepad_in_report,pressed_bytes,8)==0);
    gamepad_out_actual=64;
    /* This SET_REPORT starts under exclusive ownership but completes later. */
    request(0x21,9,0x0200,0,8,3203);
    memcpy(output,control_out,8);
    assert(gamepad_exclusive_stop());
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3204);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_STOPPING);
    assert(memcmp(gamepad_in_report,neutral,8)==0);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3205);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_OFF);
    gamepad_iface.CtrlOutCompletionEvent.fired=true;keyboard_poll(3205);
    settle(3205);
    memcpy(gamepad_out_ptr,interrupt_out,8);gamepad_out_actual=8;
    gamepad_out_ep.CompletionEvent.fired=true;
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3205);
    assert(memcmp(gamepad_in_report,neutral,8)==0);
    gamepad_out_actual=64;

    assert(gamepad_exclusive_begin());
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3206);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3207);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_READY);
    assert(gamepad_exclusive_submit(&pressed));
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3208);
    gamepad_in_actual=7;
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3209);
    gamepad_in_actual=8;
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_INTERRUPTED);
    assert(!gamepad_exclusive_submit(&pressed));
    assert(memcmp(gamepad_in_report,neutral,8)==0);
    assert(gamepad_exclusive_stop());
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3210);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3211);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_OFF);

    /* Production screen router + actual usb:ds mock transport. Menu button
       edges remain local; entry and exit wait for the actual EP81 completions. */
    ControllerRoute app={0};View app_view={0};Action route_action=ACTION_NONE;
    app_view.selected=2;
    assert(controller_route_input(&app_view,MON_A,false,&route_action));
    assert(route_action==ACTION_MENU_CONTROLLER && controller_route_action(&app,&app_view,route_action));
    assert(app_view.screen==SCREEN_CONTROLLER_MENU);
    assert(!controller_route_tick(&app,&app_view,true,MON_A,0,0,0,0));
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_OFF);
    assert(controller_route_input(&app_view,MON_A,false,&route_action));
    assert(route_action==ACTION_CONTROLLER_STANDARD && controller_route_action(&app,&app_view,route_action));
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_SYNC);
    assert(!controller_route_tick(&app,&app_view,true,MON_A,0,0,0,0) && !app_view.controller_armed);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3212);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_SYNC);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3213);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_READY);
    assert(!controller_route_tick(&app,&app_view,true,MON_A,0,0,0,0) && !app_view.controller_armed);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3214);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3215);
    assert(!controller_route_tick(&app,&app_view,true,0,0,0,0,0) && app_view.controller_armed);
    assert(!controller_route_tick(&app,&app_view,true,MON_A,0,0,0,0));
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3216);
    assert(gamepad_in_report[0]==SWITCH_GAMEPAD_A);
    assert(controller_route_action(&app,&app_view,ACTION_BACK));
    assert(!controller_route_tick(&app,&app_view,true,MON_A,0,0,0,0) && app_view.screen==SCREEN_CONTROLLER);
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3217);
    assert(memcmp(gamepad_in_report,neutral,8)==0);
    assert(!controller_route_tick(&app,&app_view,true,MON_A,0,0,0,0));
    gamepad_in_ep.CompletionEvent.fired=true;gamepad_poll(3218);
    assert(controller_route_tick(&app,&app_view,true,MON_A,0,0,0,0));
    assert(app_view.screen==SCREEN_CONTROLLER_MENU && gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_OFF);
    assert(!controller_route_tick(&app,&app_view,true,MON_A,0,0,0,0));

    /* Keyboard regression coverage. */
    request(0x21,11,0,1,0,400);
    assert(keyboard_status()->protocol==0);
    request(0xa1,3,0,1,1,401);
    assert(ctrl_in_size==1 && ctrl_in_data[0]==0);
    request(0x21,10,0x0500,1,0,402);
    assert(keyboard_status()->idle==5 && keyboard_idle_due(403));
    request(0x21,9,0x0200,1,1,404);
    assert(out_queued && output_size==1 && ctrl_out_owner==&keyboard_iface);
    *(uint8_t*)output=2;
    keyboard_iface.CtrlOutCompletionEvent.fired=true;keyboard_poll(405);
    assert(keyboard_status()->leds==2 && keyboard_status()->leds_known);
    assert(in_queued && ctrl_in_owner==&keyboard_iface && ctrl_in_size==0);
    settle(405);
    request(0xa1,1,0x0100,1,8,406);
    assert(ctrl_in_size==8);
    settle(406);
    assert(keyboard_send(2,11,500)==0 && key_report[0]==2 && key_report[2]==11);
    assert(keyboard_send(0,0,501)!=0);
    keyboard_ep.CompletionEvent.fired=true;keyboard_poll(502);
    assert(keyboard_status()->reports_completed==1 && !keyboard_status()->transfer_pending);
    request(0xa1,1,0x0100,1,8,503);
    assert(ctrl_in_size==8 && ctrl_in_data[2]==11);
    settle(503);
    assert(keyboard_send(0,0,504)==0);
    key_actual=7;keyboard_ep.CompletionEvent.fired=true;keyboard_poll(505);
    assert(keyboard_status()->reports_failed==1);
    key_actual=8;
    unsigned keyboard_cancels=key_cancels;
    assert(keyboard_send(0,4,510)==0);
    keyboard_poll(3011);
    assert(!keyboard_status()->transfer_pending && keyboard_status()->reports_failed==2);
    assert(key_cancels==keyboard_cancels+1);

    /* Bus reset re-enumerates both interfaces and clears the gamepad state. */
    assert(keyboard_send(0,4,3100)==0);
    bus_state=UsbState_Default;keyboard_poll(3101);
    assert(!keyboard_status()->ready && !keyboard_status()->transfer_pending);
    assert(!keyboard_status()->descriptor_valid && keyboard_status()->bus_resets==1);
    assert(!keyboard_status()->leds_known && keyboard_status()->leds==0);
    assert(!gamepad_status()->descriptor_valid);
    poll(3102);
    assert(!gamepad_ready());
    bus_state=UsbState_Configured;poll(3102);
    poll(3500);
    assert(!keyboard_status()->ready);
    request(0x81,6,0x2200,1,63,3501);
    assert(in_queued && !out_queued);
    keyboard_iface.CtrlInCompletionEvent.fired=true;keyboard_poll(3502);
    assert(out_queued);
    keyboard_iface.CtrlOutCompletionEvent.fired=true;keyboard_poll(3503);
    assert(keyboard_status()->descriptor_valid && keyboard_status()->ready);
    request(0x81,6,0x2200,0,86,3504);
    assert(ctrl_in_size==86);
    settle(3504);
    assert(gamepad_status()->descriptor_valid);
    assert(gamepad_status()->transfer_pending);
    assert(memcmp(gamepad_in_report,neutral,8)==0); /* reset restored neutral */
    unsigned reset_posts=gamepad_in_posts;
    gamepad_in_ep.CompletionEvent.fired=true;
    poll(3510);
    assert(gamepad_in_posts==reset_posts+1 && memcmp(gamepad_in_report,neutral,8)==0);

    assert(gamepad_exclusive_begin());
    gamepad_in_ep.CompletionEvent.fired=true;poll(3511);
    gamepad_in_ep.CompletionEvent.fired=true;poll(3512);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_READY);
    assert(gamepad_exclusive_submit(&pressed));
    gamepad_in_ep.CompletionEvent.fired=true;poll(3513);
    assert(memcmp(gamepad_in_report,pressed_bytes,8)==0);
    bus_state=UsbState_Suspended;poll(3520);
    assert(!keyboard_status()->ready && keyboard_status()->descriptor_valid);
    assert(gamepad_status()->descriptor_valid);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_INTERRUPTED);
    assert(!gamepad_exclusive_stop());
    bus_state=UsbState_Configured;poll(3530);poll(3840);
    assert(keyboard_status()->ready); /* resume does not re-enumerate */
    assert(memcmp(gamepad_in_report,neutral,8)==0);
    assert(gamepad_exclusive_stop());
    gamepad_in_ep.CompletionEvent.fired=true;poll(3841);
    gamepad_in_ep.CompletionEvent.fired=true;poll(3842);
    assert(gamepad_exclusive_status()==GAMEPAD_EXCLUSIVE_OFF);

    request(0x81,6,0x2200,1,63,4000);
    completed_status=5;keyboard_iface.CtrlInCompletionEvent.fired=true;keyboard_poll(4001);
    assert(!out_queued && !keyboard_status()->ready && keyboard_status()->error!=0);
    completed_status=3;

    keyboard_exit();assert(closed==1);
    fail_append=1;assert(keyboard_init()==99);
    assert(!keyboard_status()->initialized && closed==2);
    keyboard_exit();
    puts("PASS: composite descriptors and automatically allocated endpoints, wIndex control routing, IN data BEFORE OUT status, neutral/continuous gamepad stream, control and interrupt OUT echo, ready/reset/suspend, keyboard regression, timeout and init cleanup.");
    return 0;
}
#endif
