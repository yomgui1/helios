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
** Private Helios Structures and Definitions.
*/

#ifndef LIBRARIES_HELIOS_INTERNAL_H
#define LIBRARIES_HELIOS_INTERNAL_H

#define HELIOS_INTERNAL 1
//#define DEBUG_REFCNT 1

#include "libraries/helios.h"
#include "devices/helios.h"
#include "utils.h"

#include <exec/ports.h>

#ifdef DEBUG_REFCNT
#	define _DBG_REFCNT _DBG
#	define _WRN_REFCNT _WARN
#else
#	define _DBG_REFCNT(x,a...)
#	define _WRN_REFCNT(x,a...)
#endif /* DEBUG_REFCNT */

typedef void (*_HeliosObjectFreeFunc)(APTR);
typedef LONG (*_HeliosObjectGetAttrFunc)(APTR, ULONG, APTR);
typedef LONG (*_HeliosObjectSetAttrFunc)(APTR, ULONG, APTR);

typedef struct _HeliosSharedObject
{
	struct Node		hso_Node;
	LONG			hso_RefCnt;
	ULONG			hso_Type;
	APTR			hso_Free;
	APTR			hso_GetAttr;
	APTR			hso_SetAttr;
	LOCK_PROTO;
} _HeliosSharedObject;

struct HeliosEventListenerList
{
	struct MinList	ell_SysList;
	LOCK_PROTO;
};

struct HeliosHardware
{
	_HeliosSharedObject			hh_Base;

	struct Device *				hh_UnitDevice;		/* Opened HW device */
	struct Unit *				hh_Unit;			/* Corresponding system unit */
	LONG						hh_UnitNo;			/* Logical unit # */
	HeliosSubTask *				hh_HWTask;
	HeliosEventListenerList		hh_Listeners;
	HeliosSelfIDStream			hh_SelfIDStream;
	HeliosTopology *			hh_Topology;		/* Current topology, may be NULL */
	LOCKED_MINLIST_PROTO(hh_Devices);				/* List of connected devices */
	LOCKED_MINLIST_PROTO(hh_DeadDevices);			/* List of non-connected devices */
	UWORD						hh_LocalNodeId;		/* Known after successfull Self-ID process */ \
	UWORD						hh_BMRetry;
};

struct HeliosDevice
{
	_HeliosSharedObject			hd_Base;

	/* Runtime variables.
	 * Object must be locked before read/write
	 */
	HeliosEventListenerList		hd_Listeners;
	HeliosHardware *			hd_Hardware;	/* Up-link */
	HeliosNode *				hd_Node;		/* NULL if not connected (WARNING: owner by HW's topo) */
	WORD						hd_Generation;	/* -1 if not connected */
	UWORD						hd_NodeID;		/* -1 if not connected */
	QUADLET	*					hd_ROM;
	ULONG						hd_ROMLength;
	LOCKED_MINLIST_PROTO(hd_Units);				/* Attached units */
	BOOL 						hd_UnitScan;	/* True if unit scan requiered */

	/* Static variables.
	 * Always valid and never change,
	 * can be saved or used as hash code.
	 */
	union
	{
		UQUAD					hd_GUID;
		struct
		{
			ULONG				hd_GUID_Hi;
			ULONG				hd_GUID_Lo;
		};
	};
};

struct HeliosUnit
{
	_HeliosSharedObject			hu_Base;

	HeliosDevice *				hu_Device;			/* up link */
	const QUADLET *				hu_RomDirectory;	/* Pointer inside hu_Device->hd_Rom */
	QUADLET						hu_RomDirSize;		/* Size in bytes of hu_RomDirectory */

	/* Binding field */
	HeliosClass *				hu_BindClass;		/* driver class */
	APTR						hu_BindUData;		/* udata for driver implementor */
	struct MinNode				hu_BindNode;		/* link to class units list */
};

struct HeliosClass
{
	_HeliosSharedObject			hc_Base;

	struct Library *			hc_Library;
	STRPTR						hc_LoadedName;
	HeliosEventListenerList		hc_Listeners;
	LOCKED_MINLIST_PROTO(hc_Units);					/* Bind units */
};

#define _OBJ_GET_TYPE(o) (((_HeliosSharedObject *)(o))->hso_Type)
#define _OBJ_ISVALID(o) (_OBJ_GET_TYPE(o) != HGA_INVALID)
#define _OBJ_INVALID(o) (((_HeliosSharedObject *)(o))->hso_Type = 0)
#define _OBJ_GET_REFCNT(o) ATOMIC_FETCH(&((_HeliosSharedObject *)(o))->hso_RefCnt)

#define _OBJ_INCREF(o) ({ \
	_HeliosSharedObject *_o = (APTR)(o); \
	LONG _x = ATOMIC_FETCH(&_o->hso_RefCnt); \
	_x = ATOMIC_NCMPXCHG(&_o->hso_RefCnt, 0, _x+1); \
	_DBG_REFCNT("[%s:%p] ctn is %d\n", _o->hso_Node.ln_Name, _o, _x); _x; })

#define _OBJ_DECREF(o) ({ \
	_HeliosSharedObject *_o = (APTR)(o); \
	LONG _x = ATOMIC_FETCH(&_o->hso_RefCnt); \
	_x = ATOMIC_NCMPXCHG(&_o->hso_RefCnt, 0, _x-1); \
	_DBG_REFCNT("[%s:%p] ctn is %d\n", _o->hso_Node.ln_Name, _o, _x); _x; })

#define _OBJ_XINCREF(o) ({ \
	_HeliosSharedObject *_s = (APTR)(o); \
	LONG _x = 0;\
	if (_s) _x = _OBJ_INCREF(_s); _x; })

#define _OBJ_XDECREF(o) ({ \
	_HeliosSharedObject *_s = (APTR)(o); \
	LONG _x = 0; \
	if (_s) _x = _OBJ_DECREF(_s); _x; })

#define _OBJ_CLEAR(v) ({ \
	do { \
		if (v) { \
			_HeliosSharedObject *_tmp = (APTR)v; \
			v = NULL; \
			_OBJ_DECREF(_tmp); \
		} \
	} while (0); })

/* DEPRECATED */
#define HW_DECREF(o) _OBJ_DECREF(o)
#define HW_INCREF(o) _OBJ_INCREF(o)
#define DEV_DECREF(o) _OBJ_DECREF(o)
#define DEV_INCREF(o) _OBJ_INCREF(o)
#define CLS_DECREF(o) _OBJ_DECREF(o)
#define CLS_INCREF(o) _OBJ_INCREF(o)
#define UNIT_DECREF(o) _OBJ_DECREF(o)
#define UNIT_INCREF(o) _OBJ_INCREF(o)

#define HW_XDECREF(o) _OBJ_XDECREF(o)
#define HW_XINCREF(o) _OBJ_XINCREF(o)
#define DEV_XDECREF(o) _OBJ_XDECREF(o)
#define DEV_XINCREF(o) _OBJ_XINCREF(o)
#define CLS_XDECREF(o) _OBJ_XDECREF(o)
#define CLS_XINCREF(o) _OBJ_XINCREF(o)
#define UNIT_XDECREF(o) _OBJ_XDECREF(o)
#define UNIT_XINCREF(o) _OBJ_XINCREF(o)

#endif /* LIBRARIES_HELIOS_INTERNAL_H */
