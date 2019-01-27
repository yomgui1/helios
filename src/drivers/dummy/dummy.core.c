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
** This file is copyrights 2013 by Guillaume ROGUEZ.
**
** Dummy class core API.
*/

#include "dummy.class.h"
#include "dummy.device.h"

#include <clib/macros.h>
#include <dos/dostags.h>

#include <proto/alib.h>
#include <proto/dos.h>
#include <proto/exec.h>

/*============================================================================*/
/*--- LOCAL API --------------------------------------------------------------*/
/*============================================================================*/

/*============================================================================*/
/*--- PUBLIC API -------------------------------------------------------------*/
/*============================================================================*/
LONG
dummy_InitClass(DummyClassLib *base, HeliosClass *hc)
{
	base->hc_HeliosClass = hc;
	return 0;
}
LONG
dummy_TermClass(DummyClassLib *base)
{
	base->hc_HeliosClass = NULL;
	return 0;
}
LONG
dummy_AttemptUnitBinding(DummyClassLib *base, HeliosUnit *hu)
{
	QUADLET *unit_rom_dir=NULL;
	ULONG specid=~0, version=~0;

	Helios_GetAttrs(hu,
		HA_UnitRomDirectory, (ULONG)&unit_rom_dir,
		TAG_DONE);

	if (unit_rom_dir)
	{
		HeliosRomIterator ri;
		QUADLET key, value;
		
		Helios_InitRomIterator(&ri, unit_rom_dir);
		while (Helios_RomIterate(&ri, &key, &value))
		{
			switch (key)
			{
				case KEYTYPEV_IMMEDIATE | CSR_KEY_UNIT_SPEC_ID: specid = value; break;
				case KEYTYPEV_IMMEDIATE | CSR_KEY_UNIT_SW_VERSION: version = value; break;
			}
		}
		
		FreeVec(unit_rom_dir);
		
		_INF("UnitSpecID=0x%X, UnitSWVersion=0x%X\n", specid, version);
	}
	else
		_DBG("failed to obtain Unit ROM directory\n");

	return FALSE;
}
LONG dummy_ForceUnitBinding(DummyClassLib *base, HeliosUnit *hu)
{
	return FALSE;
}
LONG dummy_ReleaseUnitBinding(DummyClassLib *base, HeliosUnit *helios_unit)
{
	_INF("unit @ %p\n", helios_unit);
	return 0;
}
/*=== EOF ====================================================================*/