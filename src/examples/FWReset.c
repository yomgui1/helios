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
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

struct Library *HeliosBase;
static const UBYTE template[] = "HW_ID/N,NODE_ID/N";

static struct
{
	LONG *hwno;
	LONG *nodeid;
} args;

LONG write_reset(HeliosDevice *dev)
{
	struct MsgPort port;
	IOHeliosHWSendRequest ioreq;
	HeliosAPacket *p;
	LONG err;

	/* Init temporary msg port */
	port.mp_Flags   = PA_SIGNAL;
	port.mp_SigBit  = SIGB_SINGLE;
	port.mp_SigTask = FindTask(NULL);
	NEWLIST(&port.mp_MsgList);

	/* Fill iorequest */
	bzero(&ioreq, sizeof(ioreq));

	ioreq.iohhe_Req.iohh_Req.io_Message.mn_Length = sizeof(ioreq);
	ioreq.iohhe_Req.iohh_Req.io_Message.mn_ReplyPort = &port;
	ioreq.iohhe_Req.iohh_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
	ioreq.iohhe_Req.iohh_Req.io_Command = HHIOCMD_SENDREQUEST;
	ioreq.iohhe_Device = dev;

	/* No payload with writes */
	ioreq.iohhe_Req.iohh_Data = NULL;

	/* Fill packet */
	p = &ioreq.iohhe_Transaction.htr_Packet;
	Helios_FillWriteQuadletPacket(p, S100, CSR_BASE_LO + 0x00C, 0);

	err = Helios_DoIO(HGA_DEVICE, dev, &ioreq.iohhe_Req);
	if (err)
	{
		Printf("Failed, io err=%ld, RCode=%ld\n", err, p->RCode);
		return TRUE;
	}

	return FALSE;
}

int main(int argc, char **argv)
{
	APTR rdargs;
	LONG res=RETURN_FAIL, hwno=0;
	UWORD nodeid=HELIOS_LOCAL_BUS;

	rdargs = ReadArgs(template, (APTR) &args, NULL);
	if (NULL != rdargs)
	{
		if (NULL != args.hwno)
			hwno = *args.hwno;
		if (NULL != args.nodeid)
			nodeid = *args.nodeid;
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

	return res;
}
