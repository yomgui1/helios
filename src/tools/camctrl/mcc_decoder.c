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

#include <dos/dosextens.h>

#define RINGBUFFER_LENGTH (1*1024*1024)

#define FRAME_MAX_WIDTH 1440
#define FRAME_MAX_HEIGHT 1080
#define FRAME_PIXEL_BYTELENGTH 2 /* 4:2:0 and 4:1:1 are 1.5 */
#define FRAME_COUNT 2
#define FRAME_BUFFER_LENGTH (FRAME_MAX_WIDTH*FRAME_MAX_HEIGHT*FRAME_PIXEL_BYTELENGTH)

typedef struct MCCData
{
    Object * RingBufferObj;
    UBYTE *  FrameBuffer;
    UBYTE *  DisplayedFrame;
    UBYTE *  ModifiedFrame;
    ULONG DataReadySignal;
    ULONG Error;    /* Last error event */
    BOOL  Run;      /* True if the decoder accepts data */
    BOOL  Slow;     /* True if the read rate is too slow */
} MCCData;

/*==========================================================================================================================*/

static ULONG mNew(struct IClass *cl, Object *obj, struct opSet *msg)
{
    MCCData *data;
    struct TagItem *tag, *tags = msg->ops_AttrList;

    obj = (Object *) DoSuperMethodA(cl, obj, msg);
    if (NULL == obj)
    {
        return 0;
    }

    data = INST_DATA(cl, obj);
    memset(data, 0, sizeof(*data));

    /* parse initial taglist */
    while ((tag = NextTagItem(&tags)) != NULL)
    {
        switch (tag->ti_Tag)
        {
        }
    }

    // *INDENT-OFF*
    data->RingBufferObj = RingBufferObject,
        MA_RingBuffer_Length, RINGBUFFER_LENGTH,
    End;
    // *INDENT-ON*

    if (NULL != data->RingBufferObj)
    {
        data->FrameBuffer = AllocMem(FRAME_COUNT * FRAME_BUFFER_LENGTH, MEMF_PUBLIC);
        if (NULL != data->FrameBuffer)
        {
            data->DisplayedFrame = data->FrameBuffer;
            data->ModifiedFrame = data->FrameBuffer + FRAME_BUFFER_LENGTH;

            return (ULONG) obj;
        }
    }

    CoerceMethod(cl, obj, OM_DISPOSE);
    return 0;
}

static ULONG mDispose(struct IClass *cl, Object *obj, Msg msg)
{
    MCCData *data = INST_DATA(cl, obj);

    if (NULL != data->FrameBuffer)
    {
        FreeMem(data->FrameBuffer, FRAME_COUNT * FRAME_BUFFER_LENGTH);
    }

    if (NULL != data->RingBufferObj)
    {
        MUI_DisposeObject(data->RingBufferObj);
    }

    return DoSuperMethodA(cl, obj, msg);
}

static ULONG mProcess(struct IClass *cl, Object *obj, struct MUIP_Process_Process *msg)
{
    MCCData *data = INST_DATA(cl, obj);
    struct Process *myproc = (struct Process *) FindTask(NULL);
    APTR oldwindowptr = myproc->pr_WindowPtr;
    LONG data_ready;

    kprintf("[Proc %p] obj %p, proc %p\n", myproc, obj, msg->proc);

    myproc->pr_WindowPtr = (APTR) -1;

    data_ready = AllocSignal(-1);
    if (-1 == data_ready)
    {
        ObtainSemaphore((struct SignalSemaphore *) obj);
        data->DataReadySignal = data_ready;
        ReleaseSemaphore((struct SignalSemaphore *) obj);

        while (!*msg->kill)
        {
            ULONG sig = 0;

            sig = Wait(SIGBREAKF_CTRL_C | data_ready);
        }

        ObtainSemaphore((struct SignalSemaphore *) obj);
        data->DataReadySignal = -1;
        ReleaseSemaphore((struct SignalSemaphore *) obj);
        FreeSignal(data_ready);
    }

    myproc->pr_WindowPtr = oldwindowptr;
    kprintf("[Proc %p] bye\n");
    return 0;
}

static ULONG mPush(struct IClass *cl, Object *obj, struct MP_Decoder_Push *msg)
{
    MCCData *data = INST_DATA(cl, obj);

    /* Need to detect where a StartOfFrame starts ? */
    if (NULL != msg->SOF)
    {
        *msg->SOF = (0 == (msg->Buffer[0] >> 5)) && (0 == (msg->Buffer[1] >> 4));
    }

    return DoMethod(data->RingBufferObj, MM_RingBuffer_Write, msg->Buffer, msg->Length);
}


DISPATCHER(MyMCC)
{
    switch (msg->MethodID)
    {
        case OM_NEW               : mNew(cl, obj, (APTR) msg);
        case OM_DISPOSE           : mDispose(cl, obj, (APTR) msg);
        case MUIM_Process_Process : return mProcess(cl, obj, (APTR) msg);
        case MM_Decoder_Push      : return mPush(cl, obj, (APTR) msg);
    }

    return DoSuperMethodA(cl, obj, msg);
}
DISPATCHER_END


/*==========================================================================================================================*/

struct MUI_CustomClass *DecoderMCC_Create(void)
{
    return MUI_CreateCustomClass(NULL, MUIC_Process, NULL, sizeof(MCCData), DISPATCHER_REF(MyMCC));
}

void DecoderMCC_Delete(struct MUI_CustomClass *mcc)
{
    MUI_DeleteCustomClass(mcc);
}

