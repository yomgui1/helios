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
** Provide an API to handle various internal structures.
*/

/* General note:
 * HeliosBase lock is used to protect objects changes (hw/dev/unit).
 * DOC: call Helios_ReleaseObject ouside of locked regions.
 */

#include "private.h"
#include "clib/heliosclsbase_protos.h"

/*============================================================================*/
/*--- PUBLIC API CODE --------------------------------------------------------*/
/*============================================================================*/
void
Helios_ObjectInitA(APTR obj, LONG type, struct TagItem *tags)
{
	_HeliosSharedObject *hso = obj;
	
	LOCK_INIT(hso);
	hso->hso_Node.ln_Type = NT_USER;
	hso->hso_Node.ln_Pri = GetTagData(HA_Priority, 0, tags);
	hso->hso_Node.ln_Name = (STRPTR)GetTagData(HA_ObjectName, (ULONG)"HeliosObject", tags);
	hso->hso_RefCnt = 1; /* or first INCREF can't work */
	hso->hso_Type = type;
	hso->hso_Free = (APTR)GetTagData(HA_ObjectFreeFunc, 0, tags);
	hso->hso_GetAttr = (APTR)GetTagData(HA_ObjectGetAttrFunc, 0, tags);
	hso->hso_SetAttr = (APTR)GetTagData(HA_ObjectSetAttrFunc, 0, tags);
}
APTR
Helios_FindObjectA(LONG type, APTR obj, struct TagItem *tags)
{
	APTR result=NULL;
	_HeliosSharedObject *_obj = obj;
	
	/* We suppose that all user given objects has been incref'ed by the caller */
	
	switch (type)
	{
		case HGA_HARDWARE:
			/* Search by UnitNo */
			READ_LOCK_LIST(HeliosBase->hb_Hardwares);
			{
				if (!_obj)
				{
					/* Find by unit number (or first one) */
					ULONG unitno = GetTagData(HA_UnitNo, 0, tags);
					HeliosHardware *node;
						
					if (unitno < HeliosBase->hb_HwCount)
					{
						ForeachNode(&HeliosBase->hb_Hardwares, node)
						{
							if (unitno-- == 0)
							{
								result = node;
								break;
							}
						}
					}
				}
				else
				{
					/* Return next one of existing */
					if (_OBJ_GET_TYPE(_obj) == type)
						result = GetSucc(&_obj->hso_Node);
				}
			}
			UNLOCK_LIST(HeliosBase->hb_Hardwares);
			_OBJ_XINCREF(result);
			break;

		case HGA_DEVICE:
			if (!_obj)
			{
				HeliosHardware *hh = (APTR)GetTagData(HA_Hardware, 0, tags);
				
				/* We need an Hardware object to process */
				if (hh && _OBJ_GET_TYPE(hh) == HGA_HARDWARE)
				{
					/* Get by GUID or first one? */
					ULONG guid_hi = GetTagData(HA_GUID_Hi, 0, tags);
					ULONG guid_lo = GetTagData(HA_GUID_Lo, 0, tags);
					UQUAD guid = (((UQUAD)guid_hi) << 32) + guid_lo;
					
					READ_LOCK_LIST(hh->hh_Devices);
					{
						if (guid)
						{
							HeliosDevice *node;
							
							ForeachNode(&hh->hh_Devices, node)
							{
								if (node->hd_GUID == guid)
								{
									result = node;
									break;
								}
							}
						}
						else
						{
							/* Return first connected device */
							result = GetHead(&hh->hh_Devices);
						}
					}
					UNLOCK_LIST(hh->hh_Devices);
				}
			}
			else if (_OBJ_GET_TYPE(_obj) == HGA_DEVICE)
			{
				HeliosDevice *ref = obj;
				HeliosHardware *hh;
				
				SHARED_PROTECT_BEGIN(_obj);
				{
					/* Connected ref device? */
					hh = ref->hd_Hardware;
					_DBG("hh of dev %p is %p\n", ref, hh);
					if (hh)
					{
						/* Return next connected device */
						READ_LOCK_LIST(hh->hh_Devices);
						result = GetSucc(&_obj->hso_Node);
						UNLOCK_LIST(hh->hh_Devices);
					}
				}
				SHARED_PROTECT_END(_obj);
			}
			_OBJ_XINCREF(result);
			break;
			
		case HGA_CLASS:
			READ_LOCK_LIST(HeliosBase->hb_Classes);
			{
				if (!_obj)
				{
					/* Return first class */
					result = GetHead(&HeliosBase->hb_Classes);
				}
				else if (_OBJ_GET_TYPE(_obj) == HGA_CLASS)
				{
					/* Return next class */
					SHARED_PROTECT_BEGIN(_obj);
					result = GetSucc(&_obj->hso_Node);
					SHARED_PROTECT_END(_obj);
				}
			}
			UNLOCK_LIST(HeliosBase->hb_Classes);
			_OBJ_XINCREF(result);
			break;
	}
	
	_DBG("result=%p\n", result);
	return result;
}
LONG
Helios_GetAttrsA(APTR obj, struct TagItem *tags)
{
	_HeliosSharedObject *hso = obj;
	LONG count=0;
	struct TagItem *ti, *tmp_tags;

	/* Special case (base is not an shared object) */
	if (obj == HeliosBase)
	{
		tmp_tags = tags;
		while (NULL != (ti = NextTagItem(&tmp_tags)))
		{
			switch(ti->ti_Tag)
			{
				case HA_EventListenerList:
					*(HeliosEventListenerList **)ti->ti_Data = &HeliosBase->hb_Listeners;
					break;
					
				default: continue;
			}

			count++;
		}
	}
	else
	{
		if (_OBJ_ISVALID(hso))
		{
			switch (hso->hso_Type)
			{
				case HGA_CLASS:
					{
						HeliosClass *hc = obj;
						count = HeliosClass_DoMethodA(HCM_GetAttrs, (ULONG *)tags);
					}
					break;
				
				default:
					SHARED_PROTECT_BEGIN(hso);
					{
						_HeliosObjectGetAttrFunc getattr = (APTR)hso->hso_GetAttr;

						tmp_tags = tags;
						while (NULL != (ti = NextTagItem(&tmp_tags)))
						{
							switch(ti->ti_Tag)
							{
								case HA_ObjectName:
									*(STRPTR *)ti->ti_Data = hso->hso_Node.ln_Name;
									break;
									
								default:
									/* TODO: optimize this getattr check */
									if (getattr && !getattr(hso, ti->ti_Tag, (APTR)ti->ti_Data))
										continue;
							}
							
							count++;
						}
					}
					SHARED_PROTECT_END(hso);
					break;
			}
		}
		else
			_ERR("Given object is not valid\n");
	}

	return count;
}
LONG
Helios_SetAttrsA(APTR obj, struct TagItem *tags)
{
	_HeliosSharedObject *hso = obj;
	LONG count=0;
	
	if ((obj != HeliosBase) && _OBJ_ISVALID(hso))
	{
		switch (hso->hso_Type)
		{
			case HGA_CLASS:
				{
					HeliosClass *hc = obj;
					count = HeliosClass_DoMethodA(HCM_SetAttrs, (ULONG *)tags);
				}
				break;
			
			default:
				SHARED_PROTECT_BEGIN(hso);
				{
					struct TagItem *ti, *tmp_tags = tags;
					_HeliosObjectSetAttrFunc setattr = (APTR)hso->hso_SetAttr;

					while (NULL != (ti = NextTagItem(&tmp_tags)))
					{
						if (setattr && !setattr(hso, ti->ti_Tag, (APTR)ti->ti_Data))
							continue;
						
						count++;
					}
				}
				SHARED_PROTECT_END(hso);
				break;
		}
	}
	else
		_ERR("Given object is not valid\n");

	return count;
}
LONG
Helios_ObtainObject(APTR obj)
{
	return _OBJ_XINCREF(obj);
}
void
Helios_ReleaseObject(APTR obj)
{
	_HeliosSharedObject *hso = obj;
	LONG owners = _OBJ_DECREF(obj);
	
	if (!owners && hso->hso_Free)
		((_HeliosObjectFreeFunc)hso->hso_Free)(obj);
}
void
Helios_FlushA(struct TagItem *tags)
{
	/* Flush dead devices list */
	WRITE_LOCK_LIST(HeliosBase->hb_Hardwares);
	{
		HeliosHardware *hh;
		ForeachNode(&HeliosBase->hb_Hardwares, hh)
			_Helios_FlushDeadDevices(hh);
	}
	UNLOCK_LIST(HeliosBase->hb_Hardwares);
}
/*=== EOF ====================================================================*/
