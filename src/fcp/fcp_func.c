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
** IEC-61883 FCP serveur
**
*/

#include "devices/helios/fcp.h"
#include "fcp_log.h"

#include "proto/helios.h"

typedef struct FCPCommandNode
{
    struct MinNode SysNode;
    ULONG          Length;
    QUADLET *      Cmd;   
} FCPCommandNode;

//+ fcp_ComputResponseTimeStamp
UWORD fcp_ComputResponseTimeStamp(HeliosPacket *request, UWORD offset)
{
    UWORD timestamp;

    timestamp = (request->TimeStamp & 0x1fff) + offset;
    /* 13bit overflow => add one second */
    if (timestamp >= 8000) {
        timestamp -= 8000;
        timestamp += (request->TimeStamp & ~0x1fff) + 0x2000;
    } else
        timestamp += (request->TimeStamp & ~0x1fff);
    return timestamp;
}
//-

/*------------------ EXTERNAL CODE SECTION -------------------------*/

//+ fcp_RequestHandler
void fcp_RequestHandler(HeliosBus *    bus,
                        HeliosPacket * request,
                        HeliosPacket * response,
                        LONG           no_response)
{
    QUADLET *payload = NULL;
    LONG length = 0, rcode = HELIOS_RCODE_TYPE_ERROR;
    UWORD timestamp = 0;

    if (!no_response) {
        Helios_FillResponseFromRequest(request, response, rcode, timestamp, payload, length);
        Helios_SendResponse(bus, response);
    }
}
//-
//+ fcp_ResponseHandler
void fcp_ResponseHandler(HeliosBus *    bus,
                         HeliosPacket * request,
                         HeliosPacket * response,
                         LONG           no_response
                         APTR           udata)
{
    LONG rcode;
    ULONG length;
    QUADLET *data;
    struct MinList *cmdlist = udata;
    UBYTE tcode;

    tcode = request->TCode;
    switch (tcode) {
        case TCODE_WRITE_BLOCK_REQUEST:
            data = &HELIOS_GET_QDATA(request);
            length = 4;
            rcode = HELIOS_RCODE_COMPLETE;
            break;

        case TCODE_WRITE_QUADLET_REQUEST:
            data = request->Payload;
            length = request->Length;
            rcode = HELIOS_RCODE_COMPLETE;
            break;

        default:
            data = NULL;
            rcode = HELIOS_RCODE_TYPE_ERROR;
    }

    if (NULL != data) {
        FCPCommand *cmd = REMHEAD(cmdlist);

        if (NULL != cmd) {
        } else
            rcode = HELIOS_RCODE_DATA_ERROR
    }

    if (!no_response) {
        UWORD timestamp = fcp_ComputResponseTimeStamp(request, 0x4000);

        Helios_FillResponseFromRequest(request, response, rcode, timestamp, NULL, 0);
        Helios_SendResponse(bus, response);
    }
}
//-
//+ fcp_cmd_Write
LONG fcp_cmd_Write(struct IOExtFCP *ioreq)
{
    return 0;
}
//-
