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

#include "base.h"

#include "libraries/helios.h"

#include "proto/helios.h"
#include <proto/asyncio.h>

#include <stdio.h>
#include <stdlib.h>

#define COUNT_REFRESH 500

#define IEC61883_CIP_HEADER_LEN (2 * sizeof(QUADLET))
#define IEC61883_TAG_WITH_CIP 2
#define IEC61883_MPEG2_TSP_SIZE 188
#define TSP_SPH_SIZE (IEC61883_MPEG2_TSP_SIZE + 4)

#define VIDEO_FMT_NONE (-1)
#define VIDEO_FMT_NTSC 0x00
#define VIDEO_FMT_PAL  0x80

typedef struct IsoUserData {
    Object *    App;
    Object *    Decoder;
    AsyncFile * File;

    struct SignalSemaphore Lock;

    UQUAD    Total;
    UQUAD    RecordMaxLength;
    ULONG    VideoFmt;
    BOOL     DetectSOF;
    BOOL     Record;
    UBYTE    LastDBC;
} IsoUserData;

typedef struct CamCtrlMCCData {
    struct MUI_InputHandlerNode ihnode;

    Object *RecSizeObj;
    Object *RecvSpeedObj;

    STRPTR RecordFilename;
    ULONG RecordMaxLenFactor;
    UQUAD RecordMaxLength;
    UQUAD TotalPrev, TotalLastTime;

    MyDevice *         CaptureDevice;
    HeliosIsoContext * IsoCtx;
    IsoUserData        IsoUData; /* shared with the isochronous callback */
} MCCData;

static struct Library *AsyncIOBase = NULL;

/*==========================================================================================================================*/

//+ ppc_getcounter
static UQUAD ppc_getcounter(void)
{
    register ULONG tbu, tb, tbu2;

  loop:
    __asm volatile ("mftbu %0" : "=r" (tbu) );
    __asm volatile ("mftb  %0" : "=r" (tb)  );
    __asm volatile ("mftbu %0" : "=r" (tbu2));
    if (tbu != tbu2) goto loop;

    return (((UQUAD) tbu) << 32) + tb;
}
//-
//+ set_max_len
static void set_max_len(Object *obj, MCCData *data)
{
    data->IsoUData.RecordMaxLength = data->RecordMaxLength * 1 << (data->RecordMaxLenFactor * 10);
}
//-
//+ IsoCallback
static void IsoCallback(HeliosIsoContext *ctx, HeliosIRBuffer *buffer, ULONG status)
{
    IsoUserData *udata = ctx->UserData;
    UWORD dbs_fn_qpc_sph;
    UBYTE fmt, fdf, dbc;
    QUADLET *header = buffer->Header;
    APTR payload = buffer->Payload;
    BOOL stop = FALSE;

    if (0 == buffer->HeaderLength)
        return;

    dbs_fn_qpc_sph = ((*header) >> 10) & 0x3fff;
    dbc = *header & 0xff;
    fmt = ((*(header + 1)) >> 24) & 0x3f;
    fdf = ((*(header + 1)) >> 16) & 0xff;

    if ((0x1e00 == dbs_fn_qpc_sph) && (0x00 == fmt)) { /* DV */
        ULONG len = (dbs_fn_qpc_sph >> 8) << 4;
        ULONG sof;

        if (udata->VideoFmt != VIDEO_FMT_NONE) {
            /* inconsistent video format */
            if (fdf != udata->VideoFmt)
                return;
        } else {
            STRPTR s;

            udata->VideoFmt = fdf;
            switch (fdf) {
                case VIDEO_FMT_NTSC: s = "NTSC"; break;
                case VIDEO_FMT_PAL : s = "PAL"; break;
                default: s = "unknown";
            }

            DoMethod(udata->App, MUIM_Application_PushMethod, udata->App, 3, MUIM_Set, MA_CamCtrl_VideoFmt, s);
        }

        /* Drop empty packets */
        if (0 == buffer->PayloadLength)
            return;

        /* Checking packet continuity */
        if (((udata->LastDBC + 1) & 0xff) != dbc) {
            DoMethod(udata->App, MUIM_Application_PushMethod, udata->App, 2, MUIM_Application_ReturnID, ID_PACKET_LOSS);
            DoMethod(udata->App, MUIM_Application_PushMethod, udata->App, 1, MM_CamCtrl_StopRT);
            return;
        }

        udata->LastDBC = dbc;

        /* File size limiter */
        if ((udata->RecordMaxLength > 0) && ((udata->Total + len) >= udata->RecordMaxLength)) {
            len = udata->RecordMaxLength - udata->Total;
            stop = TRUE;
        }

        /* Push the packet in the decoder (with SOF detection if needed) */
        sof = FALSE;
        if (len != DoMethod(udata->Decoder, MM_Decoder_Push, payload, len, udata->DetectSOF ? &sof : NULL)) {
            DoMethod(udata->App, MUIM_Application_PushMethod, udata->App, 2, MM_CamCtrl_StopDecoder);
            DoMethod(udata->App, MUIM_Application_PushMethod, udata->App, 3, MUIM_Set, MA_CamCtrl_Error, "Decoding error occured");
        }

        if (!udata->Record && sof) {
            udata->Record = TRUE;
            udata->DetectSOF = FALSE;
        }

        /* File recording ? */
        if (udata->Record) {
            ULONG x = WriteAsync(udata->File, payload, len);

            if (len != x) {
                DoMethod(udata->App, MUIM_Application_PushMethod, udata->App, 3, MUIM_Set, MA_CamCtrl_Error, "File recording error occured");
                stop = TRUE;
            }

            len = x;
        }

        ObtainSemaphore(&udata->Lock);
        udata->Total += len;
        ReleaseSemaphore(&udata->Lock);
    } else if ((0x01b1 == dbs_fn_qpc_sph) && (0x20 == fmt)) { /* MPEG2-TS */
        LONG payload_len = buffer->PayloadLength;

        payload += 4; /* skip SPH */

        /* Drop empty packets */
        if (0 == payload_len)
            return;

        if (0 == udata->Total) {
            DoMethod(udata->App, MUIM_Application_PushMethod, udata->App, 3, MUIM_Set, MA_CamCtrl_VideoFmt, "ND");
        }

        /* File size limiter */
        if (( udata->RecordMaxLength> 0) && ((udata->Total + payload_len) >= udata->RecordMaxLength)) {
            payload_len = udata->RecordMaxLength - udata->Total;

            /* MAXLEN_LASTPACKET is forced for this codec */

            stop = TRUE;
        }

        for (; payload_len > IEC61883_MPEG2_TSP_SIZE; payload_len -= TSP_SPH_SIZE, payload += TSP_SPH_SIZE) {
            WriteAsync(udata->File, payload, IEC61883_MPEG2_TSP_SIZE);

            ObtainSemaphore(&udata->Lock);
            udata->Total += IEC61883_MPEG2_TSP_SIZE;
            ReleaseSemaphore(&udata->Lock);
        }
    }

    if (stop) {
        Helios_IsoContext_Stop(ctx);
        DoMethod(udata->App, MUIM_Application_PushMethod, udata->App, 1, MM_CamCtrl_StopRT);
    }
}
//-

/*==========================================================================================================================*/

//+ mDispose
static ULONG mDispose(struct IClass *cl, Object *obj, Msg msg)
{
    DoMethod(obj, MM_CamCtrl_StopRT);
    DoMethod(obj, MM_CamCtrl_StopSpeedMeter);

    return DoSuperMethodA(cl, obj, msg);
}
//-
//+ mSet
static ULONG mSet(struct IClass *cl, Object *obj, struct opSet * msg)
{
    MCCData *data = INST_DATA(cl, obj);
    struct TagItem *tags, *tag;

    tags = msg->ops_AttrList;
    while (NULL != (tag = NextTagItem(&tags))) {
        switch (tag->ti_Tag) {
            case MA_CamCtrl_RecordFilename:
                data->RecordFilename = (STRPTR) tag->ti_Data;
                break;

            case MA_CamCtrl_RecordMaxLength:
                data->RecordMaxLength = strtoull((STRPTR) tag->ti_Data, NULL, 10);
                set_max_len(obj, data);
                break;

            case MA_CamCtrl_RecordMaxLenFactor:
                data->RecordMaxLenFactor = tag->ti_Data;
                set_max_len(obj, data);
                break;

            case MA_CamCtrl_CaptureDevice:
                data->CaptureDevice = (APTR) tag->ti_Data;
                break;
        }
    }

    return DoSuperMethodA(cl, obj, (APTR) msg);
}
//-
//+ mStartRT
static ULONG mStartRT(struct IClass *cl, Object *obj, Msg msg)
{
    MCCData *data = INST_DATA(cl, obj);

    /* Must be stopped before */
    if (NULL != data->IsoCtx)
        return TRUE;

    if ((NULL == data->RecordFilename) || ('\0' == data->RecordFilename[0]))
        return FALSE;

    if (NULL == data->CaptureDevice)
        return FALSE;

    /*if (!get(obj_CacheTime, MUIA_Numeric_Value, &value))
        return FALSE;*/

    data->IsoUData.Decoder = DecoderObject,
        MUIA_Process_SourceClass, cl,
        MUIA_Process_SourceObject, obj,
        MUIA_Process_Name, "Test MUI Process",
        MUIA_Process_AutoLaunch, FALSE,
    End;

    if (NULL != data->IsoUData.Decoder) {

        /* Reset grab context */
        InitSemaphore(&data->IsoUData.Lock);
        data->IsoUData.App = obj;
        data->IsoUData.VideoFmt = VIDEO_FMT_NONE;
        data->IsoUData.DetectSOF = TRUE;
        data->IsoUData.LastDBC = -1;
        data->IsoUData.Record = FALSE;

        data->IsoUData.File = OpenAsync(data->RecordFilename, MODE_WRITE, 1024*1024); /* 1MB of write cache is reasonable */
        if (NULL != data->IsoUData.File) {
            data->IsoUData.Total = 0;

            /* Create an isochronous receive context using the just read broadcast channel */
            data->IsoCtx = Helios_IsoContext_Create(data->CaptureDevice->Handle->Bus, HELIOS_ISO_RECEIVE_CONTEXT,
                                               HTTAG_ISO_DMA_MODE,      HELIOS_ISO_PACKET_PER_BUFFER,
                                               HTTAG_ISO_IR_CALLBACK,   (ULONG) IsoCallback,
                                               HTTAG_ISO_HEADER_LENGTH, IEC61883_CIP_HEADER_LEN,
                                               HTTAG_ISO_BUFFER_SIZE,   1024, /* max iso packet size when speed = S100 */
                                               HTTAG_ISO_BUFFER_COUNT,  8000 * 2, /* 8000 cycles per seconds by 2 seconds */
                                               HTTAG_ISO_PAYLOAD_ALIGN, 16, /* optimisations requires that */
                                               TAG_DONE);
            if (NULL != data->IsoCtx) {
                data->IsoCtx->UserData = &data->IsoUData;
                if (Helios_IsoContext_Start(data->IsoCtx, 63, IEC61883_TAG_WITH_CIP))
                    return TRUE;
            }
        }
    }

    CoerceMethod(cl, obj, MM_CamCtrl_StopRT);

    return FALSE;
}
//-
//+ mStopRT
static ULONG mStopRT(struct IClass *cl, Object *obj, Msg msg)
{
    MCCData *data = INST_DATA(cl, obj);

    if (NULL != data->IsoCtx) {
        Helios_IsoContext_Destroy(data->IsoCtx);
        data->IsoCtx = NULL;
    }

    if (NULL != data->IsoUData.Decoder)
        MUI_DisposeObject(data->IsoUData.Decoder);

    if (NULL != data->IsoUData.File) {
        CloseAsync(data->IsoUData.File);
        data->IsoUData.File = NULL;
    }

    return TRUE;
}
//-
//+ mSartSpeedMeter
static ULONG mStartSpeedMeter(struct IClass *cl, Object *obj, Msg msg)
{
    MCCData *data;

    data = INST_DATA(cl, obj);

    data->ihnode.ihn_Object  = obj;
    data->ihnode.ihn_Millis  = COUNT_REFRESH;
    data->ihnode.ihn_Method  = MM_CamCtrl_SpeedRefresh;
    data->ihnode.ihn_Flags   = MUIIHNF_TIMER;

    data->TotalLastTime = 0;

    DoMethod(_app(obj), MUIM_Application_AddInputHandler, &data->ihnode);

    return TRUE;
}
//-
//+ mStopSpeedMeter
static ULONG mStopSpeedMeter(struct IClass *cl, Object *obj, Msg msg)
{
    MCCData *data = INST_DATA(cl, obj);

    DoMethod(_app(obj), MUIM_Application_RemInputHandler, &data->ihnode);

    return TRUE;
}
//-
//+ mSpeedRefresh
static ULONG mSpeedRefresh(struct IClass *cl, Object *obj, Msg msg)
{
    MCCData *data = INST_DATA(cl, obj);
    UQUAD value, time2;
    ULONG speed;
    char buf[80];

    ObtainSemaphore(&data->IsoUData.Lock);
    value = data->IsoUData.Total;
    time2 = ppc_getcounter();
    ReleaseSemaphore(&data->IsoUData.Lock);

    if (data->TotalLastTime > 0)
        speed = ((value - data->TotalPrev) * 33333333) / (time2 - data->TotalLastTime);
    else
        speed = 0;

    data->TotalLastTime = time2;
    data->TotalPrev = value;

    snprintf(buf, sizeof(buf), "%llu", value);
    set(data->RecSizeObj, MUIA_Text_Contents, (ULONG) buf);
    set(data->RecvSpeedObj, MUIA_Gauge_Current, speed);

    return TRUE;
}
//-

//+ DISPATCHER(CamCtrlMCC)
DISPATCHER(CamCtrlMCC)
{
    switch (msg->MethodID) {
        case OM_DISPOSE                 : mDispose(cl, obj, (APTR) msg);
        case OM_SET                     : mSet(cl, obj, (APTR) msg);
        case MM_CamCtrl_StartRT         : return mStartRT(cl, obj, (APTR) msg);
        case MM_CamCtrl_StopRT          : return mStopRT(cl, obj, (APTR) msg);
        case MM_CamCtrl_StartSpeedMeter : return mStartSpeedMeter(cl, obj, (APTR) msg);
        case MM_CamCtrl_StopSpeedMeter  : return mStopSpeedMeter(cl, obj, (APTR) msg);
        case MM_CamCtrl_SpeedRefresh    : return mSpeedRefresh(cl, obj, (APTR) msg);
    }

    return DoSuperMethodA(cl, obj, msg);
}
DISPATCHER_END
//-

/*==========================================================================================================================*/

//+ CamCtrlMCC_Create
struct MUI_CustomClass *CamCtrlMCC_Create(void)
{
    struct MUI_CustomClass *mcc;

    AsyncIOBase = OpenLibrary("asyncio.library", 0);
    if (AsyncIOBase) {
        mcc = MUI_CreateCustomClass(NULL, MUIC_Application, NULL, sizeof(MCCData), DISPATCHER_REF(CamCtrlMCC));
        if (NULL != mcc)
            return mcc;

        CloseLibrary(AsyncIOBase);
    } else
        log_Error("can't open asyncio.library.");

    return NULL;
}
//-
//+ CamCtrlMCC_Delete
void CamCtrlMCC_Delete(struct MUI_CustomClass *mcc)
{
    MUI_DeleteCustomClass(mcc);
    if (NULL != AsyncIOBase) CloseLibrary(AsyncIOBase);
}
//-
