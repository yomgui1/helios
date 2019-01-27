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
** Public Helios OHCI 1394 device header.
**
*/

#ifndef DEVICE_OHCI1394_H
#define DEVICE_OHCI1394_H

#include <devices/helios.h>

/* Possible values for HHA_DriverVersion query tag*/
#define OHCI1394_VERSION_1_0    0x00010000
#define OHCI1394_VERSION_1_1    0x00010010

/* More tags for HHIOCMD_QUERYDEVICE io request */
#define OHCI1394A_Dummy             (HELIOS_TAGBASE+0x200)
#define OHCI1394A_PciVendorId       (OHCI1394A_Dummy+0)
#define OHCI1394A_PciDeviceId       (OHCI1394A_Dummy+1)
#define OHCI1394A_Generation        (OHCI1394A_Dummy+2)

#endif /* DEVICE_OHCI1394_H */
