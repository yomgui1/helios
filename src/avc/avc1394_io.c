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
** Helios IEEE-1394 Subsystem AV/C IO routines for MorphOS.
**
*/

#include "avc1394_common.h"
#include "avc1394_libdata.h"

#include <hardware/byteswap.h>
#include <hardware/atomic.h>
#include <exec/errors.h>

#include <proto/exec.h>
#include <proto/dos.h>

/* TODO:
 * - Re-send the command if no responses on the FCP response port after 100ms
 */

/*------------------ PRIVATE CODE SECTION -------------------------*/

static LONG avc1394_SendCommandBlock(HeliosDeviceHandler *  handler,
                                     HeliosATRequest *      req,
                                     QUADLET *              block,
                                     ULONG                  length)
{
    LONG err;

#if defined(__LITTLE_ENDIAN__) | defined(_LITTLE_ENDIAN)
    {
        ULONG i;

        for (i=0; i < length; i++)
        {
            LE_SWAPLONG_P(&block[i]);
        }
    }
#endif

    req->arq_Flags = HELIOS_REQF_WRITE;
    req->arq_Offset = FCP_COMMAND_ADDR;
    req->arq_Payload = block;
    req->arq_Length = length;
    req->arq_Speed = handler->MaxSpeed;

    DB("Sending block @ %p, %lu byte(s) at node %04x\n", req->arq_Payload, req->arq_Length, handler->Id);

    /* IMPORTANT: the node shall respond here, or it's impossible to resolve cross-command issue
     * between 2 clients:
     *   - A client A sends a command to a node.
     *   - The node receives the command.
     *   - The client cancels the command, but in fact only the transaction on the local node side is cancelled.
     *   - The server is ready so a client B sends a command to the same node.
     *   - The node sends the response to the request of client A using a write request
     *      at the FCP response address on the local node.
     *   - The server, after had sent the client B command, fetch the request sent for the client A command.
     *   => cross-command issue!
     */
    err = Helios_DoATRequest(handler, req, 0, NULL);
    Helios_ReleaseATRequest(handler, req); // We don't need data here as they are never data in a write response.
    return err;
}


/*------------------ EXTERNAL CODE SECTION ------------------------*/

void avc1394_Server(void)
{
    HeliosRequestHandler fcp_response_handler;
    HeliosATRequest *req;
    HeliosATResponse *ars;
    struct MsgPort *sv_port, *helios_port, *fcp_port;
    AVC1394_ServerMsg *sv_msg;
    ULONG sv_port_sig, helios_port_sig, fcp_port_sig;
    BOOL run;

    if (!NewGetTaskAttrs(NULL, &sv_port, sizeof(struct MsgPort *),
                         TASKINFOTYPE_TASKMSGPORT, TAG_DONE)
        || (NULL == sv_port))
    {
        log_Error("No server messages port");
        return;
    }

    /* Create 2 ports to communicate with Helios */
    helios_port = CreateMsgPort();
    if (NULL == helios_port)
    {
        log_Error("No server helios messages port");
        return;
    }

    fcp_port = CreateMsgPort();
    if (NULL == fcp_port)
    {
        log_Error("No server FCP messages port");
        DeleteMsgPort(helios_port);
        return;
    }

    /* Create a request helios object in advance */
    req = Helios_AllocATObject(sizeof(*req));
    if (NULL == req)
    {
        log_APIError("Unable to create a request object to communicate with Helios");
        DeleteMsgPort(helios_port);
        DeleteMsgPort(fcp_port);
        return;
    }

    ars = Helios_AllocATObject(sizeof(*ars));
    if (NULL == ars)
    {
        log_APIError("Unable to create a response object to communicate with Helios");
        Helios_FreeATObject(req);
        DeleteMsgPort(helios_port);
        DeleteMsgPort(fcp_port);
        return;
    }

    req->arq_MsgPort = helios_port;

    /* Let ars to NULL state, because nothing to respond on fcp request */

    sv_port_sig     = 1 << sv_port->mp_SigBit;
    helios_port_sig = 1 << helios_port->mp_SigBit;
    fcp_port_sig    = 1 << fcp_port->mp_SigBit;

    fcp_response_handler.Port = fcp_port;
    fcp_response_handler.From = FCP_RESPONSE_ADDR;
    fcp_response_handler.To   = FCP_RESPONSE_ADDR;
    fcp_response_handler.Mask = -1ULL;

    ObtainSemaphore(&AVC1394Base->Sema);
    AVC1394Base->ServerPort = sv_port;
    ReleaseSemaphore(&AVC1394Base->Sema);

    /* Main loop : handle command messages from client. one by one. */
    run = TRUE;
    do
    {
        WaitPort(sv_port);

        while (NULL != (sv_msg = (APTR) GetMsg(sv_port)))
        {
            HeliosBus *bus;
            HeliosRequestMsg *fcp_msg;
            ULONG retry;
            QUADLET result = 0;

            if (AVC1394_ERR_BYE == sv_msg->Error)
            {
                DB("FCP server has receives dead msg\n");
                ReplyMsg((APTR) sv_msg);
                run = FALSE;
                break;
            }

            DB("FCP command requested: msg @ %p, Data[0] = $%08x\n", sv_msg, sv_msg->Data[0]);

            sv_msg->Error = AVC1394_ERR_NOERR;
            bus = sv_msg->DeviceHandler->Bus;

            /* Enable the FCP response handler */
            if (HELIOS_ERR_NOERR != Helios_RegisterRequestHandler(bus, &fcp_response_handler))
            {
                log_Error("Unable to register a FCP response handler. Kill the FCP server...");
                sv_msg->Error = AVC1394_ERR_BYE;
                ReplyMsg((APTR) sv_msg);
                run = FALSE;
                break;
            }

            /* Try to send command as specified by msg->Retry */
            for (retry=0; retry <= sv_msg->Retry; retry++)
            {
                LONG err;
                QUADLET command;

                command = sv_msg->Data[0];

                /* The AV/C norms says that if the size of an  FCP frame is exaclty 4 bytes,
                 * an asynchronous quadlet write should be used than the asynchronous block write.
                 * But Helios detects that automatically. So we don't take care about that here.
                 */
                err = avc1394_SendCommandBlock(sv_msg->DeviceHandler, req, sv_msg->Data, sv_msg->Length);

                /* Handle ack retry codes */
                if ((HELIOS_ACK_BUSY_X == req->arq_Ack)
                    || (HELIOS_ACK_BUSY_A == req->arq_Ack)
                    || (HELIOS_ACK_BUSY_B == req->arq_Ack))
                {
                    /* just wait a bit before retrying */
                    Delay(1);
                    continue; /* re-send command (if retry counter not exceeded) */
                }

                /* In case of other errors, reports an generic AV/C error code */
                if (HELIOS_ERR_NOERR != err)
                {
                    DB("Helios error reported on SendCommandXXX: %u\n", err);
                    sv_msg->Error = AVC1394_ERR_HELIOS;
                    goto reply_sv_msg;
                }

                /* Now, it's time to wait about a response on the FCP response address */
                result = -1;
                do
                {
                    WaitPort(fcp_port);

                    /* Normally we're waiting only for one response,
                     * but as any hacker node can also use this port, check them all!
                     */
                    while (NULL != (fcp_msg = (APTR) GetMsg(fcp_port)))
                    {
                        ULONG rcode;
                        AVC1394_CmdResponse *response;

                        DB("fcp msg: %p, TCode: %u\n", fcp_msg, fcp_msg->Packet->TCode);

                        /* We only support write quadlet/block responses.
                         * If not the case, we report a TYPE error rcode.
                         */
                        if (TCODE_WRITE_QUADLET_REQUEST == fcp_msg->Packet->TCode)
                        {
                            response = (APTR) &fcp_msg->Packet->Header[3];
                            LE_SWAPLONG_P(response);
                            rcode = HELIOS_RCODE_COMPLETE;
                        }
                        else if (TCODE_WRITE_BLOCK_REQUEST == fcp_msg->Packet->TCode)
                        {
                            response = (APTR) &fcp_msg->Packet->Payload[0];
#if defined(__LITTLE_ENDIAN__) | defined(_LITTLE_ENDIAN)
                            {
                                ULONG i;

                                for (i=0; i < length; i++)
                                {
                                    LE_SWAPLONG_P(&response[i]);
                                }
                            }
#endif
                            rcode = HELIOS_RCODE_COMPLETE;
                        }
                        else
                        {
                            log_Error("FCP response frame received with an unwaited TCode (%u).\n"
                                      "Please email the author with this report!", fcp_msg->Packet->TCode);
                            sv_msg->Error = AVC1394_ERR_SYSTEM;
                            rcode = HELIOS_RCODE_TYPE_ERROR;
                            response = NULL; /* to remove a gcc warning */
                        }

                        /* Here ars is an empty response and as rcode is only HELIOS_RCODE_COMPLETE
                         * for write tcodes, or HELIOS_RCODE_TYPE_ERROR for others, it's ok.
                         * Note: we don't wait about Ack here, reply msg port is not set.
                         * (And we don't wait for a rcode because... it's a response packet :-D)
                         */
                        Helios_ReplyRequestHandlerMsg(fcp_msg, ars, rcode);

                        if (AVC1394_ERR_NOERR == sv_msg->Error)
                        {
                            goto reply_sv_msg;
                        }

                        DB("Result: %08x\n", *(QUADLET *)response);

                        /* No correlated response ?
                         * TODO: some cases have also a differrent opcode -> not handled here
                         */
                        if (((*(QUADLET *)response) & 0x00ffff00) != (command & 0x00ffff00))
                        {
                            DB("Not correlated response detected: command=%06x, response=%06x\n", command, result);
                            ReplyMsg((APTR) fcp_msg);
                            continue;
                        }

                        /* Interim responses are valid only for Notify commands */
                        if (AVC1394_RESPONSE_INTERIM == response->RCode)
                        {
                            DB("Interim response\n");
                            if (AVC1394_CTYPE_NOTIFY != AVC1394_MASK_CTYPE(command))
                            {
                                ReplyMsg((APTR) fcp_msg);
                                continue;
                            }
                        }
                        else if (0 == response->RCode)
                        {
                            DB("0 rcode result, Retry\n");
                            ReplyMsg((APTR) fcp_msg);
                            break;
                        }

                        /* Need to retains fcp data ? */
                        if (TCODE_WRITE_QUADLET_REQUEST == fcp_msg->Packet->TCode)
                        {
                            sv_msg->Data[0] = result;
                            ReplyMsg((APTR) fcp_msg);
                        }
                        else
                        {
                            sv_msg->FCPMsg = fcp_msg;
                        }
                        break;
                    }
                }
                while (-1 == result);

                /* Good ! */
                if (result != 0)
                {
                    break;
                }

                DB("Try %u failed, retry...\n", retry);
            }

            /* Retry over ? */
            if (retry > sv_msg->Retry)
            {
                sv_msg->Error = AVC1394_ERR_RETRY_EXCEEDED;
                log_Error("Retry exceeded\n");
            }

reply_sv_msg:

            /* Remove the request handler (and let Helios handles possible ones) */
            Helios_UnregisterRequestHandler(bus, &fcp_response_handler);
            while (NULL != (fcp_msg = (APTR) GetMsg(fcp_port)))
            {
                /* Reply to any remaining FCP RESPONSE messages by an error RCode */
                Helios_ReplyRequestHandlerMsg(fcp_msg, ars, HELIOS_RCODE_ADDRESS_ERROR);
                ReplyMsg((APTR) fcp_msg);
            }

            DB("reply msg %p (result: $%08x)\n", sv_msg, result);
            ReplyMsg((APTR) sv_msg);
        }
    }
    while (run);

    /* Forbit clients to send new messages on sv_port */
    ObtainSemaphore(&AVC1394Base->Sema);
    AVC1394Base->ServerPort = NULL;
    ReleaseSemaphore(&AVC1394Base->Sema);

    Helios_FreeATObject(req);
    Helios_FreeATObject(ars);

    /* Flush/delete ports */
    while (NULL != (sv_msg = (APTR) GetMsg(sv_port)))
    {
        sv_msg->Error = AVC1394_ERR_BYE;
        ReplyMsg((APTR) sv_msg);
    }
    while (NULL != GetMsg(helios_port));

    DeleteMsgPort(fcp_port);
    DeleteMsgPort(helios_port);
    // sv_port deleted by the system

    DB("FCP server says: byebye!\n");
    return;
}

AVC1394_ServerMsg *avc1394_SendServerMsg(AVC1394_ServerMsg *sv_msg, struct MsgPort *reply_port)
{
    struct MsgPort *sv_port;

    sv_msg->SysMsg.mn_Node.ln_Type = NT_MESSAGE;
    sv_msg->SysMsg.mn_ReplyPort = reply_port;
    sv_msg->SysMsg.mn_Length = sizeof(*sv_msg);

    sv_msg->Error  = AVC1394_ERR_NOERR;
    sv_msg->FCPMsg = NULL;

    ObtainSemaphore(&AVC1394Base->Sema);
    sv_port = AVC1394Base->ServerPort;
    DB("Send msg %p\n", sv_msg);
    if (NULL != sv_port)
    {
        PutMsg(sv_port, (APTR) sv_msg);
    }
    ReleaseSemaphore(&AVC1394Base->Sema);

    if (NULL == sv_port)
    {
        log_Error("Server port unavailable");
        sv_msg = NULL;
    }

    return sv_msg;
}


#if 0

/*---------------------
 * HIGH-LEVEL-FUNCTIONS
 * --------------------
 */

/*
 * Open an AV/C descriptor
 */
int avc1394_open_descriptor(raw1394handle_t handle, nodeid_t node,
                            quadlet_t ctype, quadlet_t subunit,
                            unsigned char *descriptor_identifier, int len_descriptor_identifier,
                            unsigned char readwrite)
{
    quadlet_t  request[2];
    quadlet_t *response;
    unsigned char subfunction = readwrite?
                                AVC1394_OPERAND_DESCRIPTOR_SUBFUNCTION_WRITE_OPEN
                                :AVC1394_OPERAND_DESCRIPTOR_SUBFUNCTION_READ_OPEN;

#ifdef DEBUG
    {
        int i;
        fprintf(stderr, "Open descriptor: ctype: 0x%08X, subunit:0x%08X,\n     descriptor_identifier:", ctype, subunit);
        for (i=0; i<len_descriptor_identifier; i++)
        {
            fprintf(stderr, " 0x%02X", descriptor_identifier[i]);
        }
        fprintf(stderr,"\n");
    }
#endif

    if (len_descriptor_identifier != 1)
    {
        fprintf(stderr, "Unimplemented.\n");
    }
    /*request[0] = ctype | subunit | AVC1394_COMMAND_OPEN_DESCRIPTOR
      | ((*descriptor_identifier & 0xFF00) >> 16);
      request[1] = ((*descriptor_identifier & 0xFF) << 24) | subfunction;*/

    request[0] = ctype | subunit | AVC1394_COMMAND_OPEN_DESCRIPTOR
                 | *descriptor_identifier;
    request[1] = subfunction << 24;
    if (ctype == AVC1394_CTYPE_STATUS)
    {
        request[1] = 0xFF00FFFF;
    }

    response = avc1394_transaction_block(handle, node, request, 2, AVC1394_RETRY);
    if (response == NULL)
    {
        avc1394_transaction_block_close(handle);
        return -1;
    }

#ifdef DEBUG
    fprintf(stderr, "Open descriptor response: 0x%08X.\n", *response);
#endif

    avc1394_transaction_block_close(handle);
    return 0;
}

/*
 * Close an AV/C descriptor
 */
int avc1394_close_descriptor(raw1394handle_t handle, nodeid_t node,
                             quadlet_t ctype, quadlet_t subunit,
                             unsigned char *descriptor_identifier, int len_descriptor_identifier)
{
    quadlet_t  request[2];
    quadlet_t *response;
    unsigned char subfunction = AVC1394_OPERAND_DESCRIPTOR_SUBFUNCTION_CLOSE;

#ifdef DEBUG
    {
        int i;
        fprintf(stderr, "Close descriptor: ctype: 0x%08X, subunit:0x%08X,\n      descriptor_identifier:", ctype, subunit);
        for (i=0; i<len_descriptor_identifier; i++)
        {
            fprintf(stderr, " 0x%02X", descriptor_identifier[i]);
        }
        fprintf(stderr,"\n");
    }
#endif
    if (len_descriptor_identifier != 1)
    {
        fprintf(stderr, "Unimplemented.\n");
    }
    /*request[0] = ctype | subunit | AVC1394_COMMAND_OPEN_DESCRIPTOR
      | ((*descriptor_identifier & 0xFF00) >> 16);
      request[1] = ((*descriptor_identifier & 0xFF) << 24) | subfunction;*/

    request[0] = ctype | subunit | AVC1394_COMMAND_OPEN_DESCRIPTOR
                 | *descriptor_identifier;
    request[1] = subfunction << 24;

    response = avc1394_transaction_block(handle, node, request, 2, AVC1394_RETRY);
    if (response == NULL)
    {
        avc1394_transaction_block_close(handle);
        return -1;
    }

#ifdef DEBUG
    fprintf(stderr, "Close descriptor response: 0x%08X.\n", *response);
#endif

    avc1394_transaction_block_close(handle);
    return 0;
}

/*
 * Read an entire AV/C descriptor
 *
 * IMPORTANT:
 *   Caller must call avc1394_transaction_block_close() when finished with
 *   return value.
 */
unsigned char *avc1394_read_descriptor(raw1394handle_t handle, nodeid_t node,
                                       quadlet_t subunit,
                                       unsigned char *descriptor_identifier, int len_descriptor_identifier)
{
    quadlet_t  request[128];
    quadlet_t *response;

    if (len_descriptor_identifier != 1)
    {
        fprintf(stderr, "Unimplemented.\n");
    }

    memset(request, 0, 128*4);
    request[0] = AVC1394_CTYPE_CONTROL | subunit | AVC1394_COMMAND_READ_DESCRIPTOR
                 | *descriptor_identifier;
    request[1] = 0xFF000000;    /* read entire descriptor */
    request[2] = 0x00000000;    /* beginning from 0x0000 */

    response = avc1394_transaction_block(handle, node, request, 3, AVC1394_RETRY);
    if (response == NULL)
    {
        return NULL;
    }

    return (unsigned char *) response;
}

#endif

/*------------------ LIBRARY CODE SECTION -------------------------*/

void AVC1394_FreeServerMsg(AVC1394_ServerMsg *msg)
{
    if (NULL != msg->FCPMsg)
    {
        ReplyMsg((APTR) msg->FCPMsg);
    }
    FreeMem(msg, sizeof(*msg));
}

AVC1394_ServerMsg *AVC1394_GetUnitInfo(HeliosBus *bus, UWORD nodeid)
{
    HeliosDeviceHandler *handler;
    struct MsgPort *port;
    AVC1394_ServerMsg *msg = NULL;
    QUADLET request[2];

    request[0] = AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_UNIT
                 | AVC1394_SUBUNIT_ID_IGNORE | AVC1394_COMMAND_UNIT_INFO | 0xFF;
    request[1] = 0xFFFFFFFF;

    port = CreateMsgPort();
    if (NULL != port)
    {
        handler = Helios_AllocDeviceHandler(bus);
        if (NULL != handler)
        {
            if (NULL != Helios_ConnectDeviceHandler(handler, HTTAG_NODE_ID, nodeid, TAG_DONE))
            {
                msg = AllocMem(sizeof(*msg), MEMF_PUBLIC);
                if (NULL != msg)
                {
                    msg->DeviceHandler = handler;
                    msg->Retry = AVC1394_RETRY;
                    msg->Length = 2;
                    msg->Data = request;
                    if (NULL != avc1394_SendServerMsg(msg, port))
                    {
                        WaitPort(port);
                        GetMsg(port);

                        if (AVC1394_ERR_NOERR == msg->Error)
                        {
                            DB("UnitInfo of node $%04x: $%08x - $%08x\n",
                               nodeid, msg->FCPMsg->Packet->Payload[0], msg->FCPMsg->Packet->Payload[1]);
                        }
                    }
                    else
                    {
                        log_APIError("failed to send message");
                    }
                }
                else
                {
                    log_APIError("failed to allocate message");
                }
            }
            else
            {
                log_APIError("Helios_ConnectDeviceHandler() failed");
            }

            Helios_FreeDeviceHandler(handler);
        }
        else
        {
            log_APIError("Helios_AllocDeviceHandler() failed");
        }

        DeleteMsgPort(port);
    }
    else
    {
        log_APIError("CreateMsgPort() failed");
    }

    return msg;
}

#define EXTENSION_CODE 7
LONG AVC1394_GetSubUnitInfo(HeliosBus *bus, UWORD nodeid, QUADLET *table)
{
    HeliosDeviceHandler *handler;
    struct MsgPort *port;
    LONG err = AVC1394_ERR_SYSTEM;
    QUADLET request[2];

    port = CreateMsgPort();
    if (NULL != port)
    {
        handler = Helios_AllocDeviceHandler(bus);
        if (NULL != handler)
        {
            if (NULL != Helios_ConnectDeviceHandler(handler, HTTAG_NODE_ID, nodeid, TAG_DONE))
            {
                ULONG page;
                AVC1394_ServerMsg msg;

                msg.DeviceHandler = handler;
                msg.Retry  = AVC1394_RETRY;
                msg.Length = 2;
                msg.Data   = request;

                /* 32 subunits by A/C unit, 4 subunits per page => 8 pages. */
                for (page=0; page < 8; page++)
                {
                    request[0] = AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_UNIT
                                 | AVC1394_SUBUNIT_ID_IGNORE | AVC1394_COMMAND_SUBUNIT_INFO
                                 | page << 4 | EXTENSION_CODE;
                    request[1] = 0xFFFFFFFF;

                    DB("send req for page %u\n", page);
                    if (NULL != avc1394_SendServerMsg(&msg, port))
                    {
                        WaitPort(port);
                        GetMsg(port);

                        err = msg.Error;
                        if (NULL != msg.FCPMsg)
                        {
                            if (AVC1394_RESPONSE_IMPLEMENTED == AVC1394_MASK_RESPONSE(msg.FCPMsg->Packet->Payload[0]))
                            {
                                table[page] = msg.FCPMsg->Packet->Payload[1];
                            }
                            else
                            {
                                table[page] = 0xffffffff;
                            }
                            ReplyMsg((APTR) msg.FCPMsg);
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        log_APIError("failed to send message");
                        break;
                    }
                }
            }
            else
            {
                log_APIError("Helios_ConnectDeviceHandler() failed");
            }

            Helios_FreeDeviceHandler(handler);
        }
        else
        {
            log_APIError("Helios_AllocDeviceHandler() failed");
        }

        DeleteMsgPort(port);
    }
    else
    {
        log_APIError("CreateMsgPort() failed");
    }

    return err;

}

LONG AVC1394_CheckSubUnitType(HeliosBus *bus, UWORD nodeid, LONG type)
{
    QUADLET table[8];
    LONG i, j, entry, id;

    if (AVC1394_ERR_NOERR != AVC1394_GetSubUnitInfo(bus, nodeid, table))
    {
        return 0;
    }

    for (i=0; i<8; i++)
    {
        for (j=3; j>=0; j--)
        {
            entry = (table[i] >> (j * 8)) & 0xFF;
            if (entry == 0xff)
            {
                continue;
            }
            id = entry >> 3;
            //DB("%s: node #%u, id = $%02x\n", __FUNCTION__, nodeid, id);
            if (id == AVC1394_GET_SUBUNIT_TYPE(type))
            {
                return 1;
            }
        }
    }

    return 0;
}

