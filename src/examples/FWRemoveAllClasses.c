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

struct Library *HeliosBase;

int main(int argc, char **argv)
{
	int ret = RETURN_OK;
	
	/* Opening the library before calling its API */
	HeliosBase = OpenLibrary(HELIOS_LIBNAME, HELIOS_LIBVERSION);
	if (HeliosBase)
	{
		HeliosClass *hc=NULL;
		
		do
		{
			hc = Helios_FindObject(HGA_CLASS, hc, TAG_DONE);
			if (hc)
			{
				Helios_RemoveClass(hc);
				Helios_ReleaseObject(hc);
			}
		}
		while (hc);
		
		HeliosBase->lib_Flags |= LIBF_DELEXP;
		CloseLibrary(HeliosBase);
	}
	else
	{
		Printf("%s requires %s v%lu\n", (ULONG)argv[0], (ULONG)HELIOS_LIBNAME, HELIOS_LIBVERSION);
		ret = RETURN_ERROR;
	}
	
	return ret;
}
