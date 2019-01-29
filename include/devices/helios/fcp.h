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
** Public Helios FCP device Structures and Definitions.
**
*/

#ifndef DEVICES_HELIOS_FCP_H
#define DEVICES_HELIOS_FCP_H

#include <libraries/helios.h>
#include <exec/io.h>

/* device units are 1394 bus numbers */

#define FCP_REQUEST_ADDR  (0xfffff0000B00)
#define FCP_RESPONSE_ADDR (0xfffff0000D00)

struct IOExtFCP
{
    struct IOStdReq     SysReq;
    UBYTE               Unit;
    HeliosBus *         Bus;
};

#endif /* DEVICES_HELIOS_FCP_H */
