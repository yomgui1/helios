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
** AVC1394 AV/C 1394 Library structure header file.
**
*/

#ifndef AVC1394_LIBDATA_H
#define AVC1394_LIBDATA_H

#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/ports.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/semaphores.h>
#include <exec/execbase.h>
#include <exec/alerts.h>
#include <exec/libraries.h>
#include <exec/interrupts.h>
#include <exec/resident.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/utility.h>

#include <clib/debug_protos.h>

struct AVC1394Library
{
    struct Library          Lib;
    BPTR                    SegList;
    struct ExecBase *       MySysBase;
    struct Library *        HeliosBase;
    struct SignalSemaphore  Sema;
    struct MsgPort *        ServerPort;
};

extern struct AVC1394Library *AVC1394Base;

#endif /* AVC1394_LIBDATA_H */
