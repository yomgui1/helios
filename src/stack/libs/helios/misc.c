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

/* $Id$
** This file is copyrights 2008-2012 by Guillaume ROGUEZ.
**
** Various routines
*/

#include "private.h"

#include <devices/timer.h>
#include <exec/tasks.h>
#include <exec/semaphores.h>
#include <exec/rawfmt.h>

#include <proto/alib.h>
#include <proto/timer.h>

#include <string.h>
#include <stdarg.h>

#define is_subtask_not_dead(ctx) ((ctx)->stc_State != HELIOS_STS_DEAD)
#define DEFAULT_STACK_SIZE 32768

/*--- debugging --------------------------------------------------------------*/
#define DH_SUBTASK "<ST>"
#define _ERR_SUBTASK(_fmt, _a...) _ERR(DH_SUBTASK _fmt, ##_a)
#define _WRN_SUBTASK(_fmt, _a...) _WRN(DH_SUBTASK _fmt, ##_a)
#define _INF_SUBTASK(_fmt, _a...) _INF(DH_SUBTASK _fmt, ##_a)
#define _DBG_SUBTASK(_fmt, _a...) _DBG(DH_SUBTASK _fmt, ##_a)

/*--- Startup message used by subtasks ---------------------------------------*/
typedef enum
{
	HELIOS_STS_INIT,
	HELIOS_STS_RUN,
	HELIOS_STS_DEAD,
} HeliosSubTaskState;

struct HeliosSubTask
{
	struct Message			stc_Msg;
	
	LOCK_PROTO;
	
	HeliosSubTaskState		stc_State;
	struct Task *			stc_Task;			/* System task itself, set by parent */
	struct MsgPort *		stc_TaskPort;		/* Task msg port, valid only if task has not returned */
	APTR					stc_MemPool;		/* Mem pool used to alloc the message */
	HeliosSubTaskEntry		stc_UserEntry;		/* User code called */
	LONG					stc_RC;				/* User entry return code */
	struct TagItem			stc_UserTags[0];
};

/*============================================================================*/
/*--- LOCAL CODE -------------------------------------------------------------*/
/*============================================================================*/
static void
helios_SubTaskEntry(HeliosSubTask *stm)
{
	struct MsgPort *taskport=NULL;
	struct Message *msg;
	struct Task *self = FindTask(NULL);
	struct TagItem tags[] = {
		{HA_MsgPort, 0},
		{TAG_MORE, 0},
		{TAG_END, 0}
	};
	
	NewGetTaskAttrsA(self, &taskport, sizeof(taskport), TASKINFOTYPE_TASKMSGPORT, NULL);
	_DBG_SUBTASK("New SubTask %p: name='%s', stm=%p, port=%p\n",
		self, self->tc_Node.ln_Name, stm, taskport);
	
	/* Mark task ready */
	EXCLUSIVE_PROTECT_BEGIN(stm);
	{
		tags[0].ti_Data = (ULONG)taskport;
		tags[1].ti_Data = (ULONG)stm->stc_UserTags;
		stm->stc_State = HELIOS_STS_RUN;
	}
	EXCLUSIVE_PROTECT_END(stm);
	
	/* Run mainloop */
	stm->stc_RC = stm->stc_UserEntry(stm, tags);

	/* Mark task dead */
	EXCLUSIVE_PROTECT_BEGIN(stm);
	stm->stc_State = HELIOS_STS_DEAD;
	EXCLUSIVE_PROTECT_END(stm);

	/* Now we're sure to not receive more messages on taskport.
	 * Purge it as it must be empty before return.
	 */
	while (NULL != (msg = GetMsg(taskport)))
	{
		if (msg->mn_Node.ln_Type == NT_FREEMSG)
			FreeMem(msg, sizeof(msg));
		else
			ReplyMsg(msg);
	}
	
	_DBG_SUBTASK("SubTask %p-'%s': returns\n", self, self->tc_Node.ln_Name);
}
/*============================================================================*/
/*--- PUBLIC API CODE --------------------------------------------------------*/
/*============================================================================*/
/*=== Timer ==================================================================*/
struct timerequest *
Helios_OpenTimer(struct MsgPort *port, ULONG unit)
{
	struct timerequest *tr = NULL;

	tr = (struct timerequest *)CreateExtIO(port, sizeof(*tr));
	if (NULL != tr)
	{
		if (!OpenDevice("timer.device", unit, (struct IORequest *)tr, 0))
		{
			tr->tr_node.io_Command = TR_ADDREQUEST;

			_DBG("Opened timer.device request=%p\n", tr);
			return tr;
		}
		else
			_ERR("Failed to open timer.device\n");

		DeleteExtIO((struct IORequest *)tr);
	}
	else
		_ERR("Failed to create timer request\n");

	return NULL;
}
void
Helios_CloseTimer(struct timerequest *tr)
{
	_DBG("Closing timer.device request @ %p\n", tr);

	CloseDevice((struct IORequest *)tr);
	DeleteExtIO((struct IORequest *)tr);
}
void
Helios_DelayMS(ULONG milli)
{
	struct MsgPort port;
	struct timerequest tr;
	BYTE sigbit;
	
	sigbit = AllocSignal(-1);
	if (sigbit < 0)
	{
		_ERR("AllocSignal() failed\n");
		return;
	}
	
	/* Reply port setup */
	port.mp_Flags	= PA_SIGNAL;
	port.mp_SigBit	= sigbit;
	port.mp_SigTask	= FindTask(NULL);
	NEWLIST(&port.mp_MsgList);

	/* Timer request structure init */
	CopyMem(&HeliosBase->hb_TimeReq, &tr, sizeof(tr));
	tr.tr_node.io_Message.mn_ReplyPort = &port;
	tr.tr_node.io_Command = TR_ADDREQUEST;
	tr.tr_time.tv_secs  = milli / 1000;
	tr.tr_time.tv_micro = (milli % 1000) * 1000;
	
	_DBG("Waiting %ums...\n", milli);
	DoIO((struct IORequest *)&tr);
	
	FreeSignal(sigbit);
}
/*=== Subtasking helpers =====================================================*/
HeliosSubTask *
Helios_CreateSubTaskA(
	CONST_STRPTR name,
	HeliosSubTaskEntry entry,
	struct TagItem *tags)
{
	HeliosSubTask *ctx;
	struct TagItem *tag, *next_tag;
	LONG priority;
	APTR mempool;
	ULONG user_tags_cnt=0, stacksize, ctx_size;

	/* Defaults */
	mempool		= HeliosBase->hb_MemPool; /* Note: not forwared by tags */
	priority	= 0;
	stacksize	= DEFAULT_STACK_SIZE;

	/* Parse and counting tags */
	next_tag = tags;
	while (NULL != (tag = NextTagItem(&next_tag)))
	{
		switch (tag->ti_Tag)
		{
			case HA_MemPool:		mempool = (APTR)tag->ti_Data; break;
			case TASKTAG_PRI:		priority = tag->ti_Data; tag->ti_Tag = TAG_IGNORE; break;
			case TASKTAG_STACKSIZE:	stacksize = tag->ti_Data; tag->ti_Tag = TAG_IGNORE; break;
			default: 				user_tags_cnt++;
		}
	}
	
	/* +1 for the TAG_END */
	user_tags_cnt++;

	/* Compute memory needed for the subtask startup message and user tags */
	ctx_size = sizeof(HeliosSubTask) + user_tags_cnt*sizeof(struct TagItem);

	/* Create startup message and run the new task */
	ctx = AllocPooled(mempool, ctx_size);
	if (NULL != ctx)
	{
		ULONG i;

		LOCK_INIT(ctx);
		
		/* Duplicate function tags */
		i = 0;
		while (NULL != (tag = NextTagItem(&tags)))
		{
			ctx->stc_UserTags[i].ti_Tag = tag->ti_Tag;
			ctx->stc_UserTags[i].ti_Data = tag->ti_Data;
			i++;
		}
		ctx->stc_UserTags[i].ti_Tag = TAG_END;

		ctx->stc_Msg.mn_Node.ln_Type = NT_MESSAGE;
		ctx->stc_Msg.mn_ReplyPort = NULL;
		ctx->stc_Msg.mn_Length = ctx_size;
		ctx->stc_MemPool = mempool;
		ctx->stc_UserEntry = entry;
		ctx->stc_State = HELIOS_STS_INIT;
		
		_DBG_SUBTASK("Creating subtask '%s'...\n", name);
		ctx->stc_Task = NewCreateTask(
			TASKTAG_CODETYPE,		CODETYPE_PPC,
			TASKTAG_NAME,			(ULONG)name,
			TASKTAG_PRI,			priority,
			TASKTAG_STACKSIZE,		stacksize,
			TASKTAG_STARTUPMSG,		(ULONG)ctx,
			TASKTAG_TASKMSGPORT,	(ULONG)&ctx->stc_TaskPort,
			TASKTAG_PC,				(ULONG)helios_SubTaskEntry,
			TASKTAG_PPC_ARG1,		(ULONG)ctx,
			TAG_DONE);
		if (ctx->stc_Task)
			return ctx;
		else
			_ERR("NewCreateTask() failed\n");
			
		FreePooled(mempool, ctx, ctx_size);
	}
	else
		_ERR("Subtask startup message allocation failed\n");

	return NULL;
}
LONG
Helios_KillSubTask(HeliosSubTask *ctx, LONG mustwait)
{
	HeliosMsg *msg;
	struct Task *subtask, *self;
	struct MsgPort port;
	BYTE sigbit;
	LONG err;
	BOOL not_dead;
	
	sigbit = AllocSignal(-1);
	if (sigbit < 0)
		return HERR_NOMEM;

	EXCLUSIVE_PROTECT_BEGIN(ctx);
	{
		not_dead = is_subtask_not_dead(ctx);
		if (not_dead && mustwait)
		{
			/* Setup startup message reply port */
			port.mp_Flags   = PA_SIGNAL;
			port.mp_SigBit  = sigbit;
			port.mp_SigTask = FindTask(NULL);
			NEWLIST(&port.mp_MsgList);

			ctx->stc_Msg.mn_ReplyPort = &port;
		}
	}
	EXCLUSIVE_PROTECT_END(ctx);

	if (!not_dead)
	{
		_DBG_SUBTASK("Subtask already killed!\n");
		err = HERR_NOERR;
		goto quit;
	}

	/* Subtask cannot kill itself and wait for that in same time */
	self = FindTask(NULL);
	subtask = ctx->stc_Task;
	if (mustwait && (self == subtask))
	{
		_ERR_SUBTASK("A subtask can't kill itself and wait in same time!\n");
		err = HERR_BADCALL;
		goto remove_port_and_quit;
	}

	/* Prepare and send an Helios kill message to subtask using it's port.
	 * Note: the user entry code must listen on this port to be effective.
	 */
	
	msg = AllocMem(sizeof(*msg), MEMF_PUBLIC | MEMF_CLEAR);
	if (!msg)
	{
		err = HERR_NOMEM;
		goto remove_port_and_quit;
	}
	
	_DBG_SUBTASK("Send kill msg to task %p-'%s'\n", subtask, subtask->tc_Node.ln_Name);

	msg->hm_Msg.mn_Node.ln_Type = NT_FREEMSG;
	msg->hm_Msg.mn_ReplyPort = NULL; /* not used */
	msg->hm_Msg.mn_Length = sizeof(*msg);
	msg->hm_Type = HELIOS_MSGTYPE_DIE;

	/* As a subtask can kill itself, we're not waiting for msg's reply */
	err = Helios_SendMsgToSubTask(ctx, &msg->hm_Msg);
	
	/* Message not sent? */
	if (err != HERR_NOERR)
	{
		FreeMem(msg, sizeof(*msg));
		
		if (err != HERR_NOTASK)
		{
			if (mustwait)
				goto remove_port_and_quit;
			else
				goto quit;
		}
	}
	else if (mustwait)
	{
		_DBG_SUBTASK("Task %p wait for death of subtask %p\n", self, subtask);
		if (&ctx->stc_Msg == WaitPort(&port))
			FreePooled(ctx->stc_MemPool, ctx, ctx->stc_Msg.mn_Length);
		else
			_ERR_SUBTASK("Die signal received before subtask die! (subtask=%p)\n", subtask);
	}
	
	err = HERR_NOERR;
	goto quit;

remove_port_and_quit:
	EXCLUSIVE_PROTECT_BEGIN(ctx);
	ctx->stc_Msg.mn_ReplyPort = NULL;
	EXCLUSIVE_PROTECT_END(ctx);
	
quit:
	FreeSignal(sigbit);
	return err;
}
BOOL
Helios_GetSubTaskRC(HeliosSubTask *ctx, LONG *rc_ptr)
{
	BOOL not_dead;

	SHARED_PROTECT_BEGIN(ctx);
	{
		not_dead = is_subtask_not_dead(ctx);
		if (rc_ptr)
			*rc_ptr = ctx->stc_RC;
	}
	SHARED_PROTECT_END(ctx);

	return not_dead;
}
LONG
Helios_SignalSubTask(HeliosSubTask *ctx, ULONG sigmask)
{
	LONG err;

	SHARED_PROTECT_BEGIN(ctx);
	{
		if (is_subtask_not_dead(ctx)) {
			Signal(ctx->stc_Task, sigmask);
			err = HERR_NOERR;
		} else {
			_ERR_SUBTASK("Subtask %p killed\n", ctx->stc_Task);
			err = HERR_NOTASK;
		}
	}
	SHARED_PROTECT_END(ctx);

	return err;
}
LONG
Helios_SendMsgToSubTask(HeliosSubTask *ctx, struct Message *msg)
{
	LONG err;

	SHARED_PROTECT_BEGIN(ctx);
	{
		if (is_subtask_not_dead(ctx)) {
			_DBG_SUBTASK("Send msg %p to subtask %p, port %p\n", msg, ctx->stc_Task, ctx->stc_TaskPort);
			PutMsg(ctx->stc_TaskPort, msg);
			err = HERR_NOERR;
		} else {
			_ERR_SUBTASK("Subtask %p already dead\n", ctx->stc_Task);
			err = HERR_NOTASK;
		}
	}
	SHARED_PROTECT_END(ctx);

	return err;
}
LONG
Helios_DoMsgToSubTask(
	HeliosSubTask *ctx,
	struct Message *msg,
	struct MsgPort *replyport)
{
	BOOL allocport = (NULL == replyport) || (NULL != msg->mn_ReplyPort);
	LONG err;

	if (allocport)
	{
		replyport = CreateMsgPort();
		if (NULL == replyport)
		{
			_ERR_SUBTASK("CreateMsgPort() failed\n");
			return HERR_NOMEM;
		}
	}

	msg->mn_ReplyPort = replyport;
	
	err = Helios_SendMsgToSubTask(ctx, msg);
	if (err == HERR_NOERR)
	{
		WaitPort(replyport);
		GetMsg(replyport);
	}
	
	if (allocport)
		DeleteMsgPort(replyport);

	return err;
}
/*=== Events =================================================================*/
void
Helios_AddEventListener(HeliosEventListenerList *list, HeliosEventMsg *node)
{
	EXCLUSIVE_PROTECT_BEGIN(list);
	ADDTAIL(list, node);
	EXCLUSIVE_PROTECT_END(list);
}
void
Helios_RemoveEventListener(HeliosEventListenerList *list, HeliosEventMsg *node)
{
	EXCLUSIVE_PROTECT_BEGIN(list);
	REMOVE(node);
	EXCLUSIVE_PROTECT_END(list);
}
void
Helios_SendEvent(HeliosEventListenerList *list, ULONG event, ULONG result)
{
	HeliosEventMsg *msg, *node;
	struct timeval tv;
	struct Library *TimerBase;

	TimerBase = (struct Library *)HeliosBase->hb_TimeReq.tr_node.io_Device;
	GetSysTime(&tv);

	SHARED_PROTECT_BEGIN(list);
	{
		ForeachNode(list, node)
		{
			if (0 == (node->hm_EventMask & event))
				continue;

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
	SHARED_PROTECT_END(list);
}
/*=== EOF ====================================================================*/