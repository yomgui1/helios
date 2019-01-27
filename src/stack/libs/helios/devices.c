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
** Low-level API to handle 1394 devices.
*/

#include "private.h"

/*--- debugging --------------------------------------------------------------*/
#define _INF_DEV(dev, fmt, args...) _INF("{dev-%016llX} " fmt, (dev)->hd_GUID, ## args)
#define _WRN_DEV(dev, fmt, args...) _WRN("{dev-%016llX} " fmt, (dev)->hd_GUID, ## args)
#define _ERR_DEV(dev, fmt, args...) _ERR("{dev-%016llX} " fmt, (dev)->hd_GUID, ## args)
#define _DBG_DEV(dev, fmt, args...) _DBG("{dev-%016llX} " fmt, (dev)->hd_GUID, ## args)

#define _INF_NODE(node, fmt, args...) _INF("{node-%u} " fmt, (node)->hn_PhyID, ## args)
#define _WRN_NODE(node, fmt, args...) _WRN("{node-%u} " fmt, (node)->hn_PhyID, ## args)
#define _ERR_NODE(node, fmt, args...) _ERR("{node-%u} " fmt, (node)->hn_PhyID, ## args)
#define _DBG_NODE(node, fmt, args...) _DBG("{node-%u} " fmt, (node)->hn_PhyID, ## args)

/*============================================================================*/
/*--- LOCAL API --------------------------------------------------------------*/
/*============================================================================*/
static LONG
helios_device_get_attribute(HeliosDevice *hd, ULONG tag, APTR data)
{
	switch(tag)
	{
		case HA_EventListenerList:
			*(HeliosEventListenerList **)data = &hd->hd_Listeners;
			break;

		case HA_GUID:
			*(UQUAD *)data = hd->hd_GUID;
			break;
			
		case HA_GUID_Hi:
			*(ULONG *)data = hd->hd_GUID_Hi;
			break;
			
		case HA_GUID_Lo:
			*(ULONG *)data = hd->hd_GUID_Lo;
			break;
		
		case HA_NodeInfo:
			if (hd->hd_Node)
			{
				SHARED_PROTECT_BEGIN(&hd->hd_Hardware->hh_Base);
				CopyMemQuick(hd->hd_Node, data, sizeof(HeliosNode));
				SHARED_PROTECT_END(&hd->hd_Hardware->hh_Base);
			}
			else
				((HeliosNode *)data)->hn_Device = NULL;
			break;

		case HA_Generation:
			*(LONG *)data = hd->hd_Generation;
			break;

		case HA_NodeID:
			*(UWORD *)data = hd->hd_NodeID;
			break;

		case HA_Hardware:
			*(HeliosHardware **)data = hd->hd_Hardware;
			_OBJ_XINCREF(hd->hd_Hardware);
			break;
			
		case HA_RomLength:
			if (hd->hd_ROM)
				*(ULONG *)data = hd->hd_ROMLength;
			else
				*(ULONG *)data = 0;
			break;

		case HA_Rom:
			if (hd->hd_ROM)
			{
				_DBG_DEV(hd, "Copy %u bytes from %p to %p\n", hd->hd_ROMLength, hd->hd_ROM, data);
				CopyMem(hd->hd_ROM, (QUADLET *)data, hd->hd_ROMLength);
			}
			break;
	}
	
	return TRUE;
}
static void
helios_free_device(HeliosDevice *hd)
{
	_DBG_DEV(hd, "Freeing (Empty=%d)\n", IsListEmpty((struct List *)&hd->hd_Units));
	if (hd->hd_ROM)
		FreePooled(HeliosBase->hb_MemPool, hd->hd_ROM, hd->hd_ROMLength * sizeof(QUADLET));
	FreePooled(HeliosBase->hb_MemPool, hd, sizeof(HeliosDevice));
}
static HeliosDevice *
dev_create(QUAD guid)
{
	HeliosDevice *dev;
	
	dev = AllocPooled(HeliosBase->hb_MemPool, sizeof(HeliosDevice));
	if (!dev)
	{
		_ERR("Device allocation failed\n");
		return NULL;
	}
	
	Helios_ObjectInit(dev, HGA_DEVICE,
		HA_ObjectName, (ULONG)"Device",
		HA_ObjectFreeFunc, (ULONG)helios_free_device,
		HA_ObjectGetAttrFunc, (ULONG)helios_device_get_attribute,
		TAG_DONE);

	INIT_LOCKED_LIST(dev->hd_Units);
	NEWLIST(&dev->hd_Listeners);
	LOCK_INIT(&dev->hd_Listeners);
	dev->hd_GUID = guid;
	_DBG_DEV(dev, "New device (@%p)\n", dev);
	
	return dev;
}
static void
dev_disconnect(HeliosDevice *dev)
{
	HeliosHardware *hh = dev->hd_Hardware;

	SAFE_REMOVE(hh->hh_Devices, dev);

	WRITE_LOCK_LIST(dev->hd_Units);
	{
		HeliosUnit *unit, *next;
		ForeachNodeSafe(&dev->hd_Units, unit, next)
			_Helios_DetachUnit(unit);
	}
	UNLOCK_LIST(dev->hd_Units);

	EXCLUSIVE_PROTECT_BEGIN(&dev->hd_Base);
	{
		if (dev->hd_Node)
		{
			_INF_DEV(dev, "Disconnecting from node %u\n", dev->hd_Node->hn_PhyID);
			dev->hd_Node->hn_Device = NULL;
			dev->hd_Node = NULL;
			dev->hd_NodeID = -1;
		}

		_OBJ_CLEAR(dev->hd_Hardware);
		dev->hd_Generation = -1;
	}
	EXCLUSIVE_PROTECT_END(&dev->hd_Base);
	
	SAFE_ADDHEAD(hh->hh_DeadDevices, dev);
}
static void
dev_connect(HeliosDevice *dev, HeliosHardware *hh, HeliosNode *node, BOOL append)
{
	EXCLUSIVE_PROTECT_BEGIN(&dev->hd_Base);
	{
		/* incref new HW before decref old one
		 * as the new one can be the old one also
		 */
		_OBJ_INCREF(hh);
		_OBJ_XDECREF(dev->hd_Hardware);
		dev->hd_Hardware = hh;
		dev->hd_Node = node;
		dev->hd_NodeID = node->hn_PhyID | HELIOS_LOCAL_BUS;
		dev->hd_Generation = hh->hh_Topology->ht_Generation;
	}
	EXCLUSIVE_PROTECT_END(&dev->hd_Base);
	
	node->hn_Device = dev;
	_INF_DEV(dev, "Connected to node %u\n", node->hn_PhyID);
	
	if (append)
		SAFE_ADDTAIL(hh->hh_Devices, dev);
}
static void
dev_update_rom(HeliosDevice *dev, QUADLET *rom, ULONG romLength)
{
	/* NOTE. romLength in QUADLET unit */
	EXCLUSIVE_PROTECT_BEGIN(&dev->hd_Base);
	{
		if (dev->hd_ROMLength != romLength)
		{
			FreePooled(HeliosBase->hb_MemPool, dev->hd_ROM, dev->hd_ROMLength * sizeof(QUADLET));
			dev->hd_ROM = NULL;
		}
		
		if (!dev->hd_ROM)
		{
			dev->hd_ROM = AllocPooled(HeliosBase->hb_MemPool, romLength * sizeof(QUADLET));
			if (!dev->hd_ROM)
			{
				_ERR_DEV(dev, "ROM alloc failed (%lu bytes)\n", romLength);
				goto out;
			}
		}
		
		CopyMem(rom, dev->hd_ROM, romLength * sizeof(QUADLET));
		dev->hd_ROMLength = romLength;
		dev->hd_UnitScan = TRUE;
		_DBG_DEV(dev, "ROM updated\n");
out:
		;
	}
	EXCLUSIVE_PROTECT_END(&dev->hd_Base);
}
static UQUAD
node_get_guid(HeliosHardware *hh, HeliosNode *node, QUADLET *rom, ULONG *romLengthPtr)
{
	LONG err;

	err = Helios_ReadROM(hh,
		HELIOS_LOCAL_BUS | node->hn_PhyID,
		hh->hh_Topology->ht_Generation,
		node->hn_MaxSpeed,
		rom, romLengthPtr);
	
	if (err || *romLengthPtr < 5)
		return 0;

	return (((UQUAD)rom[3]) << 32) + rom[4];
}
/*============================================================================*/
/*--- EXPORTED API -----------------------------------------------------------*/
/*============================================================================*/
void
_Helios_OnCreatedNode(HeliosHardware *hh, HeliosNode *node)
{
	/* nothing special */
}
void
_Helios_OnUpdatedNode(HeliosHardware *hh, HeliosNode *previous, HeliosNode *current)
{
	HeliosDevice *dev = previous->hn_Device;
	
	if (!dev)
		return;
	
	dev_connect(dev, hh, current, FALSE);
}
void
_Helios_OnRemovedNode(HeliosHardware *hh, HeliosNode *node)
{
	HeliosDevice *dev = node->hn_Device;
	
	if (!dev)
		return;
		
	dev_disconnect(dev);
}

void
_Helios_ScanNode(HeliosHardware *hh, HeliosNode *node)
{
	HeliosDevice *dev;
	UQUAD guid;
	QUADLET rom[CSR_CONFIG_ROM_SIZE];
	ULONG romLength=CSR_CONFIG_ROM_SIZE;
	
	/* First try to read ROM of given node,
	 * then check if given node is connected to a device.
	 * If yes, the device's GUID is verified and
	 * if not corresponding, the device is disconnected.
	 * If corresponding, we just return.
	 * If the node is not connected, we create a new device
	 * and connect it to the node.
	 *
	 * Note: node access is safe as we're inside the topology creation phase
	 */
	
	dev = node->hn_Device;
	
	guid = node_get_guid(hh, node, rom, &romLength);
	if (guid > 0)
	{
		_DBG_NODE(node, "GUID=%016llX\n", guid);
		
		if (dev)
		{
			/* Node and device are already connected, no ROM update */
			if (dev->hd_GUID == guid)
				return;

			_WRN_NODE(node, "GUID changed (was $%016llX)!\n", dev->hd_GUID);
			dev_disconnect(dev);
		}
		
		WRITE_LOCK_LIST(hh->hh_DeadDevices);
		{
			ForeachNode(&hh->hh_DeadDevices, dev)
			{
				if (dev->hd_GUID == guid)
				{
					REMOVE(dev);
					goto connect_dev;
				}
			}
		}
		UNLOCK_LIST(hh->hh_DeadDevices);
	}
	else
	{
		_WRN_NODE(node, "No GUID\n");
		if (dev)
			dev_disconnect(dev);
		return;
	}

	dev = dev_create(guid);
	if (!dev)
		return;

connect_dev:
	dev_update_rom(dev, rom, romLength);
	dev_connect(dev, hh, node, TRUE);
}
void
_Helios_DisconnectDevice(HeliosDevice *dev)
{
	dev_disconnect(dev);
}
void
_Helios_FlushDeadDevices(HeliosHardware *hh)
{
	WRITE_LOCK_LIST(hh->hh_DeadDevices);
	{
		HeliosDevice *dev, *next;
		ForeachNodeSafe(&hh->hh_DeadDevices, dev, next)
		{
			REMOVE(dev);
			Helios_ReleaseObject(dev);
		}
	}
	UNLOCK_LIST(hh->hh_DeadDevices);
}
void
_Helios_AttachUnit(HeliosDevice *hd, HeliosUnit *hu)
{
	hu->hu_Device = hd; /* dev not incref here as is a parent */

	WRITE_LOCK_LIST(hd->hd_Units);
	ADDTAIL(&hd->hd_Units, hu); /* eat refcnt */
	UNLOCK_LIST(hd->hd_Units);
	
	_INF_DEV(hd, "new unit %p\n", hu);
}
void
_Helios_AttemptDeviceUnitsBinding(HeliosDevice *hd)
{
	READ_LOCK_LIST(hd->hd_Units);
	{
		HeliosUnit *hu;
		
		ForeachNode(&hd->hd_Units, hu)
		{
			_Helios_AttemptUnitBinding(hu);
		}
	}
	UNLOCK_LIST(hd->hd_Units);
}
/*=== EOF ====================================================================*/