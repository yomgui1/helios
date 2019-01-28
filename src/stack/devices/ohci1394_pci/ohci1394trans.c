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
** Low-level API to handle transactions.
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
**
*/


#define NDEBUG

#include "ohci1394trans.h"

#include <clib/macros.h>

#include <proto/exec.h>
#include <proto/alib.h>

#include <string.h>

typedef struct DoRequestUData
{
    struct Task *task;
    ULONG signal;
    BYTE status;
    UWORD timestamp;
    QUADLET *payload;
    ULONG length;
} DoRequestUData;

extern char *evt_strings;

#define RETRY_X 1

static HeliosResponse bad_address_response = {
    {RCode : HELIOS_RCODE_ADDRESS_ERROR},
    NULL, 0, 0
};

/*----------------------------------------------------------------------------*/
/*--- LOCAL CODE SECTION -----------------------------------------------------*/

//+ tl_SearchNextTLabel
LONG tl_SearchNextTLabel(OHCI1394Unit *unit)
{
    LONG tlabel;

    /* fully busy ? */
    if (-1 == unit->hu_TLabelMap)
        return -1;

    /* Starting from the last given TLabel, search a non-used one.
     * I don't check for a full scanning as I've already checked if
     * one free tlabel exist at least.
     */
    tlabel = unit->hu_LastTLabel;
    do
    {
        tlabel = (tlabel >= 63) ? 0 : tlabel + 1;
    }
    while (unit->hu_TLabelMap & (1ull << tlabel));

    unit->hu_LastTLabel = tlabel;
    unit->hu_TLabelMap |= 1ull << tlabel;

    return tlabel;
}
//-
//+ tl_ATCompleteCb
/* This callback shall implement the TR_DATA.confirmation service */
static void tl_ATCompleteCb(OHCI1394Unit *unit,
                            BYTE status, UWORD timestamp,
                            OHCI1394ATPacketData *pdata)
{
    HeliosTransaction *t = pdata->pd_UData;

    _INFO_UNIT(unit, "status=%d, TS=$%x, t=%p\n", status, timestamp, t);

    t->htr_Packet.Ack = status; /* Ack code or special Helios RCode */

    switch(status)
    {
        case HELIOS_ACK_COMPLETE: /* Unified-transaction (XXX: broadcast also?) */
            ohci_TL_Finish(unit, t, HELIOS_RCODE_COMPLETE);
            break;

        case HELIOS_ACK_PENDING:
            t->htr_Packet.TimeStamp = timestamp;

            /* Split-Timeout timer */
            ((OHCI1394SplitTimeReq *)t->htr_SplitTimerReq)->req.tr_node.io_Command = TR_ADDREQUEST;
            SendIO((struct IORequest *)t->htr_SplitTimerReq);
            break;

        case HELIOS_ACK_BUSY_X:
        case HELIOS_ACK_BUSY_A:
        case HELIOS_ACK_BUSY_B:
            ohci_TL_Finish(unit, t, HELIOS_RCODE_BUSY);
            break;

        case HELIOS_ACK_DATA_ERROR:
            ohci_TL_Finish(unit, t, HELIOS_RCODE_DATA_ERROR);
            break;

        case HELIOS_ACK_TYPE_ERROR:
            ohci_TL_Finish(unit, t, HELIOS_RCODE_TYPE_ERROR);
            break;

        default: /* status is already a valid RCode */
            ohci_TL_Finish(unit, t, status);
            break;
	}
}
//-
//+ tl_DoReqHandleResp
static void tl_DoReqHandleResp(HeliosTransaction *t,
                               BYTE status,
                               QUADLET *payload,
                               ULONG length)
{
    DoRequestUData *udata = t->htr_UserData;

    _INFO("t=%p, status=%d, payload=%p, length=%lu\n", t, status, payload, length);

    udata->status = status;
    if (HELIOS_RCODE_COMPLETE == status)
    {
        if (NULL != payload)
        {
            udata->length = MIN(length, udata->length);
            
            _INFO("Copy %lu bytes into buffer at $%p\n", udata->length, udata->payload);
            CopyMem(payload, udata->payload, udata->length);
        }
    }

    Signal(udata->task, udata->signal);
}
//-
//+ tl_PHY_ATCompleteCb
static void tl_PHY_ATCompleteCb(OHCI1394Unit *unit,
                                BYTE status, UWORD timestamp,
                                OHCI1394ATPacketData *pdata)
{
    DoRequestUData *udata = pdata->pd_UData;

    _INFO_UNIT(unit, "status=%d, TS=$%x\n", status, timestamp);

    udata->status = status;
    Signal(udata->task, udata->signal);
}
//-
//+ tl_Response_ATCompleteCb
static void tl_Response_ATCompleteCb(OHCI1394Unit *unit,
                                     BYTE status, UWORD timestamp,
                                     OHCI1394ATPacketData *pdata)
{
    HeliosResponse *resp = pdata->pd_UData;

    _INFO_UNIT(unit, "status=%d, TS=$%x, Response=%p\n", status, timestamp, resp);

    resp->hr_Packet.Ack = status;
    resp->hr_Packet.TimeStamp = timestamp;
    if (NULL != resp->hr_FreeFunc)
        resp->hr_FreeFunc(resp, resp->hr_AllocSize, resp->hr_FreeUData);
    FreePooled(unit->hu_MemPool, pdata, sizeof(*pdata));
}
//-
//+ tl_send_response
void tl_send_response(OHCI1394Unit *unit,
                      HeliosAPacket *req,
                      HeliosResponse *resp,
                      UBYTE generation)
{
    OHCI1394ATPacketData *pdata;
    LONG err;
    UBYTE tcode;

    _INFO_UNIT(unit, "resp@%p: RC=%d\n", resp, resp->hr_Packet.RCode);

    /* Don't response to unified transaction or broadcast transaction */
	if ((req->Ack != HELIOS_ACK_PENDING) || ((req->DestID & 0x3f) == 0x3f))
    {
		if (NULL != resp->hr_FreeFunc)
            resp->hr_FreeFunc(resp, resp->hr_AllocSize, resp->hr_FreeUData);
        return;
    }

    pdata = AllocPooled(unit->hu_MemPool, sizeof(*pdata));
    if (NULL == pdata)
    {
        _ERR_UNIT(unit, "AllocPooled() failed\n");
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
            _ERR_UNIT(unit, "bad tcode (%x)\n", req->TCode);
            return;
    }

    resp->hr_Packet.Header[0]  = (req->Speed << 16) | (req->TLabel << 10);
    resp->hr_Packet.Header[0] |= (RETRY_X << 8) | (tcode << 4);

    pdata->pd_AckCallback = tl_Response_ATCompleteCb;
    pdata->pd_UData = resp;

    err = ohci_ATContext_Send(&unit->hu_ATResponseCtx, generation,
                              &resp->hr_Packet.Header[0],
                              resp->hr_Packet.Payload, pdata,
                              req->TLabel, resp->hr_Packet.TimeStamp);

    if (HHIOERR_NO_ERROR != err)
    {
        if (NULL != resp->hr_FreeFunc)
            resp->hr_FreeFunc(resp, resp->hr_AllocSize, resp->hr_FreeUData);
        FreePooled(unit->hu_MemPool, pdata, sizeof(*pdata));
    }
}
//-

/*----------------------------------------------------------------------------*/
/*--- PUBLIC CODE SECTION ----------------------------------------------------*/

//+ ohci_TL_Register
LONG ohci_TL_Register(OHCI1394Unit *unit,
                      HeliosTransaction *t,
                      OHCI1394ATCompleteCallback cb,
                      APTR cb_udata)
{
    /* Find next available tlabel */
    LOCK_REGION(unit);
    t->htr_Packet.TLabel = tl_SearchNextTLabel(unit);
    UNLOCK_REGION(unit);

    if (-1 != t->htr_Packet.TLabel)
    {
        OHCI1394ATPacketData *pdata = &unit->hu_PacketData[t->htr_Packet.TLabel];

        _INFO_UNIT(unit, "t=%p, TL=%d\n", t, (ULONG)t->htr_Packet.TLabel);

        unit->hu_Transactions[t->htr_Packet.TLabel] = t;
        pdata->pd_AckCallback = cb;
        pdata->pd_UData = cb_udata;
        pdata->pd_Buffer = NULL;
        t->htr_Private = pdata;        
    }
    else
        _ERR_UNIT(unit, "No free tlabel\n");

    return t->htr_Packet.TLabel;
}
//-
//+ ohci_TL_FlushAll
void ohci_TL_FlushAll(OHCI1394Unit *unit)
{
    LOCK_REGION(unit);
    {
        ULONG i;

        for (i=0; i<TLABEL_MAX; i++)
        {
            HeliosTransaction *t = unit->hu_Transactions[i];

            if (NULL != t)
            {
                if (NULL != t->htr_Private)
                    ohci_CancelATPacket(unit, t->htr_Private);
                ohci_TL_Finish(unit, t, HELIOS_RCODE_CANCELLED);
            }
        }

        unit->hu_LastTLabel = -1;
        unit->hu_TLabelMap = 0;
    }
    UNLOCK_REGION(unit);
}
//-
//+ ohci_TL_Finish
/* The transaction shall be cancelled at AT-layer before calling this function */
void ohci_TL_Finish(OHCI1394Unit *unit, HeliosTransaction *t, BYTE status)
{
    BOOL cb_not_called = FALSE;

    /* Free the tlabel */
    LOCK_REGION(unit);
    {
        _INFO_UNIT(unit, "t=%p, TL=$%X\n", t, t->htr_Packet.TLabel);

        /* Pending ? */
        if ((((UBYTE)t->htr_Packet.TLabel) < 64) && (NULL != unit->hu_Transactions[t->htr_Packet.TLabel]))
        {
            cb_not_called = TRUE;
            unit->hu_Transactions[t->htr_Packet.TLabel] = NULL;
            unit->hu_TLabelMap &= ~(1ull << t->htr_Packet.TLabel);
            t->htr_Private = NULL;
            t->htr_Packet.TLabel = -1;

            /* Abort SPLIT-TIMER request */
            if (HELIOS_ACK_PENDING == t->htr_Packet.Ack)
            {
                OHCI1394SplitTimeReq *req = t->htr_SplitTimerReq;

                req->transaction = NULL;
                if (!CheckIO(&req->req.tr_node))
                    AbortIO(&req->req.tr_node);
                t->htr_SplitTimerReq = NULL;
            }
        }
    }
    UNLOCK_REGION(unit);

    /* Let application play with data (TR_DATA.confirmation service) */
    if (cb_not_called)
    {
        _INFO_UNIT(unit, "trans %p, calling cb %p, status=%d\n", t, t->htr_Callback, (WORD)status);
        t->htr_Callback(t, status, NULL, 0);
    }
}
//-
//+ ohci_TL_Cancel
void ohci_TL_Cancel(OHCI1394Unit *unit, HeliosTransaction *t)
{
    OHCI1394ATPacketData *pdata;

    /* Check if not responded yet */
    LOCK_REGION_SHARED(unit);
    pdata = t->htr_Private;
    UNLOCK_REGION_SHARED(unit);

    if (NULL != pdata)
    {
        /* Forbid AT-handler to call the ack callback */
        ohci_CancelATPacket(unit, pdata);

        /* Force the status to ACK_CANCELLED value */
        ohci_TL_Finish(unit, t, HELIOS_RCODE_CANCELLED);
    }
}
//-

//+ ohci_TL_HandleResponse
void ohci_TL_HandleResponse(OHCI1394Unit *unit, HeliosAPacket *resp)
{
    HeliosTransaction *t;
    UBYTE tlabel = resp->TLabel;
    OHCI1394ATPacketData *pdata;

    _INFO_UNIT(unit, "$%04x -> $%04x: TC=$%x, TL=$%x, Ack=%u, RCode=%u\n",
               resp->SourceID, resp->DestID, resp->TCode, resp->TLabel, resp->Ack, resp->RCode);

    LOCK_REGION(unit);
    {
        _INFO_UNIT(unit, "TL=$%x Payload=%p, %lu byte(s)\n", resp->TLabel, resp->Payload, resp->PayloadLength);
        t = unit->hu_Transactions[tlabel];

        /* for response incoming packet we're going to check if it expected or not,
         * that could speed-up bit the process if the packet wasn't expected.
         */
        if ((NULL != t) && (resp->SourceID == t->htr_Packet.DestID))
        {
            OHCI1394SplitTimeReq *req;

            /* remove transaction from the pending list */
            unit->hu_Transactions[tlabel] = NULL;
            unit->hu_TLabelMap &= ~(1ull << tlabel);
            t->htr_Packet.TLabel = -1;

            pdata = t->htr_Private;
            t->htr_Private = NULL;

            /* Abort SPLIT-TIMER request */
            req = t->htr_SplitTimerReq;
            if (0 != req->req.tr_node.io_Command)
            {
                req->transaction = NULL;
                if (!CheckIO(&req->req.tr_node))
                    AbortIO(&req->req.tr_node);
                t->htr_SplitTimerReq = NULL;
            }

            /* Cancel ATComplete handler */
            if (NULL != pdata)
                ohci_CancelATPacket(unit, pdata);
        }
        else
        {
            _ERR_UNIT(unit, "Unwaited response packet (t=%p, DestID=$%04x)\n",
                      t, NULL!=t ? t->htr_Packet.DestID : -1);
            t = NULL;
            pdata = NULL;
        }
    }
    UNLOCK_REGION(unit);

    if (NULL == t)
        return;

    if (TCODE_READ_QUADLET_RESPONSE == resp->TCode)
    {
        resp->Payload = &resp->QuadletData;
        resp->PayloadLength = 4;
    }

    /* Let the transaction layer handle the response */
    t->htr_Callback(t, resp->RCode, resp->Payload, resp->PayloadLength);
}
//-
//+ ohci_TL_HandleRequest
void ohci_TL_HandleRequest(OHCI1394Unit *unit, HeliosAPacket *req, UBYTE generation)
{
    HeliosResponse *response = NULL;

    _INFO_UNIT(unit, "$%04x -> $%04x: TC=$%x, TL=$%x, Ack=%u, Offset=$%016llx\n",
               req->SourceID, req->DestID, req->TCode, req->TLabel, req->Ack, req->Offset);

    /* don't handle if ack error */
    if ((req->Ack != HELIOS_ACK_PENDING) && (req->Ack != HELIOS_ACK_COMPLETE))
		return;

    LOCK_REGION_SHARED(&unit->hu_ReqHandlerData);
    {
        HeliosHWReqHandler *node;
        BOOL found = FALSE;

        ForeachNode(&unit->hu_ReqHandlerData.rhd_List, node)
        {
            if ((req->Offset >= node->rh_Start) && (req->Offset < (node->rh_Start+node->rh_Length)))
            {
                _INFO_UNIT(unit, "handler=%p, callback=%p\n", node, node->rh_ReqCallback);
                response = node->rh_ReqCallback(req, node->rh_UserData);
                found = TRUE;
                break; /* handlers not overlap */
            }
        }

        if (!found)
        {
            response = &bad_address_response;
            _ERR_UNIT(unit, "no handler found\n");
        }
    }
    UNLOCK_REGION_SHARED(&unit->hu_ReqHandlerData);

    if (NULL != response)
        tl_send_response(unit, req, response, generation);
}
//-

//+ ohci_TL_SendRequest
/* do not use it if tcode one of :
 * TCODE_WRITE_PHY, TCODE_WRITE_STREAM
 */
LONG ohci_TL_SendRequest(OHCI1394Unit *unit,
                         HeliosTransaction *t,
                         UWORD destid,
                         HeliosSpeed speed,
                         UBYTE generation,
                         UBYTE tcode,
                         UWORD extcode,
                         HeliosOffset offset,
                         QUADLET *payload,
                         ULONG length)
{
    OHCI1394ATPacketData *pdata;
    HeliosAPacket resp;
    UWORD nodeid;
    ULONG split_timeout;
    OHCI1394SplitTimeReq *req;

    t->htr_Packet.DestID = destid;
    t->htr_Packet.TimeStamp = 0;

    /* Prepare the packet */
    t->htr_Packet.Header[0] = AT_HEADER_SPEED(speed) |
        AT_HEADER_TLABEL(t->htr_Packet.TLabel) |
        AT_HEADER_RT(RETRY_X) |
        AT_HEADER_TCODE(tcode);
    t->htr_Packet.Header[1] = AT_HEADER_DEST_ID(destid) | (UWORD)(offset >> 32);
    t->htr_Packet.Header[2] = offset;

    switch (tcode)
    {
        case TCODE_READ_BLOCK_REQUEST:
        case TCODE_WRITE_BLOCK_REQUEST:
            t->htr_Packet.Header[3] = AT_HEADER_LEN(length);
            break;

        case TCODE_WRITE_QUADLET_REQUEST:
            t->htr_Packet.Header[3] = payload[0];
            break;

        case TCODE_LOCK_REQUEST:
            t->htr_Packet.Header[3] = AT_HEADER_LEN(length) | AT_HEADER_EXTCODE(extcode);
            break;
    }

    if (-1 == ohci_TL_Register(unit, t, tl_ATCompleteCb, t))
        return IOERR_UNITBUSY;

    pdata = t->htr_Private;

    LOCK_REGION_SHARED(unit);
    {
        split_timeout = unit->hu_SplitTimeout;
        nodeid = unit->hu_LocalNodeId;
    }
    UNLOCK_REGION_SHARED(unit);

    /* setup the SPLIT-TIMEOUT timer request */
    t->htr_SplitTimerReq = req = AllocPooled(unit->hu_MemPool, sizeof(OHCI1394SplitTimeReq));
    if (NULL == req)
    {
        _ERR("SplitTime request alloc failed\n");
        return IOERR_NOMEMORY;
    }

    CopyMem(unit->hu_SplitTimeoutIOReq, req, sizeof(OHCI1394SplitTimeReq));
    req->transaction = t;
    req->req.tr_node.io_Message.mn_Length = sizeof(OHCI1394SplitTimeReq);
    req->req.tr_node.io_Command = 0;
    req->req.tr_time.tv_secs = split_timeout >> 15;
    req->req.tr_time.tv_micro = (split_timeout & 0x7fff) * 125;

    /* Remote node ? */
    if (destid != nodeid)
        return ohci_ATContext_Send(&unit->hu_ATRequestCtx, generation, &t->htr_Packet.Header[0],
                                   payload, pdata, t->htr_Packet.TLabel, 0);

    /* Local packet handling */
    memset(&resp, 0, sizeof(resp));
    resp.Header[0] = t->htr_Packet.Header[0];
    resp.Header[1] = t->htr_Packet.Header[1];

    /* TODO: length/payload sanity checks ? */

    switch (tcode)
    {
        case TCODE_WRITE_QUADLET_REQUEST:
            t->htr_Packet.QuadletData = payload[0];
        case TCODE_LOCK_REQUEST:
            t->htr_Packet.ExtTCode = extcode;
        case TCODE_WRITE_BLOCK_REQUEST:
        case TCODE_READ_BLOCK_REQUEST:
            t->htr_Packet.Payload = payload;
            t->htr_Packet.PayloadLength = length;
            t->htr_Packet.Offset = offset;
            resp.Header[2] = t->htr_Packet.Header[2];
            resp.Header[3] = t->htr_Packet.Header[3];
            break;
            
        case TCODE_READ_QUADLET_REQUEST:
            t->htr_Packet.Payload = NULL;
            t->htr_Packet.PayloadLength = 0;
            t->htr_Packet.Offset = offset;
            break;
            
        default:
            _ERR_UNIT(unit, "Unsupported TCode $%X as local request\n", tcode);

            /* emulate HELIOS_ACK_TYPE_ERROR received */
            tl_ATCompleteCb(unit, HELIOS_ACK_TYPE_ERROR, ohci_GetTimeStamp(unit), pdata);
            return HHIOERR_NO_ERROR;
    }
    
    t->htr_Packet.TCode = tcode;
    t->htr_Packet.Speed = speed;
    t->htr_Packet.Retry = 1;
    
    /* Locking the region now, because a block of data (i.e. ROM) may be
     * used as payload response and we need to wait its usage during
     * the call to ohci_TL_HandleResponse() before modifications.
     */
    LOCK_REGION(unit);
    {
        if (ohci_GenerationOK(unit, generation))
        {
            /* Fill the response packet */
            ohci_HandleLocalRequest(unit, &t->htr_Packet, &resp);
        
            /* Handle the response as received from the AR-Resp context */
            ohci_TL_HandleResponse(unit, &resp);
        }
        else
        {
            /* Simulate a flush after busreset */
            _ERR_UNIT(unit, "Generation mismatch\n");
            pdata->pd_AckCallback(unit, HELIOS_RCODE_GENERATION, ohci_GetTimeStamp(unit), pdata);
        }
    }
    UNLOCK_REGION(unit);

    return HHIOERR_NO_ERROR;
}
//-
//+ ohci_TL_DoRequest
/* TCODE_WRITE_STREAM not supported */
LONG ohci_TL_DoRequest(OHCI1394Unit *unit,
                       UBYTE sigbit,
                       UWORD destid,
                       HeliosSpeed speed,
                       UBYTE generation,
                       UBYTE tcode,
                       UWORD extcode,
                       HeliosOffset offset,
                       QUADLET *payload,
                       ULONG length)
{
    HeliosTransaction t;
    LONG res;
    DoRequestUData udata;

    udata.task = FindTask(NULL);
    udata.signal = 1ul << sigbit;
    udata.status = HELIOS_ACK_NOTSET;
    udata.payload = payload;
    udata.length = length;

    t.htr_Callback = tl_DoReqHandleResp;
    t.htr_UserData = &udata;

    SetSignal(0, udata.signal);
    res = ohci_TL_SendRequest(unit, &t, destid, speed, generation,
                              tcode, extcode, offset, payload, length);
    if (HHIOERR_NO_ERROR == res)
    {
        ULONG sigs;
        
        sigs = Wait(udata.signal | SIGBREAKF_CTRL_C);
        if (0 == (sigs & udata.signal))
            ohci_TL_Cancel(unit, &t);
    }
    else if (IOERR_UNITBUSY)
        return HELIOS_RCODE_BUSY;
    else
        return HELIOS_RCODE_SEND_ERROR;

    return udata.status;
}
//-
//+ ohci_TL_DoPHYPacket
LONG ohci_TL_DoPHYPacket(OHCI1394Unit *unit,
                         UBYTE sigbit,
                         QUADLET value)
{
    OHCI1394ATPacketData pdata;
    DoRequestUData udata;
    LONG res;
    
    udata.task = FindTask(NULL);
    udata.signal = 1ul << sigbit;
    udata.status = HELIOS_ACK_NOTSET;

    pdata.pd_AckCallback = tl_PHY_ATCompleteCb;
    pdata.pd_UData = &udata;

    SetSignal(0, udata.signal);
    res = ohci_SendPHYPacket(unit, S100, value, &pdata);
    if (HHIOERR_NO_ERROR == res)
    {
        ULONG sigs, sig = 1ul << sigbit;

        sigs = Wait(sig | SIGBREAKF_CTRL_C);
        if (0 == (sigs & sig))
            ohci_CancelATPacket(unit, &pdata);
    }
    else if (IOERR_UNITBUSY)
        return HELIOS_ACK_BUSY_X;
    else
        return HELIOS_ACK_TYPE_ERROR;

    return udata.status;
}
//-

//+ ohci_TL_AddReqHandler
LONG ohci_TL_AddReqHandler(OHCI1394Unit *unit, HeliosHWReqHandler *reqh)
{
    LONG err = HHIOERR_FAILED;
    HeliosHWReqHandler *node;
    HeliosOffset start;

    /* Sanity checks:
     *  - Region is 4-bytes aligned and limited to 48bits.
     *  - Length shall be 4-bytes aligned and not zero.
     * TODO: check against the physicalUpperBound value.
     */
    if ((reqh->rh_RegionStart & 0xFFFF000000000003ULL) ||
        (reqh->rh_RegionStop & 0xFFFF000000000003ULL) ||
        (reqh->rh_RegionStart >= reqh->rh_RegionStop) ||
        (reqh->rh_Length & 3) ||
        (0 == reqh->rh_Length))
        return IOERR_BADADDRESS;

    /* TODO: I support only HHF_REQH_ALLOCLEN allocation type */
    if (reqh->rh_Flags != HHF_REQH_ALLOCLEN)
        return HHIOERR_FAILED;

    start = reqh->rh_RegionStart;
    node = (APTR)GetHead(&unit->hu_ReqHandlerData.rhd_List);

    _INFO_UNIT(unit, "start=$%llx, node=%p\n", start, node);

    LOCK_REGION(&unit->hu_ReqHandlerData);
    {
        while ((start + reqh->rh_Length) < reqh->rh_RegionStop)
        {
            /* no more registred handlers or requested is below */
            if ((NULL == node) || ((start + reqh->rh_Length) <= node->rh_Start))
            {
                reqh->rh_Start = start;
                ADDTAIL(&unit->hu_ReqHandlerData.rhd_List, reqh);
                err = HHIOERR_NO_ERROR;
                break;
            }

            start = node->rh_Start + node->rh_Length;
            node = (APTR)GetSucc(node);
        }
    }
    UNLOCK_REGION(&unit->hu_ReqHandlerData);

    _INFO_UNIT(unit, "err=%ld\n", err);

    return err;
}
//-
//+ ohci_TL_RemReqHandler
LONG ohci_TL_RemReqHandler(OHCI1394Unit *unit, HeliosHWReqHandler *reqh)
{
    LOCK_REGION(&unit->hu_ReqHandlerData);
    REMOVE(reqh);
    UNLOCK_REGION(&unit->hu_ReqHandlerData);

    return HHIOERR_NO_ERROR;
}
//-
