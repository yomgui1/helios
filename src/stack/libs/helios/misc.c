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

/*
**
** Various help routines
**
*/

#define NDEBUG

#include "helios_base.library.h"
#include "clib/helios_protos.h"

#include <devices/timer.h>
#include <exec/tasks.h>
#include <exec/semaphores.h>
#include <exec/rawfmt.h>

#include <proto/exec.h>
#include <proto/alib.h>
#include <proto/timer.h>

#include <string.h>
#include <stdarg.h>

struct HeliosSubTask
{
    struct Message         stc_Msg;
    struct SignalSemaphore stc_Lock;         /* Lock stc_Task reads (use ObtainSemaphoreShared) */
    STRPTR                 stc_TaskName;
    struct Task *          stc_Task;         /* NULL if task is dead */
    struct MsgPort *       stc_TaskPort;     /* Task msg port, valid only if task is running */
    HeliosSubTaskEntry     stc_SubEntry;
    APTR                   stc_Pool;
    struct Task *          stc_Waiter;
    ULONG                  stc_ReadySigBit;
    LONG                   stc_Success;
    struct TagItem         stc_ExtraTags[0];
};

/*----------------------------------------------------------------------------*/
/*--- LOCAL CODE SECTION -----------------------------------------------------*/

static void helios_SubTaskEntry(HeliosSubTask *ctx)
{
    BYTE rsb;
    struct Task *waiter;
    struct Task *task = FindTask(NULL);

    struct TagItem tags[] =
    {
        {HA_MsgPort, 0},
        {TAG_MORE, 0},
        {TAG_END, 0}
    };

    _INFO("+ SubTask %p-'%s', ctx %p\n", task, task->tc_Node.ln_Name, ctx);

    /* Init task data */
    ctx->stc_Task = task;
    tags[0].ti_Data = (ULONG)ctx->stc_TaskPort;
    tags[1].ti_Data = (ULONG)ctx->stc_ExtraTags;

    /* Run child entry code */
    ctx->stc_SubEntry(ctx, tags);

    Helios_TaskReady(ctx, 0); /* NIL if no waiter */

    ObtainSemaphore(&ctx->stc_Lock);
    ctx->stc_Task = NULL; /* This set forbid Helios_SignalSubTask() and Helios_SendMsgToSubTask() functions usage. */
    rsb = ctx->stc_ReadySigBit;
    ctx->stc_ReadySigBit = 32;
    waiter = ctx->stc_Waiter;
    ReleaseSemaphore(&ctx->stc_Lock);

    if (NULL != waiter)
    {
        ULONG sig = 1lu << rsb;

        _INFO("SubTask %p: wake-up task %p using signal %08x\n", task, waiter, sig);
        Signal(waiter, sig);
    }

    _INFO("- SubTask %p-'%s' says bye\n", task, task->tc_Node.ln_Name);
}


/*----------------------------------------------------------------------------*/
/*--- LIBRARY CODE SECTION ---------------------------------------------------*/

void Helios_WriteLockBase(void)
{
    LOCK_REGION(HeliosBase);
}

void Helios_ReadLockBase(void)
{
    LOCK_REGION_SHARED(HeliosBase);
}

void Helios_UnlockBase(void)
{
    UNLOCK_REGION(HeliosBase);
}

struct timerequest *Helios_OpenTimer(struct MsgPort *port, ULONG unit)
{
    struct timerequest *tr = NULL;

    tr = (struct timerequest *)CreateExtIO(port, sizeof(*tr));
    if (NULL != tr)
    {
        if (!OpenDevice("timer.device", unit, (struct IORequest *)tr, 0))
        {
            tr->tr_node.io_Command = TR_ADDREQUEST;

            _INFO("Opened timer.device request=%p\n", tr);
            return tr;
        }
        else
        {
            _ERR("Failed to open timer.device\n");
        }

        DeleteExtIO((struct IORequest *)tr);
    }
    else
    {
        _ERR("Failed to create timer request\n");
    }

    return NULL;
}

void Helios_CloseTimer(struct timerequest *tr)
{
    _INFO("Closing timer.device request @ %p\n", tr);

    CloseDevice((struct IORequest *)tr);
    DeleteExtIO((struct IORequest *)tr);
}

void Helios_DelayMS(ULONG milli)
{
    struct MsgPort port;
    struct timerequest tr;

    /* Reply port setup (we use the task's port) */
    port.mp_Flags   = PA_SIGNAL;
    port.mp_SigBit  = SIGB_SINGLE;
    port.mp_SigTask = FindTask(NULL);
    NEWLIST(&port.mp_MsgList);

    /* Timer request init. */
    CopyMem(&HeliosBase->hb_TimeReq, &tr, sizeof(tr));
    tr.tr_node.io_Message.mn_ReplyPort = &port;
    tr.tr_node.io_Command = TR_ADDREQUEST;
    tr.tr_time.tv_secs  = milli / 1000;
    tr.tr_time.tv_micro = (milli % 1000) * 1000;

    DoIO((struct IORequest *)&tr);
}

ULONG Helios_SignalSubTask(HeliosSubTask *ctx, ULONG sigmask)
{
    ULONG err;

    ObtainSemaphoreShared(&ctx->stc_Lock);

    if (NULL != ctx->stc_Task)
    {
        Signal(ctx->stc_Task, sigmask);
        err = HERR_NOERR;
    }
    else
    {
        _INFO("Subtask %p killed\n", ctx->stc_Task);
        err = HERR_SYSTEM;
    }

    ReleaseSemaphore(&ctx->stc_Lock);

    return err;
}

LONG Helios_SendMsgToSubTask(HeliosSubTask *ctx, struct Message *msg)
{
    ULONG err;

    ObtainSemaphoreShared(&ctx->stc_Lock);

    if (NULL != ctx->stc_Task)
    {
        msg->mn_Node.ln_Type = NT_MESSAGE;
        _INFO("Send msg %p to subtask %p using replyport %p\n", msg, ctx->stc_Task, msg->mn_ReplyPort);
        PutMsg(ctx->stc_TaskPort, msg);
        err = HERR_NOERR;
    }
    else
    {
        _INFO("Subtask %p killed\n", ctx->stc_Task);
        err = HERR_SYSTEM;
    }

    ReleaseSemaphore(&ctx->stc_Lock);

    return err;
}

LONG Helios_DoMsgToSubTask(HeliosSubTask *ctx, struct Message *msg, struct MsgPort *replyport)
{
    BOOL allocport = (NULL == replyport) || (NULL != msg->mn_ReplyPort);
    LONG err;

    if (allocport)
    {
        replyport = CreateMsgPort();
        if (NULL == replyport)
        {
            _ERR("CreateMsgPort() failed\n");
            return HERR_NOMEM;
        }
    }

    if (NULL != replyport)
    {
        msg->mn_ReplyPort = replyport;
    }

    err = Helios_SendMsgToSubTask(ctx, msg);
    if (HERR_NOERR != err)
    {
        goto end;
    }

    WaitPort(msg->mn_ReplyPort);
    GetMsg(msg->mn_ReplyPort);

end:
    if (allocport)
    {
        DeleteMsgPort(replyport);
    }

    return err;
}

HeliosSubTask *Helios_CreateSubTaskA(CONST_STRPTR name,
                                     HeliosSubTaskEntry entry,
                                     struct TagItem *tags)
{
    HeliosSubTask *ctx;
    struct TagItem *tag, *next_tag;
    LONG priority = 0;
    APTR pool = NULL;
    ULONG extra_tags_cnt = 0, size, name_len;

    /* Parse tags */
    next_tag = tags;
    while (NULL != (tag = NextTagItem(&next_tag)))
    {
        switch (tag->ti_Tag)
        {
            case HA_Pool: pool = (APTR)tag->ti_Data; tag->ti_Tag = TAG_IGNORE; break;
            case TASKTAG_PRI: priority = tag->ti_Data; tag->ti_Tag = TAG_IGNORE; break;
            default: extra_tags_cnt++;
        }
    }

    name_len = strlen(name) + 1;
    size = sizeof(HeliosSubTask) + sizeof(struct TagItem) * (extra_tags_cnt+1) + name_len;

    if (NULL != pool)
    {
        ctx = AllocVecPooled(pool, size);
    }
    else
    {
        ctx = AllocVec(size, MEMF_PUBLIC | MEMF_CLEAR);
    }

    if (NULL != ctx)
    {
        ULONG i;

        InitSemaphore(&ctx->stc_Lock);
        ctx->stc_TaskName = (APTR)ctx + (size - name_len);
        strcpy(ctx->stc_TaskName, name);

        /* Duplicate tags */
        i = 0;
        while ((NULL != (tag = NextTagItem(&tags))) && (i < extra_tags_cnt))
        {
            ctx->stc_ExtraTags[i].ti_Tag = tag->ti_Tag;
            ctx->stc_ExtraTags[i].ti_Data = tag->ti_Data;
            i++;
        }
        ctx->stc_ExtraTags[i].ti_Tag = TAG_END;

        /* Create and run the new task */
        ctx->stc_Msg.mn_Node.ln_Type = NT_MESSAGE;
        ctx->stc_Msg.mn_ReplyPort = NULL;
        ctx->stc_Msg.mn_Length = sizeof(*ctx);
        ctx->stc_Pool = pool;
        ctx->stc_SubEntry = entry;
        ctx->stc_ReadySigBit = -1;
        ctx->stc_Waiter = NULL;
        ctx->stc_Success = -1;
        ctx->stc_Task = NewCreateTask(TASKTAG_CODETYPE,    CODETYPE_PPC,
                                      TASKTAG_NAME,        (ULONG)ctx->stc_TaskName,
                                      TASKTAG_PRI,         priority,
                                      TASKTAG_STACKSIZE,   32768,
                                      TASKTAG_STARTUPMSG,  (ULONG)ctx,
                                      TASKTAG_TASKMSGPORT, (ULONG)&ctx->stc_TaskPort,
                                      TASKTAG_PC,          (ULONG)helios_SubTaskEntry,
                                      TASKTAG_PPC_ARG1,    (ULONG)ctx,
                                      TAG_DONE);
        if (NULL != ctx->stc_Task)
        {
            return ctx;
        }
        else
        {
            _ERR("NewCreateTask(...) failed\n");
        }
    }
    else
    {
        _ERR("Subtask ctx allocation failed\n");
    }

    if (NULL != pool)
    {
        FreeVecPooled(pool, ctx);
    }
    else
    {
        FreeVec(ctx);
    }

    return NULL;
}

LONG Helios_KillSubTask(HeliosSubTask *ctx)
{
    HeliosMsg msg;
    struct Task *task;
    struct MsgPort port;

    ObtainSemaphoreShared(&ctx->stc_Lock);
    task = ctx->stc_Task;
    if (task != NULL)
    {
        /* Reply port setup (we use the task's port) */
        port.mp_Flags   = PA_SIGNAL;
        port.mp_SigBit  = SIGB_SINGLE;
        port.mp_SigTask = FindTask(NULL);
        NEWLIST(&port.mp_MsgList);

        ctx->stc_Msg.mn_ReplyPort = &port;
    }
    ReleaseSemaphore(&ctx->stc_Lock);

    if (FindTask(NULL) == task)
    {
        _ERR("A subtask can't kill itself!\n");
        return HERR_SYSTEM;
    }

    if (NULL == task)
    {
        _ERR("Subtask already killed!\n");
        return HERR_NOERR;
    }

    _INFO("Send kill msg to task %p-'%s'\n", ctx->stc_Task, ctx->stc_Task->tc_Node.ln_Name);

    msg.hm_Msg.mn_Node.ln_Type = NT_MESSAGE;
    msg.hm_Msg.mn_ReplyPort = NULL;
    msg.hm_Msg.mn_Length = sizeof(msg);
    msg.hm_Type = HELIOS_MSGTYPE_TASKKILL;

    if (HERR_NOMEM == Helios_DoMsgToSubTask(ctx, (struct Message *)&msg, NULL))
    {
        return HERR_NOMEM;
    }

    /* Wait for the end of task */
    _INFO("Wait for death of subtask %p\n", task);
    for (;;)
    {
        Wait(SIGF_SINGLE);
        if (&ctx->stc_Msg == GetMsg(&port))
        {
            break;
        }
    }
    _INFO("Task %p not longer exist\n", task);

    if (NULL != ctx->stc_Pool)
    {
        FreeVecPooled(ctx->stc_Pool, ctx);
    }
    else
    {
        FreeVec(ctx);
    }

    return HERR_NOERR;
}

LONG Helios_WaitTaskReady(HeliosSubTask *ctx, LONG sigs)
{
    struct Task *task;
    LONG rsig = 0;

    ObtainSemaphore(&ctx->stc_Lock);
    {
        task = ctx->stc_Task;
        if (NULL != task)
            if (32 != ctx->stc_ReadySigBit)
            {
                rsig = AllocSignal(-1);
                if (-1 != rsig)
                {
                    ctx->stc_Waiter = FindTask(NULL);
                    ctx->stc_ReadySigBit = rsig;
                }
            }
    }
    ReleaseSemaphore(&ctx->stc_Lock);

    /* task already finished */
    if ((NULL == task) || (0 == rsig))
    {
        _ERR("Subtask %p finished, killed or busy\n", task);
        return ctx->stc_Success?0:-1;
    }

    if (-1 == rsig)
    {
        _ERR("Failed to allocate signal on task %p\n", FindTask(NULL));
        return -1;
    }

    sigs |= 1ul << rsig;

    _INFO("Wait task ready from task %p (sigs=%08x)\n", task, sigs);
    sigs = Wait(sigs);
    _INFO("Signal from task %p (%08x), continue\n", task, sigs);

    FreeSignal(rsig);
    if (sigs & (1ul << rsig))
    {
        return ctx->stc_Success?0:-1;
    }
    return sigs;
}

void Helios_TaskReady(HeliosSubTask *ctx, BOOL success)
{
    BYTE rsb;
    struct Task *waiter;

    ObtainSemaphore(&ctx->stc_Lock);
    {
        rsb = ctx->stc_ReadySigBit;
        ctx->stc_ReadySigBit = 32;
        waiter = ctx->stc_Waiter;
        ctx->stc_Waiter = NULL;
        ctx->stc_Success = success;
    }
    ReleaseSemaphore(&ctx->stc_Lock);

    if (NULL != waiter)
    {
        ULONG sig = 1lu << rsb;

        _INFO("SubTask %p: wake-up task %p using signal %08x\n", FindTask(NULL), waiter, sig);
        Signal(waiter, sig);
    }
}

LONG Helios_IsSubTaskKilled(HeliosSubTask *ctx)
{
    LONG killed;

    ObtainSemaphoreShared(&ctx->stc_Lock);
    killed = ctx->stc_Task == NULL;
    ReleaseSemaphore(&ctx->stc_Lock);

    return killed;
}

void Helios_AddEventListener(HeliosEventListenerList *list, HeliosEventMsg *node)
{
    LOCK_REGION(list);
    ADDTAIL(&list->ell_SysList, node);
    UNLOCK_REGION(list);
}

void Helios_RemoveEventListener(HeliosEventListenerList *list, HeliosEventMsg *node)
{
    LOCK_REGION(list);
    REMOVE(node);
    UNLOCK_REGION(list);
}

/* WARNING: this function assumes that the list is protected against modifications */
void Helios_SendEvent(HeliosEventListenerList *list, ULONG event, ULONG result)
{
    HeliosEventMsg *msg, *node, *next;
    struct timeval tv;
    struct Library *TimerBase;

    TimerBase = (struct Library *)HeliosBase->hb_TimeReq.tr_node.io_Device;
    GetSysTime(&tv);

    LOCK_REGION(list);
    {
        ForeachNodeSafe(list, node, next)
        {
            if (0 == (node->hm_EventMask & event))
            {
                continue;
            }

            if (HELIOS_MSGTYPE_FAST_EVENT == node->hm_Type)
            {
                REMOVE(node);
                node->hm_Time = tv;
                node->hm_Result = result;
                ReplyMsg(&node->hm_Msg);
            }
            else
            {
                msg = AllocMem(sizeof(*msg), MEMF_PUBLIC);
                if (NULL == msg)
                {
                    _ERR("Msg alloc failed\n");
                    continue;
                }

                msg->hm_Msg.mn_ReplyPort = NULL;
                msg->hm_Msg.mn_Length = sizeof(*msg);
                msg->hm_Msg.mn_Node.ln_Type = NT_FREEMSG;
                msg->hm_Type = HELIOS_MSGTYPE_EVENT;
                msg->hm_Time = tv;
                msg->hm_EventMask = event;
                msg->hm_Result = result;
                msg->hm_UserData = node->hm_UserData;

                PutMsg(node->hm_Msg.mn_ReplyPort, &msg->hm_Msg);
            }
        }
    }
    UNLOCK_REGION(list);
}

LONG Helios_VReportMsg(ULONG type, CONST_STRPTR label, CONST_STRPTR fmt, va_list args)
{
    ULONG len=0, label_len, msg_size;
    HeliosReportMsg *msg;
    struct safe_buf sb;
    va_list args_dup;

    va_copy(args_dup, args);

    /* len = label + formated fmt string */
    VNewRawDoFmt(fmt, (APTR)RAWFMTFUNC_COUNT, (APTR)&len, args);
    label_len = strlen(label);

    /* +2 for strings walls */
    msg_size = sizeof(HeliosReportMsg)+label_len+1+len+1;
    msg = AllocVecPooled(HeliosBase->hb_MemPool, msg_size);
    if (NULL != msg)
    {
        msg->hrm_SysMsg.mn_Length = msg_size;
        msg->hrm_TypeBit = type;
        msg->hrm_Label = (APTR)msg + sizeof(HeliosReportMsg);
        msg->hrm_Msg = (APTR)msg->hrm_Label + label_len + 1;

        CopyMem((STRPTR)label, (STRPTR)msg->hrm_Label, label_len);
        ((STRPTR)msg->hrm_Label)[label_len] = '\0';

        sb.buf = (STRPTR)msg->hrm_Msg;
        sb.size = len;
        VNewRawDoFmt(fmt, utils_SafePutChProc, (STRPTR)&sb, args_dup);
        len -= sb.size;
        ((STRPTR)msg->hrm_Msg)[len] = '\0';

        LOCK_REGION(HeliosBase);
        {
            //kprintf("RptMsg: [%s] %s\n", msg->hrm_Label, msg->hrm_Msg);
            ADDHEAD(&HeliosBase->hb_ReportList, msg);
        }
        UNLOCK_REGION(HeliosBase);

        Helios_SendEvent(&HeliosBase->hb_Listeners, HEVTF_NEW_REPORTMSG, 0);
        return len;
    }

    return -1;
}

LONG Helios_ReportMsg(ULONG type, CONST_STRPTR label, CONST_STRPTR fmt, ...)
{
    ULONG res;
    va_list va;

    va_start(va, fmt);
    res = Helios_VReportMsg(type, label, fmt, va);
    va_end(va);

    return res;
}

HeliosReportMsg *Helios_GetNextReportMsg(void)
{
    HeliosReportMsg *msg;

    Helios_WriteLockBase();
    msg = (HeliosReportMsg *)REMTAIL(&HeliosBase->hb_ReportList);
    Helios_UnlockBase();

    return msg;
}

void Helios_FreeReportMsg(HeliosReportMsg *msg)
{
    FreeVecPooled(HeliosBase->hb_MemPool, msg);
}

UWORD Helios_ComputeCRC16(const QUADLET *data, ULONG len)
{
    return utils_GetBlockCRC16((QUADLET *)data, len);
}

