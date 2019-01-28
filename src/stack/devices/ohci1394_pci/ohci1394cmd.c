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
** Low-level software support to OHCI 1394 compliant brigdes.
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
**
*/

/* XXX: Per default unit accesses are not multithread safe.
 * LOCK_REGION()/UNLOCK_REGION() pair shall be used to protect accesses if needed.
 */

#define NDEBUG

#include "ohci1394.device.h"
#include "ohci1394core.h"
#include "ohci1394topo.h"
#include "ohci1394trans.h"
#include "ohci1394dev.h"

#include <exec/errors.h>

#include <proto/utility.h>
#include <proto/exec.h>

#include <clib/macros.h>

#include <string.h>

/*----------------------------------------------------------------------------*/
/*--- LOCAL CODE SECTION -----------------------------------------------------*/

//+ ohci_ReplyIOReq
/* WARNING: call it with a locked unit */
static void cmd_reply_ioreq(IOHeliosHWReq *ioreq, LONG err, BYTE status)
{
    struct Message *msg;

    ioreq->iohh_Req.io_Error = err;
    
    msg = &ioreq->iohh_Req.io_Message;
    if (msg->mn_Node.ln_Type != NT_MESSAGE)
        _INFO("bad msg type (%p-%u)\n", msg, msg->mn_Node.ln_Type);

    ReplyMsg(msg);
}
//-
//+ _cmd_HandlePHYATComplete
static void _cmd_HandlePHYATComplete(OHCI1394Unit *unit,
                                     BYTE status, UWORD timestamp,
                                     OHCI1394ATPacketData *pdata)
{
    IOHeliosHWReq *ioreq = pdata->pd_UData;
    LONG err;

    _INFO_UNIT(unit, "ioreq=%p, status=%d, TS=%u.%04u\n",
               ioreq, status, timestamp >> 13, timestamp & 0x1fff);

    if (HELIOS_ACK_COMPLETE == status)
        err = HHIOERR_NO_ERROR;
    else
        err = HHIOERR_FAILED;

    FreeMem(pdata, sizeof(*pdata));
    ioreq->iohh_Private = NULL;

    ioreq->iohh_Actual = timestamp;
    cmd_reply_ioreq(ioreq, err, status);
}
//-
//+ _cmd_HandleResponse
static void _cmd_HandleResponse(HeliosTransaction *t,
                                BYTE status,
                                QUADLET *payload,
                                ULONG length)
{
    IOHeliosHWSendRequest *ioreq;
    LONG err;

    _INFO("t=%p, status=%d, payload=%p, length=%lu\n", t, status, payload, length);

    t->htr_Packet.RCode = status;
    ioreq = t->htr_UserData;

    if (HELIOS_RCODE_COMPLETE == status)
    {
        err = HHIOERR_NO_ERROR;

        if (NULL != payload)
        {
            if (NULL != ioreq->iohhe_Req.iohh_Data)
            {
                ioreq->iohhe_Req.iohh_Actual = MIN(ioreq->iohhe_Req.iohh_Length, length);
                _INFO("Copy %lu bytes into buffer at $%p\n",
                      ioreq->iohhe_Req.iohh_Actual, ioreq->iohhe_Req.iohh_Data);
                CopyMem(payload, ioreq->iohhe_Req.iohh_Data, ioreq->iohhe_Req.iohh_Actual);
            }
            else if (sizeof(QUADLET) == length)
            {
                ioreq->iohhe_Req.iohh_Actual = length;
                ioreq->iohhe_Transaction.htr_Packet.QuadletData = payload[0];
            }
            else
            {
                /* don't know how to save data */
                t->htr_Packet.RCode = status = HELIOS_RCODE_DATA_ERROR;
                ioreq->iohhe_Req.iohh_Actual = 0;
                err = HHIOERR_FAILED;
            }
        }
        else if (TCODE_WRITE_QUADLET_REQUEST == t->htr_Packet.TCode)
            ioreq->iohhe_Req.iohh_Actual = sizeof(QUADLET);
        else if ((TCODE_WRITE_BLOCK_REQUEST == t->htr_Packet.TCode) ||
                 (TCODE_WRITE_STREAM == t->htr_Packet.TCode))
            ioreq->iohhe_Req.iohh_Actual = t->htr_Packet.PayloadLength;
    }
    else
        err = HHIOERR_FAILED;

    cmd_reply_ioreq(&ioreq->iohhe_Req, err, status);
}
//-

/*----------------------------------------------------------------------------*/
/*--- PUBLIC CODE SECTION ----------------------------------------------------*/

//+ cmdQueryDevice
CMDP(cmdQueryDevice)
{
    struct TagItem *tag, *tags;
    ULONG count = 0;

    _INFO_UNIT(unit, "HHIOCMD_QUERYDEVICE (tags @ %p)\n", ioreq->iohh_Data);

    tags = (struct TagItem *) ioreq->iohh_Data;
    while (NULL != (tag = NextTagItem(&tags)))
    {
        switch (tag->ti_Tag) {
            case OHCI1394A_PciVendorId:
                *(UWORD *)tag->ti_Data = unit->hu_PCI_VID;
                count++;
                break;

            case OHCI1394A_PciDeviceId:
                *(UWORD *)tag->ti_Data = unit->hu_PCI_DID;
                count++;
                break;

            case HHA_VendorUnique:
                *(ULONG *)tag->ti_Data = unit->hu_OHCI_VendorID >> 24;
                count++;
                break;

            case HHA_VendorCompagnyId:
                *(ULONG *)tag->ti_Data = unit->hu_OHCI_VendorID & 0xffffff;
                count++;
                break;

            case HHA_Manufacturer:
                *(STRPTR *)tag->ti_Data = "Guillaume ROGUEZ";
                count++;
                break;

            case HHA_ProductName:
                *(STRPTR *)tag->ti_Data = "ohci1394_pci.device";
                count++;
                break;

            case HHA_Version:
                *(ULONG *)tag->ti_Data = VERSION;
                count++;
                break;

            case HHA_Revision:
                *(ULONG *)tag->ti_Data = REVISION;
                count++;
                break;

            case HHA_Copyright:
                *(STRPTR *)tag->ti_Data = COPYRIGHTS;
                count++;
                break;

            case HHA_Description:
                *(STRPTR *)tag->ti_Data = "Generic low-level driver for PCI OHCI 1394 Chipset";
                count++;
                break;

            case HHA_DriverVersion:
                *(ULONG *)tag->ti_Data = unit->hu_OHCI_Version;
                count++;
                break;

            case HHA_Capabilities:
                *(ULONG *)tag->ti_Data = HHF_1394A_1995;
                count++;
                break;

            case HHA_TimeStamp:
                *(UWORD *)tag->ti_Data = ohci_GetTimeStamp(unit);
                count++;
                break;

            case HHA_UpTime:
                *(UQUAD *)tag->ti_Data = ohci_UpTime(unit);
                count++;
                break;

            case HHA_LocalGUID:
                *(UQUAD *)tag->ti_Data = unit->hu_GUID;
                count++;
                break;

            case HHA_TopologyGeneration:
                LOCK_REGION_SHARED(unit);
                {
                    if (unit->hu_Topology)
                        *(ULONG *)tag->ti_Data = unit->hu_Topology->ht_Generation;
                    else
                        *(ULONG *)tag->ti_Data = 0;
                }
                UNLOCK_REGION_SHARED(unit);
                count++;
                break;

            case HHA_Topology:
                LOCK_REGION_SHARED(unit);
                {
                    if (NULL != unit->hu_Topology)
                        CopyMemQuick(unit->hu_Topology, (APTR)tag->ti_Data, sizeof(HeliosTopology));
                    else
                        ((HeliosTopology *)tag->ti_Data)->ht_Generation = 0;
                }
                UNLOCK_REGION_SHARED(unit);
                count++;
                break;

            case HHA_NodeCount:
                LOCK_REGION_SHARED(unit);
                {
                    if (NULL != unit->hu_Topology)
                        *(ULONG *)tag->ti_Data = unit->hu_Topology->ht_NodeCount;
                    else
                        *(ULONG *)tag->ti_Data = 0;
                }
                UNLOCK_REGION_SHARED(unit);
                count++;
                break;
        }
    }

    ioreq->iohh_Actual = count;
    return FALSE;
}
//-
//+ cmdReset
CMDP(cmdReset)
{
    LOCK_REGION(unit);
    {
        _INFO_UNIT(unit, "CMD_RESET\n");

        ohci_Term(unit);
        if (ohci_Init(unit))
        {
            if (!ohci_Enable(unit))
            {
                ohci_Term(unit);
                ioreq->iohh_Req.io_Error = HHIOERR_FAILED;
            }
        }
    }
    UNLOCK_REGION(unit);

    return FALSE;
}
//-
//+ cmdEnable
CMDP(cmdEnable)
{
    LOCK_REGION(unit);
    {
        _INFO_UNIT(unit, "HHIOCMD_ENABLE\n");

        if (!ohci_Enable(unit))
            ioreq->iohh_Req.io_Error = HHIOERR_FAILED;
    }
    UNLOCK_REGION(unit);

    return FALSE;
}
//-
//+ cmdDisable
CMDP(cmdDisable)
{
    LOCK_REGION(unit);
    {
        _INFO_UNIT(unit, "HHIOCMD_DISABLE\n");

        ohci_Disable(unit);
    }
    UNLOCK_REGION(unit);

    return FALSE;
}
//-
//+ cmdBusReset
CMDP(cmdBusReset)
{
    LOCK_REGION(unit);
    {
        _INFO_UNIT(unit, "HHIOCMD_BUSRESET\n");

        if (!ohci_RaiseBusReset(unit, (LONG)ioreq->iohh_Data))
            ioreq->iohh_Req.io_Error = HHIOERR_FAILED;
    }
    UNLOCK_REGION(unit);

    return FALSE;
}
//-
//+ cmdSendRequest
CMDP(cmdSendRequest)
{
    IOHeliosHWSendRequest *ioreqext;
    HeliosTransaction *t;
    HeliosAPacket *p;
    UBYTE gen;
    LONG err;
    ULONG topogen;
    UWORD destid;

    _INFO_UNIT(unit, "HHIOCMD_SENDREQUEST\n");

    if (ioreq->iohh_Req.io_Message.mn_Length < sizeof(IOHeliosHWSendRequest))
    {
        _ERR_UNIT(unit, "Invalid IO message length\n");
        ioreq->iohh_Req.io_Error = IOERR_BADLENGTH;
        return FALSE;
    }

    ioreqext = (IOHeliosHWSendRequest *)ioreq;
    t = &ioreqext->iohhe_Transaction;
    p = &t->htr_Packet;

    LOCK_REGION_SHARED(unit);
    {
        if (unit->hu_Topology)
            topogen = unit->hu_Topology->ht_Generation;
        else
            topogen = 0; /* will cause an error below */
        gen = unit->hu_OHCI_LastGeneration;
    }
    UNLOCK_REGION_SHARED(unit);

    if (NULL != ioreqext->iohhe_Device)
    {
        /* Outdated device ? */
        if ((0 == topogen) || (topogen != ioreqext->iohhe_Device->hd_Generation))
        {
            _ERR_UNIT(unit, "topogen mismatch: cur=%lu, dev=%lu\n",
                      topogen, ioreqext->iohhe_Device->hd_Generation);
            p->RCode = HELIOS_RCODE_GENERATION;
            ioreq->iohh_Req.io_Error = HHIOERR_NO_ERROR;
            return FALSE;
        }
        destid = ioreqext->iohhe_Device->hd_NodeID;
    }
    else
        destid = p->DestID;

    t->htr_Callback = _cmd_HandleResponse;
    t->htr_UserData = ioreq;

    if (TCODE_WRITE_QUADLET_REQUEST == p->TCode)
    {
        p->Payload = &p->QuadletData;
        p->PayloadLength = sizeof(QUADLET);
    }

    ioreq->iohh_Actual = 0; /* will contains number of bytes read if needed */
    err = ohci_TL_SendRequest(unit, t, destid, p->Speed, gen, p->TCode,
                              p->ExtTCode, p->Offset, p->Payload, p->PayloadLength);
    if (HHIOERR_NO_ERROR == err)
        return TRUE;

    ioreq->iohh_Req.io_Error = err;
    return FALSE;
}
//-
//+ abortSendRequest
CMDP(abortSendRequest)
{
    HeliosTransaction *t = (HeliosTransaction *)ioreq->iohh_Data;

    _INFO_UNIT(unit, "Abort SENDREQUEST\n");

    ohci_TL_Cancel(unit, t);
    return TRUE;
}
//-
//+ cmdSendPhy
CMDP(cmdSendPhy)
{
    OHCI1394ATPacketData *pdata;
    LONG err;

    _INFO_UNIT(unit, "HHIOCMD_SENDPHY\n");

    pdata = AllocMem(sizeof(OHCI1394ATPacketData), MEMF_PUBLIC);
    if (NULL == pdata)
    {
        ioreq->iohh_Req.io_Error = HHIOERR_NOMEM;
        return FALSE;
    }

    pdata->pd_AckCallback = _cmd_HandlePHYATComplete;
    pdata->pd_UData = ioreq;

    ioreq->iohh_Private = pdata;
    ioreq->iohh_Actual = HELIOS_ACK_NOTSET;

    err = ohci_SendPHYPacket(unit, S100, (QUADLET)ioreq->iohh_Data, pdata);
    if (HHIOERR_NO_ERROR != err)
    {
        ioreq->iohh_Req.io_Error = err;
        FreeMem(pdata, sizeof(*pdata));
        return FALSE;
    }

    return TRUE;
}
//-
//+ cmdAddReqHandler
CMDP(cmdAddReqHandler)
{
    LONG err;

    _INFO_UNIT(unit, "HHIOCMD_ADDREQHANDLER\n");

    err = ohci_TL_AddReqHandler(unit, (HeliosHWReqHandler *)ioreq->iohh_Data);
    ioreq->iohh_Req.io_Error = err;

    return FALSE;
}
//-
//+ cmdRemReqHandler
CMDP(cmdRemReqHandler)
{
    LONG err;

    _INFO_UNIT(unit, "HHIOCMD_REMREQHANDLER\n");

    err = ohci_TL_RemReqHandler(unit, (HeliosHWReqHandler *)ioreq->iohh_Data);
    ioreq->iohh_Req.io_Error = err;

    return FALSE;
}
//-
//+ cmdSetAttrs
CMDP(cmdSetAttrs)
{
    struct TagItem *tag, *tags;
    LONG err=0;

    _INFO_UNIT(unit, "HHIOCMD_SETATTRIBUTES\n");

    ioreq->iohh_Actual = 0;

    tags = ioreq->iohh_Data;
    while (NULL != (tag = NextTagItem(&tags)))
    {
        switch (tag->ti_Tag)
        {
            case HA_Rom:
                err = ohci_SetROM(unit, (QUADLET *)tag->ti_Data);
                ioreq->iohh_Actual++;
                break;
        }

        if (err)
            break;
    }

    ioreq->iohh_Req.io_Error = err;

    return FALSE;
}
//-
//+ cmdDelIsoCtx
CMDP(cmdDelIsoCtx)
{
    _INFO_UNIT(unit, "HHIOCMD_DELETEISOCONTEXT\n");

    ohci_IRContext_Destroy(ioreq->iohh_Data);

    return FALSE;
}
//-
//+ cmdCreateIsoCtx
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

    ioreq->iohh_Actual = 0;

    tags = ioreq->iohh_Data;
    while (NULL != (tag = NextTagItem(&tags)))
    {
        ioreq->iohh_Actual++;

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

            default: ioreq->iohh_Actual--; break;
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

    ioreq->iohh_Req.io_Error = err;
    return FALSE;
}
//-
//+ cmdStartIsoCtx
CMDP(cmdStartIsoCtx)
{
    struct TagItem *tag, *tags;
    ULONG channel=-1, isotag=0;
    OHCI1394Context *ctx=NULL;
    LONG err=HHIOERR_FAILED;

    _INFO_UNIT(unit, "HHIOCMD_STARTISOCONTEXT\n");

    ioreq->iohh_Actual = 0;

    tags = ioreq->iohh_Data;
    while (NULL != (tag = NextTagItem(&tags)))
    {
        ioreq->iohh_Actual++;

        switch (tag->ti_Tag)
        {
            case HA_IsoContext: ctx = (APTR)tag->ti_Data; break;
            case HA_IsoChannel: channel = tag->ti_Data; break;
            case HA_IsoTag: isotag = tag->ti_Data; break;

            default: ioreq->iohh_Actual--; break;
        }
    }

    if (NULL != ctx)
    {
        if (ctx->ctx_RegOffset >= OHCI1394_REG_IRECV_CONTEXT_CONTROL(0))
            ohci_IRContext_Start((OHCI1394IRCtx *)ctx, channel, isotag);
        else
            DB_NotImplemented();
    }

    ioreq->iohh_Req.io_Error = err;
    return FALSE;
}
//-
//+ cmdStopIsoCtx
CMDP(cmdStopIsoCtx)
{
    OHCI1394Context *ctx;

    _INFO_UNIT(unit, "HHIOCMD_STOPISOCONTEXT\n");

    ctx = ioreq->iohh_Data;
    if (ctx->ctx_RegOffset >= OHCI1394_REG_IRECV_CONTEXT_CONTROL(0))
        ohci_IRContext_Stop((OHCI1394IRCtx *)ioreq->iohh_Data);
    else
        DB_NotImplemented();

    return FALSE;
}
//-

