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
** Global definitions header file for the AV/C 1394.
**
*/

#ifndef AVC1394_COMMON_H
#define AVC1394_COMMON_H

#include "libraries/avc1394.h"
#include "clib/avc1394_protos.h"
#include "proto/helios.h"

#include "avc1394_log.h"

/* FCP Register Space */
#define FCP_COMMAND_ADDR  0xFFFFF0000B00ULL
#define FCP_RESPONSE_ADDR 0xFFFFF0000D00ULL

#define AVC1394_RETRY 2

extern AVC1394_ServerMsg *avc1394_SendServerMsg(AVC1394_ServerMsg *sv_msg, struct MsgPort *reply_port);

#endif
