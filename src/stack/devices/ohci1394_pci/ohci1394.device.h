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
** Private Helios OHCI 1394 device header.
**
*/

#ifndef OHCI1394_DEVICE_H
#define OHCI1394_DEVICE_H

#include "helios_internals.h"
#include "devices/helios/ohci1394.h"

#include <exec/libraries.h>
#include <exec/lists.h>
#include <dos/dos.h>
#include <utility/utility.h>
#include <devices/timer.h>

#if !defined(BUILD_DATE)
#error "BUILD_DATE are not defined but are mandatory"
#endif

#ifndef SCM_REV
#define SCM_REV "private"
#endif

#define COPYRIGHTS "\xa9\x20Guillaume\x20Roguez\x20[" SCM_REV "]"
#define VERSION 50
#define REVISION 3
#define VR_ST "50.3"
#define VERS    DEVNAME" "VR_ST
#define VSTRING DEVNAME" "VR_ST" ("BUILD_DATE") "COPYRIGHTS"\r\n"
#define VTAG "\0$VER: "DEVNAME" "VR_ST" ("BUILD_DATE") "COPYRIGHTS

/*----------------------------------------------------------------------------*/
/*--- STRUCTURES SECTION -----------------------------------------------------*/

typedef struct OHCI1394Device
{
    struct Library          hd_Library;       /* standard */
    UWORD                   hd_Flags;         /* various flags */
    BPTR                    hd_SegList;

    APTR                    hd_MemPool;       /* Memory Pool used to allocate units */
    ULONG                   hd_UnitCount;     /* Number of units found */
    struct MinList          hd_Units;         /* List of units */
} OHCI1394Device;


struct OHCI1394Unit; /* ohci1394core.h */

/*----------------------------------------------------------------------------*/
/*--- PROTOTYPES SECTION -----------------------------------------------------*/

#define CMDP(name) ULONG name (IOHeliosHWReq *ioreq, struct OHCI1394Unit *unit, OHCI1394Device *base)

extern CMDP(cmdQueryDevice);
extern CMDP(cmdReset);
extern CMDP(cmdEnable);
extern CMDP(cmdDisable);
extern CMDP(cmdBusReset);
extern CMDP(cmdSendRawPacket);
extern CMDP(cmdSendPhy);
extern CMDP(cmdSendRequest);
extern CMDP(abortSendRequest);
extern CMDP(cmdAddReqHandler);
extern CMDP(cmdRemReqHandler);
extern CMDP(cmdSetAttrs);
extern CMDP(cmdCreateIsoCtx);
extern CMDP(cmdDelIsoCtx);
extern CMDP(cmdStartIsoCtx);
extern CMDP(cmdStopIsoCtx);

#endif /* OHCI1394_DEVICE_H */
