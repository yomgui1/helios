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
static const UBYTE template[] = "HW_ID/N,DATA/N";

static struct
{
    LONG *hwno;
    LONG *data;
} args;

LONG send_phy(HeliosHardware *hw, QUADLET data)
{
    struct MsgPort port;
    IOHeliosHWReq ioreq;
    LONG err;

    /* Init temporary msg port */
    port.mp_Flags   = PA_SIGNAL;
    port.mp_SigBit  = SIGB_SINGLE;
    port.mp_SigTask = FindTask(NULL);
    NEWLIST(&port.mp_MsgList);

    /* Fill iorequest */
    bzero(&ioreq, sizeof(ioreq));

    ioreq.iohh_Req.io_Message.mn_Length = sizeof(ioreq);
    ioreq.iohh_Req.io_Message.mn_ReplyPort = &port;
    ioreq.iohh_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq.iohh_Req.io_Command = HHIOCMD_SENDPHY;

    /* No payload with writes */
    ioreq.iohh_Data = (APTR)data;

    err = Helios_DoIO(HGA_HARDWARE, hw, &ioreq);
    if (err)
    {
        Printf("Failed, io err=%ld\n", err);
        return TRUE;
    }

    return FALSE;
}

int main(int argc, char **argv)
{
    APTR rdargs;
    LONG res=RETURN_FAIL, hwno=0;
    QUADLET data=0;

    rdargs = ReadArgs(template, (APTR) &args, NULL);
    if (NULL != rdargs)
    {
        if (NULL != args.hwno)
            hwno = *args.hwno;
        if (NULL != args.data)
            data = *args.data;
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

        Helios_WriteLockBase();
        {
            while (NULL != (hw = Helios_GetNextHardware(hw)))
            {
                if (hwno == cnt++)
                    break;

                Helios_ReleaseHardware(hw);
            }
        }
        Helios_UnlockBase();

        if (NULL != hw)
        {
            Printf("Send PHY packet: $%08lx\n", data);
            res = send_phy(hw, data) ? RETURN_ERROR:RETURN_OK;
            Helios_ReleaseHardware(hw);
        }

        CloseLibrary(HeliosBase);
    }

    return res;
}
