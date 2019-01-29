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
** FCP device core file.
**
*/

#include <exec/libraries.h>
#include <exec/resident.h>
#include <exec/errors.h>

#include <proto/exec.h>

#include <fcp_version.h>

#include "devices/helios/fcp.h"
#include "fcp_log.h"

#include "proto/helios.h"

#define DEBUG_INIT(x, a...)       DB("dev_Init: " x ,##a)
#define DEBUG_OPEN(x, a...)       DB("dev_Open: " x ,##a)
#define DEBUG_CLOSE(x, a...)      DB("dev_Close: " x ,##a)
#define DEBUG_EXPUNGE(x, a...)    DB("dev_Expunge: " x ,##a)
#define DEBUG_NULL(x, a...)       DB(x ,##a)

#define MAX_HANDLED_BUS 8

struct MyDevice
{
    struct Library      Lib;
    UWORD               Pad;
    BPTR                SegList;
    struct
    {
        HeliosBus *          Bus;
        HeliosRequestHandler RequestHandler;
        HeliosRequestHandler ResponseHandler;
        struct MinList       PendingCmdList;
    } Buses[MAX_HANDLED_BUS];
};

struct Library * HeliosBase = NULL;
struct ExecBase * SysBase = NULL;

extern LONG fcp_cmd_Write(struct IOExtFCP *ioreq);
extern void fcp_RequestHandler(HeliosBus *    bus,
                               HeliosPacket * request,
                               HeliosPacket * response,
                               LONG           no_response);
extern void fcp_ResponseHandler(HeliosBus *    bus,
                                HeliosPacket * request,
                                HeliosPacket * response,
                                LONG           no_response);

static struct MyDevice *dev_Init(struct MyDevice * base,
                                 BPTR              seglist,
                                 struct ExecBase * sbase);
static ULONG dev_Expunge(void);
static struct MyDevice *dev_Open(void);
static ULONG dev_Close(void);
static ULONG dev_Reserved(void);
static ULONG cmd_Invalid(struct IOExtFCP *ioreq);
static void dev_BeginIO(void);
static void dev_AbortIO(void);

static const ULONG FuncTable[] =
{
    FUNCARRAY_32BIT_NATIVE,
    (ULONG)dev_Open,
    (ULONG)dev_Close,
    (ULONG)dev_Expunge,
    (ULONG)dev_Reserved,
    (ULONG)dev_BeginIO,
    (ULONG)dev_AbortIO,
    ~0
};

const struct
{
    ULONG        LibSize;
    const void * FuncTable;
    const void * DataTable;
    void       (*InitFunc)(void);
} init =
{
    sizeof(struct MyDevice),
    FuncTable,
    NULL,
    (void (*)()) &dev_Init
};

const struct Resident ROMTag =
{
    RTC_MATCHWORD,
    (APTR) &ROMTag,
    (APTR) (&ROMTag + 1),
    RTF_PPC | RTF_EXTENDED | RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    LIBNAME,
    VSTRING,
    (APTR) &init,
    /* New Fields */
    REVISION,
    NULL            /* No More Tags for now*/
};

const char vers[] = VERSTAG;

ULONG __abox__ = 1;
ULONG __amigappc__ = 1;

/*------------------ PRIVATE CODE SECTION -------------------------*/

static void SysError_NeedLibrary(STRPTR libname, ULONG version)
{
    // TODO
}

static ULONG internal_Expunge(struct MyDevice *base)
{
    BPTR MySegment;

    DEBUG_EXPUNGE("Base 0x%p <%s> OpenCount %ld\n",
                  base,
                  base->Lib.lib_Node.ln_Name,
                  base->Lib.lib_OpenCnt);

    MySegment = base->SegList;

    if (base->Lib.lib_OpenCnt)
    {
        DEBUG_EXPUNGE("set LIBF_DELEXP\n");
        base->Lib.lib_Flags |= LIBF_DELEXP;
        return NULL;
    }

    DEBUG_EXPUNGE("remove the device\n");
    Remove(&base->Lib.lib_Node);

    if (NULL != HeliosBase)
    {
        CloseLibrary(HeliosBase);
    }

    DEBUG_EXPUNGE("free the library\n");
    FreeMem((APTR)((ULONG)(base) - (ULONG)(base->Lib.lib_NegSize)),
            base->Lib.lib_NegSize + base->Lib.lib_PosSize);

    DEBUG_EXPUNGE("return Segment 0x%lx to ramlib\n", MySegment);
    return (ULONG) MySegment;
}


/*------------------ DEVICE CODE SECTION -------------------------*/

static struct MyDevice *dev_Init(struct MyDevice * base,
                                 BPTR              seglist,
                                 struct ExecBase * sbase)
{
    DEBUG_INIT("Base 0x%p SegList 0x%lx SysBase 0x%p\n", base, seglist, sbase);

    SysBase = sbase;
    base->SegList = seglist;

    memset(base->Buses, 0, sizeof(base->Buses));

    /* Open needed resources */
    HeliosBase = OpenLibrary("helios.library", 0);
    if (NULL != HeliosBase)
    {
        return base;
    }
    else
    {
        SysError_NeedLibrary("helios.library", 0);
    }

    FreeMem((APTR)((ULONG)(base) - (ULONG)(base->Lib.lib_NegSize)),
            base->Lib.lib_NegSize + base->Lib.lib_PosSize);
    return NULL;
}

static ULONG dev_Expunge(void)
{
    return internal_Expunge((APTR) REG_A6);
}

static struct MyDevice *dev_Open(void)
{
    ULONG unit = REG_D0;
    struct IOExtFCP *ioreq = (APTR) REG_A1;
    struct MyDevice *base = (APTR) REG_A6;
    struct MyDevice *ret = NULL;
    HeliosBridge *bridge;
    HeliosBus *bus = NULL;
    HeliosRequestHandler *fcp_request_handler = NULL, *fcp_response_handler = NULL;
    LONG err;

    DEBUG_OPEN("0x%p <%s> OpenCount %ld\n",
               base,
               base->Lib.lib_Node.ln_Name,
               base->Lib.lib_OpenCnt);

    ++base->Lib.lib_OpenCnt;
    base->Lib.lib_Flags &= ~LIBF_DELEXP;

    if (ioreq->SysReq.io_Message.mn_Length < sizeof(struct IOExtFCP))
    {
        err = IOERR_BADLENGTH;
        log_Error("Bad length");
        goto end;
    }

    if (unit >= MAX_HANDLED_BUS)
    {
        err = IOERR_OPENFAIL;
        log_Error("Bad unit number");
        goto end;
    }

    bus = base->Buses[unit].Bus;
    if (NULL == bus)
    {
        /* Find wanted unit bridge */
        bridge = NULL;
        while ((NULL != (bridge = Helios_FindNextBridge(bridge))) && (unit-- > 0));

        if (NULL == bridge)
        {
            err = IOERR_OPENFAIL;
            log_Error("unit %u not found", unit);
            goto end;
        }

        /* Obtain the associated bus */
        bus = Helios_HandleBridge(bridge);
        if (NULL == bus)
        {
            err = IOERR_OPENFAIL;
            log_Error("unable to obtain bus for unit %lu", unit);
            goto end;
        }

        base->Buses[unit].Bus = bus;
    }

    fcp_request_handler           = &base->Buses[unit].RequestHandler;
    fcp_request_handler->Callback = fcp_RequestHandler;
    fcp_request_handler->From     = FCP_REQUEST_ADDR;
    fcp_request_handler->To       = FCP_REQUEST_ADDR;
    //fcp_request_handler->UserData = base->Buses[unit].RequestPort;

    if (HELIOS_ERR_NOERR != Helios_RegisterRequestHandler(bus, fcp_request_handler))
    {
        err = IOERR_OPENFAIL;
        log_Error("unable to register FCP request handler for unit %u", unit);
        goto end;
    }

    fcp_response_handler           = &base->Buses[unit].ResponseHandler;
    fcp_response_handler->Callback = fcp_ResponseHandler;
    fcp_response_handler->From     = FCP_RESPONSE_ADDR;
    fcp_response_handler->To       = FCP_RESPONSE_ADDR;
    fcp_response_handler->UserData = &base->Buses[unit].PendingCmdList;

    NEWLIST(fcp_response_handler->UserData);

    if (HELIOS_ERR_NOERR != Helios_RegisterRequestHandler(bus, fcp_response_handler))
    {
        err = IOERR_OPENFAIL;
        log_Error("unable to register FCP response handler for unit %u", unit);
        goto end;
    }

    ioreq->Bus = bus;
    ioreq->Unit = unit;

    err = 0;
    ret = base;
    ++base->Lib.lib_OpenCnt;
end:
    --base->Lib.lib_OpenCnt;
    DEBUG_OPEN("returned: %lx, %ld\n", ret, err);
    ioreq->SysReq.io_Error = err;
    if ((0 != err) && (NULL != bus))
    {
        if (NULL != fcp_request_handler)
        {
            Helios_UnregisterRequestHandler(bus, fcp_request_handler);
        }
        if (NULL != fcp_response_handler)
        {
            Helios_UnregisterRequestHandler(bus, fcp_response_handler);
        }
        Helios_ReleaseBus(bus);
    }
    return ret;
}

static ULONG dev_Close(void)
{
    struct IOExtFCP *ioreq = (APTR)REG_A1;
    struct MyDevice *base = (APTR)REG_A6;

    DEBUG_CLOSE("0x%p <%s> OpenCount %ld\n",
                base,
                base->Lib.lib_Node.ln_Name,
                base->Lib.lib_OpenCnt);

    ioreq->SysReq.io_Device = (APTR) -1;

    if ((--base->Lib.lib_OpenCnt) > 0)
    {
        DEBUG_CLOSE("done\n");
    }
    else
    {
        DEBUG_CLOSE("Release bus: %p, unit %u\n", ioreq->Bus, ioreq->Unit);
        Helios_UnregisterRequestHandler(ioreq->Bus, &base->Buses[ioreq->Unit].RequestHandler);
        Helios_UnregisterRequestHandler(ioreq->Bus, &base->Buses[ioreq->Unit].ResponseHandler);
        Helios_ReleaseBus(ioreq->Bus);
        ioreq->Bus = NULL;

        if (base->Lib.lib_Flags & LIBF_DELEXP)
        {
            DEBUG_CLOSE("LIBF_DELEXP set\n");
            return internal_Expunge(base);
        }
    }

    return 0;
}

static ULONG dev_Reserved(void)
{
    DEBUG_NULL("FCP device: dev_Reserved called\n");
    return 0;
}

static ULONG cmd_Invalid(struct IOExtFCP *ioreq)
{
    log_Error("CMD_INVALID received: %u", ioreq->SysReq.io_Command);
    ioreq->SysReq.io_Error = IOERR_NOCMD;
    return 0;
}

static void dev_BeginIO(void)
{
    struct IOExtFCP *ioreq = (APTR) REG_A1;
    struct MyDevice *base = (APTR) REG_A6;
    ULONG ret = 0;

    DEBUG_NULL("BeginIO(%lx, %lx, %ld)\n", base, ioreq, ioreq->SysReq.io_Command);

    ioreq->SysReq.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->SysReq.io_Error = 0;

    switch(ioreq->SysReq.io_Command)
    {
        case CMD_INVALID:
        case CMD_RESET:
        case CMD_READ:
        case CMD_UPDATE:
        case CMD_CLEAR:
        case CMD_STOP:
        case CMD_START:
        case CMD_FLUSH:
            ret = cmd_Invalid(ioreq);
            break;

        case CMD_WRITE:
            ret = fcp_cmd_Write(ioreq);
            break;

        default:
            ioreq->SysReq.io_Error = IOERR_NOCMD;
            break;
    }

    if (0 != ret)
    {
        ioreq->SysReq.io_Flags &= ~IOF_QUICK;
    }
    else if (!(ioreq->SysReq.io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->SysReq.io_Message);
    }
}

static void dev_AbortIO(void)
{
    struct IOExtFCP *ioreq = (APTR) REG_A1;
    struct MyDevice *base = (APTR) REG_A6;
    BOOL abort = FALSE;

    switch(ioreq->SysReq.io_Command)
    {
        case CMD_WRITE:
            DEBUG_NULL("Aborting FCP write command\n");
            break;
    }

    if (abort)
    {
        ioreq->SysReq.io_Error = IOERR_ABORTED;
        if (!(ioreq->SysReq.io_Flags & IOF_QUICK))
        {
            DEBUG_NULL("Replying\n");
            ReplyMsg(&ioreq->SysReq.io_Message);
        }
    }
}

