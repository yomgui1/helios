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
** Low-level API to handle transactions.
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
*/

#include "ohci1394trans.h"

#include <proto/exec.h>
#include <proto/alib.h>

#include <clib/macros.h>

#include <string.h>

#define RETRY_X 1

extern char *evt_strings;

/*============================================================================*/
/*--- LOCAL STRUCTURES AND VARIABLES -----------------------------------------*/
/*============================================================================*/
typedef struct DoRequestUData
{
	struct Task *	task;
	ULONG			signal;
	BYTE			status;
	UWORD			timestamp;
	QUADLET *		payload;
	ULONG			length;
} DoRequestUData;

static HeliosResponse bad_address_response = {
	hr_SysMsg: {mn_ReplyPort: NULL},
	hr_Packet: {RCode : HELIOS_RCODE_ADDRESS_ERROR},
};
/*============================================================================*/
/*--- LOCAL API --------------------------------------------------------------*/
/*============================================================================*/
static LONG
tl_SearchNextTLabel(OHCI1394TLayer *tl)
{
	LONG tlabel;

	EXCLUSIVE_PROTECT_BEGIN(tl);
	{
		/* fully busy ? */
		if (tl->tl_TLabelMap != -1)
		{
			/* Starting from the last given TLabel, search a non-used one.
			 * I don't check for a full scanning as I've already checked if
			 * one free tlabel exist at least.
			 */
			tlabel = tl->tl_LastTLabel;
			do
			{
				tlabel = (tlabel >= 63) ? 0 : tlabel + 1;
			}
			while (tl->tl_TLabelMap & (1ull << tlabel));

			tl->tl_LastTLabel = tlabel;
			tl->tl_TLabelMap |= 1ull << tlabel;
		}
		else
			tlabel = -1;
	}
	EXCLUSIVE_PROTECT_END(tl);
	
	return tlabel;
}
/* This callback implements the TR_DATA.confirmation service.
 * Called when request is acknowledged.
 */
static void
tl_ATCompleteCb(
	OHCI1394TLayer *tl,
	BYTE ack,
	UWORD timestamp,
	OHCI1394ATData *atd)
{
	OHCI1394Transaction *t = atd->atd_UData;

	EXCLUSIVE_PROTECT_BEGIN(t);
	{
		HeliosPacket *p = t->t_Packet;

		/* Not Finished? */
		if (p)
		{
			_DBG("TL=%u, Ack=%d, TS=$%x\n", p->TLabel, ack, timestamp);
			
			p->Ack = ack;
			p->TimeStamp = timestamp;
			
			switch(ack)
			{
				case HELIOS_ACK_PENDING:  /* special case (not TR_DATA.confirmation service) */
					/* Start Split-Timeout timer */
					SendIO((struct IORequest *)&t->t_TReq);
					break;
					
				case HELIOS_ACK_COMPLETE: /* COMPLETE, Unified-transaction (XXX: broadcast also?) */
					ohci_TL_Finish(tl, t, HELIOS_RCODE_COMPLETE);
					break;

				case HELIOS_ACK_DATA_ERROR: /* DATA ERROR */
				case HELIOS_ACK_TYPE_ERROR: /* DATA ERROR */
					ohci_TL_Finish(tl, t, HELIOS_RCODE_DATA_ERROR);
					break;

				default: /* only Ack code shall be read */
					ohci_TL_Finish(tl, t, HELIOS_RCODE_INVALID);
					break;
			}
		}
	}
	EXCLUSIVE_PROTECT_END(t);
}
static void
tl_PHY_ATCompleteCb(
	OHCI1394TLayer *tl,
	BYTE ack,
	UWORD timestamp,
	OHCI1394ATData *atd)
{
	DoRequestUData *udata = atd->atd_UData;

	_DBG("Ack=%d, TS=$%x\n", ack, timestamp);

	udata->status = ack;
	Signal(udata->task, udata->signal);
}
static void
tl_Response_ATCompleteCb(
	OHCI1394TLayer *tl,
	BYTE ack,
	UWORD timestamp,
	OHCI1394ATData *atd)
{
	HeliosResponse *resp = atd->atd_UData;
	FreeMem(atd, sizeof(*atd));

	_DBG("Ack=%d, TS=$%x, Response=%p\n", ack, timestamp, resp);

	resp->hr_Packet.Ack = ack;
	resp->hr_Packet.TimeStamp = timestamp;
	
	ReplyMsg(&resp->hr_SysMsg);
}
static void
tl_send_response(
	OHCI1394TLayer *tl,
	HeliosPacket *req,
	HeliosResponse *resp)
{
	OHCI1394ATData *atd;
	LONG err;
	UBYTE tcode;

	_DBG("resp@%p: RC=%d\n", resp, resp->hr_Packet.RCode);

	/* Don't response to unified transaction or broadcast transaction */
	if ((req->Ack != HELIOS_ACK_PENDING) || ((req->DestID & 0x3f) == 0x3f))
	{
		ReplyMsg(&resp->hr_SysMsg);
		return;
	}

	/* Fill the response header data */
	resp->hr_Packet.Header[1] = (req->SourceID << 16) | (resp->hr_Packet.RCode << 12);

	switch (req->TCode)
	{
		/* Write response */
		case TCODE_WRITE_BLOCK_REQUEST:
		case TCODE_WRITE_QUADLET_REQUEST:
			tcode = TCODE_WRITE_RESPONSE;
			break;

		/* Read quadlet response */
		case TCODE_READ_QUADLET_REQUEST:
			resp->hr_Packet.Header[3] = resp->hr_Packet.QuadletData;
			tcode = TCODE_READ_QUADLET_RESPONSE;
			break;

		/* Read block response */
		case TCODE_LOCK_REQUEST:
			resp->hr_Packet.Header[3] = resp->hr_Packet.ExtTCode;
		case TCODE_READ_BLOCK_REQUEST:
			resp->hr_Packet.Header[3] |= (resp->hr_Packet.PayloadLength << 16);
			tcode = req->TCode + 2;
			break;
			
		default:
			_ERR("bad tcode (%x)\n", req->TCode);
			return;
	}

	resp->hr_Packet.Header[0]  = (req->Speed << 16) | (req->TLabel << 10);
	resp->hr_Packet.Header[0] |= (RETRY_X << 8) | (tcode << 4);

	/* XXX: In case of error, no response is sent, the request node will timeout normally */
	atd = AllocMem(sizeof(*atd), MEMF_PUBLIC);
	if (!atd)
		return;
		
	atd->atd_AckCallback = tl_Response_ATCompleteCb;
	atd->atd_UData = resp;

	err = ohci_ATContext_Send(&tl->tl_Unit->hu_ATResponseCtx,
		&resp->hr_Packet.Header[0],
		resp->hr_Packet.Payload, atd,
		req->TLabel, resp->hr_Packet.TimeStamp);

	if (HHIOERR_NO_ERROR != err)
		FreeMem(atd, sizeof(*atd));
}
/*============================================================================*/
/*--- EXPORTED API -----------------------------------------------------------*/
/*============================================================================*/
void
ohci_TL_Init(OHCI1394Unit *unit, OHCI1394TLayer *tl)
{
	int i;
	
	LOCK_INIT(tl);
	INIT_LOCKED_LIST(tl->tl_ReqHandlerList);

	tl->tl_Unit = unit;
	tl->tl_LastTLabel = -1;
	tl->tl_TLabelMap = 0;
	
	bzero(tl->tl_Transactions, sizeof(tl->tl_Transactions));
	
	for (i=0; i < TLABEL_MAX; i++)
		LOCK_INIT(&tl->tl_Transactions[i]);
}
OHCI1394Transaction *
ohci_TL_Register(
	OHCI1394TLayer *				tl,
	HeliosPacket *					packet,
	OHCI1394TransactionCompleteFunc	func,
	APTR							udata)
{
	OHCI1394Transaction *t;
	LONG tlabel;
	
	/* Reserve next available tlabel */
	tlabel = tl_SearchNextTLabel(tl);
	if (tlabel >= 0)
	{
		t = &tl->tl_Transactions[tlabel];
		
		/* Prepare the packet to be sent */
		packet->TLabel = tlabel;
		packet->TimeStamp = 0;
		packet->Ack = HELIOS_ACK_NOTSET;
		packet->RCode = HELIOS_RCODE_INVALID;
		
		/* Lock transaction as FlushAll() can be called at anytime */
		EXCLUSIVE_PROTECT_BEGIN(t);
		{
			t->t_Packet = packet;
			t->t_ATData.atd_Ctx = NULL;
			t->t_CompleteFunc = func;
			t->t_UserData = udata;
		}
		EXCLUSIVE_PROTECT_END(t);
	}
	else
		t = NULL;
	
	return t;
}
void
ohci_TL_Finish(OHCI1394TLayer *tl, OHCI1394Transaction *t, BYTE rcode)
{
	OHCI1394TransactionCompleteFunc completeFunc=NULL;
	HeliosPacket *p;
	APTR udata=NULL;
	int tlabel=-1;

	EXCLUSIVE_PROTECT_BEGIN(t);
	{
		p = t->t_Packet;
		if (p)
		{
			tlabel = p->TLabel;
			_DBG("TL=%u, Packet=%p\n", tlabel, p);

			/* Cancel AT Ack if not done yet */
			ohci_CancelATProcess(&t->t_ATData);
		
			/* Abort SPLIT-TIMER request */
			if (p->Ack == HELIOS_ACK_PENDING)
			{
				if (!CheckIO(&t->t_TReq.tr_node))
					AbortIO(&t->t_TReq.tr_node);
			}
			
			p->RCode = rcode;
			
			completeFunc = t->t_CompleteFunc;
			udata = t->t_UserData;
			t->t_Packet = NULL;
		}
	}
	EXCLUSIVE_PROTECT_END(t);
	
	/* Release the tlabel */
	EXCLUSIVE_PROTECT_BEGIN(tl);
	{
		if (tlabel > 0)
			tl->tl_TLabelMap &= ~(1ull << tlabel);
	}
	EXCLUSIVE_PROTECT_END(tl);
	
	/* TR_DATA.response service */
	if (completeFunc)
		completeFunc(p, NULL, udata);
}
void
ohci_TL_FlushAll(OHCI1394TLayer *tl)
{
	int i;
	
	/* Forbid possibility to register new transactions */
	EXCLUSIVE_PROTECT_BEGIN(tl);
	{
		/* Cancel all pending transactions */
		for (i=0; i < TLABEL_MAX; i++)
			ohci_TL_Finish(tl, &tl->tl_Transactions[i], HELIOS_RCODE_CANCELLED);

		/* Make all TLabel's available */
		tl->tl_TLabelMap = 0;
		tl->tl_LastTLabel = -1;
	}
	EXCLUSIVE_PROTECT_END(tl);
}
void
ohci_TL_CancelPacket(OHCI1394TLayer *tl, HeliosPacket *packet)
{
	OHCI1394Transaction *t = &tl->tl_Transactions[packet->TLabel];
	
	/* Protect against bad calls */
	SHARED_PROTECT_BEGIN(t);
	t = t->t_Packet == packet ? t : NULL;
	SHARED_PROTECT_END(t);
	
	if (t)
		ohci_TL_Finish(tl, t, HELIOS_RCODE_CANCELLED);
	else
		_ERR("No transaction for request %p\n", packet);
}
LONG
ohci_TL_DoPHYPacket(
	OHCI1394Unit *unit,
	UBYTE sigbit,
	QUADLET value)
{
	OHCI1394ATData atd;
	DoRequestUData udata;
	LONG err;
	
	udata.task = FindTask(NULL);
	udata.signal = 1 << sigbit;
	udata.status = HELIOS_ACK_NOTSET;

	atd.atd_AckCallback = tl_PHY_ATCompleteCb;
	atd.atd_UData = &udata;

	SetSignal(0, udata.signal);
	err = ohci_SendPHYPacket(unit, S100, value, &atd);
	if (err == HHIOERR_NO_ERROR)
	{
		ULONG sigs, sig = 1ul << sigbit;

		sigs = Wait(sig);
		
		if ((sigs & sig) == 0)
			ohci_CancelATProcess(&atd);
	}
	else if (err == IOERR_UNITBUSY)
		return HELIOS_ACK_BUSY_X;
	else
		return HELIOS_ACK_TYPE_ERROR;

	return udata.status;
}
LONG
ohci_TL_SendRequest(OHCI1394Unit *unit, OHCI1394Transaction *t)
{
	OHCI1394TLayer *tl = &unit->hu_TL;
	OHCI1394ATData *atd;
	HeliosPacket *p;
	HeliosPacket response;
	UWORD localNodeID;
	ULONG splitTimeout;
	LONG ioerr, gen;
	struct timerequest *tr;

	SHARED_PROTECT_BEGIN(unit);
	{
		splitTimeout = unit->hu_SplitTimeoutCSR;
		localNodeID = unit->hu_LocalNodeId;
		gen = unit->hu_OHCI_LastGeneration;
	}
	SHARED_PROTECT_END(unit);
	
	/* Prepare request packet for sending */
	EXCLUSIVE_PROTECT_BEGIN(t);
	{
		p = t->t_Packet;
		if (p)
		{
			/* Prepare AT ack data */
			atd = &t->t_ATData;
			atd->atd_AckCallback = tl_ATCompleteCb;
			atd->atd_UData = t;
				
			if (gen == p->Generation)
			{
				/* Encode packet's field into an OHCI compatible header */
				p->Header[0] = \
					AT_HEADER_SPEED(p->Speed) |
					AT_HEADER_TLABEL(p->TLabel) |
					AT_HEADER_RT(RETRY_X) |
					AT_HEADER_TCODE(p->TCode);
				p->Header[1] = \
					AT_HEADER_DEST_ID(p->DestID) |
					((p->Offset >> 32) & 0xFFFF);
				p->Header[2] = p->Offset;

				/* Fill extra header slot 3 (check tcode also).
				 * TCode request TCODE_WRITE_PHY, TCODE_WRITE_STREAM,
				 * or non request ones are ack'ed with HELIOS_ACK_TYPE_ERROR.
				 */
				switch (p->TCode)
				{
					case TCODE_READ_BLOCK_REQUEST:
					case TCODE_WRITE_BLOCK_REQUEST:
						p->Header[3] = AT_HEADER_LEN(p->PayloadLength);
						break;

					case TCODE_WRITE_QUADLET_REQUEST:
						p->Header[3] = p->QuadletData;
						break;

					case TCODE_LOCK_REQUEST:
						p->Header[3] = AT_HEADER_LEN(p->PayloadLength) | AT_HEADER_EXTCODE(p->ExtTCode);
						break;
						
					case TCODE_READ_QUADLET_REQUEST:
						/* nothing to do */
						break;
					
					default: /* emulate HELIOS_ACK_TYPE_ERROR received */
						_ERR_UNIT(unit, "Request %p: invalid TCode $%X\n", p, p->TCode);
						atd->atd_AckCallback(tl, HELIOS_ACK_TYPE_ERROR, ohci_GetTimeStamp(unit), atd);
						EXCLUSIVE_PROTECT_END(t);
						return HHIOERR_NO_ERROR;
				}

				if (p->DestID != localNodeID)
				{
					/* Setup the SPLIT-TIMEOUT timer request */
					tr = &t->t_TReq;
					CopyMem(unit->hu_SplitTimeoutIOReq, tr, sizeof(t->t_TReq));
					tr->tr_node.io_Message.mn_Length = sizeof(*t);
					tr->tr_node.io_Command = TR_ADDREQUEST;
					tr->tr_time.tv_secs = splitTimeout >> 15;
					tr->tr_time.tv_micro = (splitTimeout & 0x7fff) * 125;
				
					/* Send over the bus */
					ioerr = ohci_ATContext_Send(
						&unit->hu_ATRequestCtx,
						p->Header,
						p->Payload,
						atd,
						p->TLabel,
						0);
				}
				else
				{
					/* Local packet handling, emulate transport on bus */
					bzero(&response, sizeof(response));
					
					p->Retry = RETRY_X;

					/* A race condition can happen at unit lock.
					 * The BusReset task will call FinishAll() that lock transactions
					 * but in a unit locked context.
					 * So, we must release the transaction.
					 */
					EXCLUSIVE_PROTECT_END(t);

					/* Locking the region now, because a block of data (i.e. ROM) may be
					 * used as payload response and we need to wait its usage during
					 * the call to ohci_TL_HandleResponse() before modifications.
					 */
					EXCLUSIVE_PROTECT_BEGIN(unit);
					{
						EXCLUSIVE_PROTECT_BEGIN(t);
						{
							/* if not cancelled */
							if (t->t_Packet)
								ohci_HandleLocalRequest(unit, p, &response);
						}
						EXCLUSIVE_PROTECT_END(t);
					}
					EXCLUSIVE_PROTECT_END(unit);
					
					/* Handle the response as if it received from the AR-Resp context */
					ohci_TL_HandleResponse(tl, &response);
					return HHIOERR_NO_ERROR;
				}
			}
			else
			{
				_WRN_UNIT(unit, "Request %p: generation mismatch, get %u, must be %u\n", p, p->Generation, gen);
				atd->atd_AckCallback(tl, HELIOS_ACK_GENERATION, ohci_GetTimeStamp(unit), atd);
			}
		}
	}
	EXCLUSIVE_PROTECT_END(t);
	
	return HHIOERR_NO_ERROR;
}
void
ohci_TL_HandleResponse(OHCI1394TLayer *tl, HeliosPacket *resp)
{
	UBYTE tlabel = resp->TLabel;
	OHCI1394Transaction *t = &tl->tl_Transactions[tlabel];
	OHCI1394TransactionCompleteFunc completeFunc=NULL;
	HeliosPacket *p;
	APTR udata;

	_DBG("$%04x -> $%04x: TC=$%x, TL=%u, Ack=%u, RCode=%u\n",
		resp->SourceID, resp->DestID, resp->TCode, resp->TLabel, resp->Ack, resp->RCode);

	EXCLUSIVE_PROTECT_BEGIN(t);
	{
		p = t->t_Packet;
		udata = t->t_UserData;
		
		/* for response incoming packet we're going to check if it expected or not,
		 * that could speed-up bit the process if the packet wasn't expected.
		 */
		if (p &&
			(resp->TLabel == p->TLabel) &&
			(resp->SourceID == p->DestID))
		{
			/* Cancel AT Ack if not done yet */
			ohci_CancelATProcess(&t->t_ATData);
			
			/* Abort SPLIT-TIMER request */
			if (p->Ack == HELIOS_ACK_PENDING)
			{
				if (!CheckIO(&t->t_TReq.tr_node))
					AbortIO(&t->t_TReq.tr_node);
			}
			
			completeFunc = t->t_CompleteFunc;
			
			t->t_Packet = NULL;
			p->RCode = resp->RCode;
		}
		else
			_WRN("Unwaited response packet\n");
	}
	EXCLUSIVE_PROTECT_END(t);
	
	if (completeFunc)
	{
		EXCLUSIVE_PROTECT_BEGIN(tl);
		tl->tl_TLabelMap &= ~(1ull << tlabel);
		EXCLUSIVE_PROTECT_END(tl);

		/* TR_DATA.response service */
		completeFunc(p, resp, udata);
	}
}
LONG
ohci_TL_AddReqHandler(OHCI1394TLayer *tl, HeliosRequestHandler *reqh)
{
	LONG err = HHIOERR_FAILED;
	HeliosRequestHandler *node;
	HeliosOffset start;

	/* Sanity checks:
	 *  - Region is 4-bytes aligned and limited to 48bits.
	 *  - Length shall be 4-bytes aligned and not zero.
	 * TODO: check against the physicalUpperBound value.
	 */
	if ((reqh->hrh_RegionStart & 0xFFFF000000000003ULL) ||
		(reqh->hrh_RegionStop & 0xFFFF000000000003ULL) ||
		(reqh->hrh_RegionStart >= reqh->hrh_RegionStop) ||
		(reqh->hrh_Length & 3) ||
		(0 == reqh->hrh_Length))
		return IOERR_BADADDRESS;

	if (!TypeOfMem(reqh->hrh_Callback))
		return HHIOERR_FAILED;

	/* TODO: I support only HHF_REQH_ALLOCLEN allocation type */
	if (reqh->hrh_Flags != HELIOS_RHF_ALLOCATE)
		return HHIOERR_FAILED;

	start = reqh->hrh_RegionStart;

	WRITE_LOCK_LIST(tl->tl_ReqHandlerList);
	{
		node = (APTR)GetHead(&tl->tl_ReqHandlerList);
		_DBG("start=$%llx, node=%p\n", start, node);

		while ((start + reqh->hrh_Length) < reqh->hrh_RegionStop)
		{
			/* no more registred handlers or requested is below */
			if ((NULL == node) || ((start + reqh->hrh_Length) <= node->hrh_Start))
			{
				reqh->hrh_Start = start;
				ADDTAIL(&tl->tl_ReqHandlerList, reqh);
				err = HHIOERR_NO_ERROR;
				break;
			}

			start = node->hrh_Start + node->hrh_Length;
			node = (APTR)GetSucc(node);
		}
	}
	UNLOCK_LIST(tl->tl_ReqHandlerList);

	return err;
}
LONG
ohci_TL_RemReqHandler(OHCI1394TLayer *tl, HeliosRequestHandler *reqh)
{
	SAFE_REMOVE(tl->tl_ReqHandlerList, reqh);
	return HHIOERR_NO_ERROR;
}
void
ohci_TL_HandleRequest(OHCI1394TLayer *tl, HeliosPacket *req)
{
	HeliosResponse *response = NULL;
	BOOL found = FALSE;

	_DBG("$%04x -> $%04x: TC=$%x, TL=%u, Ack=%u, Off=$%016llx\n",
		req->SourceID, req->DestID, req->TCode, req->TLabel, req->Ack, req->Offset);

	/* don't handle if ack error */
	if ((req->Ack != HELIOS_ACK_PENDING) && (req->Ack != HELIOS_ACK_COMPLETE))
		return;

	READ_LOCK_LIST(tl->tl_ReqHandlerList);
	{
		HeliosRequestHandler *node;

		ForeachNode(&tl->tl_ReqHandlerList, node)
		{
			if ((req->Offset >= node->hrh_Start) &&
				(req->Offset < (node->hrh_Start+node->hrh_Length)))
			{
				response = node->hrh_Callback(req, node->hrh_UserData);
				found = TRUE;
				break; /* handlers not overlap */
			}
		}
	}
	UNLOCK_LIST(tl->tl_ReqHandlerList);
	
	if (!found)
	{
		response = &bad_address_response;
		_ERR("no handler found\n");
	}

	if (response)
		tl_send_response(tl, req, response);
}
/*=== EOF ====================================================================*/