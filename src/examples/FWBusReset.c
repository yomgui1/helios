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
*/

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/helios.h>

struct Library *HeliosBase;
static const UBYTE template[] = "HW_UNIT/N,LBR=LONGBUSRESET/S";

static struct {
	LONG *unitno;
	BOOL lbr;
} args = {NULL, 0};

int main(int argc, char **argv)
{
	APTR rdargs;
	LONG unitno=0;

	rdargs = ReadArgs(template, (APTR) &args, NULL);
	if (NULL != rdargs)
	{
		if (NULL != args.unitno)
			unitno = *args.unitno;
	}
	else
	{
		PrintFault(IoErr(), NULL);
		return RETURN_ERROR;
	}

	/* Opening the library before calling its API */
	HeliosBase = OpenLibrary(HELIOS_LIBNAME, HELIOS_LIBVERSION);
	if (HeliosBase)
	{
		HeliosHardware *hh = NULL;
		
		hh = Helios_FindObject(HGA_HARDWARE, NULL,
			HA_UnitNo, unitno,
			TAG_DONE);

		if (hh)
		{
			struct IOStdReq ioreq;
	
			ioreq.io_Message.mn_Length = sizeof(ioreq);
			ioreq.io_Command = HHIOCMD_BUSRESET;
			ioreq.io_Data = (APTR)!args.lbr;
			
			Helios_DoIO(hh, (struct IORequest *)&ioreq);
			
			/* Returned hardware object has been incref'ed for our usage.
			 * As we don't need anymore, we need to release it.
			 */
			Helios_ReleaseObject(hh);
		}

		CloseLibrary(HeliosBase);
	}
	else
		return RETURN_FAIL;

	return RETURN_OK;
}
