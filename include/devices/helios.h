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
**
*/

#ifndef DEVICE_HELIOS_H
#define DEVICE_HELIOS_H

#include <libraries/helios.h>
#include <exec/io.h>
#include <exec/errors.h>

/* Command supported by devices */

#define HHIOCMD_QUERYDEVICE       (CMD_NONSTD+0)
#define HHIOCMD_ENABLE            (CMD_NONSTD+1)
#define HHIOCMD_DISABLE           (CMD_NONSTD+2)
#define HHIOCMD_BUSRESET          (CMD_NONSTD+3)
#define HHIOCMD_SETATTRIBUTES     (CMD_NONSTD+4)
#define HHIOCMD_SENDPHY           (CMD_NONSTD+5)
#define HHIOCMD_SENDREQUEST       (CMD_NONSTD+6)
#define HHIOCMD_SENDRESPONSE      (CMD_NONSTD+7)
#define HHIOCMD_ADDREQHANDLER     (CMD_NONSTD+8)
#define HHIOCMD_REMREQHANDLER     (CMD_NONSTD+9)
#define HHIOCMD_CREATEISOCONTEXT  (CMD_NONSTD+10)
#define HHIOCMD_DELETEISOCONTEXT  (CMD_NONSTD+11)
#define HHIOCMD_STARTISOCONTEXT   (CMD_NONSTD+12)
#define HHIOCMD_STOPISOCONTEXT    (CMD_NONSTD+13)

/* They also support following standard IO commands:
 * - CMD_RESET
 */

/* HHIOCMD_QUERY Tags */
#define HHA_Dummy              (HELIOS_TAGBASE+0x100)
#define HHA_Manufacturer       (HHA_Dummy+0)
#define HHA_ProductName        (HHA_Dummy+1)
#define HHA_Version            (HHA_Dummy+2)
#define HHA_Revision           (HHA_Dummy+3)
#define HHA_Copyright          (HHA_Dummy+4)
#define HHA_Description        (HHA_Dummy+5)
#define HHA_DriverVersion      (HHA_Dummy+6)
#define HHA_Capabilities       (HHA_Dummy+7)
#define HHA_TimeStamp          (HHA_Dummy+8)
#define HHA_UpTime             (HHA_Dummy+9)
//                             (HHA_Dummy+10)
#define HHA_Topology           (HHA_Dummy+11)
#define HHA_SplitTimeout       (HHA_Dummy+12)
#define HHA_VendorUnique       (HHA_Dummy+13)
#define HHA_VendorCompagnyId   (HHA_Dummy+14)
#define HHA_NodeCount          (HHA_Dummy+15)
#define HHA_TopologyGeneration (HHA_Dummy+16)
#define HHA_LocalGUID          (HHA_Dummy+17)

/* Hardware Capabilities */
#define HHF_1394A_1995 (1<<0) /* Speeds: s100, s200, s400 */
#define HHF_1394A_2000 (1<<1)
#define HHF_1394B_2002 (1<<2) /* Speeds: s800, s1600, s3200 */

/* IORequest errors */
#define HHIOERR_NO_ERROR                  0
#define HHIOERR_FAILED                    1
#define HHIOERR_UNRECOVERABLE_ERROR_STATE 2
#define HHIOERR_UNIT_DISABLED             3
#define HHIOERR_NOMEM                     5

/* RequestHandler flags */
#define HHF_REQH_ALLOCLEN 1 /* let Helios find the start address in given region */

typedef HeliosResponse * (*HeliosHWReqCallback)(HeliosAPacket *request, APTR udata);

typedef struct HeliosHWReqHandler
{
    struct MinNode      rh_SysNode;
    HeliosOffset        rh_RegionStart;
    HeliosOffset        rh_RegionStop;
    HeliosOffset        rh_Start;
    ULONG               rh_Length;
    ULONG               rh_Reserved;
    HeliosHWReqCallback rh_ReqCallback;
    APTR                rh_UserData;
    ULONG               rh_Flags;
} HeliosHWReqHandler;

typedef struct IOHeliosHWReq
{
    struct IORequest    iohh_Req;
    APTR                iohh_Data;      /* Versatile field */
    ULONG               iohh_Length;    /* Dependent of iohh_Data usage */
    LONG                iohh_Actual;    /* Versatile field */
    APTR                iohh_Private;
} IOHeliosHWReq;

typedef struct IOHeliosHWSendRequest
{
    IOHeliosHWReq         iohhe_Req;              /* Data: response payload, length: response payload length */
    struct HeliosDevice * iohhe_Device;           /* If NULL, use DestID field in transaction's packet */
    HeliosTransaction     iohhe_Transaction;      /* Just fill the packet */
} IOHeliosHWSendRequest;

#endif /* DEVICE_HELIOS_H */
