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

#define HELIOS_LIBNAME "helios.library"

int main(int argc, char **argv)
{
    struct Library *HeliosBase;

    HeliosBase = OpenLibrary(HELIOS_LIBNAME, 52);
    if (NULL != HeliosBase)
    {
        HeliosClass *hc;
        HeliosHardware *hw;
        APTR next;

        hc = Helios_GetNextClass(NULL);
        while (NULL != hc)
        {
            next = Helios_GetNextClass(hc);
            Helios_ReleaseClass(hc);
            Helios_RemoveClass(hc);
            hc = next;
        }

        hw = Helios_GetNextHardware(NULL);
        while (NULL != hw)
        {
            next = Helios_GetNextHardware(hw);
            Helios_ReleaseHardware(hw);
            Helios_RemoveHardware(hw);
            hw = next;
        }

        CloseLibrary(HeliosBase);
    }

    return 0;
}
