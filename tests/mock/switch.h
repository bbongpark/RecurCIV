#pragma once
/* Minimal host-side usb:ds simulator; not a replacement Nintendo Switch SDK. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef uint32_t Result;
typedef enum { UsbState_Detached,UsbState_Attached,UsbState_Powered,UsbState_Default,UsbState_Address,UsbState_Configured,UsbState_Suspended } UsbState;
typedef enum { UsbDeviceSpeed_None,UsbDeviceSpeed_Low,UsbDeviceSpeed_Full,UsbDeviceSpeed_High,UsbDeviceSpeed_Super } UsbDeviceSpeed;
typedef struct { bool fired; } Event;
typedef struct { Event SetupEvent,CtrlInCompletionEvent,CtrlOutCompletionEvent; } UsbDsInterface;
typedef struct { Event CompletionEvent; uint8_t address; } UsbDsEndpoint;
typedef struct { uint32_t id,requestedSize,transferredSize,urb_status; } UsbDsReportEntry;
typedef struct { UsbDsReportEntry report[8];uint32_t report_count; } UsbDsReportData;
#pragma pack(push,1)
struct usb_control_setup { uint8_t bmRequestType,bRequest;uint16_t wValue,wIndex,wLength; };
struct usb_hid_descriptor { uint8_t bLength,bDescriptorType;uint16_t bcdHID;uint8_t bCountryCode,bNumDescriptors,bClassDescriptorType;uint16_t wClassDescriptorLength; };
struct usb_device_descriptor { uint8_t bLength,bDescriptorType;uint16_t bcdUSB;uint8_t bDeviceClass,bDeviceSubClass,bDeviceProtocol,bMaxPacketSize0;uint16_t idVendor,idProduct,bcdDevice;uint8_t iManufacturer,iProduct,iSerialNumber,bNumConfigurations; };
struct usb_interface_descriptor { uint8_t bLength,bDescriptorType,bInterfaceNumber,bAlternateSetting,bNumEndpoints,bInterfaceClass,bInterfaceSubClass,bInterfaceProtocol,iInterface; };
struct usb_endpoint_descriptor { uint8_t bLength,bDescriptorType,bEndpointAddress,bmAttributes;uint16_t wMaxPacketSize;uint8_t bInterval; };
#pragma pack(pop)
#define USB_DT_DEVICE 1
#define USB_DT_CONFIG 2
#define USB_DT_INTERFACE 4
#define USB_DT_ENDPOINT 5
#define USB_DT_HID 0x21
#define USB_DT_REPORT 0x22
#define USB_CLASS_HID 3
#define USB_TRANSFER_TYPE_INTERRUPT 3
#define R_FAILED(r) ((r)!=0)
#define R_SUCCEEDED(r) ((r)==0)
#define Module_Libnx 345
#define LibnxError_IncompatSysVer 1
#define LibnxError_BadUsbCommsWrite 2
#define LibnxError_NotInitialized 3
#define MAKERESULT(m,d) ((Result)((m)|((d)<<9)))
#define KERNELRESULT(name) ((Result)(0x10000+Kernel_##name))
enum { Kernel_TimedOut=1,Kernel_InvalidState=2 };
#define HOSVER_MAJOR(v) (((v)>>16)&255)
#define HOSVER_MINOR(v) (((v)>>8)&255)
#define HOSVER_MICRO(v) ((v)&255)
uint32_t hosversionGet(void);
bool hosversionBefore(unsigned,unsigned,unsigned);
int appletGetAppletType(void);
enum { AppletType_Application=0,AppletType_LibraryApplet=2 };
Result appletSetAutoSleepDisabled(bool flag);
Result appletIsAutoSleepDisabled(bool *out);
typedef enum { AppletHookType_OnFocusState,AppletHookType_OnOperationMode,AppletHookType_OnPerformanceMode,
    AppletHookType_OnExitRequest,AppletHookType_OnResume,AppletHookType_OnCaptureButtonShortPressed } AppletHookType;
typedef enum { AppletFocusState_InFocus=1,AppletFocusState_OutOfFocus,AppletFocusState_Background } AppletFocusState;
typedef struct AppletHookCookie { struct AppletHookCookie *next;void (*callback)(AppletHookType,void *);void *param; } AppletHookCookie;
void appletHook(AppletHookCookie *,void (*callback)(AppletHookType,void *),void *);
void appletUnhook(AppletHookCookie *);
AppletFocusState appletGetFocusState(void);
Result appletSetRequiresCaptureButtonShortPressedMessage(bool);
void armDCacheFlush(void *,size_t);
Result eventWait(Event *,uint64_t);
Result eventClear(Event *);
Result usbDsInitialize(void);
void usbDsExit(void);
Result usbDsClearDeviceData(void);
Result usbDsAddUsbLanguageStringDescriptor(uint8_t *,const uint16_t *,uint16_t);
Result usbDsAddUsbStringDescriptor(uint8_t *,const char *);
Result usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed,struct usb_device_descriptor *);
Result usbDsSetBinaryObjectStore(const void *,size_t);
Result usbDsRegisterInterfaceEx(UsbDsInterface **,uint8_t);
Result usbDsInterface_AppendConfigurationData(UsbDsInterface *,UsbDeviceSpeed,const void *,size_t);
Result usbDsInterface_RegisterEndpoint(UsbDsInterface *,UsbDsEndpoint **,uint8_t);
Result usbDsInterface_EnableInterface(UsbDsInterface *);
Result usbDsEnable(void);
Result usbDsInterface_StallCtrl(UsbDsInterface *);
Result usbDsInterface_CtrlInPostBufferAsync(UsbDsInterface *,void *,size_t,uint32_t *);
Result usbDsInterface_CtrlOutPostBufferAsync(UsbDsInterface *,void *,size_t,uint32_t *);
Result usbDsInterface_GetSetupPacket(UsbDsInterface *,void *,size_t);
Result usbDsEndpoint_Cancel(UsbDsEndpoint *);
Result usbDsEndpoint_GetReportData(UsbDsEndpoint *,UsbDsReportData *);
Result usbDsGetState(UsbState *);
Result usbDsInterface_GetCtrlInReportData(UsbDsInterface *,UsbDsReportData *);
Result usbDsInterface_GetCtrlOutReportData(UsbDsInterface *,UsbDsReportData *);
Result usbDsParseReportData(UsbDsReportData *,uint32_t,uint32_t *,uint32_t *);
Result usbDsEndpoint_PostBufferAsync(UsbDsEndpoint *,void *,size_t,uint32_t *);
