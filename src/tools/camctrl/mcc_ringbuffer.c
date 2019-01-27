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

typedef struct MCCData {
    APTR  Buffer;   /* The global buffer */
    ULONG Length;   /* Total length of the global buffer */
    ULONG Avail;    /* Number of bytes readable in the global buffer */
    APTR  ReadPtr;  /* Next not read byte (NULL while no data are available) */
    APTR  WritePtr; /* Where to place incoming data */
    ULONG Flags;
    UQUAD TotalMissed; /* Number of bytes loosen during all writes overflow */
    ULONG TrigLength;
} MCCData;

#define RBF_OVERRUN (1<<0)

/*==========================================================================================================================*/
//+ flush_buffer
static void flush_buffer(MCCData *data)
{
    data->ReadPtr = NULL;
    data->WritePtr = data->Buffer;
    data->Avail = 0;
    data->Flags = 0;
    data->TotalMissed = 0;
    data->TrigLength = -1; /* never trig */
}
//-

//+ mNew
static ULONG mNew(struct IClass *cl, Object *obj, struct opSet *msg)
{
    MCCData *data;
    struct TagItem *tag, *tags = msg->ops_AttrList;
    ULONG clear;

    obj = (Object *) DoSuperMethodA(cl, obj, msg);
    if (NULL == obj) return 0;

    data = INST_DATA(cl, obj);
    memset(data, 0, sizeof(*data));

    data->Length = 0;
    clear = FALSE;

    /* parse initial taglist */
    while ((tag = NextTagItem(&tags)) != NULL) {
        switch (tag->ti_Tag) {
            case MA_RingBuffer_Length : data->Length = tag->ti_Data;
            case MA_RingBuffer_Clear  : clear = tag->ti_Data;
        }
    }

    if (data->Length > 0) {
        data->Buffer = AllocMem(data->Length, MEMF_PUBLIC | (clear ? MEMF_CLEAR : 0));
        if (NULL != data->Buffer) {
            flush_buffer(data);
            return (ULONG) obj;
        }
    }

    CoerceMethod(cl, obj, OM_DISPOSE);
    return 0;
}
//-
//+ mDispose
static ULONG mDispose(struct IClass *cl, Object *obj, Msg msg)
{
    MCCData *data = INST_DATA(cl, obj);

    if (NULL != data->Buffer)
        FreeMem(data->Buffer, data->Length);

    return DoSuperMethodA(cl, obj, msg);
}
//-
//+ mGet
static ULONG mGet(struct IClass *cl, Object *obj, struct opGet *msg)
{
    MCCData *data = INST_DATA(cl, obj);
    ULONG *store = msg->opg_Storage;

    switch (msg->opg_AttrID) {
        case MA_RingBuffer_Length:
            *store = data->Length;
            return TRUE;

        case MA_RingBuffer_Overrun:
            ObtainSemaphore((struct SignalSemaphore *) obj);
            *store = RBF_OVERRUN == (data->Flags & RBF_OVERRUN);
            data->Flags &= ~RBF_OVERRUN;
            ReleaseSemaphore((struct SignalSemaphore *) obj);
            return TRUE;

        case MA_RingBuffer_TotalMissed:
            ObtainSemaphore((struct SignalSemaphore *) obj);
            *store = data->TotalMissed;
            ReleaseSemaphore((struct SignalSemaphore *) obj);
            return TRUE;
    }

    return DoSuperMethodA(cl, obj, (APTR) msg);
}
//-
//+ mSet
static ULONG mSet(struct IClass *cl, Object *obj, struct opSet * msg)
{
    MCCData *data = INST_DATA(cl, obj);
    struct TagItem *tags, *tag;
    BOOL trig;

    tags = msg->ops_AttrList;
    while (NULL != (tag = NextTagItem(&tags))) {
        switch (tag->ti_Tag) {
            case MA_RingBuffer_Overrun      : /* does nothing, just for notifications */ break;
            case MA_RingBuffer_Trig         : /* does nothing, just for notifications */ break;

            case MA_RingBuffer_TrigOnLength :
                ObtainSemaphore((struct SignalSemaphore *) obj);
                data->TrigLength = tag->ti_Data;
                trig = data->Avail >= data->TrigLength;
                ReleaseSemaphore((struct SignalSemaphore *) obj);
                if (trig)
                    set(obj, MA_RingBuffer_Trig, TRUE);
                break;
        }
    }

    return DoSuperMethodA(cl, obj, (APTR) msg);
}
//-
//+ mFlush
static ULONG mFlush(struct IClass *cl, Object *obj)
{
    MCCData *data = INST_DATA(cl, obj);

    ObtainSemaphore((struct SignalSemaphore *) obj);
    flush_buffer(data);
    ReleaseSemaphore((struct SignalSemaphore *) obj);

    return 0;
}
//-
//+ mRead
static ULONG mRead(struct IClass *cl, Object *obj, struct MP_RingBuffer_IO *msg)
{
    MCCData *data = INST_DATA(cl, obj);
    ULONG len = 0, l;

    if (0 == msg->Length)
        return 0;

    ObtainSemaphore((struct SignalSemaphore *) obj);

    /* A write has been done ?  */
    if (NULL != data->ReadPtr) {
        /* Remove the overrun flag */
        data->Flags &= ~RBF_OVERRUN;

        /* Limit the requested length to the available length (ReadPtr shall not overrun the WritePtr) */
        len = MIN(data->Avail, msg->Length);
        data->Avail -= len;

        /* Part-1 : from ReadPtr to the end of buffer */
        l = data->Length - (data->ReadPtr - data->Buffer);
        l = MIN(l, len);
        len -= l;

        CopyMem(data->ReadPtr, msg->Buffer, l);

        /* Part-2 : from start of buffer to the WritePtr - 1 */
        if (len > 0) {
            CopyMem(data->Buffer, msg->Buffer + l, len);
            data->ReadPtr = data->Buffer + len;
        } else {
            data->ReadPtr += l;
            if (data->ReadPtr >= (data->Buffer + data->Length))
                data->ReadPtr = data->Buffer;
        }
    }

    ReleaseSemaphore((struct SignalSemaphore *) obj);

    return len;
}
//-
//+ mWrite
static ULONG mWrite(struct IClass *cl, Object *obj, struct MP_RingBuffer_IO *msg)
{
    MCCData *data = INST_DATA(cl, obj);
    ULONG len = 0, l, trig;

    if (0 == msg->Length)
        return 0;

    /* Limit the write length to the buffer length */
    if (msg->Length > data->Length)
        len = data->Length;

    ObtainSemaphore((struct SignalSemaphore *) obj);

    /* Handle write overrun */
    if ((data->Avail + len) > data->Length) {
        data->Avail = data->Length;
        l = (data->Avail + len) - data->Length; /* len of extra bytes */
        data->ReadPtr += l;
        data->TotalMissed += l;

        if (0 == (data->Flags & RBF_OVERRUN)) {
            data->Flags |= RBF_OVERRUN;
            DoMethod(_app(obj), MUIM_Application_PushMethod, obj, 3, MUIM_Set, MA_RingBuffer_Overrun, TRUE);
        }
    } else
        data->Avail += len;

    trig = data->Avail >= data->TrigLength;

    /* Part-1 : from WritePtr to the end of buffer */
    l = data->Length - (data->WritePtr - data->Buffer);
    l = MIN(l, len);
    len -= l;

    CopyMem(msg->Buffer, data->WritePtr, l);

    /* Part-2 : from start of buffer to the WritePtr - 1 */
    if (len > 0) {
        CopyMem(msg->Buffer + l, data->Buffer, len);
        data->WritePtr = data->Buffer + len;
    } else {
        data->WritePtr += l;
        if (data->WritePtr >= (data->Buffer + data->Length))
            data->WritePtr = data->Buffer;
    }

    ReleaseSemaphore((struct SignalSemaphore *) obj);

    if (trig)
        DoMethod(_app(obj), MUIM_Application_PushMethod, obj, 3, MUIM_Set, MA_RingBuffer_Trig, TRUE);

    return len;
}
//-

//+ DISPATCHER(MyMCC)
DISPATCHER(MyMCC)
{
    switch (msg->MethodID) {
        case OM_NEW              : mNew(cl, obj, (APTR) msg);
        case OM_DISPOSE          : mDispose(cl, obj, (APTR) msg);
        case OM_GET              : mGet(cl, obj, (APTR) msg);
        case OM_SET              : mSet(cl, obj, (APTR) msg);
        case MM_RingBuffer_Flush : mFlush(cl, obj);
        case MM_RingBuffer_Read  : mRead(cl, obj, (APTR) msg);
        case MM_RingBuffer_Write : mWrite(cl, obj, (APTR) msg);
    }

    return DoSuperMethodA(cl, obj, msg);
}
DISPATCHER_END
//-

/*==========================================================================================================================*/

//+ RingBufferMCC_Create
struct MUI_CustomClass *RingBufferMCC_Create(void)
{
    return MUI_CreateCustomClass(NULL, MUIC_Semaphore, NULL, sizeof(MCCData), DISPATCHER_REF(MyMCC));
}
//-
//+ RingBufferMCC_Delete
void RingBufferMCC_Delete(struct MUI_CustomClass *mcc)
{
    MUI_DeleteCustomClass(mcc);
}
//-
