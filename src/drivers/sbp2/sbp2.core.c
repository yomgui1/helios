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
** This file is copyrights 2008-2013 by Guillaume ROGUEZ.
**
** SBP2 class core API.
*/

/* Define it when SCSI data address shall be 4-bytes aligned */
//#define ERROR_ON_BAD_ALIGNMENT
#define DEBUG_MEM

#include "sbp2.class.h"
#include "sbp2.device.h"
#include "sbp2.iocmd.h"

#include <clib/macros.h>
#include <hardware/byteswap.h>
#include <scsi/commands.h>
#include <scsi/values.h>
#include <dos/dostags.h>

#include <proto/alib.h>
#include <proto/dos.h>
#include <proto/mount.h>
#include <proto/exec.h>

#include <string.h>
#include <stdlib.h>

#define YESNO(x) ((x)?"yes":"no")

/* Status write response codes */
#define SBP2_STATUS_REQUEST_COMPLETE	0x0
#define SBP2_STATUS_TRANSPORT_FAILURE	0x1
#define SBP2_STATUS_ILLEGAL_REQUEST		0x2
#define SBP2_STATUS_VENDOR_DEPENDENT	0x3

/* Request status */
#define SBP2_REQ_ST_DUMMY_ORB_COMPLETED	11

/* ISO/IEC 14776-312 : 2001 */
#define SPC2_VERSION 4

#define SBP2_CYCLE_LIMIT (200 << 12) /* 200 x 125us cycles */
#define SBP2_RETRY_LIMIT 15

#define REMOVABLE_CHECK_TIMEOUT 3000

#define CSR_SPLIT_TIMEOUT 0x018
#define CSR_BUSY_TIMEOUT 0x210

#define SBP2_UNIT_SPEC_ID_ENTRY	0x0000609e
#define SBP2_SW_VERSION_ENTRY	0x00010483

#define SB2_STATE_RESET		0
#define SB2_STATE_ACTIVE	1
#define SB2_STATE_SUSPENDED	2
#define SB2_STATE_DEAD		3

/* SBP2 ROM keys */
#define KEY_SBP2_COMMAND_SET_SPEC_ID    0x38
#define KEY_SBP2_COMMAND_SET            0x39
#define KEY_SBP2_UNIT_CHARACTERISTICS   0x3a
#define KEY_SBP2_COMMAND_SET_REVISION   0x3b
#define KEY_SBP2_FIRMWARE_REVISION      0x3c
#define KEY_SBP2_RECONNECT_TIMEOUT      0x3d
#define KEY_SBP2_CSR_OFFSET             0x54
#define KEY_SBP2_LOGICAL_UNIT_NUMBER    0x14
#define KEY_SBP2_LOGICAL_UNIT_DIRECTORY 0xd4

#define KEYTYPE_IMMEDIATE   0
#define KEYTYPE_OFFSET      1
#define KEYTYPE_LEAF        2
#define KEYTYPE_DIRECTORY   3

/*--- debugging --------------------------------------------------------------*/
#define _INF_HUNIT(hu, fmt, args...) _INF("{hu-%p} " fmt, hu, ## args)
#define _WRN_HUNIT(hu, fmt, args...) _WRN("{hu-%p} " fmt, hu, ## args)
#define _ERR_HUNIT(hu, fmt, args...) _ERR("{hu-%p} " fmt, hu, ## args)
#define _DBG_HUNIT(hu, fmt, args...) _DBG("{hu-%p} " fmt, hu, ## args)

#define _INF_UNIT(u, fmt, args...) _INF("{SBP2-%p} " fmt, (u), ## args)
#define _WRN_UNIT(u, fmt, args...) _WRN("{SBP2-%p} " fmt, (u), ## args)
#define _ERR_UNIT(u, fmt, args...) _ERR("{SBP2-%p} " fmt, (u), ## args)
#define _DBG_UNIT(u, fmt, args...) _DBG("{SBP2-%p} " fmt, (u), ## args)

#define _INF_ORB(u, o, fmt, args...) _INF_UNIT(u, "{ORB-%p} " fmt, (o), ## args)
#define _WRN_ORB(u, o, fmt, args...) _WRN_UNIT(u, "{ORB-%p} " fmt, (o), ## args)
#define _ERR_ORB(u, o, fmt, args...) _ERR_UNIT(u, "{ORB-%p} " fmt, (o), ## args)
#define _DBG_ORB(u, o, fmt, args...) _DBG_UNIT(u, "{ORB-%p} " fmt, (o), ## args)

/*============================================================================*/
/*--- LOCAL CODE -------------------------------------------------------------*/
/*============================================================================*/
#if 0
#ifndef NDEBUG
//+ sbp2_do_read_block
static LONG sbp2_do_read_block(SBP2ClassLib *base,
							   HeliosDevice *dev,
							   HeliosOffset offset,
							   HeliosSpeed speed,
							   APTR buf,
							   ULONG length)
{
	IOHeliosHWSendRequest ioreq;
	HeliosAPacket *p;
	LONG err;

	_INFO_1394("Read block @ offset $%016llX, length=%lu\n", offset, length);

	/* Fill iorequest */
	bzero(&ioreq, sizeof(struct IORequest));
	ioreq.iohhe_Req.iohh_Req.io_Message.mn_Length = sizeof(ioreq);
	ioreq.iohhe_Req.iohh_Req.io_Command = HHIOCMD_SENDREQUEST;
	ioreq.iohhe_Device = dev;

	/* Set the receive payload information */
	ioreq.iohhe_Req.iohh_Data = buf;
	ioreq.iohhe_Req.iohh_Length = length;

	/* Fill packet */
	p = &ioreq.iohhe_Transaction.htr_Packet;
	Helios_FillReadBlockPacket(p, speed, offset, length);

	err = Helios_DoIO(HGA_DEVICE, dev, &ioreq.iohhe_Req);
	if ((HHIOERR_NO_ERROR == err) && (ioreq.iohhe_Req.iohh_Actual == length))
		return 0;

	_ERR_1394("Error=%d, RCode=%d\n", err, p->RCode);

	return -1;
}
//-
//+ sbp2_do_write_block
static LONG sbp2_do_write_block(SBP2ClassLib *base,
								HeliosDevice *dev,
								HeliosOffset offset,
								HeliosSpeed speed,
								APTR buf,
								ULONG length)
{
	IOHeliosHWSendRequest ioreq;
	HeliosAPacket *p;
	LONG err;

	_INFO_1394("Write block @ offset $%016llX, length=%lu\n", offset, length);

	/* Fill iorequest */
	bzero(&ioreq, sizeof(struct IORequest));
	ioreq.iohhe_Req.iohh_Req.io_Message.mn_Length = sizeof(ioreq);
	ioreq.iohhe_Req.iohh_Req.io_Command = HHIOCMD_SENDREQUEST;
	ioreq.iohhe_Device = dev;
	ioreq.iohhe_Req.iohh_Data = NULL;
	ioreq.iohhe_Req.iohh_Length = 0;

	/* Fill packet */
	p = &ioreq.iohhe_Transaction.htr_Packet;
	Helios_FillWriteBlockPacket(p, speed, offset, buf, length);

	err = Helios_DoIO(HGA_DEVICE, dev, &ioreq.iohhe_Req);
	if ((HHIOERR_NO_ERROR == err) && (ioreq.iohhe_Req.iohh_Actual == length))
		return 0;

	_ERR_1394("Error=%d, RCode=%d\n", err, p->RCode);

	return -1;
}
//-
//+ dump_agent_state
static void dump_agent_state(SBP2Unit *unit)
{
	HeliosOffset offset=unit->u_ORBLoginResponse.command_agent+SBP2_AGENT_STATE;
	QUADLET status=0;
	LONG err;
	STRPTR s;

	err = sbp2_do_read_quadlet(unit->u_SBP2ClassBase, unit->u_HeliosDevice, offset, unit->u_MaxSpeed, &status);
	if (!err)
	{
		switch (status)
		{
			case 0: s="RESET"; break;
			case 1: s="ACTIVE"; break;
			case 2: s="SUSPENDED"; break;
			case 3: s="DEAD"; break;
			default: s="<unknown state>"; break;
		}
		
		_INFO_1394("DEV[$%p] AGENT_STATE=$%08x (%s)\n", unit, status, s);
	}
	else
		_ERR_1394("DEV[$%p] Can't fetch AGENT_STATE register offset %llx, err=%ld\n", unit, offset, err);
}
//-
#else
#define dump_agent_state(x)
#endif

//+ sbp2_add_timereq
static struct timerequest *sbp2_add_timereq(SBP2Unit *unit, ULONG timeout)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;
	struct timerequest *tr = NULL;

	tr = AllocMem(sizeof(*tr), MEMF_PUBLIC);
	if (NULL != tr)
	{
		CopyMem(unit->u_IOTimeReq, tr, sizeof(*tr));
		
		tr->tr_time.tv_secs = timeout / 1000;
		tr->tr_time.tv_micro = (timeout % 1000) * 1000;
		
		SendIO(&tr->tr_node);
	}

	return tr;
}
//-
//+ sbp2_free_timereq
static void sbp2_free_timereq(SBP2Unit *unit, struct timerequest *tr)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;

	FreeMem(tr, sizeof(*tr));
}
//-
//+ sbp2_agent_reset
static void sbp2_agent_reset(SBP2Unit *unit)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;
	IOHeliosHWSendRequest ioreq;
	HeliosAPacket *p;
#if 0
	ORBDummy orb;
	SBP2ORBRequest orbreq;
	struct MsgPort *port;
	ULONG status_signal;
	LONG ioerr, rcode;
	BYTE sigbit;
#endif

	/* TODO: procedure not correct, need to implement the one in SBP-2 doc
	 * at chapter 9.1.1.
	 */

	/* Fill iorequest */
	bzero(&ioreq, sizeof(struct IORequest));
	ioreq.iohhe_Req.iohh_Req.io_Message.mn_Length = sizeof(ioreq);
	ioreq.iohhe_Req.iohh_Req.io_Command = HHIOCMD_SENDREQUEST;
	ioreq.iohhe_Device = unit->u_HeliosDevice;

	/* No payload with writes */
	ioreq.iohhe_Req.iohh_Data = NULL;

	/* Fill packet */
	p = &ioreq.iohhe_Transaction.htr_Packet;
	Helios_FillWriteQuadletPacket(p, unit->u_MaxSpeed,
								  unit->u_ORBLoginResponse.command_agent + SBP2_AGENT_RESET,
								  0);

	Helios_DoIO(HGA_DEVICE, unit->u_HeliosDevice, &ioreq.iohhe_Req);

	dump_agent_state(unit);

#if 0
	port = CreateMsgPort();
	if (NULL == port)
	{
		_ERR("Failed to alloc port\n");
		return;
	}

	sigbit = AllocSignal(-1);
	if (sigbit < 0)
		goto out;

	status_signal = 1ul << sigbit;

	bzero(&orbreq, sizeof(orbreq));
	bzero(&orb, sizeof(orb));

	orbreq.or_Base.iohhe_Req.iohh_Req.io_Message.mn_Length = sizeof(orbreq);
	orbreq.or_Base.iohhe_Req.iohh_Req.io_Message.mn_ReplyPort = port;

	orb.next.q = ORBPOINTER_NULL;
	orb.control = ORBCONTROLF_NOTIFY | (3<<13);

	sbp2_send_orb_req(unit, &orbreq, &orb,
					  unit->u_ORBLoginResponse.command_agent + SBP2_ORB_POINTER,
					  sbp2_complete_dummy_orb, status_signal);
	sbp2_safe_waitmsg(port, &orbreq);

	ioerr = orbreq.or_Base.iohhe_Req.iohh_Req.io_Error;
	rcode = orbreq.or_Base.iohhe_Transaction.htr_Packet.RCode;

	_INFO("DummyORB: err=%ld, rcode=%ld\n", ioerr, rcode);

	if (!ioerr)
		sbp2_wait(unit, status_signal, ORB_TIMEOUT);

	sbp2_cancel_orbs(unit);
	FreeSignal(sigbit);
out:
	DeleteMsgPort(port);
#endif
}
//-
//+ sbp2_scsi_test_unit_ready
LONG sbp2_scsi_test_unit_ready(SBP2Unit *unit, UBYTE *sensedata)
{
	UBYTE cmd6[6];
	struct SCSICmd scsicmd;

	bzero(cmd6, 6);

	scsicmd.scsi_Data = NULL;
	scsicmd.scsi_Length = 0;
	scsicmd.scsi_Command = cmd6;
	scsicmd.scsi_CmdLength = 6;
	scsicmd.scsi_Flags = SCSIF_AUTOSENSE;
	scsicmd.scsi_SenseData = sensedata;
	scsicmd.scsi_SenseLength = 18;

	cmd6[0] = SCSI_TEST_UNIT_READY;

	return sbp2_do_scsi_cmd(unit, &scsicmd, ORB_TIMEOUT);
}
//-
//+ sbp2_logout
static void sbp2_logout(SBP2Unit *unit)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;
	ORBLogout orb;

	_INFO("====== SBP2 LOGOUT: %s ======\n",
		  unit->u_UnitName?unit->u_UnitName:(STRPTR)FindTask(NULL)->tc_Node.ln_Name);

	bzero(&orb, sizeof(orb));

	orb.control = ORBCONTROLF_NOTIFY | ORBCONTROLTYPE_LOGOUT;
	orb.loginid = unit->u_ORBLoginResponse.login_id;
	orb.statusfifo = unit->u_FSReqHandler.rh_Start;

	sbp2_do_management_orb(unit, &orb);
	sbp2_agent_reset(unit);

	LOCK_REGION(unit);
	{
		unit->u_Flags.Logged = 0;
		unit->u_Flags.AcceptIO = 0;
		unit->u_Flags.Ready = 0;
	}
	UNLOCK_REGION(unit);
}
//-
//+ sbp2_reconnect
static BOOL sbp2_reconnect(SBP2Unit *unit)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;
	ORBReconnect orb;
	ULONG retries;
	LONG err;

	/* Filling the login ORB data */
	orb.reserved = 0;
	orb.reserved1 = 0;
	orb.reserved2 = 0;
	orb.control = ORBCONTROLF_NOTIFY | ORBCONTROLTYPE_RECONNECT;
	orb.loginid = unit->u_ORBLoginResponse.login_id;
	orb.statusfifo = unit->u_FSReqHandler.rh_Start;

	retries = 5;

retry:
	_INFO("====== SBP2 RECONNECT: %s (try #%u) ======\n",
		  unit->u_UnitName?unit->u_UnitName:(STRPTR)FindTask(NULL)->tc_Node.ln_Name, 6-retries);
	err = sbp2_do_management_orb(unit, &orb);
	if (err)
	{
		if ((HELIOS_RCODE_BUSY == err) && retries--)
		{
			_INFO("Retry in 1s\n");
			Helios_DelayMS(1000);
			goto retry;
		}

		_ERR("Reconnect on node $%llx failed (err=%ld), fallback on login\n", unit->u_GUID, err);
		return sbp2_login(unit);
	}

	_INFO("Reconnect on node $%llx OK\n", unit->u_GUID);

	return TRUE;
}
//-
//+ sbp2_mount_cd
static void sbp2_mount_cd(SBP2Unit *unit)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;
	struct IOStdReq ioreq;
	ULONG *data;
	LONG ioerr;

	data = AllocVecDMA(unit->u_BlockSize, MEMF_PUBLIC);
	if (NULL == data)
	{
		_ERR("Failed to alloc block for READ10 cmd\n");
		return;
	}

	ioreq.io_Error = 0;
	ioreq.io_Command = TD_READ64;
	ioreq.io_HighOffset = 0;
	ioreq.io_LowOffset = 0x8000; /* 16th block, blocksize=2048 */
	ioreq.io_Length = unit->u_BlockSize;
	ioreq.io_Data = data;

	ioerr = sbp2_iocmd_read64(unit, &ioreq);

	if (ioerr || (ioreq.io_Actual != unit->u_BlockSize))
	{
		_ERR("Failed to mount CD, ioerr=%ld, actual=%lu\n", ioerr, ioreq.io_Actual);
		goto out;
	}

	_INFO("CD data: [0]=$%08x, [1]=$%08x\n", data[0], data[1]);
	if ((LE_SWAPLONG_C(0x01434430) == data[0])
		&& (LE_SWAPLONG_C(0x30310100) == data[1]))
	{
		_INFO("CD-ROM recognized, mount it (NotifyUnit=%p)\n", unit->u_NotifyUnit);
		sbp2_automount(unit);
	}

out:
	FreeVecDMA(data);
}
//-
//+ sbp2_on_medium_changed
static void sbp2_on_medium_changed(SBP2Unit *unit)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;
	BOOL ready;

	LOCK_REGION(unit);
	unit->u_ChangeCounter++;
	ready = unit->u_Flags.Ready;
	UNLOCK_REGION(unit);

	if (ready)
	{
		struct IOStdReq ioreq;
		LONG ioerr;

		_INFO("Device %llx, LUN %u: medium inserted\n", unit->u_GUID, unit->u_LUN);

		/* Send a SCSI start command */
		ioreq.io_Command = CMD_START;
		ioerr = sbp2_iocmd_start_stop(unit, &ioreq);
		if (ioerr)
			return;

		unit->u_BlockSize = 0;
		sbp2_get_capacity(unit);

		/* Automount CD-ROM/WriteOnce units */
		if ((2048 == unit->u_BlockSize)
			&& ((PDT_CDROM == unit->u_DeviceType) || (PDT_WORM == unit->u_DeviceType)))
		{
			if (unit->u_Flags.AutoMountCD)
				sbp2_mount_cd(unit);
		}
	}
	else
	{
		_INFO("Device %llx, LUN %u: medium removed\n", unit->u_GUID, unit->u_LUN);
		sbp2_unmount_all(unit);
	}
}
//-
//+ sbp2_flush_io
static void sbp2_flush_io(SBP2Unit *unit)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;
	struct IOStdReq *ioreq;

	/* Block io receiption */
	LOCK_REGION(unit);
	{
		ULONG flushed = 0;
		while (NULL != (ioreq = (struct IOStdReq *)GetMsg(&unit->u_SysUnit.unit_MsgPort)))
		{
			_WARN("Flushing ioreq %p\n", ioreq);
			ioreq->io_Error = IOERR_ABORTED;
			ioreq->io_Actual = 0;
			ReplyMsg(&ioreq->io_Message);
			flushed++;
		}
	}
	UNLOCK_REGION(unit);
}
//-
//+ sbp2_handle_ioreq
static void sbp2_handle_ioreq(SBP2Unit *unit, struct IOStdReq *ioreq)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;

	switch (ioreq->io_Command)
	{
		case HD_SCSICMD:
			sbp2_iocmd_scsi(unit, ioreq);
			break;

		case TD_GETGEOMETRY:
			sbp2_iocmd_get_geometry(unit, ioreq);
			break;

		case CMD_START:
		case CMD_STOP:
		case TD_EJECT:
			sbp2_iocmd_start_stop(unit, ioreq);
			break;

		case CMD_READ:
			ioreq->io_Actual = 0;
		case TD_READ64:
			sbp2_iocmd_read64(unit, ioreq);
			break;

		case CMD_WRITE:
		case TD_FORMAT:
			ioreq->io_Actual = 0;
		case TD_WRITE64:
		case TD_FORMAT64:
			sbp2_iocmd_write64(unit, ioreq);
			break;

		default:
			_ERR("IO: not handled cmd %u\n", ioreq->io_Command);
			ioreq->io_Error = IOERR_NOCMD;
			ioreq->io_Actual = 0; /* to make happy some programs like HDConfig */
			break;
	}

	if (ioreq->io_Error && (IOERR_NOCMD != ioreq->io_Error))
		_ERR("IO: command %lu failed, io_Error=%ld, io_Actual=%lu\n",
			 ioreq->io_Command, ioreq->io_Error, ioreq->io_Actual);

	ReplyMsg(&ioreq->io_Message);
}
//-
//+ sbp2_handle_removable
static void sbp2_handle_removable(SBP2Unit *unit)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;
	UBYTE sensedata[18];
	LONG ioerr;
	BOOL ready_changed=FALSE;

	bzero(sensedata, sizeof(sensedata));

	ioerr = sbp2_scsi_test_unit_ready(unit, sensedata);
	if (ioerr)
	{
		if (HFERR_BadStatus == ioerr)
		{
			UBYTE key = sensedata[2] & SK_MASK;
			UBYTE asc = sensedata[12];

			/* Remove Ready flags on "LOGICAL UNIT NOT READY" or "MEDIUM NOT PRESENT" */
			if ((SK_NOT_READY == key) && ((0x04 == asc) || (0x3a == asc)))
			{
				if (unit->u_Flags.Ready)
				{
					LOCK_REGION(unit);
					unit->u_Flags.Ready = 0;
					UNLOCK_REGION(unit);

					ready_changed = TRUE;
				}
			}
		}
		else
			_ERR("SCSI: Test unit ready cmd failed, ioerr=%d\n", ioerr);
	}
	else if (!unit->u_Flags.Ready)
	{
		_INFO("Device %llx, LUN %u is ready\n", unit->u_GUID, unit->u_LUN);

		LOCK_REGION(unit);
		unit->u_Flags.Ready = 1;
		UNLOCK_REGION(unit);

		ready_changed = TRUE;
	}

	if (ready_changed)
		sbp2_on_medium_changed(unit);
}
static void sbp2_driver_task(SBP2ClassLib *base, SBP2Unit *unit)
{
	HeliosDevice *dev = unit->u_HeliosDevice;
	ULONG sigs, gen=0, ioreq_signal, timer_signal;
	BOOL run=TRUE;
	HeliosEventListenerList *dev_ell=NULL, *hw_ell=NULL;
	HeliosEventMsg sbp2_dev_listener;
	HeliosEventMsg sbp2_hw_listener;
	IOHeliosHWReq h_ioreq;
	struct MsgPort *evt_port=NULL;
	struct timerequest *removable_tr=NULL;
	struct timerequest *reconnect_tr=NULL;
	HeliosEventMsg *evt;
	BYTE io_sigbit=-1;
	STRPTR task_name;

	INIT_LOCKED_LIST(unit->u_PendingORBs);
	
	unit->u_SysUnit.unit_OpenCnt = 0;
	unit->u_SysUnit.unit_MsgPort.mp_Flags = PA_IGNORE;
	unit->u_SysUnit.unit_MsgPort.mp_SigTask = NULL;

	bzero(&sbp2_dev_listener, sizeof(sbp2_dev_listener));
	bzero(&sbp2_hw_listener, sizeof(sbp2_hw_listener));

	/* Set unit defaults.
	 * All this values maybe used by the device interface, so must be valid
	 * before adding the unit to the device's units list.
	 */
	unit->u_UnitNo = 0;
	unit->u_ChangeCounter = 1;
	unit->u_Flags.WriteProtected = 1; /* TODO: Forced */
	unit->u_Flags.AutoMountCD = 1;

	{
		ULONG value;
		char buf[60];

		unit->u_AutoReconnect = 0;
		
		if ((GetVar("helios_autoreconnect", buf, sizeof(buf), 0) > 0) &&
			(StrToLong(buf, &value) > 0))
			unit->u_AutoReconnect = value;

		_INFO("Auto-Reconnect=%lu\n", unit->u_AutoReconnect);
	}

	/* For the mount.library */
	unit->u_NotifyUnit = MountCreateNotifyUnitTags(MOUNTATTR_DEVICE, (ULONG)base->hc_DevBase,
												   MOUNTATTR_UNIT, (ULONG)&unit->u_SysUnit,
												   TAG_DONE);
	if (NULL == unit->u_NotifyUnit)
	{
		_ERR("MountCreateNotifyUnitTags() failed\n");
		goto release_unit;
	}

	/* We search for a free sbp2-unit number and add it to the available list */
	LOCK_REGION(base);
	{
		SBP2Unit *node;

		ForeachNode(&base->hc_Units, node)
		{
			/* Free and not devOpen unit ? */
			if (node->u_UnitNo == unit->u_UnitNo)
			{
				/* Restart the loop from head with the next number */
				unit->u_UnitNo++;
				node = (SBP2Unit *)base->hc_Units.mlh_Head;
			}
		}

		if (unit->u_UnitNo > base->hc_MaxUnitNo)
			base->hc_MaxUnitNo = unit->u_UnitNo;

		ADDTAIL(&base->hc_Units, unit);
		unit->u_Flags.Public = TRUE;

		Helios_ReportMsg(HRMB_INFO, "SBP2", "SBP2 unit #%u bind on device $%llx",
						 unit->u_UnitNo, unit->u_GUID);
	}
	UNLOCK_REGION(base);

	/* Msg ports and signals */
	evt_port = CreateMsgPort();
	unit->u_TimerPort = CreateMsgPort();
	unit->u_OrbPort = CreateMsgPort();
	unit->u_ORBStatusSigBit = AllocSignal(-1);
	io_sigbit = AllocSignal(-1);

	if ((NULL == evt_port) ||
		(NULL == unit->u_TimerPort) ||
		(NULL == unit->u_OrbPort) ||
		(-1 == unit->u_ORBStatusSigBit) ||
		(-1 == io_sigbit))
		goto release_unit;

	unit->u_IOTimeReq = Helios_OpenTimer(unit->u_TimerPort, UNIT_VBLANK);
	if (NULL == unit->u_IOTimeReq)
		goto release_unit;

	_INFO("Task %s ressources OK, get info now\n", FindTask(NULL)->tc_Node.ln_Name);

	/* Obtain helios device/unit data */
	dev_ell = NULL;
	Helios_GetAttrs(HGA_DEVICE, dev,
					HA_Generation, (ULONG)&gen,
					HA_Hardware, (ULONG)&unit->u_HeliosHW, /* NR */
					HA_EventListenerList, (ULONG)&dev_ell,
					TAG_DONE);
	
	/* Something wrong with the device ? */
	if ((gen != unit->u_Generation) || (NULL == unit->u_HeliosHW) || (NULL == dev_ell))
		goto release_unit;

	hw_ell = NULL;
	Helios_GetAttrs(HGA_HARDWARE, unit->u_HeliosHW,
					HA_EventListenerList, (ULONG)&hw_ell,
					TAG_DONE);
	if (NULL == hw_ell)
	{
		_ERR("SBP2 unit %p, failed to obtain HW data\n", unit);
		goto release_unit;
	}

	/* Parse the unit ROM directory block.
	 * So we lock the device to keep a ROM pointer valid
	 * during this time.
	 */
	Helios_ReadLockDevice(dev);
	{
		QUADLET *dir=NULL;

		Helios_GetAttrs(HGA_UNIT, unit->u_HeliosUnit,
						HA_UnitRomDirectory, (ULONG)&dir,
						TAG_DONE);
		if (NULL != dir)
			sbp2_getrominfo(unit, dir);
		else
		{
			Helios_UnlockDevice(dev);
			goto release_unit;
		}
	}
	Helios_UnlockDevice(dev);

	/* Register SBP2 status request handler and some events */
	unit->u_FSReqHandler.rh_RegionStart = HELIOS_HIGHMEM_START;
	unit->u_FSReqHandler.rh_RegionStop = HELIOS_HIGHMEM_STOP;
	unit->u_FSReqHandler.rh_Length = 0x100;
	unit->u_FSReqHandler.rh_Flags = HHF_REQH_ALLOCLEN;
	unit->u_FSReqHandler.rh_ReqCallback = sbp2_fs_reqhandler;
	unit->u_FSReqHandler.rh_UserData = unit;

	h_ioreq.iohh_Req.io_Message.mn_Length = sizeof(h_ioreq);
	h_ioreq.iohh_Req.io_Command = HHIOCMD_ADDREQHANDLER;
	h_ioreq.iohh_Data = &unit->u_FSReqHandler;

	if (Helios_DoIO(HGA_DEVICE, dev, &h_ioreq))
	{
		_ERR("Can't register status req handler on device %p\n", dev);
		goto release_unit;
	}

	/* Registers hardware events */
	sbp2_hw_listener.hm_Msg.mn_ReplyPort = evt_port;
	sbp2_hw_listener.hm_Msg.mn_Length = sizeof(sbp2_hw_listener);
	sbp2_hw_listener.hm_Type = HELIOS_MSGTYPE_EVENT;
	sbp2_hw_listener.hm_EventMask = HEVTF_HARDWARE_BUSRESET;
	Helios_AddEventListener(hw_ell, &sbp2_hw_listener);

	/* Registers device events */
	sbp2_dev_listener.hm_Msg.mn_ReplyPort = evt_port;
	sbp2_dev_listener.hm_Msg.mn_Length = sizeof(sbp2_dev_listener);
	sbp2_dev_listener.hm_Type = HELIOS_MSGTYPE_EVENT;
	sbp2_dev_listener.hm_EventMask = HEVTF_DEVICE_NEW_UNIT
		| HEVTF_DEVICE_DEAD
		| HEVTF_DEVICE_REMOVED
		| HEVTF_DEVICE_UPDATED;
	Helios_AddEventListener(dev_ell, &sbp2_dev_listener);

	/* Prepare IO port to accept requests from the device interface */
	LOCK_REGION(unit);
	{
		unit->u_SysUnit.unit_MsgPort.mp_SigBit = io_sigbit;
		unit->u_SysUnit.unit_MsgPort.mp_SigTask = FindTask(NULL);
		unit->u_SysUnit.unit_MsgPort.mp_Node.ln_Type = NT_MSGPORT;
		unit->u_SysUnit.unit_MsgPort.mp_Flags = PA_SIGNAL;
	}
	UNLOCK_REGION(unit);

	ioreq_signal = 1ul << io_sigbit;
	timer_signal = 1ul << unit->u_TimerPort->mp_SigBit;

	/* Login and target initialization */
	if (!sbp2_update(unit))
		goto release_unit;

	/* Main loop */
	while (run)
	{
		/* Add new timeout request for the removable periodic task */
		if (unit->u_Flags.Removable && (NULL == removable_tr))
			removable_tr = sbp2_add_timereq(unit, REMOVABLE_CHECK_TIMEOUT);

		if ((unit->u_AutoReconnect > 0) && unit->u_Flags.Logged && (NULL == reconnect_tr))
			reconnect_tr = sbp2_add_timereq(unit, unit->u_AutoReconnect);

		sigs = Wait(SIGBREAKF_CTRL_C | (1ul << evt_port->mp_SigBit) | timer_signal | ioreq_signal);

		/* exit? */
		if (sigs & SIGBREAKF_CTRL_C)
			run = FALSE;

		/* Handle Helios events first */
		if (sigs & (1ul << evt_port->mp_SigBit))
		{
			BOOL update = FALSE;

			while (NULL != (evt = (HeliosEventMsg *)GetMsg(evt_port)))
			{
				switch (evt->hm_Type)
				{
					case HELIOS_MSGTYPE_EVENT:
						if (run)
						{
							switch (evt->hm_EventMask)
							{
								case HEVTF_HARDWARE_BUSRESET:
									LOCK_REGION(unit);
									{
										_INFO("Bus reset detected, set ready to 0\n");
										unit->u_Flags.Ready = 0;
									}
									UNLOCK_REGION(unit);
									break;

								case HEVTF_DEVICE_DEAD:
								case HEVTF_DEVICE_REMOVED:
									LOCK_REGION(unit);
									{
										unit->u_Flags.Logged = 0;
										unit->u_Flags.Ready = 0;
										unit->u_Flags.AcceptIO = 0;
									}
									UNLOCK_REGION(unit);
									run = FALSE;
									break;

								case HEVTF_DEVICE_UPDATED:
									update = TRUE;
									break;
							}
							break;
						}

						FreeMem(evt, evt->hm_Msg.mn_Length);
						break;

					default:
						ReplyMsg((APTR)evt);
				}
			}

			if (run & update)
				sbp2_update(unit);
		}
		
		/* Device IO requests */
		if (run && ((sigs & ioreq_signal) || (unit->u_Flags.ProcessIO)))
		{
			struct IOStdReq *ioreq;

			/* do not process io until reconnect */
			if (unit->u_Flags.AcceptIO)
			{
				unit->u_Flags.ProcessIO = 0;
				while (NULL != (ioreq = (struct IOStdReq *)GetMsg(&unit->u_SysUnit.unit_MsgPort)))
				{
					sbp2_handle_ioreq(unit, ioreq);
					if (!unit->u_Flags.AcceptIO)
					{
						unit->u_Flags.ProcessIO = 1;
						break;
					}
				}
			}
			else
				unit->u_Flags.ProcessIO = 1;
		}

		/* Periodic jobs */
		if (sigs & timer_signal)
		{
			struct timerequest *tr;

			while (NULL != (tr = (APTR)GetMsg(unit->u_TimerPort)))
			{
				if (tr == removable_tr)
				{
					if (run)
						sbp2_handle_removable(unit);
					removable_tr = NULL;
				}
				else if (tr == reconnect_tr)
				{
					if (run && !sbp2_reconnect(unit))
						run = FALSE;
					reconnect_tr = NULL;
				}

				sbp2_free_timereq(unit, tr);
			}
		}
	}

release_unit:
	_INFO("Releasing sbp2 unit %p (H-Unit=%p)\n", unit, unit->u_HeliosUnit);

	/* Forbid new IO requests */
	LOCK_REGION(unit);
	{
		unit->u_Flags.AcceptIO = 0;
		unit->u_Flags.Ready = 0;
		unit->u_SysUnit.unit_MsgPort.mp_Flags = PA_IGNORE;
		unit->u_SysUnit.unit_MsgPort.mp_SigTask = NULL;
	}
	UNLOCK_REGION(unit);

	/* Remove all mount points and notifications */
	sbp2_unmount_all(unit);

	/* Flush and abort pending device IO requests */
	sbp2_flush_io(unit);

	LOCK_REGION(unit);
	{
		/* see the note in sbp2_decref() */
		if (unit->u_Flags.Public)
		{
			REMOVE(unit);
			unit->u_Flags.Public = FALSE;
		}
	}
	UNLOCK_REGION(unit);

	/* Logout (fails in case of bad generation) */
	if (unit->u_Flags.Logged)
		sbp2_logout(unit);

	/* Cancel pending ORBs */
	sbp2_cancel_orbs(unit);

	/* Unregister the status request handler */
	if (0 != unit->u_FSReqHandler.rh_Start)
	{
		bzero(&h_ioreq, sizeof(struct IORequest));
		h_ioreq.iohh_Req.io_Message.mn_Length = sizeof(h_ioreq);
		h_ioreq.iohh_Req.io_Command = HHIOCMD_REMREQHANDLER;
		h_ioreq.iohh_Data = &unit->u_FSReqHandler;
		Helios_DoIO(HGA_DEVICE, dev, &h_ioreq);
	}

	/* Unregister all Helios events */
	if (0 != sbp2_dev_listener.hm_EventMask)
		Helios_RemoveEventListener(dev_ell, &sbp2_dev_listener);

	if (0 != sbp2_hw_listener.hm_EventMask)
		Helios_RemoveEventListener(hw_ell, &sbp2_hw_listener);

	/* Flush helios event ports */
	while (NULL != (evt = (HeliosEventMsg *)GetMsg(evt_port)))
	{
		switch (evt->hm_Type)
		{
			case HELIOS_MSGTYPE_EVENT:
				FreeMem(evt, evt->hm_Msg.mn_Length);
				break;
		}
	}

	/* Delete all links with Helios stuffs (except the base) */
	Helios_UnbindUnit(unit->u_HeliosUnit); /* No-op if already done */
	Helios_ReleaseUnit(unit->u_HeliosUnit);
	Helios_ReleaseDevice(unit->u_HeliosDevice);
	Helios_ReleaseHardware(unit->u_HeliosHW);

	unit->u_HeliosUnit = NULL;
	unit->u_HeliosDevice = NULL;
	unit->u_HeliosHW = NULL;

	/* Abort periodic timers */
	if ((NULL != removable_tr) && !CheckIO(&removable_tr->tr_node))
		AbortIO(&removable_tr->tr_node);

	if ((NULL != reconnect_tr) && !CheckIO(&reconnect_tr->tr_node))
		AbortIO(&reconnect_tr->tr_node);

	/* Proper timer device flush */
	while ((NULL != removable_tr) || (NULL != reconnect_tr))
	{
		struct timerequest *tr;

		while (NULL != (tr = (APTR)GetMsg(unit->u_TimerPort)))
		{
			if (tr == removable_tr)
				removable_tr = NULL;

			if (tr == reconnect_tr)
				reconnect_tr = NULL;

			sbp2_free_timereq(unit, tr);
		}
	}

	/* Free system ressources only used here.
	 * Shared resources that could be used by the device interface
	 * remain valid until no unit open cnt reach 0.
	 */
	if (NULL != unit->u_IOTimeReq)
		Helios_CloseTimer(unit->u_IOTimeReq);
	if (NULL != unit->u_TimerPort)
		DeleteMsgPort(unit->u_TimerPort);
	if (NULL != unit->u_OrbPort)
		DeleteMsgPort(unit->u_OrbPort);
	if (NULL != evt_port)
		DeleteMsgPort(evt_port);
	if (io_sigbit >= 0)
		FreeSignal(io_sigbit);
	if (unit->u_ORBStatusSigBit>= 0)
		FreeSignal(unit->u_ORBStatusSigBit);
	if (NULL != unit->u_OneBlock)
		FreePooled(base->hc_MemPool, unit->u_OneBlock, unit->u_OneBlockSize);

	task_name = unit->u_TaskName;

	LOCK_REGION(base);
	sbp2_decref(unit);
	UNLOCK_REGION(base);

	/* Now 'unit' is considered as invalid pointer and shall not be used */

	_INFO("%s says bye (sbp2 unit was %p)\n", FindTask(NULL)->tc_Node.ln_Name, unit);

	if (NULL != task_name)
	{
		static UBYTE const default_task_name[] = "<sbp2 driver task (killed)>";

		NewSetTaskAttrsA(NULL, (APTR)&default_task_name, sizeof(STRPTR), TASKINFOTYPE_NAME, NULL);
		FreeVecPooled(base->hc_MemPool, task_name);
	}

	/* TODO: BAD CODE, REMOVE ME SOON */
	Forbid();
	--base->hc_Lib.lib_OpenCnt;
	Permit();
}
#endif
static void
sbp2_read_ascii_field(STRPTR in, STRPTR out, ULONG len)
{
	ULONG i;

	for (i=0; i < (len-1); i++)
	{
		UBYTE c = in[i];

		if (c > 0x20)
			out[i] = c;
		else
		{
			out[i] = 0;
			break;
		}
	}
	out[len-1] = '\0';
}
static LONG
sbp2_do_read_quadlet(SBP2Unit *unit,
	HeliosOffset offset,
	HeliosSpeed speed,
	QUADLET *buf)
{
	LONG err;
	struct IOStdReq ioreq;
	HeliosPacket _p, *p = &_p;

	bzero(&ioreq, sizeof(ioreq));
	ioreq.io_Message.mn_Node.ln_Type = NT_MESSAGE;
	ioreq.io_Message.mn_Length = sizeof(ioreq);
	ioreq.io_Command = HHIOCMD_SENDREQUEST;
	ioreq.io_Data = p;

	p->DestID = unit->u_NodeID;
	p->Generation = unit->u_Generation;

	Helios_FillReadQuadletPacket(p, speed, offset);
	err = Helios_DoIO(unit->u_HeliosHardware, (struct IORequest *)&ioreq);
	if (!err)
		*buf = p->QuadletData;
	return err;
}
static LONG
sbp2_do_write_quadlet(SBP2Unit *unit,
	HeliosOffset offset,
	HeliosSpeed speed,
	QUADLET value)
{
	struct IOStdReq ioreq;
	HeliosPacket _p, *p = &_p;

	ZERO(ioreq);
	ioreq.io_Message.mn_Node.ln_Type = NT_MESSAGE;
	ioreq.io_Message.mn_Length = sizeof(ioreq);
	ioreq.io_Command = HHIOCMD_SENDREQUEST;
	ioreq.io_Data = p;

	p->DestID = unit->u_NodeID;
	p->Generation = unit->u_Generation;

	Helios_FillWriteQuadletPacket(p, speed, offset, value);
	return Helios_DoIO(unit->u_HeliosHardware, (struct IORequest *)&ioreq);
}
static void
sbp2_send_write_block(SBP2Unit *unit,
	struct IOStdReq *ioreq,
	HeliosPacket *p,
	APTR data,
	ULONG length,
	HeliosOffset offset,
	HeliosSpeed speed)
{
	ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
	ioreq->io_Command = HHIOCMD_SENDREQUEST;
	ioreq->io_Data = p;

	Helios_FillWriteBlockPacket(p, speed, offset, data, length);
	Helios_SendIO(unit->u_HeliosHardware, (struct IORequest *)ioreq);
}
static void
sbp2_send_orb_req(SBP2Unit *unit,
	SBP2ORBRequest *orbreq,
	APTR orb,
	HeliosOffset offset,
	ORBDoneCallback orbdone,
	ULONG status_signal)
{
	orbreq->or_ORBAddr.addr.hi = 0;
	orbreq->or_ORBAddr.addr.lo = LE_SWAPLONG(utils_GetPhyAddress(orb));
	orbreq->or_ORBDone = orbdone;
	orbreq->or_ORBStTask = FindTask(NULL);
	orbreq->or_ORBStSignal = status_signal;
	orbreq->or_Packet.DestID = unit->u_NodeID;
	orbreq->or_Packet.Generation = unit->u_Generation;

	SetSignal(0, status_signal); /* as can be set after a orb cancel if re-used */

	WRITE_LOCK_LIST(unit->u_PendingORBs);
	{
		_DBG_UNIT(unit, "sending ORB request @ %p\n", orbreq);
		ADDTAIL(&unit->u_PendingORBs, &orbreq->or_Node);

		/* In the unit locked region because the ORB can be cancelled */
		sbp2_send_write_block(unit,
			&orbreq->or_Base, &orbreq->or_Packet,
			&orbreq->or_ORBAddr, 8,
			offset, unit->u_MaxSpeed);
	}
	UNLOCK_LIST(unit->u_PendingORBs);
}
static void
sbp2_config_logical_unit(SBP2Unit *unit, QUADLET value)
{
	unit->u_Flags.Ordered = 1 & (value >> 14);
	unit->u_DeviceType = 0x1f & (value >> 16);
	unit->u_LUN = value & 0xffff;
	_INF_UNIT(unit, "=> Unit characteristics : Ordered=%s, DeviceType=$%02X, LUN=%u\n",
		YESNO(unit->u_Flags.Ordered), unit->u_DeviceType, unit->u_LUN);
}
static void
sbp2_scan_logical_unit_directory(SBP2Unit *unit, const QUADLET *dir)
{
	HeliosRomIterator ri;
	QUADLET key, value;

	Helios_InitRomIterator(&ri, dir);
	while (Helios_RomIterate(&ri, &key, &value))
	{
		if (KEY_SBP2_LOGICAL_UNIT_NUMBER == key)
			sbp2_config_logical_unit(unit, value);
	}
}
static LONG
sbp2_wait(SBP2Unit *unit, ULONG signal, ULONG timeout)
{
	LONG err=0;
	struct MsgPort *port;
	struct timerequest tr;
	ULONG sigs, timer_signal=0;

	port = CreateMsgPort();
	if (port)
	{
		timer_signal = 1ul << port->mp_SigBit;

		CopyMem(unit->u_IOTimeReq, &tr, sizeof(tr));
		tr.tr_node.io_Message.mn_ReplyPort = port;
		tr.tr_time.tv_secs = timeout / 1000;
		tr.tr_time.tv_micro = (timeout % 1000) * 1000;

		SendIO(&tr.tr_node);
	} /* else no timeout possible ... */

	sigs = Wait(signal | timer_signal);
	if (sigs & timer_signal)
	{
		_ERR_UNIT(unit, "timeout\n");
		err = HERR_TIMEOUT;
	}

	if (port)
	{
		if (!CheckIO(&tr.tr_node))
		{
			AbortIO(&tr.tr_node);
			WaitIO(&tr.tr_node);
		}

		while (GetMsg(port));
		DeleteMsgPort(port);
	}

	return err;
}
static QUADLET
sbp2_get_agent_state(SBP2Unit *unit)
{
	QUADLET state = SB2_STATE_DEAD; /* safe state if read failed */
	sbp2_do_read_quadlet(unit,
		unit->u_ORBLoginResponse.command_agent + SBP2_AGENT_STATE,
		S100, &state);
	return state;
}
static void
sbp2_agent_reset(SBP2Unit *unit)
{
	_DBG_UNIT(unit, "request agent reset...\n");
	sbp2_do_write_quadlet(unit,
		unit->u_ORBLoginResponse.command_agent + SBP2_AGENT_RESET,
		S100, 0);
}
static BOOL
sbp2_inquiry(SBP2Unit *unit)
{
	UBYTE cmd6[6];
	struct SCSICmd scsicmd;
	ULONG inquiry_len;
	UBYTE *inquirydata;
	UBYTE sensedata[18];
	LONG err=0, pass, i;
	UBYTE device_type;
	BOOL ret=FALSE;

	inquirydata = AllocVecDMA(256, MEMF_PUBLIC);
	if (NULL == inquirydata)
	{
		_ERR_UNIT(unit, "Failed to alloc 256 bytes for INQUIRY data\n");
		return FALSE;
	}

	scsicmd.scsi_Length = 255;
	scsicmd.scsi_Command = cmd6;
	scsicmd.scsi_CmdLength = 6;
	scsicmd.scsi_Flags = SCSIF_READ|SCSIF_AUTOSENSE;
	scsicmd.scsi_SenseData = sensedata;
	scsicmd.scsi_SenseLength = 18;

	pass = 1;
	inquiry_len = 36;

try_inquiry:
	scsicmd.scsi_Data = (UWORD *) inquirydata;
	bzero(inquirydata, 256);
	bzero(sensedata, sizeof(sensedata));
	bzero(cmd6, 6);
	
	cmd6[0] = SCSI_INQUIRY;
	cmd6[4] = inquiry_len;

	/* Attempt 3 times to ignore UNIT_ATTENTION */
	for (i=0; i<3; i++)
	{
		_INF_UNIT(unit, "do SCSI_INQUIRY, pass %u/3, data=%p\n", pass, inquirydata);
		err = sbp2_do_scsi_cmd(unit, &scsicmd, ORB_TIMEOUT);

		if ((HFERR_BadStatus == err) &&
			(SK_UNIT_ATTENTION == (scsicmd.scsi_SenseData[2] & SK_MASK)) &&
			((0x28 == scsicmd.scsi_SenseData[12]) ||
			 (0x29 == scsicmd.scsi_SenseData[12])) &&
			(0 == scsicmd.scsi_SenseData[13]))
			continue;

		break;
	}
	
	if (!err)
	{
		/* Check if we can get more data */
		if ((1 == pass) && (inquirydata[4] > (inquiry_len-4)) &&
			(!unit->u_Flags.Inquiry36))
		{
			inquiry_len = inquirydata[4]+4;
			pass = 2;
			goto try_inquiry;
		}
	}
	else if (2 == pass)
	{
		_ERR_UNIT(unit, "INQUIRY: failed with len=%u, retry 36 bytes\n");
		inquiry_len = 36;
		unit->u_Flags.Inquiry36 = 1;
		pass = 3;
		goto try_inquiry;
	}
	else
		goto out;

	log_DumpMem(inquirydata, inquirydata[4]+4, FALSE, "INQUIRY Data:\n");
	_DBG_UNIT(unit, "INQUIRY: done, %lu bytes\n", inquirydata[4]+4);

	/* Read ASCII VendorID, ProductID and ProductVersion fields  */
	sbp2_read_ascii_field(&inquirydata[8], unit->u_VendorID, SBP2_VENDORID_LEN+1);
	sbp2_read_ascii_field(&inquirydata[16], unit->u_ProductID, SBP2_PRODUCTID_LEN+1);
	sbp2_read_ascii_field(&inquirydata[32], unit->u_ProductVersion, SBP2_PRODUCTVERSION_LEN+1);

	_INF_UNIT(unit, "Unit #%u, Vendor: %s, Product: %s, Version: %s\n",
		unit->u_UnitNo, unit->u_VendorID, unit->u_ProductID, unit->u_ProductVersion);

	/* Check for device type if not known */
	device_type = inquirydata[0] & PDT_MASK;
	if ((unit->u_DeviceType >= 0x12) || (device_type >= 0x12))
	{
		_ERR_UNIT(unit, "Unknown device type ($%02x)\n", device_type);
		goto out;
	}
	else
		_INF_UNIT(unit, "INQUIRY: device type = $%02x\n", device_type);

	if ((unit->u_DeviceType != 0xff) && (unit->u_DeviceType != device_type))
	{
		_WRN_UNIT(unit, "ROM device type differents of one in INQUIRY data (%u != %u)\n",
			unit->u_DeviceType, device_type);
		unit->u_DeviceType = device_type;
	}

	/* Per device type fine tunning */
	switch (unit->u_DeviceType)
	{
		case PDT_DIRECT_ACCESS:
		case PDT_SIMPLE_DIRECT_ACCESS:
			unit->u_Flags.WriteProtected = 0; /* TODO */
			break;
	}

	/* Version shall be SPC */
	if (inquirydata[2] != SPC2_VERSION)
		_WRN_UNIT(unit, "SPC2 version is $%02x (should be $%02x)\n", inquirydata[2], SPC2_VERSION);

	unit->u_Flags.Removable = 0 != (inquirydata[1] & 0x80);
	_INF_UNIT(unit, "INQUIRY: removable support: %s\n", YESNO(unit->u_Flags.Removable));

	ret = TRUE;

out:
	FreeVecDMA(inquirydata);
	return ret;
}
static void
sbp2_complete_managment_orb(SBP2Unit *unit, SBP2ORBRequest *orbreq, ORBStatus *status)
{
	if (status)
		CopyMem(status, &orbreq->or_ORBStatus, sizeof(orbreq->or_ORBStatus));
}
static LONG
sbp2_do_management_orb(SBP2Unit *unit, APTR orb)
{
	SBP2ORBRequest orbreq;
	LONG err;
	ULONG sigs, status_signal = 1ul << unit->u_ORBStatusSigBit;
	ULONG orb_sig = 1ul << unit->u_OrbPort->mp_SigBit;

	bzero(&orbreq, sizeof(orbreq));
	orbreq.or_Base.io_Message.mn_Length = sizeof(orbreq);
	orbreq.or_Base.io_Message.mn_ReplyPort = unit->u_OrbPort;
	
	/* Send the ORB request */
	sbp2_send_orb_req(unit, &orbreq, orb, unit->u_MgtAgentBase,
		sbp2_complete_managment_orb, status_signal);

	/* Wait for 1394 transport */
	sigs = Wait(SIGBREAKF_CTRL_C | orb_sig);
	if (sigs & orb_sig)
	{
		GetMsg(unit->u_OrbPort);

		if ((HHIOERR_NO_ERROR != orbreq.or_Base.io_Error) ||
			(HELIOS_RCODE_COMPLETE != orbreq.or_Packet.RCode))
		{
			_ERR_ORB(unit, orb, "transport error, ioerr=%ld, rcode=%ld\n",
				orbreq.or_Base.io_Error, orbreq.or_Packet.RCode);

			err = HERR_IO;
			goto out;
		}

		/* The Mgt_ORB_timeout starts from now */
		err = sbp2_wait(unit, status_signal, unit->u_MgtTimeout);
		if (HERR_TIMEOUT == err)
		{
			_ERR_ORB(unit, orb, "status timeout\n");
			goto out;
		}

		if (STATUS_GET_RESPONSE(orbreq.or_ORBStatus) ||
			STATUS_GET_SBP_STATUS(orbreq.or_ORBStatus))
		{
			_ERR_ORB(unit, orb, "status error: <%u:%u>\n",
				 STATUS_GET_RESPONSE(orbreq.or_ORBStatus),
				 STATUS_GET_SBP_STATUS(orbreq.or_ORBStatus));
			err = HERR_IO;
		}
	}
	else
		err = HERR_IO; /* Wait() interrupted */

out:
	sbp2_cancel_orbs(unit);
	return err;
}
static void
sbp2_automount_task(SBP2Unit *unit)
{
	MountMountTags(unit->u_NotifyUnit, TAG_END);
}
static void
sbp2_automount(SBP2Unit *unit)
{
	struct Process *proc;

	/* Run an auto-mount task */
	proc = CreateNewProcTags(NP_CodeType,     CODETYPE_PPC,
							 NP_Name,         (ULONG)"<Helios AutoMount>",
							 NP_Priority,     0,
							 NP_Entry,        (ULONG)sbp2_automount_task,
							 NP_PPC_Arg1,     (ULONG)unit,
							 TAG_DONE);
	if (!proc)
		_ERR_UNIT(unit, "Auto-mount task creation failed\n");
}
static LONG
sbp2_do_start_unit(SBP2Unit *unit)
{
	struct IOStdReq ioreq;

	ioreq.io_Command = CMD_START;
	return sbp2_iocmd_start_stop(unit, &ioreq);
}
static void
sbp2_get_capacity(SBP2Unit *unit)
{
	UBYTE cmd10[10];
	struct SCSICmd scsicmd;
	ULONG capacity[2];
	UBYTE sensedata[18];
	LONG err;
	ULONG block_len;

	bzero(capacity, sizeof(capacity));
	bzero(sensedata, sizeof(sensedata));

	scsicmd.scsi_Data = (UWORD *) capacity;
	scsicmd.scsi_Length = sizeof(capacity);
	scsicmd.scsi_Command = cmd10;
	scsicmd.scsi_CmdLength = 10;
	scsicmd.scsi_Flags = SCSIF_READ|SCSIF_AUTOSENSE;
	scsicmd.scsi_SenseData = sensedata;
	scsicmd.scsi_SenseLength = 18;
	cmd10[0] = SCSI_DA_READ_CAPACITY;
	cmd10[1] = 0;
	cmd10[2] = 0;
	cmd10[3] = 0;
	cmd10[4] = 0;
	cmd10[5] = 0;
	cmd10[6] = 0;
	cmd10[7] = 0;
	cmd10[8] = 0;
	cmd10[9] = 0;

	/* Set default values */
	if(!unit->u_BlockSize)
	{
		if ((PDT_CDROM == unit->u_DeviceType) || (PDT_WORM == unit->u_DeviceType))
		{
			unit->u_Geometry.dg_SectorSize = unit->u_BlockSize = 2048;
			unit->u_BlockShift = 11;
		}
		else
		{
			unit->u_Geometry.dg_SectorSize = unit->u_BlockSize = 512;
			unit->u_BlockShift = 9;
		}
	}

	_INF_UNIT(unit, "Send SCSI_DA_READ_CAPACITY\n");
	err = sbp2_do_scsi_cmd(unit, &scsicmd, ORB_TIMEOUT);
	if (err)
	{
		_ERR_UNIT(unit, "READ_CAPACITY: failed, err=%ld\n", err);
		return;
	}

	block_len = LE_SWAPLONG(capacity[0]);
	if (block_len == 0xffffffff)
	{
		_ERR_UNIT(unit, "READ_CAPACITY: Block length too big, need 16bytes CDB unsupported by SBP2!\n");
		return;
	}

	unit->u_Geometry.dg_TotalSectors = block_len+1;
	unit->u_Geometry.dg_SectorSize = unit->u_BlockSize = LE_SWAPLONG(capacity[1]);
	unit->u_BlockShift = 0;
	while((1ul<<unit->u_BlockShift) < unit->u_BlockSize)
		unit->u_BlockShift++;

	_INF_UNIT(unit, "GEOMETRY: SectorSize: %lu, TotalSectors: %lu\n",
		  unit->u_Geometry.dg_SectorSize, unit->u_Geometry.dg_TotalSectors);

	return;
}
static void
sbp2_send_dummy_orb(SBP2Unit *unit)
{
	ORBDummy orb;
	ZERO(orb);

	orb.next.q = ORBPOINTER_NULL;
	orb.control = ORBCONTROLF_NOTIFY | (3<<13);
	
	SBP2ORBRequest orbreq;
	ZERO(orbreq);
	
	orbreq.or_Base.io_Message.mn_Length = sizeof(orbreq);
	orbreq.or_Base.io_Message.mn_ReplyPort = unit->u_OrbPort;
	
	/* Send the ORB request */
	ULONG status_signal = 1ul << unit->u_ORBStatusSigBit;
	_DBG_UNIT(unit, "sending dummy ORB...\n");
	sbp2_send_orb_req(unit, &orbreq, &orb,
		unit->u_ORBLoginResponse.command_agent + SBP2_ORB_POINTER,
		sbp2_complete_managment_orb, status_signal);
	
	/* Wait for 1394 transport */
	ULONG orb_sig = 1ul << unit->u_OrbPort->mp_SigBit;
	ULONG sigs = Wait(SIGBREAKF_CTRL_C | orb_sig);
	
	if (sigs & orb_sig)
	{
		GetMsg(unit->u_OrbPort);

		if ((HHIOERR_NO_ERROR != orbreq.or_Base.io_Error) ||
			(HELIOS_RCODE_COMPLETE != orbreq.or_Packet.RCode))
		{
			_ERR_ORB(unit, orb, "transport error, ioerr=%ld, rcode=%ld\n",
				orbreq.or_Base.io_Error, orbreq.or_Packet.RCode);
			goto out;
		}

		/* The Mgt_ORB_timeout starts from now */
		LONG err = sbp2_wait(unit, status_signal, unit->u_MgtTimeout);
		if (HERR_TIMEOUT == err)
		{
			_ERR_ORB(unit, orb, "status timeout\n");
			goto out;
		}

		if (STATUS_GET_RESPONSE(orbreq.or_ORBStatus) || 
			(STATUS_GET_SBP_STATUS(orbreq.or_ORBStatus) != 0 &&
			 STATUS_GET_SBP_STATUS(orbreq.or_ORBStatus) != SBP2_REQ_ST_DUMMY_ORB_COMPLETED))
		{
			_ERR_ORB(unit, orb, "status error: <%u:%u>\n",
				 STATUS_GET_RESPONSE(orbreq.or_ORBStatus),
				 STATUS_GET_SBP_STATUS(orbreq.or_ORBStatus));
		}
	}

out:
	sbp2_cancel_orbs(unit);
}
static BOOL
sbp2_login(SBP2Unit *unit)
{
	ORBLogin orb;
	int i;

	_DBG_UNIT(unit, "LOGIN: proceed...\n");

	memset(&unit->u_ORBLoginResponse, 0xff, sizeof(ORBLoginResponse));

	/* Filling the login ORB data */
	orb.password.q = 0; /* No password */
	orb.passwordlen = 0;
	orb.response.q = utils_GetPhyAddress(&unit->u_ORBLoginResponse);
	orb.responselen = sizeof(ORBLoginResponse);
	orb.control = ORBCONTROLF_NOTIFY | ORBCONTROLTYPE_LOGIN
				| ORBLOGINCONTROLF_EXCLUSIVE | ORBLOGINRECONNECT(2);
	orb.lun = unit->u_LUN;

	/* Where the target will write the ORB status block */
	orb.statusfifo = unit->u_FifoStatusRH.hrh_Start;

	log_DumpMem(&orb, sizeof(orb), FALSE, "Login ORB:\n");

	i = 0;
	for (i=0; i<LOGIN_RETRY; i++)
	{
		LONG err;

		/* 250ms before next login ORB */
		if (i > 0)
			Helios_DelayMS(250);

		/* Sending the Login ORB to the target management agent */
		err = sbp2_do_management_orb(unit, &orb);
		if (!err)
		{
			unit->u_ORBLoginResponse.command_agent &= ~(0xFFFFull << 48);

			_INF_UNIT(unit, "LOGIN succeed!\n");
			_DBG_UNIT(unit, "Length        : %u\n", unit->u_ORBLoginResponse.length);
			_DBG_UNIT(unit, "LoginID       : $%X\n", unit->u_ORBLoginResponse.login_id);
			_DBG_UNIT(unit, "CommandAgent  : $%016llX\n", unit->u_ORBLoginResponse.command_agent);
			_DBG_UNIT(unit, "ReconnectHold : $%X\n", unit->u_ORBLoginResponse.reconnect_hold);
			
			/* Reset and activate the target command agent */
			sbp2_agent_reset(unit);
			sbp2_send_dummy_orb(unit);
			_DBG_UNIT(unit, "agent state is %u\n", sbp2_get_agent_state(unit));

			/* Set the CSR BUSY_TIMEOUT with a sane value */
			sbp2_do_write_quadlet(unit, CSR_BASE_LO + CSR_BUSY_TIMEOUT,
				unit->u_MaxSpeed, SBP2_CYCLE_LIMIT | SBP2_RETRY_LIMIT);

#ifndef NDEBUG
			{
				QUADLET value;
				sbp2_do_read_quadlet(unit, CSR_BASE_LO + CSR_BUSY_TIMEOUT,
					unit->u_MaxSpeed, &value);
				_DBG_UNIT(unit, "New BUSY_TIMEOUT=$%08x\n", value);
			}
#endif
			
			return TRUE;
		}
		else if (HERR_TIMEOUT != err)
			break;
	}

	return FALSE;
}
static BOOL
sbp2_update(SBP2Unit *unit)
{
	ULONG gen=0;
	UWORD localid=~0;
	HeliosNode nodeinfo = {0};
	LONG res;

	// TODO: AcceptIO usage not effective

	EXCLUSIVE_PROTECT_BEGIN(unit);
	unit->u_Flags.Ready = 0;
	EXCLUSIVE_PROTECT_END(unit);

	/* Get info on local node */
	res = Helios_GetAttrs(unit->u_HeliosHardware,
		HA_NodeID, (ULONG)&localid,
		TAG_DONE);
	if (1 != res)
	{
		_ERR_UNIT(unit, "Failed to obtain local node id\n");
		return FALSE;
	}

	/* Get info on target */
	res = Helios_GetAttrs(unit->u_HeliosDevice,
		HA_Generation, (ULONG)&gen,
		HA_NodeInfo, (ULONG)&nodeinfo,
		TAG_DONE);
	if (res == 2)
	{
		unit->u_Generation = gen;
		unit->u_NodeID = HELIOS_LOCAL_BUS | nodeinfo.hn_PhyID;
		unit->u_MaxSpeed = nodeinfo.hn_MaxSpeed;

		/* SBP2 max payload size = 2 ^ (max_payload + 2).
		 * At S100, max payload size is 512 (= 2 ^ 9),
		 * Then the size double at each speed step.
		 * So max_payload is max_speed + 7.
		 * The maximum possible for this value is 15.
		 * => max_speed = 7.
		 * But currently the maximal speed defined by IEEE1394
		 * standard is S3200=5 => max_payload = 5+7 = 12.
		 */
		unit->u_MaxPayload = MIN(nodeinfo.hn_MaxSpeed + 7, 12u);

		_INF_UNIT(unit, "Gen=%u, NodeID=$%04x, MaxSpeed=%u, MaxPayload=%lu\n",
			unit->u_Generation, unit->u_NodeID,
			unit->u_MaxSpeed, 1ul << (unit->u_MaxPayload+2));
		
		if (unit->u_Flags.Logged)
		{
			_WRN_UNIT(unit, "Reconnect needed\n");
			DB_NotFinished();
#if 0
			unit->u_Flags.Logged = sbp2_reconnect(unit);
			if (unit->u_Flags.Logged)
			{
				EXCLUSIVE_PROTECT_BEGIN(unit);
				{
					unit->u_Flags.AcceptIO = 1;
					unit->u_Flags.Ready = 1;
				}
				EXCLUSIVE_PROTECT_END(unit);

				return TRUE;
			}
#endif
		}
		else
		{
			unit->u_Flags.Logged = sbp2_login(unit);
			if (unit->u_Flags.Logged && sbp2_inquiry(unit))
			{
				EXCLUSIVE_PROTECT_BEGIN(unit);
				unit->u_Flags.AcceptIO = 1;
				EXCLUSIVE_PROTECT_END(unit);

				DB_NotFinished();

				if (!unit->u_Flags.Removable)
				{
					if (!sbp2_do_start_unit(unit))
					{
						unit->u_BlockSize = 0;
						sbp2_get_capacity(unit);

						EXCLUSIVE_PROTECT_BEGIN(unit);
						unit->u_Flags.Ready = 1;
						EXCLUSIVE_PROTECT_END(unit);

						/* Auto-mount */
						if ((PDT_SIMPLE_DIRECT_ACCESS == unit->u_DeviceType) ||
							(PDT_DIRECT_ACCESS == unit->u_DeviceType))
							sbp2_automount(unit);
					}
					else
						_ERR_UNIT(unit, "START cmd failed on non-removable unit\n");
				}
				return TRUE;
			}
		}
	}
	else
		_WRN_UNIT(unit, "failed to obtain device info\n");

	EXCLUSIVE_PROTECT_BEGIN(unit);
	unit->u_Flags.AcceptIO = 0;
	EXCLUSIVE_PROTECT_END(unit);

	return FALSE;
}
static void
sbp2_freemem(APTR p, ULONG s, APTR udata)
{
	struct ExecBase *SysBase = udata;
	FreeMem(p, s);
}
static HeliosResponse *
sbp2_fifo_status_rh(HeliosPacket *request, APTR udata)
{
	SBP2Unit *unit = udata;
	HeliosResponse *response;
	ORBStatus *status;
	BOOL not_unified = request->Ack == HELIOS_ACK_PENDING;

	_DBG_UNIT(unit, "FIFO_STATUS: TC=$%x, Off=$%012llx, Pl=%p, Len=%u\n",
		request->TCode, request->Offset,
		request->Payload, request->PayloadLength);

	/* This request handler doesn't send any response packets.
	 * We just set orb RCode to complete and call the orb done callback,
	 */

	if (not_unified)
	{
		response = AllocMem(sizeof(*response), MEMF_PUBLIC|MEMF_CLEAR);
		if (!response)
		{
			_ERR_UNIT(unit, "Can't allocated response\n");
			return NULL;
		}

		//response->hr_FreeFunc = sbp2_freemem;
		//response->hr_FreeUData = SysBase;
		//response->hr_AllocSize = sizeof(*response);
	}
	else
		response = NULL;

	if ((TCODE_WRITE_BLOCK_REQUEST != request->TCode)
		|| (NULL == request->Payload)
		|| (request->PayloadLength < 8)
		|| (request->PayloadLength > sizeof(*status)))
	{
		if (not_unified)
			response->hr_Packet.RCode = HELIOS_RCODE_TYPE_ERROR;
		return response;
	}

	if (not_unified)
		response->hr_Packet.RCode = HELIOS_RCODE_COMPLETE;

	status = (APTR)request->Payload;
	/* bypass non-orb related status */
	if ((STATUS_GET_SOURCE(*status) <= 1) &&
		(STATUS_GET_ORB_HIGH(*status) == 0))
	{
		ULONG orb_low = STATUS_GET_ORB_LOW(*status);
		SBP2ORBRequest *orbreq = NULL;

		WRITE_LOCK_LIST(unit->u_PendingORBs);
		{
			struct MinNode *node;

			/* search associated ORB in pending list */
			ForeachNode(&unit->u_PendingORBs, node)
			{
				SBP2ORBRequest *req = (APTR)node - offsetof(SBP2ORBRequest, or_Node);

				if (req->or_ORBAddr.addr.lo == orb_low)
				{
					orbreq = req;
					break;
				}
			}

			if (orbreq)
			{
				_DBG_UNIT(unit, "ORB req %p: status replied\n", orbreq);
				REMOVE(&orbreq->or_Node);
			}
		}
		UNLOCK_LIST(unit->u_PendingORBs);

		if (orbreq)
		{
			orbreq->or_ORBDone(unit, orbreq, status);
			Signal(orbreq->or_ORBStTask, orbreq->or_ORBStSignal);
		}
		else
			_WRN_UNIT(unit, "No pending ORB at Phy offset $%08x\n", orb_low);
	}
	else
		_WRN_HUNIT(unit, "Drop non-orb related status\n");

	return response;
}
static LONG
sbp2_alloc_fsreqh(SBP2Unit *unit)
{
	struct IOStdReq ioreq;
	
	unit->u_FifoStatusRH.hrh_RegionStart = HELIOS_HIGHMEM_START;
	unit->u_FifoStatusRH.hrh_RegionStop = HELIOS_HIGHMEM_STOP;
	unit->u_FifoStatusRH.hrh_Length = 0x100;
	unit->u_FifoStatusRH.hrh_Flags = HELIOS_RHF_ALLOCATE;
	unit->u_FifoStatusRH.hrh_Callback = sbp2_fifo_status_rh;
	unit->u_FifoStatusRH.hrh_UserData = unit;

	bzero(&ioreq, sizeof(struct IORequest));
	ioreq.io_Message.mn_Length = sizeof(ioreq);
	ioreq.io_Command = HHIOCMD_ADDREQHANDLER;
	ioreq.io_Data = &unit->u_FifoStatusRH;

	return Helios_DoIO(unit->u_HeliosHardware, (struct IORequest *)&ioreq);
}
static void
sbp2_dealloc_fsreqh(SBP2Unit *unit)
{
	struct IOStdReq ioreq;
	
	if (unit->u_FifoStatusRH.hrh_Start)
	{
		bzero(&ioreq, sizeof(struct IORequest));
		ioreq.io_Message.mn_Length = sizeof(ioreq);
		ioreq.io_Command = HHIOCMD_REMREQHANDLER;
		ioreq.io_Data = &unit->u_FifoStatusRH;

		if (Helios_DoIO(unit->u_HeliosHardware, (struct IORequest *)&ioreq))
			_WRN_UNIT(unit, "Helios_DoIO() failed on FIFO status request removal\n");
	}
}
static void
sbp2_read_unit_name(SBP2Unit *unit, const QUADLET *dir)
{
	UBYTE buf[60];
	LONG len;

	len = Helios_ReadTextualDescriptor(dir, buf, sizeof(buf));
	if (len > 0)
	{
		STRPTR name;

		name = AllocVecPooled(unit->u_Class->hc_MemPool, len+1);
		if (NULL != name)
		{
			unit->u_UnitName = name;
			CopyMem(buf, name, len);
			name[len] = '\0';

			_INF_UNIT(unit, "=> ROM Unit name: %s\n", name);
		}
		else
			_ERR_UNIT(unit, "Name alloc failed for %u bytes\n", len);
	}
	else
		_ERR_UNIT(unit, "Helios_ReadTextualDescriptor() failed\n");
}
static void
sbp2_getrominfo(SBP2Unit *unit, const QUADLET *dir)
{
	HeliosRomIterator ri;
	QUADLET key, value;

	/* Default values */
	unit->u_MgtTimeout = 0;
	unit->u_ORBSize = 8;
	unit->u_MaxReconnectHold = 0;
	unit->u_MgtAgentBase = 0xFFFFF0010000ull; /* As defined by the SBP-2 */
	unit->u_Flags.Ordered = 1;
	unit->u_DeviceType = 0xff; /* non existant device type */
	unit->u_LUN = 0;

	Helios_InitRomIterator(&ri, dir);
	while (Helios_RomIterate(&ri, &key, &value))
	{
		switch (key)
		{
			case CSR_KEY_MODEL_ID:
				_INF_UNIT(unit, "=> Model id             : $%06X\n", value);
				break;

			case KEY_SBP2_COMMAND_SET_SPEC_ID:
				_INF_UNIT(unit, "=> Command set spec id  : $%06X\n", value);
				break;

			case KEY_SBP2_COMMAND_SET:
				_INF_UNIT(unit, "=> Command set          : $%06X\n", value);
				break;

			case KEY_SBP2_UNIT_CHARACTERISTICS:
				/* timeout base is 500ms */
				unit->u_MgtTimeout = (0xff & (value >> 8)) * 500;
				unit->u_ORBSize = 0xff & value;

				_INF_UNIT(unit, "=> Unit characteristics : $%06x [Mgt_ORB_timeout=%lu, ORB_size=%u]\n",
					value, unit->u_MgtTimeout, unit->u_ORBSize);
				break;

			case KEY_SBP2_COMMAND_SET_REVISION:
				_INF_UNIT(unit, "=> Command set revision : $%06X\n", value);
				break;

			case KEY_SBP2_FIRMWARE_REVISION:
				_INF_UNIT(unit, "=> Firmare revision     : $%06X\n", value);
				break;

			case KEY_SBP2_RECONNECT_TIMEOUT:
				unit->u_MaxReconnectHold = value & 0xffff;
				_INF_UNIT(unit, "=> Unit characteristics : MaxReconnectHold=%u\n",
					unit->u_MaxReconnectHold);
				break;

			case KEY_SBP2_CSR_OFFSET:
				unit->u_MgtAgentBase = CSR_BASE_LO + value * 4;
				_INF_UNIT(unit, "=> Management Agent CSR : $%016llX\n", unit->u_MgtAgentBase);
				break;

			case KEY_SBP2_LOGICAL_UNIT_NUMBER:
				sbp2_config_logical_unit(unit, value);
				break;

			case KEY_SBP2_LOGICAL_UNIT_DIRECTORY:
				sbp2_scan_logical_unit_directory(unit, &ri.actual[value - 1]);
				break;

			case (KEYTYPE_LEAF<<6) | CSR_KEY_TEXTUAL_DESCRIPTOR:
				sbp2_read_unit_name(unit, &ri.actual[value - 1]);
				break;

			default:
				_DBG_UNIT(unit, "Optional ROM key $%02x, Value: $%06x\n", key, value);
		}
	}

	/* clamp ORB management timeout to sane values (in milliseconds) */
	unit->u_MgtTimeout = MIN(MAX(unit->u_MgtTimeout, 5000), 40000);
}
static void
sbp2_free_unit(SBP2Unit *sbp2_unit)
{
	FreeMem(sbp2_unit, sizeof(*sbp2_unit));
}
static void
sbp2_driver_task(SBP2ClassLib *base, SBP2Unit *unit)
{
	HeliosEventListenerList *dev_ell=NULL, *hw_ell=NULL;
	HeliosEventMsg sbp2_dev_listener;
	HeliosEventMsg sbp2_hw_listener;
	struct MsgPort *evt_port=NULL;
	struct timerequest *removable_tr=NULL;
	struct timerequest *reconnect_tr=NULL;
	HeliosEventMsg *evt;
	ULONG sigs, gen=0, ioreq_signal, timer_signal;
	BYTE io_sigbit=-1;
	STRPTR task_name;
	BOOL run;

	bzero(&sbp2_hw_listener, sizeof(sbp2_hw_listener));
	bzero(&sbp2_dev_listener, sizeof(sbp2_dev_listener));

	/* Set unit defaults.
	 * All this values maybe used by the device interface, so must be valid
	 * before adding the unit to the device's units list.
	 */
	unit->u_SysUnit.unit_MsgPort.mp_Flags = PA_IGNORE;
	unit->u_SysUnit.unit_MsgPort.mp_SigTask = NULL;
	unit->u_ChangeCounter = 1;
	unit->u_Flags.WriteProtected = 1; /* TODO: Forced */
	unit->u_Flags.AutoMountCD = 1;
	unit->u_NodeID = ~0;
	
	INIT_LOCKED_LIST(unit->u_PendingORBs);

	/* Parse the unit ROM directory block.
	 * So we lock the device to keep a ROM pointer valid
	 * during this time.
	 */
	QUADLET *rom_dir=NULL;
	Helios_GetAttrs(unit->u_HeliosUnit,
		HA_UnitRomDirectory, (ULONG)&rom_dir,
		TAG_DONE);
	if (!rom_dir)
	{
		_ERR_UNIT(unit, "failed to obtain unit ROM directory\n");
		goto release_unit;
	}
	
	sbp2_getrominfo(unit, rom_dir);
	FreeVec(rom_dir);

	Helios_GetAttrs(unit->u_HeliosDevice,
		HA_Hardware, (ULONG)&unit->u_HeliosHardware, /* NR */
		HA_EventListenerList, (ULONG)&dev_ell,
		HA_NodeID, (ULONG)&unit->u_NodeID,
		TAG_DONE);

	if (!unit->u_HeliosHardware)
	{
		_ERR_UNIT(unit, "failed to obtain hardware\n");
		goto release_unit;
	}

	if (unit->u_NodeID == ~0)
	{
		_ERR_UNIT(unit, "failed to obtain device NodeID\n");
		goto release_hw;
	}

	if (!dev_ell)
	{
		_ERR_UNIT(unit, "failed to obtain device's listener list\n");
		goto release_hw;
	}
	
	Helios_GetAttrs(unit->u_HeliosHardware,
		HA_EventListenerList, (ULONG)&hw_ell,
		TAG_DONE);
		
	if (!hw_ell)
	{
		_ERR_UNIT(unit, "failed to obtain hardware's listener list\n");
		goto release_hw;
	}

	/* Alloc system resources */
	io_sigbit = AllocSignal(-1);
	evt_port = CreateMsgPort();
	unit->u_TimerPort = CreateMsgPort();
	unit->u_OrbPort = CreateMsgPort();
	unit->u_ORBStatusSigBit = AllocSignal(-1);
	unit->u_NotifyUnit = MountCreateNotifyUnitTags(MOUNTATTR_DEVICE, (ULONG)base->hc_DevBase,
												   MOUNTATTR_UNIT, (ULONG)&unit->u_SysUnit,
												   TAG_DONE);

	if ((NULL == unit->u_NotifyUnit) ||
		(NULL == evt_port) ||
		(NULL == unit->u_TimerPort) ||
		(NULL == unit->u_OrbPort) ||
		(-1 == unit->u_ORBStatusSigBit) ||
		(-1 == io_sigbit))
		goto dealloc_ressources;
		
	unit->u_IOTimeReq = Helios_OpenTimer(unit->u_TimerPort, UNIT_VBLANK);
	if (NULL == unit->u_IOTimeReq)
		goto dealloc_ressources;

	/* Prepare IO port to accept requests from the device interface */
	unit->u_SysUnit.unit_MsgPort.mp_SigBit = io_sigbit;
	unit->u_SysUnit.unit_MsgPort.mp_SigTask = FindTask(NULL);
	unit->u_SysUnit.unit_MsgPort.mp_Node.ln_Type = NT_MSGPORT;
	unit->u_SysUnit.unit_MsgPort.mp_Flags = PA_SIGNAL;

	/* Alloc a request region for the Fifo Status */
	if (sbp2_alloc_fsreqh(unit))
	{
		_ERR_UNIT(unit, "FIFO status request handler alloc failed\n");
		goto dealloc_ressources;
	}

	/* Register ourself to hardware events */
	sbp2_hw_listener.hm_Msg.mn_ReplyPort = evt_port;
	sbp2_hw_listener.hm_Msg.mn_Length = sizeof(sbp2_hw_listener);
	sbp2_hw_listener.hm_Type = HELIOS_MSGTYPE_EVENT;
	sbp2_hw_listener.hm_EventMask = HEVTF_HARDWARE_BUSRESET;
	Helios_AddEventListener(hw_ell, &sbp2_hw_listener);

	/* Register ourself to device events */
	sbp2_dev_listener.hm_Msg.mn_ReplyPort = evt_port;
	sbp2_dev_listener.hm_Msg.mn_Length = sizeof(sbp2_dev_listener);
	sbp2_dev_listener.hm_Type = HELIOS_MSGTYPE_EVENT;
	sbp2_dev_listener.hm_EventMask = HEVTF_DEVICE_NEW_UNIT
		| HEVTF_DEVICE_DEAD
		| HEVTF_DEVICE_REMOVED
		| HEVTF_DEVICE_UPDATED;
	Helios_AddEventListener(dev_ell, &sbp2_dev_listener);

	/* First login and target initialization */
	if (!sbp2_update(unit))
		goto forbid_io;

	/* Main loop */
	run = TRUE;
	while (run)
	{
		sigs = Wait(SIGBREAKF_CTRL_C | (1ul << evt_port->mp_SigBit));

		if (sigs & SIGBREAKF_CTRL_C)
			run = FALSE;
			
		/* Helios events */
		if (sigs & (1ul << evt_port->mp_SigBit))
		{
			BOOL update = FALSE;

			while (NULL != (evt = (HeliosEventMsg *)GetMsg(evt_port)))
			{
				switch (evt->hm_Type)
				{
					case HELIOS_MSGTYPE_EVENT:
						if (run)
						{
							switch (evt->hm_EventMask)
							{
								case HEVTF_HARDWARE_BUSRESET:
									EXCLUSIVE_PROTECT_BEGIN(unit);
									{
										_DBG_UNIT(unit, "Bus reset detected, set ready to 0\n");
										unit->u_Flags.Ready = 0;
									}
									EXCLUSIVE_PROTECT_END(unit);
									break;

								case HEVTF_DEVICE_DEAD:
								case HEVTF_DEVICE_REMOVED:
									EXCLUSIVE_PROTECT_BEGIN(unit);
									{
										unit->u_Flags.Logged = 0;
										unit->u_Flags.Ready = 0;
										unit->u_Flags.AcceptIO = 0;
									}
									EXCLUSIVE_PROTECT_END(unit);
									run = FALSE;
									break;

								case HEVTF_DEVICE_UPDATED:
									update = TRUE;
									break;
							}
							break;
						}

						FreeMem(evt, evt->hm_Msg.mn_Length);
						break;

					default:
						ReplyMsg((APTR)evt);
				}
			}

			if (run & update)
				sbp2_update(unit);
		}
	}
	
forbid_io:
	/* Forbid new IO requests */
	EXCLUSIVE_PROTECT_BEGIN(unit);
	{
		unit->u_Flags.AcceptIO = 0;
		unit->u_Flags.Ready = 0;
		unit->u_SysUnit.unit_MsgPort.mp_Flags = PA_IGNORE;
		unit->u_SysUnit.unit_MsgPort.mp_SigTask = NULL;
	}
	EXCLUSIVE_PROTECT_END(unit);

	/* Remove all mount points and notifications */
	sbp2_unmount_all(unit);

	/* Unregister all Helios events */
	if (0 != sbp2_dev_listener.hm_EventMask)
		Helios_RemoveEventListener(dev_ell, &sbp2_dev_listener);

	if (0 != sbp2_hw_listener.hm_EventMask)
		Helios_RemoveEventListener(hw_ell, &sbp2_hw_listener);

	/* Flush Helios events port */
	while (NULL != (evt = (HeliosEventMsg *)GetMsg(evt_port)))
	{
		switch (evt->hm_Type)
		{
			case HELIOS_MSGTYPE_EVENT:
				FreeMem(evt, evt->hm_Msg.mn_Length);
				break;
		}
	}

dealloc_ressources:
	sbp2_dealloc_fsreqh(unit);
	if (unit->u_NotifyUnit)
		MountDeleteNotifyUnit(unit->u_NotifyUnit);
	if (unit->u_UnitName)
		FreeVecPooled(base->hc_MemPool, unit->u_UnitName);
	if (unit->u_OneBlock)
		FreePooled(base->hc_MemPool, unit->u_OneBlock, unit->u_OneBlockSize);
	if (unit->u_IOTimeReq)
		Helios_CloseTimer(unit->u_IOTimeReq);
	if (unit->u_TimerPort)
		DeleteMsgPort(unit->u_TimerPort);
	if (unit->u_OrbPort)
		DeleteMsgPort(unit->u_OrbPort);
	if (evt_port)
		DeleteMsgPort(evt_port);
	if (io_sigbit >= 0)
		FreeSignal(io_sigbit);
	if (unit->u_ORBStatusSigBit>= 0)
		FreeSignal(unit->u_ORBStatusSigBit);

release_hw:
	Helios_ReleaseObject(unit->u_HeliosHardware);

release_unit:
	Helios_SetAttrs(unit->u_HeliosUnit, HA_UserData, 0, TAG_DONE);
	Helios_ReleaseObject(unit->u_HeliosUnit);
	Helios_ReleaseObject(unit->u_HeliosDevice);

	sbp2_free_unit(unit);
	
	Helios_ReleaseObject(base->hc_HeliosClass);
}
static BOOL
sbp2_scsi_setup(SBP2Unit *unit, SBP2SCSICmdReq *req)
{
	struct SCSICmd *scsicmd = req->sr_Cmd;
	ORBSCSICommand *orb = &req->sr_ORB;

	orb->next.q = ORBPOINTER_NULL;
	orb->control = ORBCONTROLPAYLOAD(unit->u_MaxPayload)
		| ORBCONTROLSPEED(unit->u_MaxSpeed)
		| ORBCONTROLF_NOTIFY;

	if (scsicmd->scsi_Flags & SCSIF_READ)
		orb->control |= ORBCONTROLF_READ;

	if (scsicmd->scsi_CmdLength >= 16)
	{
		CopyMemQuick(scsicmd->scsi_Command, orb->cdb, 16);
		scsicmd->scsi_CmdActual = 16;
	}
	else
	{
		bzero(orb->cdb, 16);
		CopyMem(scsicmd->scsi_Command, orb->cdb, scsicmd->scsi_CmdLength);
		scsicmd->scsi_CmdActual = scsicmd->scsi_CmdLength;
	}

	if (scsicmd->scsi_Data)
	{
#ifdef ERROR_ON_BAD_ALIGNMENT
		if (0 != ((ULONG)scsicmd->scsi_Data % 4))
		{
			_ERR_UNIT(unit, "SCSI: data address not aligned on 4-bytes: %p\n",
				 scsicmd->scsi_Data);
			return FALSE;
		}
#endif

		orb->desc_hi = (ULONG)unit->u_NodeID << 16; /* local node id */

		if (scsicmd->scsi_Length <= 0xffff)
		{
			orb->desc_lo = utils_GetPhyAddress(scsicmd->scsi_Data);
			orb->datalen = scsicmd->scsi_Length;
		}
		else
		{
			ULONG i;
			ULONG remains = scsicmd->scsi_Length;
			ULONG phy = utils_GetPhyAddress(scsicmd->scsi_Data);

			/* not enough SG pagse ?*/
			if (remains > (ORB_PAGE_SIZE * ORB_SG_PAGES))
			{
				_ERR_UNIT(unit, "SCSI: not enough SGPages for required data length: %lu\n",
						  scsicmd->scsi_Length);
				return FALSE;
			}

			/* Prepare ScatterGather pages (unrestricted pages) */
			for (i=0; (i < ORB_SG_PAGES) && (0 != remains); i++)
			{
				ULONG len = MIN(ORB_PAGE_SIZE, remains);

				req->sr_SGPages[i].length_addr_hi = BE_SWAPLONG(len << 16);
				req->sr_SGPages[i].addr_lo = BE_SWAPLONG(phy);

				remains -= len;
				phy += len;
			}
			
			/* Make sure that SG pages are in memory */
			CacheFlushDataArea(req->sr_SGPages, sizeof(SBP2SGPage)*i);

			orb->desc_lo = utils_GetPhyAddress(req->sr_SGPages);
			orb->datalen = i;
			orb->control |= ORBCONTROLF_PAGETABLE | ORBCONTROLPAGESIZE(0);
		}
	}
	else
	{
		orb->desc_hi = NULL;
		orb->desc_lo = NULL;
		orb->datalen = 0;
	}

	_DBG_ORB(unit, orb, "Data phy addr %llx, len=%u\n", ((UQUAD)orb->desc_hi << 32) + orb->desc_lo, orb->datalen);
	log_DumpMem(orb, sizeof(*orb), FALSE, "ORB dump:\n");
	return TRUE;
}
static SBP2ORBRequest *
sbp2_alloc_orb_req(SBP2Unit *unit, ULONG length)
{
	SBP2ORBRequest *orbreq;

	length = GET_ALIGNED2(MAX(sizeof(*orbreq), length), 8);

	/* align ORB request on cache line */
	orbreq = AllocPooledAligned(unit->u_Class->hc_MemPool, length, 8, 0);
	if (NULL != orbreq)
		orbreq->or_Base.io_Message.mn_Length = length;
	else
		_ERR_UNIT(unit, "Failed to alloc ORB request\n");

	return orbreq;
}
static void
sbp2_free_orb_req(SBP2Unit *unit, SBP2ORBRequest *orbreq)
{
	FreePooled(unit->u_Class->hc_MemPool, orbreq, orbreq->or_Base.io_Message.mn_Length);
}
static ULONG
sbp2_sense_data_from_status(const UBYTE *status, UBYTE *sense_data)
{
	sense_data[0] = 0x70;        /* current error response code */
	sense_data[1] = 0x0;         /* reserved */
	sense_data[2] = ((status[1]&0xf0)<<1) + (status[1]&0x0f);
	sense_data[3] = status[4];   /* Information (status bytes 4-7) */
	sense_data[4] = status[5];
	sense_data[5] = status[6];
	sense_data[6] = status[7];
	sense_data[7] = 10;          /* Additional sense length (n-7, n=17) */
	sense_data[8] = status[8];   /* CDB-dependent (status bytes 8-11) */
	sense_data[9] = status[9];
	sense_data[10] = status[10];
	sense_data[11] = status[11];
	sense_data[12] = status[2];  /* sense code */
	sense_data[13] = status[3];  /* sense code qualifer */
	sense_data[14] = status[12]; /* fru (status bytes 12-13) */
	sense_data[15] = status[13];

	/* SAM-2 status code */
	return status[0] & 0x3f;
}
static void
sbp2_complete_scsi_orb(SBP2Unit *unit, SBP2ORBRequest *orbreq, ORBStatus *status)
{
	SBP2SCSICmdReq *req = (APTR)orbreq;
	struct SCSICmd *cmd = req->sr_Cmd;
	UBYTE scsi_status = 0;

	if (status)
	{
		orbreq->or_ORBStatus.status = status->status;
		log_DumpMem(status, (STATUS_GET_LEN(*status)+1)*sizeof(QUADLET), FALSE, "Status dump:\n");

		/*if (STATUS_GET_DEAD(*status))
			sbp2_agent_reset(req->sr_Unit);*/
		if (STATUS_GET_DEAD(*status))
		{
			_WRN_UNIT(unit, "status says agent is dead\n");
			_DBG_UNIT(unit, "agent state is %u\n", sbp2_get_agent_state(unit));
		}
		
		switch (STATUS_GET_RESPONSE(*status))
		{
			case SBP2_STATUS_REQUEST_COMPLETE:
				scsi_status = SCSI_GOOD;

				#if 1
				/* Trash data cache of region written by the FW DMA */
				if (NULL != cmd->scsi_Data)
					CacheTrashCacheArea(cmd->scsi_Data, cmd->scsi_Length);
				#endif
				cmd->scsi_Actual = cmd->scsi_Length;

				if (STATUS_GET_LEN(*status) > 1)
				{
					if ((18 >= cmd->scsi_SenseLength) && (NULL != cmd->scsi_SenseData))
					{
						scsi_status = sbp2_sense_data_from_status(status->data, cmd->scsi_SenseData);
						cmd->scsi_SenseActual = 18;
						_INF_UNIT(unit, "SCSI: sense data for SCSI cmd %p, status=%u\n", cmd, scsi_status);
					}
					else
					{
						scsi_status = status->data[0] & 0x3f;
						_WRN_UNIT(unit, "SCSI[$%02x] Failed: Status=%u, SenseData=[key=$%x, asc/ascq=$%02x/$%02x] (not forwared)\n",
							cmd->scsi_Command[0], scsi_status,
							(((status->data[1]&0xf0)<<1) + (status->data[1]&0x0f)) & SK_MASK,
							status->data[2], status->data[3]);
					}
				}
				break;
				
			case SBP2_STATUS_TRANSPORT_FAILURE:
				_WRN_UNIT(unit, "SCSI: TRANSPORT_FAILURE\n");
				scsi_status = HFERR_Phase;
				break;
				
			case SBP2_STATUS_ILLEGAL_REQUEST:
			case SBP2_STATUS_VENDOR_DEPENDENT:
				_WRN_UNIT(unit, "SCSI: ILLEGAL_REQUEST/VENDOR_DEPENDENT (%u)\n",
					STATUS_GET_RESPONSE(*status));
				scsi_status = HFERR_Phase;
				break;
		}
	}
	else
	{
		/* The ORB has been cancelled (bus reset?) */
		scsi_status = SCSI_TASK_ABORTED;
	}

	cmd->scsi_Status = scsi_status;
}
/*============================================================================*/
/*--- PUBLIC CODE ------------------------------------------------------------*/
/*============================================================================*/
void
sbp2_cancel_orbs(SBP2Unit *unit)
{
	WRITE_LOCK_LIST(unit->u_PendingORBs);
	{
		struct MinNode *node, *next;

		ForeachNodeSafe(&unit->u_PendingORBs, node, next)
		{
			SBP2ORBRequest *req = (APTR)node - offsetof(SBP2ORBRequest, or_Node);

			REMOVE(node);

			if (!CheckIO((struct IORequest *)&req->or_Base))
			{
				AbortIO((struct IORequest *)&req->or_Base);
				WaitIO((struct IORequest *)&req->or_Base);
			}
			
			req->or_ORBDone(unit, req, NULL);
			_WRN_UNIT(unit, "cancel ORB request @ %p\n", req);
			Signal(req->or_ORBStTask, req->or_ORBStSignal);
		}
	}
	UNLOCK_LIST(unit->u_PendingORBs);
}
LONG
sbp2_do_scsi_cmd(SBP2Unit *unit, struct SCSICmd *scsicmd, ULONG timeout)
{
	SBP2SCSICmdReq *req;
	LONG ioerr=0, retry;
	ULONG sigs, status_signal = 1ul << unit->u_ORBStatusSigBit;
	ULONG orb_sig = 1ul << unit->u_OrbPort->mp_SigBit;

	scsicmd->scsi_Actual = 0;
	scsicmd->scsi_SenseActual = 0;
	
	req = (SBP2SCSICmdReq *) sbp2_alloc_orb_req(unit, sizeof(SBP2SCSICmdReq));
	if (NULL == req)
		goto transport_error;

	/* Initialize the SCSI command request */
	req->sr_Base.or_Base.io_Message.mn_ReplyPort = unit->u_OrbPort;
	req->sr_Cmd = scsicmd;
	req->sr_Unit = unit;

	if (!sbp2_scsi_setup(unit, req))
		goto transport_error;

	retry = 3; /* try if target reply BUSY */
orb_retry:
	/* Send the ORB request through the 1394 link */
	sbp2_send_orb_req(unit, &req->sr_Base, &req->sr_ORB,
		unit->u_ORBLoginResponse.command_agent + SBP2_ORB_POINTER,
		sbp2_complete_scsi_orb, status_signal);

	/* Wait for 1394 transport */
	sigs = Wait(SIGBREAKF_CTRL_C | orb_sig);
	if (sigs & orb_sig)
	{
		GetMsg(unit->u_OrbPort);

		ioerr = req->sr_Base.or_Base.io_Error;
		if (ioerr)
		{
			LONG ack = req->sr_Base.or_Packet.Ack;
			LONG rcode = req->sr_Base.or_Packet.RCode;
			
			_ERR_UNIT(unit, "SCSI[$%02x]: ORB send error (ack=%ld, rcode=%ld)\n",
					scsicmd->scsi_Command[0], ack, rcode);

			ioerr = HFERR_Phase;

			if (HHIOERR_ACK == ioerr)
			{
				switch (ack)
				{
					case HELIOS_ACK_BUSY_X:
					case HELIOS_ACK_BUSY_A:
					case HELIOS_ACK_BUSY_B:
						/* Retry the write packet only in case of BUSY ack */
						Helios_DelayMS(10);
						_WRN_UNIT(unit, "SCSI[$%02x]: retry (%u left)\n", scsicmd->scsi_Command[0], retry);
						goto orb_retry;
				}
			}
			else if (HHIOERR_RCODE == ioerr)
			{
				if (HELIOS_RCODE_GENERATION == rcode)
					ioerr = TDERR_PostReset;
			}

			sbp2_cancel_orbs(unit);
			sbp2_agent_reset(unit);

			scsicmd->scsi_CmdActual = 0;
			/* scsicmd->scsi_Status set during the cancel_orb */
			goto out;
		}
		
		/* Wait ORB status response */
		if (HERR_TIMEOUT == sbp2_wait(unit, status_signal, timeout))
		{
			_ERR_UNIT(unit, "SCSI[$%02x]: status timeout (%lums)\n", scsicmd->scsi_Command[0], timeout);

			sbp2_cancel_orbs(unit);
			sbp2_agent_reset(unit);

			scsicmd->scsi_SenseActual = 0;
			ioerr = HFERR_Phase;
		}
		else if (SBP2_STATUS_REQUEST_COMPLETE != STATUS_GET_RESPONSE(req->sr_Base.or_ORBStatus))
		{
			_ERR_UNIT(unit, "SCSI[$%02x]: ORB status failure, response code is %x\n",
				 scsicmd->scsi_Command[0], STATUS_GET_RESPONSE(req->sr_Base.or_ORBStatus));

			sbp2_cancel_orbs(unit);
			sbp2_agent_reset(unit);

			scsicmd->scsi_SenseActual = 0;
			ioerr = HFERR_Phase;
		}
		else
		{
			/* Status received, check value */
			if (SCSI_GOOD != scsicmd->scsi_Status)
			{
				ioerr = HFERR_BadStatus;

				/* target has also send Sense data? */
				if (scsicmd->scsi_SenseActual > 0)
					_ERR_UNIT(unit, "SCSI[$%02x]: bad status, %lu sense bytes, [key=$%x, asc/ascq=$%02x/$%02x]\n",
						 scsicmd->scsi_Command[0], scsicmd->scsi_SenseActual,
						 scsicmd->scsi_SenseData[2] & SK_MASK,
						 scsicmd->scsi_SenseData[12], scsicmd->scsi_SenseData[13]);
				else
					_ERR_UNIT(unit, "SCSI[$%02x]: bad status, no sense data\n", scsicmd->scsi_Command[0]);
			}
			else
				ioerr = 0;
		}
	}
	else
		sbp2_cancel_orbs(unit);
		
	goto out;

transport_error:
	scsicmd->scsi_CmdActual = 0;
	scsicmd->scsi_Status = SCSI_CHECK_CONDITION;
	ioerr = HFERR_Phase;

out:
	if (req) sbp2_free_orb_req(unit, &req->sr_Base);

	/* in case of error, check if its not due to a bus-reset */
	if (ioerr)
	{
		ULONG gen;

		if ((1 == Helios_GetAttrs(unit->u_HeliosDevice,
			HA_Generation, (ULONG)&gen,
			TAG_DONE)) && (gen != unit->u_Generation))
		{
			EXCLUSIVE_PROTECT_BEGIN(unit);
			unit->u_Flags.Ready = 0;
			EXCLUSIVE_PROTECT_END(unit);
		}
	}

	return ioerr;
}
void
sbp2_unmount_all(SBP2Unit *unit)
{
	_INF_UNIT(unit, "Dismounting...\n", unit->u_UnitNo);
	MountDisMountTags(unit->u_NotifyUnit, TAG_END);
	MountNotifyAll(unit->u_NotifyUnit, -1);
}
#if 0
// WARNING: must be called into a r-locked SBP2ClassBase region.
void sbp2_incref(SBP2Unit *unit)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;

	LOCK_REGION(unit);
	++unit->u_SysUnit.unit_OpenCnt;
	UNLOCK_REGION(unit);
}
// WARNING: must be called into a w-locked SBP2ClassBase region.
void sbp2_decref(SBP2Unit *unit)
{
	SBP2ClassLib *base = unit->u_SBP2ClassBase;
	LONG cnt;

	LOCK_REGION(unit);
	{
		cnt = --unit->u_SysUnit.unit_OpenCnt;

		/* As we free the unit when cnt = 0,
		 * We prefere to force unit remove from the device list.
		 * But as the unit's handler task can also remove it before
		 * calling this function, Public flags is here to arbitrate that.
		 * It's just a security against bad applications don't well pair
		 * device Open/Close calls.
		 */
		if (!cnt && unit->u_Flags.Public)
		{
			REMOVE(unit);
			unit->u_Flags.Public = FALSE;
		}
	}
	UNLOCK_REGION(unit);

	if (!cnt)
	{
		SBP2Unit *node;
		ULONG maxno;
		
		_INFO("Freeing sbp2 unit %p-<%s>\n", unit, unit->u_UnitName);
		
		/* Find new unit # max */
		maxno = base->hc_MaxUnitNo;
		ForeachNode(&base->hc_Units, node)
		{
			if (node->u_UnitNo > maxno)
				maxno = node->u_UnitNo;
		}
		base->hc_MaxUnitNo = maxno;
		
		if (NULL != unit->u_NotifyUnit)
			MountDeleteNotifyUnit(unit->u_NotifyUnit);
		
		if (NULL != unit->u_UnitName)
			FreeVecPooled(base->hc_MemPool, unit->u_UnitName);
		
		/* Free unit itself */
		FreePooled(base->hc_MemPool, unit, sizeof(*unit));

		Forbid();
		--base->hc_Lib.lib_OpenCnt;
		Permit();
	}
}
#endif
LONG
sbp2_InitClass(SBP2ClassLib *base, HeliosClass *hc)
{
	base->hc_HeliosClass = hc;
	return 0;
}
LONG
sbp2_TermClass(SBP2ClassLib *base)
{
	base->hc_HeliosClass = NULL;
	return 0;
}
LONG
sbp2_AttemptUnitBinding(SBP2ClassLib *base, HeliosUnit *hu)
{
	QUADLET *unit_rom_dir=NULL;
	ULONG specid=~0, version=~0;

	Helios_GetAttrs(hu,
		HA_UnitRomDirectory, (ULONG)&unit_rom_dir,
		TAG_DONE);

	if (unit_rom_dir)
	{
		HeliosRomIterator ri;
		QUADLET key, value;
		
		Helios_InitRomIterator(&ri, unit_rom_dir);
		while (Helios_RomIterate(&ri, &key, &value))
		{
			switch (key)
			{
				case KEYTYPEV_IMMEDIATE | CSR_KEY_UNIT_SPEC_ID: specid = value; break;
				case KEYTYPEV_IMMEDIATE | CSR_KEY_UNIT_SW_VERSION: version = value; break;
			}
		}
		
		FreeVec(unit_rom_dir);

		if ((SBP2_UNIT_SPEC_ID_ENTRY == specid) && (SBP2_SW_VERSION_ENTRY == version))
			return sbp2_ForceUnitBinding(base, hu);
	}
	else
		_DBG("failed to obtain Unit ROM directory\n");

	return FALSE;
}
LONG
sbp2_ForceUnitBinding(SBP2ClassLib *base, HeliosUnit *hu)
{
	HeliosHardware *hh=NULL;
	HeliosDevice *hd=NULL;
	SBP2Unit *sbp2_unit=NULL;
	UWORD nodeid=-1;
	LONG count;
	char buf[64];

	Helios_GetAttrs(hu,
		HA_Device, (ULONG)&hd, /* NR */
		TAG_DONE);
	if (hd)
	{
		ULONG gen=0;
		UQUAD guid=0;

		_DBG_HUNIT(hu, "device @ %p\n", hd);

		count = Helios_GetAttrs(hd,
			HA_Generation, (ULONG)&gen,
			HA_GUID, (ULONG)&guid,
			HA_NodeID, (ULONG)&nodeid,
			TAG_DONE);
		if (count == 3)
		{
			_INF_HUNIT(hu, "New SBP2 unit (Dev=0x%04X/0x%016llx)\n", nodeid, guid);

			/* Create a new unit structure */
			sbp2_unit = AllocMem(sizeof(*sbp2_unit), MEMF_PUBLIC | MEMF_CLEAR);
			if (sbp2_unit)
			{
				LOCK_INIT(sbp2_unit);
				NEWLIST(&sbp2_unit->u_SysUnit.unit_MsgPort.mp_MsgList); /* used to forward IO msg */
				
				sbp2_unit->u_Class = base;
				sbp2_unit->u_GUID = guid;
				sbp2_unit->u_Generation = gen;
				sbp2_unit->u_HeliosDevice = hd; /* NR from Helios_GetAttrs */
				sbp2_unit->u_HeliosUnit = hu; /* NR see below */

				Helios_ObtainObject(hu);
				Helios_ObtainObject(base->hc_HeliosClass);

				/* Spawn the sbp2 unit handler task */
				sbp2_unit->u_DriverTask = (struct Task *)CreateNewProcTags(NP_CodeType,     CODETYPE_PPC,
																			NP_Name,         (ULONG)"SBP2 drive",
																			NP_Priority,     5,
																			NP_PPCStackSize, 8192,
																			NP_Entry,        (ULONG)sbp2_driver_task,
																			NP_PPC_Arg1,     (ULONG)base,
																			NP_PPC_Arg2,     (ULONG)sbp2_unit,
																			TAG_DONE);
				if (sbp2_unit->u_DriverTask)
				{
					Helios_SetAttrs(hu, HA_UserData, (ULONG)sbp2_unit->u_DriverTask, TAG_DONE);
					return TRUE;
				}
				else
					_ERR_HUNIT(hu, "can't create SBP2 unit task %s\n", buf);

				Helios_ReleaseObject(hu);
				Helios_ReleaseObject(base->hc_HeliosClass);
				sbp2_free_unit(sbp2_unit);
			}
			else
				_ERR_HUNIT(hu, "SBP2 unit alloc failed\n");
		}
		else
			_ERR_HUNIT(hu, "failed to obtain unit info\n");

		Helios_ReleaseObject(hd);
	}
	else
		_ERR_HUNIT(hu, "failed to obtain device\n");

	return FALSE;
}
LONG
sbp2_ReleaseUnitBinding(SBP2ClassLib *base, HeliosUnit *hu)
{
	struct Task *task=NULL;
	
	if (Helios_GetAttrs(hu, HA_UserData, (ULONG)&task, TAG_DONE) && task)
		Signal(task, SIGBREAKF_CTRL_C);
	
	return 0;
}
/*=== EOF ====================================================================*/