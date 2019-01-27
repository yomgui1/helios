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
** This file is copyrights 2013 by Guillaume ROGUEZ.
**
** DUMMY class private API header file.
*/

#ifndef DUMMY_PRIVATE_H
#define DUMMY_PRIVATE_H

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include "proto/helios.h"

#include "utils.h"
#include "debug.h"

struct DummyClassLib;
typedef struct DummyClassLib DummyClassLib;

/* dummy logical unit: */
typedef struct DummyUnit
{
	struct Unit          u_SysUnit;

	/* Management data */
	LOCK_PROTO;
	LONG                 u_UnitNo;
	LONG                 u_LUN;
	DummyClassLib *       u_DummyClassBase;
} DummyUnit;

extern LONG dummy_InitClass(DummyClassLib *base, HeliosClass *hc);
extern LONG dummy_TermClass(DummyClassLib *base);
extern LONG dummy_AttemptUnitBinding(DummyClassLib *base, HeliosUnit *unit);
extern LONG dummy_ForceUnitBinding(DummyClassLib *base, HeliosUnit *unit);
extern LONG dummy_ReleaseUnitBinding(DummyClassLib *base, HeliosUnit *helios_unit);

#endif /* DUMMY_PRIVATE_H */
