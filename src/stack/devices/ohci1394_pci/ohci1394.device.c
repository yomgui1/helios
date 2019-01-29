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
** MorphOS OHCI-1394 device interface implemention.
**
*/

#define NDEBUG

#include "ohci1394.device.h"
#include "ohci1394core.h"
#include "ohci1394trans.h"
#include "debug.h"

#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <devices/timer.h>

#include <proto/exec.h>
#include <proto/utility.h>

static OHCI1394Device *devInit(OHCI1394Device *base,
                                BPTR seglist,
                                struct ExecBase *sysbase);
static OHCI1394Device *devOpen(void);
static BPTR devExpunge(void);
static BPTR devClose(void);
static void devBeginIO(void);
static void devAbortIO(void);
static int devReserved(void);

/*------------------ PRIVATE GLOBALS SECTION ----------------------*/

static const ULONG devFuncTable[] = {
    FUNCARRAY_32BIT_NATIVE,

    (ULONG) devOpen,
    (ULONG) devClose,
    (ULONG) devExpunge,
    (ULONG) devReserved,
    (ULONG) devBeginIO,
    (ULONG) devAbortIO,

    ~0
};

static const struct {
    ULONG  LibSize;
    const void  *FuncTable;
    const void  *DataTable;
    void (*InitFunc)(void);
} dev_init = {
    sizeof(OHCI1394Device),
    devFuncTable,
    NULL,
    (void (*)())devInit
};

/*------------------ PUBLIC GLOBALS SECTION -----------------------*/

struct Resident LibResident=
{
    RTC_MATCHWORD,
    &LibResident,
    &LibResident + 1,
    RTF_PPC | RTF_EXTENDED | RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    DEVNAME,
    VERSION_STR,
    (APTR) &dev_init,
    /* New Fields */
    REVISION,
    NULL            /* No More Tags for now*/
};

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *PCIXBase;
struct Library *UtilityBase;
struct Library *HeliosBase;

ULONG __abox__ = 1;
const char *vers = "\0$VER:"DEVNAME" "VERSION_STR"."REVISION_STR" ("DATE") "COPYRIGHTS;


/*------------------ PRIVATE CODE SECTION -------------------------*/

static int devReserved(void) { return 0; }

static OHCI1394Device *devInit(OHCI1394Device *base,
                               BPTR seglist,
                               struct ExecBase *sysbase)
{
    _INFO("+ base=%lx, seglist=%lx, sysbase=%lx\n", base, seglist, sysbase);

    SysBase = sysbase;

    HeliosBase = OpenLibrary("helios.library", 52);
    if (NULL != HeliosBase)
    {
        PCIXBase = OpenLibrary("pcix.library", 50);
        if (NULL != PCIXBase)
        {
            DOSBase = (struct DosLibrary *) OpenLibrary("dos.library", 39);
            if (NULL != DOSBase)
            {
                UtilityBase = OpenLibrary("utility.library", 39);
                if (NULL != UtilityBase)
                {
                    base->hd_MemPool = CreatePool(MEMF_PUBLIC|MEMF_CLEAR, 16384, 4096);
                    if (NULL != base->hd_MemPool)
                    {
                        base->hd_UnitCount = 0;
                        base->hd_SegList = seglist;

                        /* Scan the PCI bus and generate units nodes */
                        if (ohci_ScanPCI(base))
                        {
                            _INFO("- dev.OpenCnt=%ld\n", base->hd_Library.lib_OpenCnt);
                            return base;
                        }
                        else
                            _ERR("PCI scan failed!\n");
                    }
                    else
                        _ERR("CreatePool() failed!\n");

                    CloseLibrary((struct Library *)UtilityBase);
                }
                else
                    _ERR("OpenLibrary(\"utility.library\", 39) failed!\n");

                CloseLibrary((struct Library *)UtilityBase);
            }
            else
                _ERR("OpenLibrary(\"dos.library\", 39) failed!\n");

            CloseLibrary(PCIXBase);
        }
        else
            _ERR("OpenLibrary(\"pcix.library\", 50) failed!\n");

        CloseLibrary(HeliosBase);
    }
    else
        _ERR("OpenLibrary(\"helios.library\", 50) failed!\n");

    _INFO("- Failed\n");
    return NULL;
}

OHCI1394Device *devOpen(void)
{
    LONG unit = REG_D0;
    IOHeliosHWReq  *ioreq = (APTR) REG_A1;
    OHCI1394Device *base = (APTR) REG_A6;
    OHCI1394Device *ret = NULL;
    LONG err;

    _INFO("+ ioreq=%lx, base=%lx\n", ioreq, base);

    ++base->hd_Library.lib_OpenCnt;
    base->hd_Library.lib_Flags &= ~LIBF_DELEXP;

    _INFO("dev.OpenCnt=%ld\n", base->hd_Library.lib_OpenCnt);

    /* Sanity checks */
    if (ioreq->iohh_Req.io_Message.mn_Length < sizeof(IOHeliosHWReq))
    {
        err = IOERR_BADLENGTH;
        _ERR("Bad length\n");
        goto end;
    }

    if ((unit < 0) || (unit > base->hd_UnitCount))
    {
        err = IOERR_OPENFAIL;
        _ERR("Invalid unit number: %ld\n", unit);
        goto end;
    }
    
    /* Open the unit (initialize PCI and 1394 HW at first open) */
    err = ohci_OpenUnit(base, ioreq, unit);
    if (err)
    {
        _ERR("ohci_OpenUnit() failed, err=%ld\n", err);
        goto end;
    }

    /* Open successed */
    err = 0;
    ret = base;

    base->hd_Library.lib_Flags &= ~LIBF_DELEXP;
    ++base->hd_Library.lib_OpenCnt;

  end:
    --base->hd_Library.lib_OpenCnt;
    ioreq->iohh_Req.io_Error = err;

    _INFO("- ret=%p, err=%ld, dev.OpenCnt=%ld\n", ret, err, base->hd_Library.lib_OpenCnt);
    return ret;
}

static BPTR internalExpunge(OHCI1394Device *base) {
    BPTR ret = 0;

    _INFO("+ base=%lx\n", base);

    base->hd_Library.lib_Flags |= LIBF_DELEXP;
    if (base->hd_Library.lib_OpenCnt == 0)
    {
        ret = base->hd_SegList;
        Remove(&base->hd_Library.lib_Node);
        FreeMem((char *)base - base->hd_Library.lib_NegSize,
                base->hd_Library.lib_NegSize + base->hd_Library.lib_PosSize);
    }

    _INFO("-\n");

    return ret;
}

static BPTR devExpunge(void)
{
    OHCI1394Device *base = (APTR) REG_A6;
    return internalExpunge(base);
}

static BPTR devClose(void)
{
    IOHeliosHWReq *ioreq = (APTR) REG_A1;
    OHCI1394Device *base = (APTR) REG_A6;
    BPTR ret = 0;

    _INFO("+ ioreq=%lx, base=%lx, dev.OpenCnt=%ld\n", ioreq, base, base->hd_Library.lib_OpenCnt);

    ohci_CloseUnit(base, ioreq);

    /* Trash user structure to if he want to re-use it */
    ioreq->iohh_Req.io_Unit   = (APTR) -1;
    ioreq->iohh_Req.io_Device = (APTR) -1;

    if (--base->hd_Library.lib_OpenCnt == 0)
    {
        _INFO("Freeing ressources...\n");
        DeletePool(base->hd_MemPool);
        CloseLibrary(UtilityBase);
        CloseLibrary((struct Library *) DOSBase);
        CloseLibrary(PCIXBase);
        CloseLibrary(HeliosBase);

        if (base->hd_Library.lib_Flags & LIBF_DELEXP)
            ret = internalExpunge(base);
    }

    _INFO("- dev.OpenCnt=%ld\n", base->hd_Library.lib_OpenCnt);
    return ret;
}

static void devBeginIO(void)
{
    IOHeliosHWReq *ioreq = (APTR) REG_A1;
    OHCI1394Device *base = (APTR) REG_A6;
    OHCI1394Unit *unit;
    ULONG ret;

    _INFO("+ base=%lx, ioreq=%lx\n", base, ioreq);

    unit = (APTR) ioreq->iohh_Req.io_Unit;

    _INFO("unit: %p, IO Quick: %d\n", unit, ioreq->iohh_Req.io_Flags & IOF_QUICK);

    ioreq->iohh_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->iohh_Req.io_Error = 0;

    LOCK_REGION_SHARED(unit);
    ret = unit->hu_Flags.UnrecoverableError;
    UNLOCK_REGION_SHARED(unit);

    /* Block any command except CMD_RESET if the unit is in an unrecoverable state */
    if (ret && (CMD_RESET != ioreq->iohh_Req.io_Command))
    {
        _ERR("UnrecoverableError\n");
        ioreq->iohh_Req.io_Error = HHIOERR_UNRECOVERABLE_ERROR_STATE;
    }
    else
    {
        //_INFO("cmd = %08x\n", ioreq->iohh_Req.io_Command);
        switch(ioreq->iohh_Req.io_Command)
        {
            case CMD_RESET:
                ret = cmdReset(ioreq, unit, base);
                break;

            case HHIOCMD_QUERYDEVICE:
                ret = cmdQueryDevice(ioreq, unit, base);
                break;

            case HHIOCMD_ENABLE:
                ret = cmdEnable(ioreq, unit, base);
                break;

            case HHIOCMD_DISABLE:
                ret = cmdDisable(ioreq, unit, base);
                break;

            case HHIOCMD_BUSRESET:
                ret = cmdBusReset(ioreq, unit, base);
                break;

            case HHIOCMD_SENDPHY:
                ret = cmdSendPhy(ioreq, unit, base);
                break;
                
            case HHIOCMD_SENDREQUEST:
                ret = cmdSendRequest(ioreq, unit, base);
                break;

            case HHIOCMD_ADDREQHANDLER:
                ret = cmdAddReqHandler(ioreq, unit, base);
                break;

            case HHIOCMD_REMREQHANDLER:
                ret = cmdRemReqHandler(ioreq, unit, base);
                break;

            case HHIOCMD_SETATTRIBUTES:
                ret = cmdSetAttrs(ioreq, unit, base);
                break;

            case HHIOCMD_CREATEISOCONTEXT:
                ret = cmdCreateIsoCtx(ioreq, unit, base);
                break;

            case HHIOCMD_DELETEISOCONTEXT:
                ret = cmdDelIsoCtx(ioreq, unit, base);
                break;

            case HHIOCMD_STARTISOCONTEXT:
                ret = cmdStartIsoCtx(ioreq, unit, base);
                break;

            case HHIOCMD_STOPISOCONTEXT:
                ret = cmdStopIsoCtx(ioreq, unit, base);
                break;

            default:
                _INFO("CMD_INVALID\n");
                ioreq->iohh_Req.io_Error = IOERR_NOCMD;
                ret = 0;
                break;
        }
    }

    if (ret)
        ioreq->iohh_Req.io_Flags &= ~IOF_QUICK;
    else if (!(ioreq->iohh_Req.io_Flags & IOF_QUICK))
        ReplyMsg(&ioreq->iohh_Req.io_Message);

    _INFO("- ret=%lu\n", ret);
}

static void devAbortIO(void)
{
    IOHeliosHWReq *ioreq = (APTR) REG_A1;
    OHCI1394Device *base = (APTR) REG_A6;
    OHCI1394Unit *unit = (OHCI1394Unit *)ioreq->iohh_Req.io_Unit;
    LONG cancelled;

    _INFO("+ base=%lx, ioreq=%lx\n", base, ioreq);

    switch(ioreq->iohh_Req.io_Command)
    {
        case HHIOCMD_SENDREQUEST:
            cancelled = abortSendRequest(ioreq, unit, base);
            break;

        default: /* Other commands can't be cancelled */
            cancelled = FALSE;
            /*if (!(ioreq->iohh_Req.io_Flags & IOF_QUICK))
                ReplyMsg(&ioreq->iohh_Req.io_Message);*/
    }

    if (cancelled)
        ioreq->iohh_Req.io_Error = IOERR_ABORTED;

    _INFO("-\n");
}

