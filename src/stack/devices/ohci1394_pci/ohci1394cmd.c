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
** BeginIO/AbordIO low-level functions.
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
*/

#include "ohci1394.device.h"
#include "ohci1394core.h"
#include "ohci1394trans.h"

#include <exec/errors.h>

#include <proto/utility.h>
#include <proto/exec.h>

#include <clib/macros.h>

#include <string.h>

/*============================================================================*/
/*--- LOCAL CODE -------------------------------------------------------------*/
/*============================================================================*/
/* WARNING: call it with a locked unit */
static void
cmd_reply_ioreq(struct IOStdReq *ioreq, LONG err)
{
	struct Message *msg;

	ioreq->io_Error = err;
	
	msg = &ioreq->io_Message;
	if (msg->mn_Node.ln_Type != NT_MESSAGE)
		_ERR("bad msg type (%p-%u)\n", msg, msg->mn_Node.ln_Type);

	ReplyMsg(msg);
}
static void
_cmd_HandlePHYATComplete(
	OHCI1394TLayer *tl,
	BYTE ack,
	UWORD timestamp,
	OHCI1394ATData *atd)
{
	struct IOStdReq *ioreq = atd->atd_UData;
	LONG err;

	_DBG("Ack=%d, TS=%u.%04u\n", ack, timestamp >> 13, timestamp & 0x1fff);

	if (ack == HELIOS_ACK_COMPLETE)
		err = HHIOERR_NO_ERROR;
	else
		err = HHIOERR_FAILED;

	FreeMem(atd, sizeof(*atd));
	cmd_reply_ioreq(ioreq, err);
}
static void
_cmd_HandleResponse(HeliosPacket *req, HeliosPacket *resp, APTR udata)
{
	struct IOStdReq *ioreq = udata;
	LONG err;

	if (req->RCode == HELIOS_RCODE_COMPLETE)
	{
		err = HHIOERR_NO_ERROR;
		
		/* Response waited? */
		if (req->TCode != TCODE_WRITE_QUADLET_REQUEST &&
			req->TCode != TCODE_WRITE_BLOCK_REQUEST)
		{
			if (resp)
				_DBG("Request %p (OK): TL=%u, Q=$%08X, PL=%p (%u)\n",
					req, req->TLabel, resp->QuadletData,
					resp->Payload, resp->PayloadLength);
			else
				_ERR("Resquest %p: Ok without response packet!?\n", req);
			
			/* Read request? */
			if (req->TCode == TCODE_READ_QUADLET_REQUEST)
				req->QuadletData = resp->QuadletData;
			else if (resp->PayloadLength > 0)
			{
				if (req->Payload)
				{
					req->PayloadLength = MIN(req->PayloadLength, resp->PayloadLength);
					_DBG("Copy %lu bytes from $%p to $%p\n", req->PayloadLength,
						req->Payload, resp->Payload);
					CopyMem(resp->Payload, req->Payload, req->PayloadLength);
				}
				else
					; /* don't know how to save data, but transaction is ok */
			}
		}
	}
	else if (req->RCode == HELIOS_RCODE_INVALID)
	{
		/* Ack error code only */
		_DBG("Request %p (ERR): TL=%u, Ack=%d\n", req, req->TLabel, req->Ack);
		err = HHIOERR_ACK;
	}
	else
	{
		_DBG("Request %p (ERR): TL=%u, RCode=%d\n", req, req->TLabel, req->RCode);
		err = HHIOERR_RCODE;
	}
	
	cmd_reply_ioreq(ioreq, err);
}
/*============================================================================*/
/*--- PUBLIC API CODE --------------------------------------------------------*/
/*============================================================================*/
CMDP(cmdReset)
{
	EXCLUSIVE_PROTECT_BEGIN(unit);
	{
		if (!ohci_ResetUnit(unit))
			ioreq->io_Error = HHIOERR_FAILED;
	}
	EXCLUSIVE_PROTECT_END(unit);

	return FALSE;
}
CMDP(cmdBusReset)
{
	EXCLUSIVE_PROTECT_BEGIN(unit);
	{
		/* Data = bus reset type */
		if (!ohci_RaiseBusReset(unit, (LONG)ioreq->io_Data))
			ioreq->io_Error = HHIOERR_FAILED;
	}
	EXCLUSIVE_PROTECT_END(unit);

	return FALSE;
}
CMDP(cmdEnable)
{
	EXCLUSIVE_PROTECT_BEGIN(unit);
	{
		if (!ohci_Enable(unit))
			ioreq->io_Error = HHIOERR_FAILED;
	}
	EXCLUSIVE_PROTECT_END(unit);

	return FALSE;
}
CMDP(cmdDisable)
{
	EXCLUSIVE_PROTECT_BEGIN(unit);
	{
		ohci_Disable(unit);
	}
	EXCLUSIVE_PROTECT_END(unit);

	return FALSE;
}
CMDP(cmdSendRequest)
{
	OHCI1394Transaction *t;
	LONG ioerr;
	
	/* io_Data = (HeliosPacket *)*/
	if (ioreq->io_Message.mn_Length >= sizeof(struct IOStdReq))
	{
		t = ohci_TL_Register(&unit->hu_TL, ioreq->io_Data, _cmd_HandleResponse, ioreq);
		if (t)
		{
			ioreq->io_Actual = 0;
			ioerr = ohci_TL_SendRequest(unit, t);
			if (ioerr)
				ioreq->io_Error = ioerr;
		}
		else
		{
			_ERR_UNIT(unit, "No more available TLabel\n");
			ioreq->io_Error = IOERR_UNITBUSY;
		}
	}
	else
	{
		_ERR_UNIT(unit, "Invalid IO message length\n");
		ioreq->io_Error = IOERR_BADLENGTH;
	}
	
	return (HHIOERR_NO_ERROR == ioreq->io_Error);
}
CMDP(abortSendRequest)
{
	/* io_Data = (HeliosPacket *)*/
	ohci_TL_CancelPacket(&unit->hu_TL, ioreq->io_Data);
	return TRUE;
}
CMDP(cmdSendPhy)
{
	OHCI1394ATData *atd;

	atd = AllocMem(sizeof(OHCI1394ATData), MEMF_PUBLIC);
	if (NULL == atd)
	{
		ioreq->io_Error = HHIOERR_NOMEM;
		return FALSE;
	}

	atd->atd_AckCallback = _cmd_HandlePHYATComplete;
	atd->atd_UData = ioreq;

	ioreq->io_Error = ohci_SendPHYPacket(unit, S100, (QUADLET)ioreq->io_Data, atd);
	if (ioreq->io_Error != HHIOERR_NO_ERROR)
	{
		FreeMem(atd, sizeof(*atd));
		return FALSE;
	}

	return TRUE;
}
CMDP(cmdAddReqHandler)
{
	ioreq->io_Error = ohci_TL_AddReqHandler(&unit->hu_TL, ioreq->io_Data);
	return FALSE;
}
CMDP(cmdRemReqHandler)
{
	ioreq->io_Error = ohci_TL_RemReqHandler(&unit->hu_TL, ioreq->io_Data);
	return FALSE;
}
#if 0
CMDP(cmdSetAttrs)
{
	struct TagItem *tag, *tags;
	LONG err=0;

	_INFO_UNIT(unit, "HHIOCMD_SETATTRIBUTES\n");

	ioreq->io_Actual = 0;

	tags = ioreq->io_Data;
	while (NULL != (tag = NextTagItem(&tags)))
	{
		switch (tag->ti_Tag)
		{
			case HA_Rom:
				err = ohci_SetROM(unit, (QUADLET *)tag->ti_Data);
				ioreq->io_Actual++;
				break;
		}

		if (err)
			break;
	}

	ioreq->io_Error = err;

	return FALSE;
}
CMDP(cmdDelIsoCtx)
{
	_INFO_UNIT(unit, "HHIOCMD_DELETEISOCONTEXT\n");

	ohci_IRContext_Destroy(ioreq->io_Data);

	return FALSE;
}
CMDP(cmdCreateIsoCtx)
{
	OHCI1394IRCtx **ctx_p=NULL;
	struct TagItem *tag, *tags;
	LONG err=HHIOERR_FAILED, type=-1, index=-1;
	UWORD ibuf_size=0, ibuf_count=0, hlen=0;
	UBYTE payload_align=1;
	APTR callback=NULL, udata=NULL;
	BOOL dropempty=FALSE;

	_INFO_UNIT(unit, "HHIOCMD_CREATEISOCONTEXT\n");

	ioreq->io_Actual = 0;

	tags = ioreq->io_Data;
	while (NULL != (tag = NextTagItem(&tags)))
	{
		ioreq->io_Actual++;

		switch (tag->ti_Tag)
		{
			case HA_IsoContext: ctx_p = (APTR)tag->ti_Data; break;
			case HA_IsoType: type = tag->ti_Data; break;
			case HA_IsoContextID: index = tag->ti_Data; break;
			case HA_IsoBufferSize: ibuf_size = tag->ti_Data; break;
			case HA_IsoBufferCount: ibuf_count = tag->ti_Data; break;
			case HA_IsoHeaderLenght: hlen = tag->ti_Data; break;
			case HA_IsoPayloadAlign: payload_align = tag->ti_Data; break;
			case HA_IsoCallback: callback = (APTR)tag->ti_Data; break;
			case HA_UserData: udata = (APTR)tag->ti_Data; break;
			case HA_IsoRxDropEmpty: dropempty = tag->ti_Data; break;

			default: ioreq->io_Actual--; break;
		}
	}

	_INFO("type=%u, ctx_p=%p, idx=%ld, ibuf_size=%u, ibuf_count=%u, hlen=%u, payloadAlign=%u, cb=%p, udata=%p\n",
		  type, ctx_p, index, ibuf_size, ibuf_count, hlen, payload_align, callback, udata);

	if ((type >= 0) && (NULL != ctx_p))
	{
		if (HELIOS_ISO_RX_CTX == type)
		{
			if ((ibuf_size > 0) && (ibuf_count > 0) && (hlen > 0) && (payload_align > 0))
			{
				OHCI1394IRCtxFlags flags;

				flags.DropEmpty = dropempty;
				*ctx_p = ohci_IRContext_Create(unit, index, ibuf_size, ibuf_count,
											   hlen, payload_align, callback, udata, flags);
				if (NULL != *ctx_p)
					err = 0;
			}
		}
		else
			DB_NotImplemented();
	}

	ioreq->io_Error = err;
	return FALSE;
}
CMDP(cmdStartIsoCtx)
{
	struct TagItem *tag, *tags;
	ULONG channel=-1, isotag=0;
	OHCI1394Context *ctx=NULL;
	LONG err=HHIOERR_FAILED;

	_INFO_UNIT(unit, "HHIOCMD_STARTISOCONTEXT\n");

	ioreq->io_Actual = 0;

	tags = ioreq->io_Data;
	while (NULL != (tag = NextTagItem(&tags)))
	{
		ioreq->io_Actual++;

		switch (tag->ti_Tag)
		{
			case HA_IsoContext: ctx = (APTR)tag->ti_Data; break;
			case HA_IsoChannel: channel = tag->ti_Data; break;
			case HA_IsoTag: isotag = tag->ti_Data; break;

			default: ioreq->io_Actual--; break;
		}
	}

	if (NULL != ctx)
	{
		if (ctx->ctx_RegOffset >= OHCI1394_REG_IRECV_CONTEXT_CONTROL(0))
			ohci_IRContext_Start((OHCI1394IRCtx *)ctx, channel, isotag);
		else
			DB_NotImplemented();
	}

	ioreq->io_Error = err;
	return FALSE;
}
CMDP(cmdStopIsoCtx)
{
	OHCI1394Context *ctx;

	_INFO_UNIT(unit, "HHIOCMD_STOPISOCONTEXT\n");

	ctx = ioreq->io_Data;
	if (ctx->ctx_RegOffset >= OHCI1394_REG_IRECV_CONTEXT_CONTROL(0))
		ohci_IRContext_Stop((OHCI1394IRCtx *)ioreq->io_Data);
	else
		DB_NotImplemented();

	return FALSE;
}
#endif
/*=== EOF ====================================================================*/
