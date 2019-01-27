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
** Public common header file for Helios devices.
*/

#ifndef DEVICE_HELIOS_H
#define DEVICE_HELIOS_H

#include <libraries/helios.h>
#include <exec/io.h>
#include <exec/errors.h>

/* For old MorphOS-SDK */
#ifndef QUERYSUBCLASS_FIREWIRE_HWDRIVER
#define QUERYSUBCLASS_FIREWIRE_HWDRIVER     30000
#define QUERYSUBCLASS_FIREWIRE_SBP          30001
#define QUERYSUBCLASS_FIREWIRE_AVC          30002
#endif

/* Device specific IO commands */

#define HHIOCMD_BUSRESET          (CMD_NONSTD+0)    /* (struct IOStdReq *) */
#define HHIOCMD_ENABLE            (CMD_NONSTD+1)    /* (struct IORequest *) */
#define HHIOCMD_DISABLE           (CMD_NONSTD+2)    /* (struct IORequest *) */
#define HHIOCMD_SETATTRIBUTES     (CMD_NONSTD+3)    /* (IOHeliosHWSendRequest *) */
#define HHIOCMD_SENDPHY           (CMD_NONSTD+4)
#define HHIOCMD_SENDREQUEST       (CMD_NONSTD+5)
#define HHIOCMD_SENDRESPONSE      (CMD_NONSTD+6)
#define HHIOCMD_ADDREQHANDLER     (CMD_NONSTD+7)
#define HHIOCMD_REMREQHANDLER     (CMD_NONSTD+8)
#define HHIOCMD_CREATEISOCONTEXT  (CMD_NONSTD+9)
#define HHIOCMD_DELETEISOCONTEXT  (CMD_NONSTD+10)
#define HHIOCMD_STARTISOCONTEXT   (CMD_NONSTD+11)
#define HHIOCMD_STOPISOCONTEXT    (CMD_NONSTD+12)

/* Device's may also support following standard IO commands:
 * - CMD_RESET
 */

/* Tags defined for QueryGetDeviceAttr(), data = (HeliosHWQuery *) */
#define HHA_Dummy               (HELIOS_TAGBASE+0x100)
/*
 * return supported 1394 standard (see HSTD_xxx)
 * hhq_Data: (ULONG *)
 */
#define HHA_Standard            (HHA_Dummy+0)
/*
 * return supported implementation version
 * hhq_Data: (ULONG *)
 */
#define HHA_DriverVersion       (HHA_Dummy+1)
/*
 * return GUID of the local node
 * hhq_Data: (UQUAD *)
 */
#define HHA_LocalGUID           (HHA_Dummy+2)
/*
 * return NodeID of the local node
 * hhq_Data: (UBYTE *)
 */
#define HHA_LocalNodeId         (HHA_Dummy+3)
/*
 * return transaction split timeout (in number of 1394 cycles)
 * hhq_Data: (ULONG *)
 */
#define HHA_SplitTimeout        (HHA_Dummy+4)
/*
 * return value of CYCLE_TIME CSR register
 * hhq_Data: (ULONG *)
 */
#define HHA_CycleTime           (HHA_Dummy+5)
/*
 * return value of BUS_TIME + CYCLE_TIME CSR registers in atomic way
 * hhq_Data: (ULONG *)
 */
#define HHA_UpTime              (HHA_Dummy+6)
/*
 * return vendor's unique number (OUI)
 * hhq_Data: (ULONG *)
 */
#define HHA_VendorId            (HHA_Dummy+7)
/*
 * make a copy of the local bus SelfID stream
 * hhq_Data: (HeliosSelfIDStream *)
 */
#define HHA_SelfID              (HHA_Dummy+8)

/* Helios specific IORequest error codes */
#define HHIOERR_NO_ERROR                  0
#define HHIOERR_FAILED                    1 /* Generic error code */
#define HHIOERR_NOMEM                     2 /* No memory enough */
#define HHIOERR_UNIT_DISABLED             3 /* HHIOCMD_ENABLE must be called */
#define HHIOERR_UNRECOVERABLE_ERROR       4 /* HW fatal error (CMD_RESET must be called) */
#define HHIOERR_ACK                       5 /* packet send without ack complete */
#define HHIOERR_RCODE                     6 /* packet send and acknownledged, but response error */

typedef struct HeliosHWQuery
{
	struct Unit *	hhq_Unit;
	APTR			hhq_Data;
} HeliosHWQuery;

#endif /* DEVICE_HELIOS_H */
