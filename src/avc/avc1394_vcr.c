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
** VCR core API for AV/C 1394.
**
*/

#include "avc1394_common.h"

#include <proto/exec.h>

#define CTLVCR0 AVC1394_CTYPE_CONTROL | AVC1394_SUBUNIT_TYPE_TAPE_RECORDER | AVC1394_SUBUNIT_ID_0
#define STATVCR0 AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_TAPE_RECORDER | AVC1394_SUBUNIT_ID_0
#define CTLTUNER0 AVC1394_CTYPE_CONTROL | AVC1394_SUBUNIT_TYPE_TUNER | AVC1394_SUBUNIT_ID_0
#define STATTUNER0 AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_TUNER | AVC1394_SUBUNIT_ID_0
#define TUNER0 AVC1394_SUBUNIT_TYPE_TUNER | AVC1394_SUBUNIT_ID_0
#define CTLUNIT AVC1394_CTYPE_CONTROL | AVC1394_SUBUNIT_TYPE_UNIT | AVC1394_SUBUNIT_ID_IGNORE
#define STATUNIT AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_UNIT | AVC1394_SUBUNIT_ID_IGNORE

/*------------------ PRIVATE CODE SECTION -------------------------*/

static LONG vcr_SendCommand(HeliosBus *bus, ULONG nodeid, QUADLET command, QUADLET *result)
{
    HeliosDeviceHandler *handler;
    struct MsgPort *port;
    AVC1394_ServerMsg msg;
    LONG err = AVC1394_ERR_SYSTEM;

    port = CreateMsgPort();
    if (NULL != port)
    {
        handler = Helios_AllocDeviceHandler(bus);
        if (NULL != handler)
        {
            if (NULL != Helios_ConnectDeviceHandler(handler, HTTAG_NODE_ID, nodeid, TAG_DONE))
            {
                msg.DeviceHandler = handler;
                msg.Retry = AVC1394_RETRY;
                msg.Length = 1;
                msg.Data = &command;
                if (NULL != avc1394_SendServerMsg(&msg, port))
                {
                    WaitPort(port);
                    GetMsg(port);
                    err = msg.Error;
                    *result = command;
                }
                else
                {
                    log_APIError("failed to send message");
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

static LONG vcr_DoTransaction(HeliosBus *bus, ULONG nodeid, QUADLET command, QUADLET *result)
{
    HeliosDeviceHandler *handler;
    struct MsgPort *port;
    AVC1394_ServerMsg msg;
    QUADLET request[2];
    LONG err = AVC1394_ERR_SYSTEM;

    request[0] = command;
    request[1] = 0xFFFFFFFF;

    port = CreateMsgPort();
    if (NULL != port)
    {
        handler = Helios_AllocDeviceHandler(bus);
        if (NULL != handler)
        {
            if (NULL != Helios_ConnectDeviceHandler(handler, HTTAG_NODE_ID, nodeid, TAG_DONE))
            {
                msg.DeviceHandler = handler;
                msg.Retry = AVC1394_RETRY;
                msg.Length = 2;
                msg.Data = request;
                if (NULL != avc1394_SendServerMsg(&msg, port))
                {
                    WaitPort(port);
                    GetMsg(port);

                    err = msg.Error;
                    if (NULL != msg.FCPMsg)
                    {
                        *result = msg.FCPMsg->Packet->Payload[1];
                        ReplyMsg((APTR) msg.FCPMsg);
                    }
                }
                else
                {
                    log_APIError("failed to send message");
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


/*------------------ LIBRARY CODE SECTION -------------------------*/

LONG AVC1394_VCR_Eject(HeliosBus *bus, UWORD nodeid, QUADLET *status)
{
    return vcr_SendCommand(bus, nodeid, CTLVCR0 | AVC1394_VCR_COMMAND_LOAD_MEDIUM | AVC1394_VCR_OPERAND_LOAD_MEDIUM_EJECT, status);
}

LONG AVC1394_VCR_Stop(HeliosBus *bus, UWORD nodeid, QUADLET *status)
{
    return vcr_SendCommand(bus, nodeid, CTLVCR0 | AVC1394_VCR_COMMAND_WIND | AVC1394_VCR_OPERAND_WIND_STOP, status);
}

LONG AVC1394_VCR_IsPlaying(HeliosBus *bus, UWORD nodeid)
{
    ULONG err;
    QUADLET result;

    err = vcr_SendCommand(bus, nodeid,
                          STATVCR0| AVC1394_VCR_COMMAND_TRANSPORT_STATE | AVC1394_VCR_OPERAND_TRANSPORT_STATE, &result);
    if ((AVC1394_ERR_NOERR == err) && (AVC1394_VCR_RESPONSE_TRANSPORT_STATE_PLAY == AVC1394_MASK_OPCODE(result)))
    {
        return AVC1394_GET_OPERAND0(result);
    }
    else
    {
        return 0;
    }
}

LONG AVC1394_VCR_IsRecording(HeliosBus *bus, UWORD nodeid)
{
    ULONG err;
    QUADLET result;

    err = vcr_SendCommand(bus, nodeid,
                          STATVCR0 | AVC1394_VCR_COMMAND_TRANSPORT_STATE | AVC1394_VCR_OPERAND_TRANSPORT_STATE, &result);
    if ((AVC1394_ERR_NOERR == err) && (AVC1394_VCR_RESPONSE_TRANSPORT_STATE_RECORD == AVC1394_MASK_OPCODE(result)))
    {
        return AVC1394_GET_OPERAND0(result);
    }
    else
    {
        return 0;
    }
}

LONG AVC1394_VCR_Play(HeliosBus *bus, UWORD nodeid, QUADLET *status)
{
    if (AVC1394_VCR_OPERAND_PLAY_FORWARD == AVC1394_VCR_IsPlaying(bus, nodeid))
    {
        return vcr_SendCommand(bus, nodeid, CTLVCR0 | AVC1394_VCR_COMMAND_PLAY | AVC1394_VCR_OPERAND_PLAY_SLOWEST_FORWARD, status);
    }
    else
    {
        return vcr_SendCommand(bus, nodeid, CTLVCR0 | AVC1394_VCR_COMMAND_PLAY | AVC1394_VCR_OPERAND_PLAY_FORWARD, status);
    }
}

LONG AVC1394_VCR_Reverse(HeliosBus *bus, UWORD nodeid, QUADLET *status)
{
    if (AVC1394_VCR_OPERAND_PLAY_REVERSE == AVC1394_VCR_IsPlaying(bus, nodeid))
    {
        return vcr_SendCommand(bus, nodeid, CTLVCR0 | AVC1394_VCR_COMMAND_PLAY | AVC1394_VCR_OPERAND_PLAY_SLOWEST_REVERSE, status);
    }
    else
    {
        return vcr_SendCommand(bus, nodeid, CTLVCR0 | AVC1394_VCR_COMMAND_PLAY | AVC1394_VCR_OPERAND_PLAY_REVERSE, status);
    }
}

LONG AVC1394_VCR_Pause(HeliosBus *bus, UWORD nodeid, QUADLET *status)
{
    LONG mode;

    mode = AVC1394_VCR_IsRecording(bus, nodeid);
    if (mode)
    {
        if (AVC1394_VCR_OPERAND_RECORD_PAUSE == mode)
        {
            return vcr_SendCommand(bus, nodeid, CTLVCR0 | AVC1394_VCR_COMMAND_RECORD | AVC1394_VCR_OPERAND_RECORD_RECORD, status);
        }
        else
        {
            return vcr_SendCommand(bus, nodeid, CTLVCR0 | AVC1394_VCR_COMMAND_RECORD | AVC1394_VCR_OPERAND_RECORD_PAUSE, status);
        }
    }
    else
    {
        if (AVC1394_VCR_OPERAND_PLAY_FORWARD_PAUSE == AVC1394_VCR_IsPlaying(bus, nodeid))
        {
            return vcr_SendCommand(bus, nodeid, CTLVCR0 | AVC1394_VCR_COMMAND_PLAY | AVC1394_VCR_OPERAND_PLAY_FORWARD, status);
        }
        else
        {
            return vcr_SendCommand(bus, nodeid, CTLVCR0 | AVC1394_VCR_COMMAND_PLAY | AVC1394_VCR_OPERAND_PLAY_FORWARD_PAUSE, status);
        }
    }
}


#if 0
QUADLET AVC1394_VCR_Status(HeliosBus bus, ULONG generation, ULONG id)
{
    return AVC1394_DoTransaction(bus, generation, id, AVC1394_RETRY,
                                 STATVCR0
                                 | AVC1394_VCR_COMMAND_TRANSPORT_STATE
                                 | AVC1394_VCR_OPERAND_TRANSPORT_STATE);

}

int avc1394_vcr_is_recording(HeliosDevice dev)
{
    quadlet_t response = avc1394_transaction(handle, node, STATVCR0
                                             | AVC1394_VCR_COMMAND_TRANSPORT_STATE | AVC1394_VCR_OPERAND_TRANSPORT_STATE,
                                             AVC1394_RETRY);
    if (AVC1394_MASK_OPCODE(response)
        == AVC1394_VCR_RESPONSE_TRANSPORT_STATE_RECORD)
    {
        return AVC1394_GET_OPERAND0(response);
    }
    else
    {
        return 0;
    }
}


void avc1394_vcr_trick_play(raw1394handle_t handle, nodeid_t node, int speed)
{
    if (!avc1394_vcr_is_recording(handle, node))
    {
        if (speed == 0)
        {
            avc1394_send_command(handle, node, CTLVCR0
                                 | AVC1394_VCR_COMMAND_PLAY | AVC1394_VCR_OPERAND_PLAY_FORWARD);
        }
        else if (speed > 0)
        {
            if (speed > 14)
            {
                speed = 14;
            }
            avc1394_send_command(handle, node, CTLVCR0
                                 | AVC1394_VCR_COMMAND_PLAY | (AVC1394_VCR_OPERAND_PLAY_NEXT_FRAME + speed));
        }
        else
        {
            if (speed < -14)
            {
                speed = -14;
            }
            avc1394_send_command(handle, node, CTLVCR0
                                 | AVC1394_VCR_COMMAND_PLAY | (AVC1394_VCR_OPERAND_PLAY_PREVIOUS_FRAME - speed));
        }
    }
}


void avc1394_vcr_rewind(raw1394handle_t handle, nodeid_t node)
{
    if (avc1394_vcr_is_playing(handle, node))
    {
        avc1394_send_command(handle, node, CTLVCR0
                             | AVC1394_VCR_COMMAND_PLAY | AVC1394_VCR_OPERAND_PLAY_FASTEST_REVERSE);
    }
    else
    {
        avc1394_send_command(handle, node, CTLVCR0
                             | AVC1394_VCR_COMMAND_WIND | AVC1394_VCR_OPERAND_WIND_REWIND);
    }
}

void avc1394_vcr_forward(raw1394handle_t handle, nodeid_t node)
{
    if (avc1394_vcr_is_playing(handle, node))
    {
        avc1394_send_command(handle, node, CTLVCR0
                             | AVC1394_VCR_COMMAND_PLAY | AVC1394_VCR_OPERAND_PLAY_FASTEST_FORWARD);
    }
    else
    {
        avc1394_send_command(handle, node, CTLVCR0
                             | AVC1394_VCR_COMMAND_WIND | AVC1394_VCR_OPERAND_WIND_FAST_FORWARD);

    }
}


void avc1394_vcr_next(raw1394handle_t handle, nodeid_t node)
{
    if (avc1394_vcr_is_playing(handle, node))
    {
        avc1394_send_command(handle, node, CTLVCR0
                             | AVC1394_VCR_COMMAND_PLAY | AVC1394_VCR_OPERAND_PLAY_NEXT_FRAME);
    }
}

void avc1394_vcr_next_index(raw1394handle_t handle, nodeid_t node)
{
    quadlet_t request[2];
    if (avc1394_vcr_is_playing(handle, node))
    {
        request[0] = CTLVCR0 | AVC1394_VCR_COMMAND_FORWARD |
                     AVC1394_VCR_MEASUREMENT_INDEX;
        request[1] = 0x01FFFFFF;
        avc1394_send_command_block(handle, node, request, 2);
    }
}

void avc1394_vcr_previous(raw1394handle_t handle, nodeid_t node)
{
    if (avc1394_vcr_is_playing(handle, node))
    {
        avc1394_send_command(handle, node, CTLVCR0
                             | AVC1394_VCR_COMMAND_PLAY | AVC1394_VCR_OPERAND_PLAY_PREVIOUS_FRAME);
    }
}

void avc1394_vcr_previous_index(raw1394handle_t handle, nodeid_t node)
{
    quadlet_t request[2];
    if (avc1394_vcr_is_playing(handle, node))
    {
        request[0] = CTLVCR0 | AVC1394_VCR_COMMAND_BACKWARD |
                     AVC1394_VCR_MEASUREMENT_INDEX;
        request[1] = 0x01FFFFFF;
        avc1394_send_command_block(handle, node, request, 2);
    }
}


void avc1394_vcr_record(raw1394handle_t handle, nodeid_t node)
{
    avc1394_send_command(handle, node, CTLVCR0
                         | AVC1394_VCR_COMMAND_RECORD | AVC1394_VCR_OPERAND_RECORD_RECORD);
}

char *avc1394_vcr_decode_status(quadlet_t response)
{
    /*quadlet_t resp0 = AVC1394_MASK_RESPONSE_OPERAND(response, 0);
    quadlet_t resp1 = AVC1394_MASK_RESPONSE_OPERAND(response, 1);*/
    quadlet_t resp2 = AVC1394_MASK_RESPONSE_OPERAND(response, 2);
    quadlet_t resp3 = AVC1394_MASK_RESPONSE_OPERAND(response, 3);

    if (response == 0)
    {
        return "OK";
    }
    else if (resp2 == AVC1394_VCR_RESPONSE_TRANSPORT_STATE_LOAD_MEDIUM)
    {
        return("Loading Medium");
    }
    else if (resp2 == AVC1394_VCR_RESPONSE_TRANSPORT_STATE_RECORD)
    {
        if (resp3 == AVC1394_VCR_OPERAND_RECORD_PAUSE)
        {
            return("Recording Paused");
        }
        else
        {
            return("Recording");
        }
    }
    else if (resp2 == AVC1394_VCR_RESPONSE_TRANSPORT_STATE_PLAY)
    {
        if (resp3 >= AVC1394_VCR_OPERAND_PLAY_FAST_FORWARD_1
            && resp3 <= AVC1394_VCR_OPERAND_PLAY_FASTEST_FORWARD)
        {
            return("Playing Fast Forward");
        }
        else if (resp3 >= AVC1394_VCR_OPERAND_PLAY_FAST_REVERSE_1
                 && resp3 <= AVC1394_VCR_OPERAND_PLAY_FASTEST_REVERSE)
        {
            return("Playing Reverse");
        }
        else if (resp3 == AVC1394_VCR_OPERAND_PLAY_FORWARD_PAUSE)
        {
            return("Playing Paused");
        }
        else
        {
            return("Playing");
        }
    }
    else if (resp2 == AVC1394_VCR_RESPONSE_TRANSPORT_STATE_WIND)
    {
        if (resp3 == AVC1394_VCR_OPERAND_WIND_HIGH_SPEED_REWIND)
        {
            return("Winding backward at incredible speed");
        }
        else if (resp3 == AVC1394_VCR_OPERAND_WIND_STOP)
        {
            return("Winding stopped");
        }
        else if (resp3 == AVC1394_VCR_OPERAND_WIND_REWIND)
        {
            return("Winding reverse");
        }
        else if (resp3 == AVC1394_VCR_OPERAND_WIND_FAST_FORWARD)
        {
            return("Winding forward");
        }
        else
        {
            return("Winding");
        }
    }
    else
    {
        return("Unknown");
    }
}

/* Get the time code on tape in format HH:MM:SS:FF */
char *
avc1394_vcr_get_timecode(raw1394handle_t handle, nodeid_t node)
{
    quadlet_t  request[2];
    quadlet_t *response;
    char      *output = NULL;

    request[0] = STATVCR0 | AVC1394_VCR_COMMAND_TIME_CODE |
                 AVC1394_VCR_OPERAND_TIME_CODE_STATUS;
    request[1] = 0xFFFFFFFF;
    response = avc1394_transaction_block(handle, node, request, 2, AVC1394_RETRY);
    if (response == NULL || response[1] == 0xffffffff)
    {
        avc1394_transaction_block_close(handle);
        return NULL;
    }

    output = malloc(12);
    if (output)
        // consumer timecode format
        sprintf(output, "%2.2x:%2.2x:%2.2x:%2.2x",
                response[1] & 0x000000ff,
                (response[1] >> 8) & 0x000000ff,
                (response[1] >> 16) & 0x000000ff,
                (response[1] >> 24) & 0x000000ff);

    avc1394_transaction_block_close(handle);
    return output;
}

/* Get the time code on tape in format HH:MM:SS:FF */
int
avc1394_vcr_get_timecode2(raw1394handle_t handle, nodeid_t node, char *output)
{
    quadlet_t  request[2];
    quadlet_t *response;

    request[0] = STATVCR0 | AVC1394_VCR_COMMAND_TIME_CODE |
                 AVC1394_VCR_OPERAND_TIME_CODE_STATUS;
    request[1] = 0xFFFFFFFF;
    response = avc1394_transaction_block(handle, node, request, 2, AVC1394_RETRY);
    if (response == NULL || response[1] == 0xffffffff)
    {
        avc1394_transaction_block_close(handle);
        return -1;
    }

    // consumer timecode format
    sprintf(output, "%2.2x:%2.2x:%2.2x:%2.2x",
            response[1] & 0x000000ff,
            (response[1] >> 8) & 0x000000ff,
            (response[1] >> 16) & 0x000000ff,
            (response[1] >> 24) & 0x000000ff);

    avc1394_transaction_block_close(handle);
    return 0;
}


/* Go to the time code on tape in format HH:MM:SS:FF */
void
avc1394_vcr_seek_timecode(raw1394handle_t handle, nodeid_t node, char *timecode)
{
    quadlet_t  request[2];
    unsigned int hh,mm,ss,ff;

    request[0] = CTLVCR0 | AVC1394_VCR_COMMAND_TIME_CODE |
                 AVC1394_VCR_OPERAND_TIME_CODE_CONTROL;

    // consumer timecode format
    sscanf(timecode, "%2x:%2x:%2x:%2x", &hh, &mm, &ss, &ff);
    request[1] =
        ((ff & 0x000000ff) << 24) |
        ((ss & 0x000000ff) << 16) |
        ((mm & 0x000000ff) <<  8) |
        ((hh & 0x000000ff) <<  0) ;
    printf("timecode: %08x\n", request[1]);

    avc1394_send_command_block( handle, node, request, 2);
}

#endif
