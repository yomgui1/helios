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

#include <exec/execbase.h>
#include <proto/exec.h>

#include <string.h>

#define HELIOS_LIBNAME "helios.library"
#define MAX_HW_UNITS 8

int main(int argc, char **argv)
{
    struct Library *HeliosBase;
    struct Task *task;
    LONG result=-1;

    task = FindTask(NULL);
    task->tc_UserData = (APTR)0x837105; /* The magic key */

    HeliosBase = OpenLibrary(HELIOS_LIBNAME, 52);
    if (NULL != HeliosBase)
    {
        ULONG cnt;

        Helios_AddClass("Helios/sbp2.class", 50);

        for (cnt=0; cnt<MAX_HW_UNITS; cnt++)
        {
            Helios_AddHardware("Helios/ohci1394_pci.device", cnt);
        }

        CloseLibrary(HeliosBase);
        result = 0;
    }

    return result;
}
