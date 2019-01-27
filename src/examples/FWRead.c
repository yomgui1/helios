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
static const UBYTE template[] = "NDI=NODEID/A/N,HW=UNIT/N,OFFSET/A/N,LENGTH/N,SPEED/N,CSR/S,BINARY/S";

static struct {
	ULONG *nodeid;
	ULONG *unitno;
	ULONG *offset;
	ULONG *length;
	ULONG *speed;
	LONG csr;
	LONG binary;
} args = {0};

int main(int argc, char **argv)
{
	APTR rdargs;
	ULONG unitno=0;
	ULONG nodeid=0;
	HeliosOffset offset=0;
	ULONG length=4;
	ULONG speed=0;
	int ret = RETURN_OK;

	rdargs = ReadArgs(template, (APTR) &args, NULL);
	if (NULL != rdargs)
	{
		if (NULL != args.nodeid)
			nodeid = *args.nodeid;
		if (NULL != args.unitno)
			unitno = *args.unitno;
		if (NULL != args.offset)
			offset = *args.offset;
		if (NULL != args.length)
			length = *args.length;
		if (NULL != args.speed)
			speed = *args.speed;
	}
	else
	{
		PrintFault(IoErr(), NULL);
		return RETURN_ERROR;
	}
	
	length /= 4;
	if (length == 0)
		length = 1;
		
	if (args.csr)
		offset += CSR_BASE_LO;
	
	switch (speed)
	{
		case 400: speed = S400; break;
		case 200: speed = S200; break;
		default: speed = S100;
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
			LONG gen, count;

			/* Ask which is the current generation */
			count = Helios_GetAttrs(hh,
					HA_Generation,	(ULONG)&gen,
					TAG_DONE);
			if ((count == 1) && (gen >= 0))
			{
				ULONG i;
				HeliosPacket packet;
				struct IOStdReq ioreq;
				
				/* Pre-fill the IO request */
				ioreq.io_Message.mn_Length = sizeof(ioreq);
				ioreq.io_Command = HHIOCMD_SENDREQUEST;
				ioreq.io_Data = &packet;

				/* Pre-fill constant packet fields */
				packet.DestID = nodeid;
				packet.Generation = gen;
				
				/* Read and print requested packet */
				for (i=0; i < length; i++, offset += sizeof(QUADLET))
				{
					int retry=5;
					LONG ioerr;
					
					Helios_FillReadQuadletPacket(&packet, speed, offset);
					packet.QuadletData = 0;
					
					do
					{
						ioerr = Helios_DoIO(hh, (struct IORequest *)&ioreq);
						if (ioerr)
						{
							/* stop if not a busy Ack error */
							if ((HELIOS_ACK_BUSY_X != packet.Ack) &&
								(HELIOS_ACK_BUSY_A != packet.Ack) &&
								(HELIOS_ACK_BUSY_B != packet.Ack))
								break;
						}
					}
					while (retry-- && ioerr);
					
					if (!ioerr)
					{
						if (args.binary)
							Write(Output(), &packet.QuadletData, sizeof(QUADLET));
						else
							Printf("0x%08lX\n", packet.QuadletData);
					}
					else if (i == 0)
					{
						fprintf(stderr, "Failed to read at offset=0x%08lX%08lX, ioerr=%ld\n",
							(ULONG)(offset >> 32), (ULONG)offset, ioerr);
						ret = RETURN_FAIL;
						break;
					}
				}
			}
			else
			{
				fprintf(stderr, "Failed to obtain current Generation\n");
				ret = RETURN_ERROR;
			}
			
			Helios_ReleaseObject(hh);
		}
		else
		{
			fprintf(stderr, "Hardware unit %lu not found\n", unitno);
			ret = RETURN_WARN;
		}
		
		CloseLibrary(HeliosBase);
	}
	else
	{
		fprintf(stderr, "%s requires %s v%lu\n", (ULONG)argv[0], (ULONG)HELIOS_LIBNAME, HELIOS_LIBVERSION);
		ret = RETURN_ERROR;
	}
	
	return ret;
}
