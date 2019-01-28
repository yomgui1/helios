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
** Helios SBP2 device header.
*/

#ifndef SBP2_DEVICE_H
#define SBP2_DEVICE_H

#include  "private.h"

#ifndef DEVNAME
#   define DEVNAME              "sbp2.device"
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

/* Exec device side of SBP2 */
typedef struct SBP2Device
{
	struct Library		dv_Library;       /* standard */
	UWORD				dv_Flags;         /* various flags */
	BPTR				dv_SegList;       /* device seglist */

	SBP2ClassLib *		dv_SBP2ClassBase; /* up link */
	struct ExecBase *	dv_SysBase;
	struct Library *	dv_MountBase;

	LOCK_PROTO;
	LOCKED_MINLIST_PROTO(dv_Units);
	ULONG				dv_UnitCount;	/* Length of hc_Units */
} SBP2Device;

extern const ULONG devFuncTable[];
extern BPTR devCleanup(SBP2Device *base);

#endif /* SPB2_DEVICE_H */
