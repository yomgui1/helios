/* Copyright 2008-2013,2019 Guillaume Roguez

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

#ifndef BASE_H
#define BASE_H

#include <private/mui2intuition/mui.h>
#include <clib/macros.h>

#undef USE_INLINE_STDARG
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <clib/alib_protos.h>
#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/utility.h>

#ifndef DISPATCHER
#define DISPATCHER(Name) \
static ULONG Name##_Dispatcher(void); \
static struct EmulLibEntry GATE ##Name##_Dispatcher = { TRAP_LIB, 0, (void (*)(void)) Name##_Dispatcher }; \
static ULONG Name##_Dispatcher(void) { struct IClass *cl=(struct IClass*)REG_A0; Msg msg=(Msg)REG_A1; Object *obj=(Object*)REG_A2;
#define DISPATCHER_REF(Name) &GATE##Name##_Dispatcher
#define DISPATCHER_END }
#endif

#undef NewObject

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#endif

#define INIT_HOOK(h, f) { struct Hook *_h = (struct Hook *)(h); \
    _h->h_Entry = (APTR) HookEntry; \
    _h->h_SubEntry = (APTR) (f); }

#define PreviewObject NewObject(gPreviewMCC->mcc_Class, NULL
#define CamCtrlObject NewObject(gCamCtrlMCC->mcc_Class, NULL
#define DecoderObject NewObject(gDecoderMCC->mcc_Class, NULL
#define RingBufferObject NewObject(gRingBufferMCC->mcc_Class, NULL

struct HeliosDeviceHandle;

typedef struct MyDevice {
    struct MinNode              SysNode;
    struct HeliosDeviceHandle * Handle;
    STRPTR                      Name;
    ULONG                       Index;
    BOOL                        AVC;
} MyDevice;

enum {
    TAGBASE_YOMGUI = (TAG_USER | (0x3f4c << 16)),

    /* CamCtrl MCC */

    MM_CamCtrl_StartRT,
    MM_CamCtrl_StopRT,
    MM_CamCtrl_StartSpeedMeter,
    MM_CamCtrl_StopSpeedMeter,
    MM_CamCtrl_SpeedRefresh,
    MM_CamCtrl_StopDecoder,

    MA_CamCtrl_Error,
    MA_CamCtrl_CaptureDevice,
    MA_CamCtrl_RecordFilename,
    MA_CamCtrl_RecordMaxLength,
    MA_CamCtrl_RecordMaxLenFactor,
    MA_CamCtrl_VideoFmt,

    /* Ring Buffer MCC */

    MM_RingBuffer_Flush,
    MM_RingBuffer_Read,
    MM_RingBuffer_Write,

    MA_RingBuffer_Length,           /* IG. */
    MA_RingBuffer_Clear,            /* I.. */
    MA_RingBuffer_Overrun,          /* .GS */
    MA_RingBuffer_TotalMissed,      /* .G. */
    MA_RingBuffer_TrigOnLength,     /* I.S */
    MA_RingBuffer_Trig,             /* ..S */

    /* Decoder MCC */

    MM_Decoder_Push,
};

struct MP_RingBuffer_IO {
    ULONG   MethodID;
    UBYTE * Buffer;
    ULONG   Length;
};

struct MP_Decoder_Push {
    ULONG   MethodID;
    UBYTE *  Buffer;
    ULONG   Length;
    ULONG * SOF;
};

enum {
    ID_DEVICE_SELECTED=1,
    ID_RECORD_START,
    ID_RECORD_STOP,
    ID_PACKET_LOSS,
};

#define MV_Decoder_Error_None   0
#define MV_Decoder_Error_TooBig 1

extern void kprintf();

extern void log_Debug(STRPTR fmt, ...);
extern void log_Warn(STRPTR fmt, ...);
extern void log_Error(STRPTR fmt, ...);

extern struct Library *HeliosBase;
extern struct MUI_CustomClass *gCamCtrlMCC;
extern struct MUI_CustomClass *gPreviewMCC;
extern struct MUI_CustomClass *gRingBufferMCC;
extern struct MUI_CustomClass *gDecoderMCC;

extern struct MUI_CustomClass *CamCtrlMCC_Create(void);
extern void CamCtrlMCC_Delete(struct MUI_CustomClass *mcc);

extern struct MUI_CustomClass *DecoderMCC_Create(void);
extern void DecoderMCC_Delete(struct MUI_CustomClass *mcc);

extern struct MUI_CustomClass *RingBufferMCC_Create(void);
extern void RingBufferMCC_Delete(struct MUI_CustomClass *mcc);

extern struct MUI_CustomClass *VideoPreviewMCC_Create(void);
extern void VideoPreviewMCC_Delete(struct MUI_CustomClass *mcc);

#endif /* BASE_H */
