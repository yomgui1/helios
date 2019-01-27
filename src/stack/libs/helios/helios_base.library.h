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
** Helios Library structure header file.
*/

#ifndef HELIOS_BASE_LIBRARY_H
#define HELIOS_BASE_LIBRARY_H

#include "private.h"

#include <exec/libraries.h>
#include <devices/timer.h>
#include <clib/debug_protos.h>

struct HeliosBase
{
	struct Library			hb_Lib;
	BPTR					hb_SegList;

	LOCK_PROTO;

	APTR					hb_MemPool;
	struct timerequest		hb_TimeReq;
	ULONG					hb_HwCount;
	HeliosEventListenerList	hb_Listeners;
	
	LOCKED_MINLIST_PROTO(hb_Hardwares);
	LOCKED_MINLIST_PROTO(hb_Classes);
};

extern struct HeliosBase *HeliosBase;

#endif /* HELIOS_BASE_LIBRARY_H */
