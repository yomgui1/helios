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

#include "proto/helios.h"

#include <exec/execbase.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include <string.h>

int main(int argc, char **argv)
{
	struct Library *HeliosBase;

	HeliosBase = OpenLibrary(HELIOS_LIBNAME, HELIOS_LIBVERSION);
	if (NULL != HeliosBase)
	{
		Forbid();
		{
			struct MsgPort *port, *next;
			CONST_STRPTR portName = "HELIOS.";
			
			ForeachNodeSafe(&SysBase->PortList, port, next)
			{
				if (!strncmp(port->mp_Node.ln_Name, portName, sizeof(portName)))
					Signal(port->mp_SigTask, SIGBREAKF_CTRL_C);
			}
			/* Expunge library if possible */
			HeliosBase->lib_Flags |= LIBF_DELEXP;
		}
		Permit();

		CloseLibrary(HeliosBase);
	}

	return 0;
}
