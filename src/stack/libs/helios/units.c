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
** Provide an API to bus a IEEE1394 devices ROM.
*/

#include "private.h"

/*============================================================================*/
/*--- LOCAL CODE -------------------------------------------------------------*/
/*============================================================================*/
static BOOL
unit_check_dir_crc(const QUADLET *dir)
{
	int length = dir[0] >> 16;
	int crc = utils_GetBlockCRC16((QUADLET *)&dir[1], length);
	return crc == (dir[0] & 0xFFFF);
}
static void
unit_free(HeliosUnit *hu)
{
	_DBG("freeing unit @ %p\n", hu);
	FreePooled(HeliosBase->hb_MemPool, hu, sizeof(HeliosUnit));
}
static LONG
unit_get_attribute(HeliosUnit *hu, ULONG tag, APTR storage)
{
	switch(tag)
	{
		case HA_Device:
			*(HeliosDevice **)storage = hu->hu_Device;
			_OBJ_XINCREF(hu->hu_Device);
			break;

		case HA_UnitRomDirectory:
			{
				QUADLET *dir = AllocVec(hu->hu_RomDirSize, MEMF_PUBLIC);
				if (!dir)
					return FALSE;
				CopyMem((APTR)hu->hu_RomDirectory, dir, hu->hu_RomDirSize);
				*(const QUADLET **)storage = dir;
			}
			break;
			
		case HA_UserData:
			*(APTR *)storage = hu->hu_BindUData;
			break;

		default:
			return FALSE;
	}
	
	return TRUE;
}
static LONG
unit_set_attribute(HeliosUnit *hu, ULONG tag, APTR data)
{
	switch(tag)
	{
		case HA_UserData:
			hu->hu_BindUData = data;
			break;

		default:
			return FALSE;
	}
	
	return TRUE;
}
static HeliosUnit *
unit_parse_unit_dir(const QUADLET *dir)
{
	HeliosUnit *hu = NULL;
	
	if (unit_check_dir_crc(dir))
	{
		hu = AllocPooled(HeliosBase->hb_MemPool, sizeof(HeliosUnit));
		if (hu)
		{
			Helios_ObjectInit(hu, HGA_UNIT,
				HA_ObjectFreeFunc, (ULONG)unit_free,
				HA_ObjectGetAttrFunc, (ULONG)unit_get_attribute,
				HA_ObjectSetAttrFunc, (ULONG)unit_set_attribute,
				TAG_DONE);
			hu->hu_RomDirectory = dir;
			
			/* Find the size of this directory */
			HeliosRomIterator ri;
			Helios_InitRomIterator(&ri, dir);
			hu->hu_RomDirSize = (APTR)ri.end - (APTR)dir;
			_DBG("RomDirSize=%u\n", hu->hu_RomDirSize);
		}
		else
			_ERR("AllocPooled failed\n");
	}
	else
		_WRN("Bad unit directory CRC\n");
	
	return hu;
}
static HeliosUnit *
unit_get_next_unit(HeliosRomIterator *ri)
{
	QUADLET key, value;
	HeliosUnit *hu;
	
	while (Helios_RomIterate(ri, &key, &value))
	{
		switch (key)
		{
			case KEYTYPEV_DIRECTORY | CSR_KEY_UNIT_DIRECTORY:
				_DBG("parsing unit directory...\n");
				hu = unit_parse_unit_dir(&ri->actual[value - 1]);
				if (hu)
					return hu;
				break;

#if 1 /* DEBUG */
			case KEYTYPEV_LEAF | CSR_KEY_TEXTUAL_DESCRIPTOR:
				{
					char buffer[64];
					Helios_ReadTextualDescriptor(&ri->actual[value - 1], buffer, sizeof(buffer)-1);
					buffer[sizeof(buffer)-1] = '0';
					_INF("Root descriptor: %s\n", buffer);
				}
				break;

			case KEYTYPEV_IMMEDIATE | CSR_KEY_MODEL_ID:
				_DBG("Model ID: %06X\n", value);
				break;

			case KEYTYPEV_IMMEDIATE | CSR_KEY_NODE_CAPABILITIES:
				_DBG("Node Capabilities: %06X\n", value);
				break;
				
			case KEYTYPEV_IMMEDIATE | CSR_KEY_MODULE_VENDOR_ID:
				_DBG("Module VendorID: %06X\n", value);
				break;

			default:
				_DBG("unhandled key: %02X-%06X\n", key, value);
#endif
		}
	}
	
	return NULL;
}
/*============================================================================*/
/*--- EXPORTED API -----------------------------------------------------------*/
/*============================================================================*/
void
_Helios_GenUnits(HeliosHardware *hh)
{
	/* For all connected device, parse ROM to find logical units
	 * Create a Unit object if found.
	 * Search if a classes want to handle it.
	 */
	READ_LOCK_LIST(hh->hh_Devices);
	{
		HeliosDevice *hd;

		ForeachNode(&hh->hh_Devices, hd)
		{
			SHARED_PROTECT_BEGIN(&hd->hd_Base);
			{
				if (hd->hd_UnitScan)
				{
					hd->hd_UnitScan = FALSE;
					
					if (hd->hd_ROM && (hd->hd_ROMLength > ROM_ROOT_DIR_OFFSET))
					{
						QUADLET *dir = hd->hd_ROM + ROM_ROOT_DIR_OFFSET;
						HeliosUnit *hu;

						/* Check CRC: we don't process invalid ROM */
						if (unit_check_dir_crc(dir))
						{
							HeliosRomIterator ri;

							Helios_InitRomIterator(&ri, dir);
							do
							{
								hu = unit_get_next_unit(&ri);
								if (hu)
									_Helios_AttachUnit(hd, hu);
							}
							while (hu);
						}
						else
							_WRN("Bad unit directory CRC\n");
					}
					else
						_WRN("small or no ROM (%p-%u)\n", hd->hd_ROM, hd->hd_ROMLength);
				}
			}
			SHARED_PROTECT_END(&hd->hd_Base);
			
			/* Units list complete, do binding now */
			_Helios_AttemptDeviceUnitsBinding(hd);
		}
	}
	UNLOCK_LIST(hh->hh_Devices);
}
void
_Helios_DetachUnit(HeliosUnit *hu)
{
	/* WARNING: must be called with owner list write locked */
	
	_INF("Disconnecting unit %p from device %p\n", hu, hu->hu_Device);
	
	REMOVE(hu);
	
	EXCLUSIVE_PROTECT_BEGIN(&hu->hu_Base);
	{
		HeliosClass *hc = hu->hu_BindClass; /* symbol needed for HeliosClass_DoMethod() */
		
		if (hc)
		{
			HeliosClass_DoMethod(HCM_ReleaseUnitBinding, (ULONG)hu);
			SAFE_REMOVE(hu->hu_BindClass->hc_Units, (struct Node *)&hu->hu_BindNode);
			Helios_ReleaseObject(hu);
			Helios_ReleaseObject(hc);
		}
		
		hu->hu_Device = NULL;
		hu->hu_RomDirectory = NULL;
		hu->hu_BindClass = NULL;
		/* DON'T CLEAR: hu->hu_BindUData = NULL; */
	}
	EXCLUSIVE_PROTECT_END(&hu->hu_Base);
	
	Helios_ReleaseObject(hu);
}
void
_Helios_AttemptUnitBinding(HeliosUnit *hu)
{
	HeliosClass *hc;
	
	EXCLUSIVE_PROTECT_BEGIN(&hu->hu_Base);
	{
		if (hu->hu_Device)
		{
			if (!hu->hu_BindClass)
			{
				/* Loop on all registered classes
				 * and try to bind it on the unit.
				 */
				READ_LOCK_LIST(HeliosBase->hb_Classes);
				{
					ForeachNode(&HeliosBase->hb_Classes, hc)
					{
						if (HeliosClass_DoMethod(HCM_AttemptUnitBinding, (ULONG)hu))
						{
							if (_OBJ_INCREF(hc))
							{
								hu->hu_BindClass = hc;
								SAFE_ADDTAIL(hc->hc_Units, (struct Node *)&hu->hu_BindNode);
								_OBJ_INCREF(hu);
								
								_INF("Unit @%p (dev $%016llx) is bind on class @%p '%s'\n",
									hu, hu->hu_Device->hd_GUID, hc, hc->hc_Base.hso_Node.ln_Name);
								break;
							}
						}
					}
				}
				UNLOCK_LIST(HeliosBase->hb_Classes);
			}
			else
				_WRN("Unit @%p (dev $%016llx) already bind on class @%p '%s'\n",
					hu, hu->hu_Device->hd_GUID, hu->hu_BindClass, hu->hu_BindClass->hc_Base.hso_Node.ln_Name);
		}
		else
			_WRN("Trying to bind non attached unit @%p\n", hu);
	}
	EXCLUSIVE_PROTECT_END(&hu->hu_Base);
}
/*=== EOF ====================================================================*/