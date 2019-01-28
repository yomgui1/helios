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

struct Library *HeliosBase;
static const UBYTE template[] = "HW_UNIT/N,LBR=LONGBUSRESET/S";

static struct {
    LONG *unitno;
    BOOL shortbr;
} args;

int main(int argc, char **argv)
{
    APTR rdargs;
    LONG unitno = 0;

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

    HeliosBase = OpenLibrary("helios.library", 52);
    if (NULL != HeliosBase)
    {
        ULONG cnt=0;
        HeliosHardware *hw = NULL;

        Helios_WriteLockBase();
        {
            while (NULL != (hw = Helios_GetNextHardware(hw)))
            {
                if (unitno == cnt++)
                    break;

                Helios_ReleaseHardware(hw);
            }
        }
        Helios_UnlockBase();

        if (NULL != hw)
        {
            Helios_BusReset(hw, args.shortbr);
            Helios_ReleaseHardware(hw);
        }

        CloseLibrary(HeliosBase);
    }
    else
        return RETURN_FAIL;

    return RETURN_OK;
}
