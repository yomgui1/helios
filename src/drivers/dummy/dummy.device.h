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
** Dummy device header.
*/

#ifndef DUMMY_DEVICE_H
#define DUMMY_DEVICE_H

#include  "private.h"

#ifndef DEVNAME
#   define DEVNAME              "dummy.device"
#endif
#ifndef DEV_VERSION
#   define DEV_VERSION          0
#endif
#ifndef DEV_REVISION
#   define DEV_REVISION         0
#endif
#ifndef DEV_VERSION_STR
#   define DEV_VERSION_STR      "0"
#endif
#ifndef DEV_REVISION_STR
#   define DEV_REVISION_STR     "0"
#endif
#ifndef DEV_DATE
#   define DEV_DATE             __DATE__
#endif
#ifndef DEV_COPYRIGHTS
#   define DEV_COPYRIGHTS       "(C) Guillaume ROGUEZ"
#endif

#define RC_DONTREPLY -1
#define RC_OK 0

/* Exec device side */
typedef struct DummyDevice
{
	struct Library		dv_Library;			/* standard */
	UWORD				dv_Flags;			/* various flags */
	BPTR				dv_SegList;			/* device seglist */
	DummyClassLib *		dv_DummyClassBase;	/* up link */
	struct ExecBase *	dv_SysBase;
	
	LOCK_PROTO;
	ULONG				dv_UnitCount;
	LOCKED_MINLIST_PROTO(dv_Units);
} DummyDevice;

extern const ULONG devFuncTable[];
extern BPTR devCleanup(DummyDevice *base);

#endif /* DUMMY_DEVICE_H */
