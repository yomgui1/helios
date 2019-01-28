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

int main(int argc, char **argv)
{
    HeliosBase = OpenLibrary("helios.library", 52);
    if (NULL != HeliosBase)
    {
        Helios_DBG_DumpDevices(NULL);
        CloseLibrary(HeliosBase);
    }
    else
        Printf("Failed to open helios.library v50\n");

    return 0;
}
