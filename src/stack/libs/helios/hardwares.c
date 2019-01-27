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
** Hardwares related API
*/

#include "private.h"
#include "topology.h"

#include <proto/query.h>
#include <proto/alib.h>
#include <proto/timer.h>

#include <hardware/byteswap.h>
#include <string.h> /* bzero */

#define PHY_CONFIG_GAP_COUNT(gap_count)	(((gap_count) << 16) | (1 << 22))
#define PHY_CONFIG_ROOT_ID(node_id)		((((node_id) & 0x3f) << 24) | (1 << 23))
#define PHY_IDENTIFIER(id)				((id) << 30)

#define PHY_PACKET_CONFIG		0x0
#define PHY_PACKET_LINK_ON		0x1
#define PHY_PACKET_SELF_ID		0x2

/*--- debugging --------------------------------------------------------------*/
#define _INF_HW(hh, fmt, args...) _INF("{hh-%d} " fmt, (hh)->hh_UnitNo, ## args)
#define _WRN_HW(hh, fmt, args...) _WRN("{hh-%d} " fmt, (hh)->hh_UnitNo, ## args)
#define _ERR_HW(hh, fmt, args...) _ERR("{hh-%d} " fmt, (hh)->hh_UnitNo, ## args)
#define _DBG_HW(hh, fmt, args...) _DBG("{hh-%d} " fmt, (hh)->hh_UnitNo, ## args)

/*--- local variables---------------------------------------------------------*/
static const UBYTE gHeliosGapCountTable[] =
{
	63, 5, 7, 8, 10, 13, 16, 18, 21, 24, 26, 29, 32, 35, 37, 40
};

/*============================================================================*/
/*--- LOCAL API --------------------------------------------------------------*/
/*============================================================================*/
static LONG
helios_disable_hw(HeliosHardware *hh)
{
	struct IORequest ioreq;
	
	ioreq.io_Message.mn_Length = sizeof(ioreq);
	ioreq.io_Command = HHIOCMD_DISABLE;
	
	return Helios_DoIO(hh, &ioreq);
}
static LONG
helios_bus_reset(HeliosHardware *hh, ULONG shortreset)
{
	struct IOStdReq ioreq;
	
	ioreq.io_Message.mn_Length = sizeof(ioreq);
	ioreq.io_Command = HHIOCMD_BUSRESET;
	ioreq.io_Data = (APTR)shortreset;
	
	return Helios_DoIO(hh, (struct IORequest *)&ioreq);
}
static BOOL
helios_gen_new_topo(HeliosHardware *hh, UBYTE gen)
{
	if (!_Helios_QueryHWDev(hh, HHA_SelfID, &hh->hh_SelfIDStream))
	{
		_ERR_HW(hh, "Failed to query SelfID stream\n");
		return FALSE;
	}
	
	if (gen != hh->hh_SelfIDStream.hss_Generation)
	{
		_WRN_HW(hh, "Topo creation failed, generation mismatch (%u != %u)\n",
			gen, hh->hh_SelfIDStream.hss_Generation);
		return FALSE;
	}
	
	return _Helios_UpdateTopologyMapping(hh);
}
static LONG
helios_process_bm(HeliosHardware *hh)
{
	/* Returns 1 if BM is stable (no BR trigged), else 0 */
	
	HeliosTopology *topo = hh->hh_Topology;
	struct IOStdReq ioreq;
	HeliosPacket p;
	UBYTE maxhops, gap_count;
	UBYTE new_root_phyid;
	QUADLET data[2];
	LONG ioerr;

retry:
	/* No IRM node? Try to be root */
	if ((topo->ht_IRMNodeID != -1) &&
		!topo->ht_Nodes[topo->ht_IRMNodeID].hn_Flags.IsLinkOn)
	{
		_DBG_HW(hh, "No valid IRM, try to be root\n");
		new_root_phyid = topo->ht_LocalNodeID;
		goto set_config;
	}

	/* Ask IRM who is the BM node */
	ioreq.io_Message.mn_Length = sizeof(ioreq);
	ioreq.io_Command = HHIOCMD_SENDREQUEST;
	ioreq.io_Data = &p;
	ioreq.io_Length = sizeof(p);

	data[0] = LE_SWAPLONG_C(0x3f);
	data[1] = LE_SWAPLONG(topo->ht_LocalNodeID);
	
	Helios_FillLockPacket(&p, S100,
		CSR_BASE_LO + CSR_BUS_MANAGER_ID,
		EXTCODE_COMPARE_SWAP, data, 8);
	p.DestID = HELIOS_LOCAL_BUS | topo->ht_IRMNodeID;
	p.Generation = topo->ht_Generation;

	ioerr = Helios_DoIO(hh, (struct IORequest *)&ioreq);
	if (ioerr)
	{
		/* Bus reset occured? Retry 125ms later */
		if ((ioerr == HHIOERR_RCODE) &&
			(p.RCode == HELIOS_RCODE_GENERATION))
		{
			_DBG_HW(hh, "BusReset, retry 125ms later...\n");
			Helios_DelayMS(125);
			goto retry;
		}

		/* Others errors => try to be root */
		new_root_phyid = topo->ht_LocalNodeID;
		goto set_config;
	}

	_DBG_HW(hh, "IRM lock(BUS_MANAGER_ID): $%08X\n", data[0]);
	if (data[0] != LE_SWAPLONG_C(0x3f))
	{
		_INF_HW(hh, "Someone else is BM, act only as IRM\n");
		if (topo->ht_LocalNodeID == topo->ht_IRMNodeID)
		{
			/* TODO: set broadcast channel */
		}

		return 1; /* stable */
	}
	
	_INF_HW(hh, "Local node is BM\n");

	/* Fallback on local node for root if one in topo is not linked */
	if (!topo->ht_Nodes[topo->ht_RootNodeID].hn_Flags.IsLinkOn)
		new_root_phyid = topo->ht_LocalNodeID;
	else
		new_root_phyid = topo->ht_RootNodeID;

set_config:
	/* find max hops from root node */
	maxhops = topo->ht_Nodes[topo->ht_RootNodeID].hn_MaxHops;
	if (maxhops >= sizeof(gHeliosGapCountTable)/sizeof(gHeliosGapCountTable[0]))
		maxhops = 0;

	/* Try 5 times to optimize GAP Count */
	gap_count = gHeliosGapCountTable[maxhops];
	if ((hh->hh_BMRetry++ < 5) &&
		((gap_count != topo->ht_GapCount) ||
		 (new_root_phyid != topo->ht_RootNodeID)))
	{
		_DBG_HW(hh, "Set node %u as root, gapCount=%u\n", new_root_phyid, gap_count);
		ioreq.io_Message.mn_Length = sizeof(ioreq);
		ioreq.io_Command = HHIOCMD_SENDPHY;
		ioreq.io_Data = (APTR)(
			PHY_IDENTIFIER(PHY_PACKET_CONFIG) |
			PHY_CONFIG_ROOT_ID(new_root_phyid) |
			PHY_CONFIG_GAP_COUNT(gap_count));
		Helios_DoIO(hh, (struct IORequest *)&ioreq);
		helios_bus_reset(hh, TRUE);
		return 0;
	}
	else
	{
		hh->hh_BMRetry = 0;
		
		if (topo->ht_LocalNodeID == topo->ht_IRMNodeID)
		{
			/* TODO: set broadcast channel */
		}
	}
	
	return 1; /* stable */
}
static LONG
helios_add_hardware(HeliosHardware *hh)
{
	LONG count = -1;
	BOOL found = FALSE;
	
	WRITE_LOCK_LIST(HeliosBase->hb_Hardwares);
	{
		HeliosHardware *node;
		
		ForeachNode(&HeliosBase->hb_Hardwares, node)
		{
			found = (node->hh_Unit == hh->hh_Unit) &&
					(node->hh_UnitNo == hh->hh_UnitNo);
		}
		
		if (!found)
		{
			/* Hardware object ready, publish it */
			ADDTAIL(&HeliosBase->hb_Hardwares, hh);
			
			count = HeliosBase->hb_HwCount++;
			/* done at init: HW_INCREF(hh); */
			_INF_HW(hh, "New object @%p\n", hh);
		}
	}
	UNLOCK_LIST(HeliosBase->hb_Hardwares);
	
	return count;
}
static void
helios_remove_hardware(HeliosHardware *hh)
{
	WRITE_LOCK_LIST(HeliosBase->hb_Hardwares);
	{
		REMOVE(hh);
		HeliosBase->hb_HwCount--;
		_INF_HW(hh, "Removed\n");
	}
	UNLOCK_LIST(HeliosBase->hb_Hardwares);
}
static void
helios_free_hardware(HeliosHardware *hh)
{
	_DBG_HW(hh, "Freeing (Empty=%d,%d)\n",
		IsListEmpty((struct List *)&hh->hh_Devices),
		IsListEmpty((struct List *)&hh->hh_DeadDevices));
	FreePooled(HeliosBase->hb_MemPool, hh, sizeof(HeliosHardware));
}
static LONG
helios_hardware_get_attribute(HeliosHardware *hh, ULONG tag, APTR data)
{
	switch(tag)
	{
		case HA_EventListenerList:
			*(HeliosEventListenerList **)data = &hh->hh_Listeners;
			break;

		case HA_NodeID:
			*(UWORD *)data = hh->hh_LocalNodeId;
			break;
			
		case HA_Generation:
			if (hh->hh_Topology)
				*(LONG *)data = hh->hh_Topology->ht_Generation;
			else
				return FALSE;
			break;

		default: /* Forward request to hw device */
			if (!_Helios_QueryHWDev(hh, tag, (APTR)data))
				return FALSE;
	}
	
	return TRUE;
}
static void
helios_hardware_task(HeliosSubTask *hwtaskctx, struct TagItem *tags)
{
	struct HeliosBase *tmpheliosbase;
	HeliosHardware *hh;
	struct MsgPort *taskport, *hhport;
	STRPTR name;
	ULONG sigs;
	LONG unitNo, count;
	struct IOStdReq *iobase = NULL;
	HeliosEventMsg myListener;
	BOOL run;
	HeliosMsg *msg;
	HeliosDevice *dev;
	APTR next;
	char portPubName[64];
	
	taskport = (APTR) GetTagData(HA_MsgPort, 0, tags);
	name = (APTR) GetTagData(HA_Device, 0, tags);
	unitNo = GetTagData(HA_Unit, -1, tags);

	if (!TypeOfMem(taskport) || !TypeOfMem(name) || (unitNo < 0))
	{
		_ERR("bad tags (taskport: %p, name: %p, UnitNo: %u)\n", taskport, name, unitNo);
		return;
	}
	
	/* Just to increase the Helios library open counter, as the global HeliosBase is already set */
	tmpheliosbase = utils_SafeOpenLibrary(HELIOS_LIBNAME, HELIOS_LIBVERSION);
	if (!tmpheliosbase)
		return;
	
	hh = AllocPooled(HeliosBase->hb_MemPool, sizeof(HeliosHardware));
	if (!hh)
	{
		_ERR("Hardware object allocation failed\n");
		goto close_hbase;
	}
	
	Helios_ObjectInit(hh, HGA_HARDWARE,
		HA_ObjectName, (ULONG)name,
		HA_ObjectFreeFunc, (ULONG)helios_free_hardware,
		HA_ObjectGetAttrFunc, (ULONG)helios_hardware_get_attribute,
		TAG_DONE);

	INIT_LOCKED_LIST(hh->hh_Devices);
	INIT_LOCKED_LIST(hh->hh_DeadDevices);
	NEWLIST(&hh->hh_Listeners);
	LOCK_INIT(&hh->hh_Listeners);
	
	hh->hh_HWTask = hwtaskctx;
	hh->hh_UnitNo = unitNo;
	
	/* This port is used for HW device communication */
	hhport = CreateMsgPort();
	if (!hhport)
	{
		_ERR_HW(hh, "CreateMsgPort() failed\n");
		goto release_hh;
	}
	
	iobase = (APTR)CreateExtIO(hhport, sizeof(struct IOStdReq));
	if (!iobase)
	{
		_ERR_HW(hh, "CreateExtIO() failed\n");
		goto delete_hhport;
	}
	
	if (OpenDevice(name, unitNo, (struct IORequest *)iobase, 0))
	{
		_ERR_HW(hh, "OpenDevice() failed\n");
		goto delete_hhiobase;
	}
	
	hh->hh_UnitDevice = iobase->io_Device;
	hh->hh_Unit = iobase->io_Unit;

	count = helios_add_hardware(hh);
	if (count < 0)
	{
		_ERR_HW(hh, "Already managed\n");
		goto close_hhdevice;
	}
	
	/* Add this task as lister on SelfID events */
	bzero(&myListener, sizeof(myListener));
	myListener.hm_Msg.mn_ReplyPort = taskport;
	myListener.hm_Msg.mn_Length = sizeof(myListener);
	myListener.hm_Type = HELIOS_MSGTYPE_EVENT;
	myListener.hm_EventMask = HEVTF_HARDWARE_SELFID;
	
	HeliosEventListenerList *listenersList = NULL;
	if (!_Helios_QueryHWDev(hh, HA_EventListenerList, &listenersList))
	{
		_ERR_HW(hh, "get listeners list failed\n");
		goto remove_hh;
	}
	
	Helios_AddEventListener(listenersList, &myListener);
	
	run = TRUE;
	do
	{
		int newTopoGen = -1;
		
		/* wait for HW events or subtask messages */
		sigs = Wait((1 << taskport->mp_SigBit) | SIGBREAKF_CTRL_C);
		if (sigs & SIGBREAKF_CTRL_C)
		{
			_WRN_HW(hh, "*BREAK*\n");
			break;
		}
		
		/* process messages */
		while (NULL != (msg = (APTR)GetMsg(taskport)))
		{
			switch (msg->hm_Type)
			{
				case HELIOS_MSGTYPE_INIT:
					_DBG_HW(hh, "Init msg rcv\n");
					
					/* Make taskport public */
					utils_SafeSPrintF(portPubName, sizeof(portPubName), "HELIOS.%ld", count);
					taskport->mp_Node.ln_Name = portPubName;
					AddPort(taskport);
					
					/* Everything are installed
					 * Raise a bus reset to construct topology and do the BM job.
					 */
					if (helios_bus_reset(hh, TRUE))
						_ERR_HW(hh, "Short bus reset request failed\n");
					
					msg->hm_Result = (LONG)taskport;
					break;

				case HELIOS_MSGTYPE_DIE:
					_WRN_HW(hh, "Die msg rcv\n");
					run = FALSE;
					FreeMem(msg, msg->hm_Msg.mn_Length);
					continue;
					
				case HELIOS_MSGTYPE_EVENT:
					_DBG_HW(hh, "Event msg rcv (res=%d)\n", msg->hm_Result);
					newTopoGen = msg->hm_Result;
					FreeMem(msg, msg->hm_Msg.mn_Length);
					continue;
			}
			
			ReplyMsg((struct Message *)msg);
		}
		
		/* Create new topo, then process the BM job if new topo generated */
		if (run && newTopoGen != -1)
		{
			if (helios_gen_new_topo(hh, newTopoGen) && helios_process_bm(hh))
			{
				_Helios_GenUnits(hh);
			}
		}
	}
	while (run);

	if (taskport->mp_Node.ln_Name);
		RemPort(taskport);

	Helios_RemoveEventListener(listenersList, &myListener);

remove_hh:
	helios_remove_hardware(hh);
close_hhdevice:
	CloseDevice((struct IORequest *)iobase);

	ForeachNodeSafe(&hh->hh_Devices, dev, next)
		_Helios_DisconnectDevice(dev);

	_Helios_FreeTopology(hh);

	ForeachNodeSafe(&hh->hh_DeadDevices, dev, next)
	{
		REMOVE(dev);
		Helios_ReleaseObject(dev);
	}
	
	while (NULL != (msg = (APTR)GetMsg(taskport)))
	{
		switch (msg->hm_Type)
		{
			case HELIOS_MSGTYPE_DIE:
			case HELIOS_MSGTYPE_EVENT:
				FreeMem(msg, msg->hm_Msg.mn_Length);
				break;
				
			default:
				ReplyMsg((struct Message *)msg);
		}
	}

delete_hhiobase:
	DeleteExtIO((struct IORequest *)iobase);
delete_hhport:
	DeleteMsgPort(hhport);
release_hh:
	Helios_ReleaseObject(hh);
close_hbase:
	CloseLibrary((APTR)tmpheliosbase);
	FreeVec(name);
}
/*============================================================================*/
/*--- EXPORTED API -----------------------------------------------------------*/
/*============================================================================*/
LONG
_Helios_QueryHWDev(HeliosHardware *hh, ULONG attr, APTR result)
{
	HeliosHWQuery hhq;

	hhq.hhq_Unit = hh->hh_Unit;
	hhq.hhq_Data = result;

	return QueryGetDeviceAttr(hh->hh_UnitDevice, &hhq, attr);
}
/*============================================================================*/
/*--- PUBLIC API -------------------------------------------------------------*/
/*============================================================================*/
void
Helios_SendIO(HeliosHardware *hh, struct IORequest *ioreq)
{
	ioreq->io_Device = hh->hh_UnitDevice;
	ioreq->io_Unit = hh->hh_Unit;
	ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
	
	SendIO((struct IORequest *)ioreq);
}
LONG
Helios_DoIO(HeliosHardware *hh, struct IORequest *ioreq)
{
	struct MsgPort port;
	BYTE sigbit;
	LONG err;
	
	sigbit = AllocSignal(-1);
	if (sigbit < 0)
		return HHIOERR_NOMEM;

	/* Build a on-stack signal msg port */
	port.mp_Flags	= PA_SIGNAL;
	port.mp_SigBit	= sigbit;
	port.mp_SigTask	= FindTask(NULL);
	NEWLIST(&port.mp_MsgList);

	ioreq->io_Device = hh->hh_UnitDevice;
	ioreq->io_Unit = hh->hh_Unit;
	ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
	ioreq->io_Message.mn_ReplyPort = &port;

	err = DoIO((struct IORequest *)ioreq);
	
	FreeSignal(sigbit);
	return err;
}
struct MsgPort *
Helios_AddHardware(STRPTR name, LONG unit)
{
	char procname[64];
	struct TagItem tags[] = {
		{HA_Device, 0},
		{HA_Unit, (ULONG)unit},
		{TASKTAG_PRI, 5},
		{TAG_END, 0},
	};
	HeliosSubTask *subtask;
	HeliosMsg msg;
	STRPTR copy_of_name;
	ULONG length = strlen(name);
	
	if (!length)
	{
		_ERR("Empty name\n");
		return NULL;
	}
	
	copy_of_name = AllocVec(length + 1, MEMF_PUBLIC);
	if (!copy_of_name)
	{
		_ERR("Name duplication failed\n");
		return NULL;
	}
	
	CopyMem(name, copy_of_name, length + 1);
	tags[0].ti_Data = (ULONG)copy_of_name;
	
	utils_SafeSPrintF(procname, sizeof(procname), "helioshw <%s/%ld>", FilePart(copy_of_name), unit);
	subtask = Helios_CreateSubTaskA(procname, (HeliosSubTaskEntry)helios_hardware_task, tags);
	if (!subtask)
	{
		FreeVec(copy_of_name);
		_ERR("Failed to create hw task '%s'\n", procname);
		return NULL;
	}
	
	/* Wait for hardware object creation */
	msg.hm_Msg.mn_Length = sizeof(msg);
	msg.hm_Msg.mn_Node.ln_Type = NT_MESSAGE;
	msg.hm_Type = HELIOS_MSGTYPE_INIT;
	msg.hm_Result = 0;
	
	if ((Helios_DoMsgToSubTask(subtask, &msg.hm_Msg, NULL) != HERR_NOERR) || (msg.hm_Result == 0))
	{
		Helios_KillSubTask(subtask, TRUE);
		return NULL;
	}
	
	return (struct MsgPort *)msg.hm_Result;
}
/*=== EOF ====================================================================*/