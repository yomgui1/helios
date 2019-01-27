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
*/

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/helios.h>
#include <stdio.h>
#include <string.h>

struct Library *HeliosBase;
static const UBYTE template[] = "FILE/A";

static struct {
	STRPTR filename;
} args = {0};

int main(int argc, char **argv)
{
	APTR rdargs;
	int ret = RETURN_OK;

	rdargs = ReadArgs(template, (APTR) &args, NULL);
	if (NULL == rdargs)
	{
		PrintFault(IoErr(), NULL);
		return RETURN_ERROR;
	}
	
	/* Opening the library before calling its API */
	HeliosBase = OpenLibrary(HELIOS_LIBNAME, HELIOS_LIBVERSION);
	if (HeliosBase)
	{
		HeliosClass *hc=Helios_AddClass(args.filename, 0);
		
		if (hc)
			Helios_ReleaseObject(hc);
		else
		{
			Printf("Failed to add class '%s'\n", (ULONG)args.filename);
			ret = RETURN_FAIL;
		}
		
		CloseLibrary(HeliosBase);
	}
	else
	{
		Printf("%s requires %s v%lu\n", (ULONG)argv[0], (ULONG)HELIOS_LIBNAME, HELIOS_LIBVERSION);
		ret = RETURN_ERROR;
	}
	
	return ret;
}
