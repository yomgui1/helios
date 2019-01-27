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
** Provide API to handle helios classes.
**
** Helios classes implements various 1394 application protocols,
** like the SBP2 protocol.
** They are implemented as system devices with extras methods.
*/

#include "private.h"

/*============================================================================*/
/*--- LOCAL API --------------------------------------------------------------*/
/*============================================================================*/
static void
class_free(HeliosClass *hc)
{
	_DBG("Freeing class %p-%s\n", hc, hc->hc_Base.hso_Node.ln_Name);
	
	/* As hc pointer will not be longer valid ask the class to forget it */
	HeliosClass_DoMethod(HCM_Terminate);
	
	hc->hc_Library->lib_Flags |= LIBF_DELEXP;
	CloseLibrary(hc->hc_Library);
	FreeVecPooled(HeliosBase->hb_MemPool, hc->hc_LoadedName);
	FreePooled(HeliosBase->hb_MemPool, hc, sizeof(*hc));
}
static LONG
class_get_attribute(HeliosClass *hc, ULONG tag, APTR data)
{
	switch(tag)
	{
		case HA_ClassLoadName:
			*(STRPTR *)data = hc->hc_LoadedName;
			break;
			
		default:
			return FALSE;
	}
	
	return TRUE;
}
/*============================================================================*/
/*--- PUBLIC API -------------------------------------------------------------*/
/*============================================================================*/
HeliosClass *
Helios_AddClass(STRPTR name, ULONG version)
{
	struct Library *cls_base;
	HeliosClass *hc=NULL;

	_DBG("Loading class <%s> v%lu...\n", name, version);
	cls_base = OpenLibrary(name, version);
	if (NULL != cls_base)
	{
		WRITE_LOCK_LIST(HeliosBase->hb_Classes);
		{
			/* If not already exists, create or respawn a HeliosClass shared object.
			 * Do nothing if the class already queued.
			 */
			hc = (HeliosClass *)FindName((struct List *)&HeliosBase->hb_Classes,
				cls_base->lib_Node.ln_Name);
			if (NULL == hc)
			{
				hc = AllocPooled(HeliosBase->hb_MemPool, sizeof(HeliosClass));
				if (NULL != hc)
				{
					ULONG prio;

					Helios_ObjectInit(hc, HGA_CLASS,
						HA_ObjectName, (ULONG)cls_base->lib_Node.ln_Name,
						HA_ObjectFreeFunc, (ULONG)class_free,
						HA_ObjectGetAttrFunc, (ULONG)class_get_attribute,
						TAG_DONE);

					NEWLIST(&hc->hc_Listeners);
					LOCK_INIT(&hc->hc_Listeners);
					INIT_LOCKED_LIST(hc->hc_Units);
					
					hc->hc_Library = cls_base;
					hc->hc_LoadedName = utils_DupStr(HeliosBase->hb_MemPool, name);
					cls_base = NULL; /* and don't close it ;-) */

					/* HeliosClass structure ok.
					 * Ask for initialization to the class.
					 */
					HeliosClass_DoMethod(HCM_Initialize, (ULONG)hc);

					/* Obtain class priority (silent errors) */
					prio = 0;
					Helios_GetAttrs(hc,
									HA_Priority, (ULONG)&prio,
									TAG_END);
					hc->hc_Base.hso_Node.ln_Pri = prio;

					Enqueue((struct List *)&HeliosBase->hb_Classes, &hc->hc_Base.hso_Node);
					
					//Helios_SendEvent(&HeliosBase->hb_Listeners, HEVTF_CLASS_ADDED, (ULONG)hc);
					_INF("New class added: %p-%s\n", hc, hc->hc_Base.hso_Node.ln_Name);
				}
				else
					_ERR("Failed to allocate class <%s>\n", hc->hc_Base.hso_Node.ln_Name);
			}
			else
				_ERR("Class <%s> already exist\n", hc->hc_Base.hso_Node.ln_Name);
		}
		UNLOCK_LIST(HeliosBase->hb_Classes);
		
		if (NULL != cls_base)
		{
			cls_base->lib_Flags |= LIBF_DELEXP;
			CloseLibrary(cls_base);
		}
	}
	else
		_ERR("Class <%s> v%lu not found\n", name, version);

	CLS_XINCREF(hc);
	return hc;
}
void
Helios_RemoveClass(HeliosClass *hc)
{
	_INF("Removing class %p\n", hc);

	SAFE_REMOVE(HeliosBase->hb_Classes, hc);

	WRITE_LOCK_LIST(hc->hc_Units);
	{
		struct MinNode *node, *next;
		
		ForeachNodeSafe(&hc->hc_Units, node, next)
		{
			HeliosUnit *hu = (APTR)node - offsetof(HeliosUnit, hu_BindNode);

			HeliosClass_DoMethod(HCM_ReleaseUnitBinding, (ULONG)hu);
			
			hu->hu_BindClass = NULL;
			hu->hu_BindUData = NULL;
			REMOVE(node);
			Helios_ReleaseObject(hu);
			Helios_ReleaseObject(hc);
		}
	}
	UNLOCK_LIST(hc->hc_Units);

	//Helios_SendEvent(&HeliosBase->hb_Listeners, HEVTF_CLASS_REMOVED, (ULONG)hc);
	Helios_ReleaseObject(hc);
}
/*=== EOF ====================================================================*/