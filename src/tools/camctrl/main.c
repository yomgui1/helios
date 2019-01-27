/* Copyright 2008-2013, 2018 Guillaume Roguez

This file is part of Helios.

Helios is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Helios is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Helios.  If not, see <https://www.gnu.org/licenses/>.

*/

#include "libraries/helios.h"
#include "proto/helios.h"

#include "base.h"

#include <libraries/asl.h>
#include <hardware/byteswap.h>
#include <utility/hooks.h>

#undef USE_INLINE_STDARG
#include <proto/intuition.h>
#include <clib/alib_protos.h>
#define USE_INLINE_STDARG

#include <proto/exec.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#define MyToggleButton(spec) \
    ImageObject,\
        ImageButtonFrame,\
        MUIA_Background, MUII_ButtonBack,\
        MUIA_Image_Spec, (spec),\
        MUIA_InputMode, MUIV_InputMode_Toggle,\
        MUIA_ShowSelState, TRUE,\
    End

#define Image(f) MUI_NewObject(MUIC_Dtpic,\
    MUIA_Dtpic_Name, f,\
    MUIA_Dtpic_LightenOnMouse, TRUE,\
    MUIA_Dtpic_DarkenSelState, TRUE,\
    MUIA_InputMode, MUIV_InputMode_Toggle,\
    MUIA_ShowSelState, FALSE,\
    End

#define ALLOCFAILURE() log_Error("Not enough memory to process.")

//+ TYPES and STRUCTURES
typedef struct MyBusHandle {
    struct MinNode  SysNode;
    HeliosBusHandle Handle;
} MyBusHandle;

enum {
    VCR_RECORD=0,
    VCR_REWIND,
    VCR_FORWARD,
    VCR_PLAY,
    VCR_START,
    VCR_END,
    VCR_SIZE,
};

enum {
    LOG_LEVEL_ALL=0,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
};

enum {
    MAXLEN_EXACT=0,
    MAXLEN_LASTPACKET,
};
//-

Object *app = NULL, *gDecoderObj = NULL;
Object *win_main, *obj_CaptureDeviceList, *obj_CaptureDeviceGroup, *obj_InputFormat;
Object *obj_CaptureFile, *obj_VCRButtons[VCR_SIZE], *obj_VCRButtonsGroup;
Object *obj_PreviewFormat, *obj_Preview, *obj_RecSize, *obj_RecvSpeed, *obj_RecvVideoFmt;
Object *obj_CaptureGroup, *obj_MaxLen, *obj_MaxLenFactor, *obj_CacheTime;

struct MinList gBusHandleList;
struct MinList gDeviceList;
ULONG gBusCount, gDevCount = 0;

static ULONG gLogLevel = LOG_LEVEL_DEBUG;

struct Library *HeliosBase = NULL;
struct MUI_CustomClass *gCamCtrlMCC = NULL;
struct MUI_CustomClass *gDecoderMCC = NULL;
struct MUI_CustomClass *gPreviewMCC = NULL;
struct MUI_CustomClass *gRingBufferMCC = NULL;

/*==========================================================================================================================*/

//+ vLogMsg
static void vLogMsg(ULONG level, STRPTR fmt, va_list va)
{
    if (gLogLevel > level)
        return;

    if (NULL == app)
        vprintf(fmt, va);
    else
        vprintf(fmt, va);
    putchar('\n');
}
//-
//+ LogMsg
static void LogMsg(ULONG level, STRPTR fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    vLogMsg(level, fmt, va);
    va_end(va);
}
//-

//+ log_Error
void log_Error(STRPTR fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    vLogMsg(LOG_LEVEL_ERROR, fmt, va);
    va_end(va);
}
//-
//+ log_Debug
void log_Debug(STRPTR fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    vLogMsg(LOG_LEVEL_DEBUG, fmt, va);
    va_end(va);
}
//-
//+ log_Warn
void log_Warn(STRPTR fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    vLogMsg(LOG_LEVEL_WARN, fmt, va);
    va_end(va);
}
//-

/*==========================================================================================================================*/

//+ my_atexit
static void my_atexit(void)
{
    MyBusHandle *handle;
    MyDevice *dev;
    APTR next;

    if (NULL != app) MUI_DisposeObject(app);

    ForeachNodeSafe(&gDeviceList, dev, next) {
        Helios_Device_Release(dev->Handle);
        FreeVec(dev->Name);
        FreeMem(dev, sizeof(*dev));
    }

    ForeachNodeSafe(&gBusHandleList, handle, next) {
        Helios_Bus_Release(handle->Handle.Bus);
        FreeMem(handle, sizeof(*handle));
    }

    if (NULL != HeliosBase) CloseLibrary(HeliosBase);
    if (NULL != gPreviewMCC) VideoPreviewMCC_Delete(gPreviewMCC);
    if (NULL != gDecoderMCC) DecoderMCC_Delete(gDecoderMCC);
    if (NULL != gCamCtrlMCC) CamCtrlMCC_Delete(gCamCtrlMCC);
    if (NULL != gRingBufferMCC) RingBufferMCC_Delete(gRingBufferMCC);
}
//-
//+ fail
static LONG fail(char *str)
{
    if (str) {
        puts(str);
        exit(20);
    }

    exit(0);
    return 0;
}
//-
//+ Init
BOOL Init(void)
{
    gCamCtrlMCC = CamCtrlMCC_Create();
    if (NULL == gCamCtrlMCC) {
        log_Error("CamCtrl MCC creation failed");
        return FALSE;
    }

    gPreviewMCC = VideoPreviewMCC_Create();
    if (NULL == gPreviewMCC) {
        log_Error("Preview MCC creation failed");
        return FALSE;
    }

    gDecoderMCC = DecoderMCC_Create();
    if (NULL == gDecoderMCC) {
        log_Error("Decoder MCC creation failed");
        return FALSE;
    }

    gRingBufferMCC = RingBufferMCC_Create();
    if (NULL == gRingBufferMCC) {
        log_Error("RingBuffer MCC creation failed");
        return FALSE;
    }

    HeliosBase = OpenLibrary("helios.library", 0);
    if (NULL != HeliosBase) {
        HeliosBridge *bridge;
        MyBusHandle *handle;
        int i;

        NEWLIST(&gBusHandleList);

        i = gBusCount = 0;
        bridge = NULL;
        while (NULL != (bridge = Helios_Bridge_Next(bridge))) {
            handle = AllocMem(sizeof(*handle), MEMF_PUBLIC | MEMF_CLEAR);
            if (NULL != handle) {
                HeliosBus *bus;

                bus = Helios_Bridge_Handle(bridge);
                if (NULL != bus) {
                    handle->Handle.Bus = bus;
                    if (Helios_BusHandle_Connect(&handle->Handle)) {
                        ADDTAIL(&gBusHandleList, handle);
                        gBusCount++;
                    } else {
                        log_Error("Failed to connect on bus %u.", i);
                        Helios_Bus_Release(bus);
                    }
                } else {
                    log_Error("Failed to handle bus #%u.", i);
                    FreeMem(handle, sizeof(*handle));
                }
            } else
                ALLOCFAILURE();

            i++;
        }

        if (gBusCount > 0)
            return TRUE;

        log_Error("No Firewire bus found on this machine!");
    } else
        log_Error("can't open helios.library.");

    return FALSE;
}
//-
//+ CheckNode
static MyDevice *CheckNode(HeliosBusHandle *bh, UBYTE nodeid)
{
    HeliosDeviceHandle *dh;
    HeliosRomDirectory *dir;
    MyDevice *dev;

    if (nodeid == bh->Local) return NULL;

    dh = Helios_Device_Obtain(bh, HTTAG_NODE_ID, nodeid, TAG_DONE);
    if (NULL != dh) {
        if (Helios_Device_GetAttr(dh, HTTAG_ROM_DIR, (ULONG *) &dir)) {
            if ((0xa02d == dir->unit_spec_id) && (0x01 == (dir->unit_sw_version >> 16))) {
                dev = AllocMem(sizeof(*dev), MEMF_PUBLIC | MEMF_CLEAR);
                if (NULL != dev) {
                    if (NULL != dir->label) {
                        dev->Name = AllocVec(strlen(dir->label), MEMF_PUBLIC);
                        if (NULL != dev->Name)
                            strcpy(dev->Name, dir->label);
                        else
                            ALLOCFAILURE();
                    } else {
                        dev->Name = AllocVec(16, MEMF_PUBLIC);
                        if (NULL != dev->Name)
                            snprintf(dev->Name, 16, "<%.6X-%.6X>", dir->vendor_id, dir->model_id);
                        else
                            ALLOCFAILURE();
                    }

                    if (NULL != dev->Name) {
                        dev->AVC = dir->unit_sw_version & 1;
                        dev->Handle = dh;
                        FreeVec(dir);
                        return dev;
                    }

                    FreeMem(dev, sizeof(*dev));
                } else
                    ALLOCFAILURE();
            }

            FreeVec(dir);
        } else
            log_Debug("Failed to get node ROM information for node %u.", nodeid);

        Helios_Device_Release(dh);
    } else
        log_Debug("Failed to obtain handle for node %u.", nodeid);

    return NULL;
}
//-
//+ ProcessHeliosMsg
static BOOL ProcessHeliosMsg(void)
{
    return TRUE;
}
//-
//+ DoGlobalInit
static void DoGlobalInit(void)
{
    int i;
    MyDevice *dev;
    MyBusHandle *handle;

    NEWLIST(&gDeviceList);

    /* Scan all bus to found valid nodes */
    ForeachNode(&gBusHandleList, handle) {
        for (i=0; i < handle->Handle.NodeCount; i++) {
            dev = CheckNode(&handle->Handle, i);
            if (NULL != dev) {
                dev->Index = gDevCount++;
                ADDTAIL(&gDeviceList, dev);
                DoMethod(obj_CaptureDeviceList, MUIM_List_InsertSingle, dev->Name, dev->Index);
            }
        }
    }
}
//-
//+ main
int main(int argc, char **argv)
{
    BOOL run = TRUE;
    ULONG sigs, mui_sigmask, helios_sigmask=0;
    static CONST_STRPTR title = "FireWire Audio/Video Stream Capture Tool";

    atexit(my_atexit);

    if (!Init())
        fail("Init() failed.");

    //+ ApplicationObject
    app = CamCtrlObject,
        MUIA_Application_Title      , title,
        MUIA_Application_Version    , "$VER: FWGrab 0.1 ("__DATE__")",
        MUIA_Application_Copyright  , "©2009, Guillaume ROGUEZ",
        MUIA_Application_Author     , "Guillaume ROGUEZ",
        MUIA_Application_Description, "Capture and control IEEE1394 device using the IEC61883 and AV/C protocol",
        MUIA_Application_Base       , "FWGrab",

        SubWindow, win_main = WindowObject,
            MUIA_Window_Title, title,
            MUIA_Window_ID   , MAKE_ID('M','A','I','N'),

            WindowContents, HGroup,
                Child, HGroup, GroupFrameT("Available Streaming Devices"),
                    MUIA_Weight, 33,
                    Child, obj_CaptureDeviceList = ListviewObject,
                        MUIA_Listview_Input, FALSE,
                        MUIA_Listview_List, ListObject,
                            ReadListFrame,
                            MUIA_CycleChain, TRUE,
                        End,
                    End,
                End,

                Child, obj_CaptureGroup = VGroup,
                    MUIA_Weight, 100,
                    MUIA_Disabled, TRUE,

                    Child, ColGroup(2),
                        Child, Label2("Capture file:"),
                        Child, PopaslObject,
                            MUIA_Popasl_Type, ASL_FileRequest,
                            MUIA_Popstring_String, obj_CaptureFile = StringObject,
                                StringFrame,
                                MUIA_String_MaxLen, 512,
                                MUIA_CycleChain, TRUE,
                            End,
                            MUIA_Popstring_Button, PopButton(MUII_PopFile),
                        End,

                        Child, Label2("Capture File Max Length (0=unlimited):"),
                        Child, HGroup,
                            Child, obj_MaxLen = StringObject,
                                StringFrame,
                                MUIA_String_Accept, "0123456789",
                                MUIA_String_MaxLen, 20,
                                MUIA_String_Contents, "0",
                                MUIA_String_Format, MUIV_String_Format_Right,
                                MUIA_CycleChain, TRUE,
                            End,
                            Child, obj_MaxLenFactor = CycleObject,
                                MUIA_Cycle_EntryString, "Bytes|KiloBytes|MegaBytes|GigaBytes",
                                MUIA_CycleChain, TRUE,
                            End,
                        End,

                        Child, Label2("Iso buffer cache (in seconds):"),
                        Child, HGroup,
                            Child, obj_CacheTime = SliderObject,
                                MUIA_Slider_Min, 1,
                                MUIA_Slider_Max, 10,
                                MUIA_Numeric_Default, 2,
                                MUIA_Numeric_Format, "%lu second(s)",
                                MUIA_CycleChain, TRUE,
                            End,
                        End,
                    End,

                    Child, RectangleObject, MUIA_Weight, 0, MUIA_Rectangle_HBar, TRUE, End,

                    Child, HGroup, GroupSpacing(0),
                        Child, HSpace(0),

                        Child, VGroup,
                            Child, obj_PreviewFormat = CycleObject, GroupFrameT("Preview format"),
                                MUIA_FixWidthTxt, " Preview type ",
                                MUIA_Cycle_EntryString, "None|Auto|PAL-4:3|PAL-16:9|NTSC-4:3|NTSC-16:9",
                            End,

                            Child, obj_Preview = PreviewObject,
                                VirtualFrame,
                            End,

                            Child, HGroup,
                                TextFrame,
                                Child, HSpace(0),

                                Child, obj_VCRButtons[VCR_RECORD]  = Image("images/rec.png"),

                                Child, RectangleObject, MUIA_Weight, 0, MUIA_Rectangle_VBar, TRUE, End,

                                Child, obj_VCRButtonsGroup = HGroup,
                                    Child, obj_VCRButtons[VCR_START]   = Image("images/start.png"),
                                    Child, obj_VCRButtons[VCR_REWIND]  = Image("images/rev.png"),
                                    Child, obj_VCRButtons[VCR_PLAY]    = Image("images/play.png"),
                                    Child, obj_VCRButtons[VCR_FORWARD] = Image("images/fwd.png"),
                                    Child, obj_VCRButtons[VCR_END]     = Image("images/end.png"),
                                End,

                                Child, HSpace(0),
                            End,
                        End,

                        Child, HSpace(0),
                    End,

                    Child, RectangleObject, MUIA_Weight, 0, MUIA_Rectangle_HBar, TRUE, End,

                    Child, HGroup, GroupFrameT("Realtime Information"),
                        Child, Label2("Video format:"),
                        Child, obj_RecvVideoFmt = TextObject,
                            TextFrame,
                            MUIA_Background, MUII_TextBack,
                            MUIA_Text_Contents, "<not set>",
                            MUIA_Text_PreParse, MUIX_C,
                        End,

                        Child, Label2("Recorded size (bytes):"),
                        Child, obj_RecSize = TextObject,
                            TextFrame,
                            MUIA_Background,    MUII_TextBack,
                            MUIA_Text_Contents, "0",
                            MUIA_Text_Copy,     FALSE,
                            MUIA_Text_PreParse, MUIX_R,
                        End,
                        Child, Label2("Receiving Speed:"),
                        Child, obj_RecvSpeed = GaugeObject,
                            MUIA_Gauge_Horiz,    TRUE,
                            MUIA_Gauge_Divide,   1024,
                            MUIA_Gauge_Max,      6000,
                            MUIA_Gauge_InfoText, "%lu KB/s",
                        End,
                    End,
                End,
            End,
        End,
    End;
    //-

    if (!app)
        fail("Failed to create Application.");

    //+ Notifications
    DoMethod(win_main, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    DoMethod(obj_VCRButtons[VCR_RECORD], MUIM_Notify, MUIA_Selected, TRUE,
             app, 9, MUIM_MultiSet, MUIA_Selected, FALSE,
             obj_VCRButtons[VCR_START], obj_VCRButtons[VCR_END], obj_VCRButtons[VCR_REWIND],
             obj_VCRButtons[VCR_FORWARD], obj_VCRButtons[VCR_PLAY], NULL);

    DoMethod(obj_VCRButtons[VCR_START], MUIM_Notify, MUIA_Selected, TRUE,
             app, 9, MUIM_MultiSet, MUIA_Selected, FALSE,
             obj_VCRButtons[VCR_RECORD], obj_VCRButtons[VCR_END], obj_VCRButtons[VCR_REWIND],
             obj_VCRButtons[VCR_FORWARD], obj_VCRButtons[VCR_PLAY], NULL);

    DoMethod(obj_VCRButtons[VCR_END], MUIM_Notify, MUIA_Selected, TRUE,
             app, 9, MUIM_MultiSet, MUIA_Selected, FALSE,
             obj_VCRButtons[VCR_START], obj_VCRButtons[VCR_RECORD], obj_VCRButtons[VCR_REWIND],
             obj_VCRButtons[VCR_FORWARD], obj_VCRButtons[VCR_PLAY], NULL);

    DoMethod(obj_VCRButtons[VCR_REWIND], MUIM_Notify, MUIA_Selected, TRUE,
             app, 9, MUIM_MultiSet, MUIA_Selected, FALSE,
             obj_VCRButtons[VCR_START], obj_VCRButtons[VCR_END], obj_VCRButtons[VCR_RECORD],
             obj_VCRButtons[VCR_FORWARD], obj_VCRButtons[VCR_PLAY], NULL);

    DoMethod(obj_VCRButtons[VCR_FORWARD], MUIM_Notify, MUIA_Selected, TRUE,
             app, 9, MUIM_MultiSet, MUIA_Selected, FALSE,
             obj_VCRButtons[VCR_START], obj_VCRButtons[VCR_END], obj_VCRButtons[VCR_REWIND],
             obj_VCRButtons[VCR_RECORD], obj_VCRButtons[VCR_PLAY], NULL);

    DoMethod(obj_VCRButtons[VCR_PLAY], MUIM_Notify, MUIA_Selected, TRUE,
             app, 9, MUIM_MultiSet, MUIA_Selected, FALSE,
             obj_VCRButtons[VCR_START], obj_VCRButtons[VCR_END], obj_VCRButtons[VCR_REWIND],
             obj_VCRButtons[VCR_FORWARD], obj_VCRButtons[VCR_RECORD], NULL);

    DoMethod(obj_CaptureDeviceList, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime,
             app, 9, MUIM_Application_ReturnID, ID_DEVICE_SELECTED);

    DoMethod(obj_VCRButtons[VCR_RECORD], MUIM_Notify, MUIA_Selected, TRUE,
             app, 2, MUIM_Application_ReturnID, ID_RECORD_START);

    DoMethod(obj_VCRButtons[VCR_RECORD], MUIM_Notify, MUIA_Selected, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_RECORD_STOP);

    /* Record filename */
    DoMethod(obj_MaxLen, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime,
             app, 3, MUIM_Set, MA_CamCtrl_RecordFilename, MUIV_TriggerValue);

    /* Record max len */
    DoMethod(obj_MaxLen, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime,
             app, 3, MUIM_Set, MA_CamCtrl_RecordFilename, MUIV_TriggerValue);

    /* Record max len factor */
    DoMethod(obj_MaxLenFactor, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             app, 3, MUIM_Set, MA_CamCtrl_RecordMaxLenFactor, MUIV_TriggerValue);

    /* Video format */
    DoMethod(app, MUIM_Notify, MA_CamCtrl_VideoFmt, MUIV_EveryTime,
             obj_RecvVideoFmt, 3, MUIM_Set, MUIA_Text_Contents, MUIV_TriggerValue);
    //-

    DoGlobalInit();

    set(obj_VCRButtonsGroup, MUIA_Disabled, TRUE);
    set(win_main, MUIA_Window_Open, TRUE);

    //+ Mainloop
    while (run) {
        LONG id = DoMethod(app, MUIM_Application_Input, &mui_sigmask);

        switch (id) {
            case MUIV_Application_ReturnID_Quit:
                run = FALSE;
                break;

            case ID_DEVICE_SELECTED:
                {
                    MyDevice *dev;
                    LONG active;

                    if (!get(obj_CaptureDeviceList, MUIA_List_Active, &active) || (MUIV_List_Active_Off == active))
                        break;

                    set(obj_CaptureGroup, MUIA_Disabled, FALSE);

                    ForeachNode(&gDeviceList, dev) {
                        if (dev->Index != active) continue;
                        set(app, MA_CamCtrl_CaptureDevice, dev);
                        set(obj_VCRButtonsGroup, MUIA_Disabled, !dev->AVC);
                        break;
                    }
                }
                break;

            case ID_PACKET_LOSS:
                set(obj_VCRButtons[VCR_RECORD], MUIA_Selected, FALSE);
                MUI_Request(app, win_main, 0, "Data error", "*_OK", "Packet loss...\nRecording stopped.");
                break;
        }

        if (run) {
            sigs = mui_sigmask | helios_sigmask;
            if (sigs)
                sigs = Wait(sigs);
            else
                sigs = 0;

            if (sigs & helios_sigmask)
                run = ProcessHeliosMsg();
        }
    }
    //-

    set(win_main, MUIA_Window_Open, FALSE);

    return fail(NULL);
}
//-
