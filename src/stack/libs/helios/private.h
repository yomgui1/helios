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
** Private header file for Helios library code.
*/

#ifndef HELIOS_PRIVATE_H
#define HELIOS_PRIVATE_H

#include "helios_internals.h"

#include "helios_base.library.h"
#include "clib/helios_protos.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#define HeliosClassBase (hc->hc_Library)
#define __NOLIBBASE__
#include "proto/heliosclsbase.h"
#undef __NOLIBBASE__

#include <stdarg.h>

#define Helios_FindObject(__p0, __p1, ...) \
	({ULONG _tags[] = { __VA_ARGS__ }; \
	Helios_FindObjectA(__p0, __p1, (struct TagItem *)_tags);})

#define Helios_SetAttrs(__p0, ...) \
	({ULONG _tags[] = { __VA_ARGS__ }; \
	Helios_SetAttrsA(__p0, (struct TagItem *)_tags);})

#define Helios_CreateROM(__p0, ...) \
	({ULONG _tags[] = { __VA_ARGS__ }; \
	Helios_CreateROMTagList(__p0, (CONST struct TagItem *)_tags);})

#define Helios_ObjectInit(__p0, __p1, ...) \
	({ULONG _tags[] = { __VA_ARGS__ }; \
	Helios_ObjectInitA(__p0, __p1, (struct TagItem *)_tags);})

#define Helios_GetAttrs(__p0, ...) \
	({ULONG _tags[] = { __VA_ARGS__ }; \
	Helios_GetAttrsA(__p0, (struct TagItem *)_tags);})

#define Helios_Flush(...) \
	({ULONG _tags[] = { __VA_ARGS__ }; \
	Helios_FlushA((struct TagItem *)_tags);})

#define Helios_CreateSubTask(__p0, __p1, ...) \
	({ULONG _tags[] = { __VA_ARGS__ }; \
	Helios_CreateSubTaskA(__p0, __p1, (struct TagItem *)_tags);})

extern LONG _Helios_QueryHWDev(HeliosHardware *hh, ULONG attr, APTR result);
extern LONG _Helios_DoHWIOReq(HeliosHardware *hh, struct IORequest *ioreq);
extern void _Helios_GenUnits(HeliosHardware *hh);
extern void _Helios_DetachUnit(HeliosUnit *unit);
extern void _Helios_OnCreatedNode(HeliosHardware *hh, HeliosNode *node);
extern void _Helios_OnUpdatedNode(HeliosHardware *hh, HeliosNode *previous, HeliosNode *current);
extern void _Helios_OnRemovedNode(HeliosHardware *hh, HeliosNode *node);
extern void _Helios_ScanNode(HeliosHardware *hh, HeliosNode *node);
extern void _Helios_DisconnectDevice(HeliosDevice *dev);
extern void _Helios_FlushDeadDevices(HeliosHardware *hh);
extern void _Helios_AttachUnit(HeliosDevice *hd, HeliosUnit *hu);
extern void _Helios_AttemptUnitBinding(HeliosUnit *hu);
extern void _Helios_AttemptDeviceUnitsBinding(HeliosDevice *hd);

#endif /* HELIOS_PRIVATE_H */
