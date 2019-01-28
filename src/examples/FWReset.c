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
**
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

    HeliosBase = OpenLibrary("helios.library", 52);
    if (NULL != HeliosBase)
    {
        ULONG cnt=0;
        HeliosHardware *hw = NULL;
        HeliosDevice *dev=NULL;

        Helios_WriteLockBase();
        {
            while (NULL != (hw = Helios_GetNextHardware(hw)))
            {
                if (hwno == cnt++)
                {
                    while (NULL != (dev = Helios_GetNextDevice(dev,
                                                               HA_Hardware, (ULONG)hw,
                                                               TAG_DONE)))
                    {
                        ULONG value;

                        if (1 == Helios_GetAttrs(HGA_DEVICE, dev,
                                                 HA_NodeID, (ULONG)&value,
                                                 TAG_DONE))
                        {
                            if (value == nodeid)
                                break;
                        }

                        Helios_ReleaseDevice(dev);
                    }

                    break;
                }

                Helios_ReleaseHardware(hw);
            }
        }
        Helios_UnlockBase();

        if (NULL != dev)
        {
            Printf("Reset node %04lx\n", nodeid);
            res = write_reset(dev) ? RETURN_ERROR:RETURN_OK;
            Helios_ReleaseDevice(dev);
        }
        else
            res = RETURN_FAIL;

        if (NULL != hw)
            Helios_ReleaseHardware(hw);

        CloseLibrary(HeliosBase);
    }

    return res;
}
