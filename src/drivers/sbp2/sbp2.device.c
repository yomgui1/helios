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
** MorphOS SBP2-device driver implemention.
**
*/

//#define NDEBUG
//#define DEBUG_LIB

#include "sbp2.device.h"
#include "sbp2.class.h"

#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <devices/timer.h>
#include <libraries/query.h>
#include <devices/trackdisk.h>

#include <proto/exec.h>
#include <proto/mount.h>

static SBP2Device *devOpen(void);
static BPTR devExpunge(void);
static BPTR devClose(void);
static void devBeginIO(void);
static void devAbortIO(void);
static int devQuery(void);
static struct DiskPort *devCreateDiskPort(void);
static void devDeleteDiskPort(void);
static ULONG devInquiry(void);
static ULONG devMount(void);
static void devDisMount(void);
static ULONG devReserved(void);

#define MountBase   (base->dv_MountBase)
#define SysBase     (base->dv_SysBase)
#define UtilityBase (base->dv_SBP2ClassBase->hc_UtilityBase)

/*------------------ PRIVATE GLOBALS SECTION ----------------------*/

const ULONG devFuncTable[] =
{
    FUNCARRAY_32BIT_NATIVE,

    (ULONG) devOpen,
    (ULONG) devClose,
    (ULONG) devExpunge,
    (ULONG) devQuery,
    (ULONG) devBeginIO,
    (ULONG) devAbortIO,
    (ULONG) devReserved, //devCreateDiskPort
    (ULONG) devReserved, //devDeleteDiskPort
    (ULONG) devReserved, //devInquiry
    (ULONG) devMount,
    (ULONG) devDisMount,

    ~0
};

static struct TagItem QueryTags[] =
{
	{QUERYINFOATTR_NAME, (ULONG) DEVNAME},
	{QUERYINFOATTR_IDSTRING, (ULONG) VSTRING},
    {QUERYINFOATTR_COPYRIGHT, (ULONG) "(c) 2008-2010 by Guillaume ROGUEZ"},
    {QUERYINFOATTR_AUTHOR, (ULONG) "Guillaume ROGUEZ"},
    {QUERYINFOATTR_RELEASETAG, (ULONG) "beta"},
	{QUERYINFOATTR_DESCRIPTION, (ULONG) "SBP2 class for Helios"},
	{QUERYINFOATTR_VERSION, VERSION},
	{QUERYINFOATTR_REVISION, REVISION},
	{QUERYINFOATTR_CODETYPE, MACHINE_PPC},
	{QUERYINFOATTR_SUBTYPE, QUERYSUBTYPE_DEVICE},
	{QUERYINFOATTR_CLASS, QUERYCLASS_STORAGE},
    {QUERYINFOATTR_SUBCLASS, QUERYSUBCLASS_STORAGE_FIREWIRE},
	{QUERYINFOATTR_DEVICE_UNITS, 64},
	{QUERYINFOATTR_DEVICE_LUNS, 1},
	{TAG_DONE,0}
};

/*------------------- PUBLIC CODE SECTION -------------------------*/

SBP2Device *devInit(SBP2Device *base,
                    BPTR seglist,
                    struct ExecBase *sbase)
{
    _INFO_LIB("Base=%p, SysBase=%p\n", base, sbase);
    SysBase = sbase;

    MountBase = OpenLibrary("mount.library", 50);
    if (NULL == MountBase)
        return NULL;

    return base;
}

BPTR devCleanup(SBP2Device *base)
{
    _INFO_LIB("Remove from mem device %s\n", DEVNAME);

    Forbid();
    REMOVE(&base->dv_Library.lib_Node);
    Permit();

    CloseLibrary(MountBase);

    FreeMem((char *)base - base->dv_Library.lib_NegSize,
            base->dv_Library.lib_NegSize + base->dv_Library.lib_PosSize);

    return 0;
}

static ULONG devReserved(void)
{
    return 0;
}


static LONG sbp2_io_addchangeint(SBP2Device *base, SBP2Unit *unit, struct IOStdReq *ioreq)
{
    APTR node=NULL;
    struct Interrupt *intr;

    ioreq->io_Actual = 0;
    intr = (struct Interrupt *)ioreq->io_Data;

    LOCK_REGION_SHARED(unit);
    {
        if (NULL != unit->u_NotifyUnit)
        {
            node = MountCreateNotifyNodeTags(unit->u_NotifyUnit,
                 							 MOUNTATTR_INTERRUPT,   (ULONG)intr,
                 							 MOUNTATTR_IOREQUEST,   (ULONG)ioreq,
                 							 MOUNTATTR_TASK,        (ULONG)ioreq->io_Message.mn_ReplyPort->mp_SigTask,
                 							 MOUNTATTR_CHANGESTATE, unit->u_Flags.Ready ? 0:~0,
                 							 TAG_END);
        }
        else
            node = NULL;
    }
    UNLOCK_REGION_SHARED(unit);

    if (NULL != node)
    {
        sbp2_incref(unit);
        ioreq->io_Error	= 0;
	    return RC_DONTREPLY;
    }

    return (ioreq->io_Error = TDERR_NoMem);
}

static LONG sbp2_io_remchangeint(SBP2Device *base, SBP2Unit *unit, struct IOStdReq *ioreq)
{
    APTR node=NULL;

    LOCK_REGION_SHARED(unit);
    {
        if (NULL != unit->u_NotifyUnit)
        {
            node = MountFindNotifyNodeTags(unit->u_NotifyUnit,
						                   MOUNTATTR_IOREQUEST, (ULONG) ioreq,
						                   TAG_END);
        }
    }
    UNLOCK_REGION_SHARED(unit);

    if (NULL != node)
    {
        MountDeleteNotifyNode(node);
        sbp2_decref(unit);
    }

	/* clear QuickIO */
    ioreq->io_Actual = 0;
    ioreq->io_Error = 0;
	
    return RC_OK;
}


/*------------------- PRIVATE CODE SECTION ------------------------*/

SBP2Device *devOpen(void)
{
    LONG unitno = REG_D0;
    LONG flags = REG_D1;
    struct IORequest  *ioreq = (APTR) REG_A1;
    SBP2Device *base = (APTR) REG_A6;
    SBP2Device *ret = NULL;
    SBP2Unit *sbp2_unit;
    LONG err;

    _INFO_LIB("Task '%s' requests unit #%ld, (OpenCnt=%ld)\n",
              FindTask(NULL)->tc_Node.ln_Name, unitno,
              base->dv_Library.lib_OpenCnt);

    ++base->dv_Library.lib_OpenCnt;
    base->dv_Library.lib_Flags &= ~LIBF_DELEXP;

    /* Sanity checks */
    if (ioreq->io_Message.mn_Length < sizeof(struct IOStdReq))
    {
        err = IOERR_BADLENGTH;
        _ERR("Bad length\n");
    }
    else if ((unitno < 0) || (unitno > base->dv_SBP2ClassBase->hc_MaxUnitNo))
    {
        err = IOERR_OPENFAIL;
        _ERR("Invalid unit number: %ld\n", unitno);
    }
    else
    {
        /* default values */
        err = IOERR_OPENFAIL;
        ioreq->io_Unit = NULL;
        
        /* Search for the given unitno */
        LOCK_REGION(base->dv_SBP2ClassBase);
        {
            ForeachNode(&base->dv_SBP2ClassBase->hc_Units, sbp2_unit)
            {
                if (unitno == sbp2_unit->u_UnitNo)
                {
                    ioreq->io_Unit = &sbp2_unit->u_SysUnit;
                    sbp2_incref(sbp2_unit);
                    break;
                }
            }
        }
        UNLOCK_REGION(base->dv_SBP2ClassBase);

        if (NULL != ioreq->io_Unit)
        {
            /* Open successed */
            ioreq->io_Device = (struct Device *)&base->dv_Library;
            ioreq->io_Flags = flags;

            err = 0;
            ret = base;

            base->dv_Library.lib_Flags &= ~LIBF_DELEXP;
            ++base->dv_Library.lib_OpenCnt;
        }
        else
            _ERR("UnitNo %ld not found\n", unitno);
    }

    --base->dv_Library.lib_OpenCnt;
    ioreq->io_Error = err;

    _INFO_LIB("[%s] ret %p, ioreq@%p=[io_Error=%ld, io_Unit=%p] (OpenCnt=%ld)\n",
              FindTask(NULL)->tc_Node.ln_Name, ret, ioreq, err, ioreq->io_Unit,
              base->dv_Library.lib_OpenCnt);
    return ret;
}

static BPTR devExpunge(void)
{
    /* This device is expunged by the SBP2 class itself, not externally */
    return 0;
}

static BPTR devClose(void)
{
    struct IORequest *ioreq = (APTR) REG_A1;
    SBP2Device *base = (APTR) REG_A6;
    SBP2Unit *sbp2_unit = (APTR)ioreq->io_Unit;

    /* Trash user structure to if he want to re-use it */
    ioreq->io_Unit   = (APTR) -1;
    ioreq->io_Device = (APTR) -1;

    LOCK_REGION(base->dv_SBP2ClassBase);
    sbp2_decref(sbp2_unit);
    UNLOCK_REGION(base->dv_SBP2ClassBase);

    --base->dv_Library.lib_OpenCnt;

    _INFO_LIB("[%s] unit %p closed, dev.OpenCnt=%ld\n",
              FindTask(NULL)->tc_Node.ln_Name, sbp2_unit,
              base->dv_Library.lib_OpenCnt);

    return 0;
}

static void devBeginIO(void)
{
    SBP2Device *base = (APTR) REG_A6;
    struct IOStdReq *ioreq = (APTR) REG_A1;
    SBP2Unit *sbp2_unit = (APTR)ioreq->io_Unit;
    LONG ret;

    ioreq->io_Error = 0;

    switch (ioreq->io_Command)
    {
        case CMD_START:
        case CMD_STOP:
        case CMD_RESET:
        case CMD_FLUSH:
        case CMD_READ:
        case CMD_WRITE:
        case TD_READ64:
        case TD_WRITE64:
        case TD_FORMAT:
        case TD_FORMAT64:
        case TD_GETGEOMETRY:
        case TD_EJECT:
        case HD_SCSICMD:
            LOCK_REGION_SHARED(sbp2_unit);
            {
                if (sbp2_unit->u_Flags.AcceptIO)
                {
                    ioreq->io_Flags &= ~IOF_QUICK;
                    ret = RC_DONTREPLY;
                    PutMsg(&sbp2_unit->u_SysUnit.unit_MsgPort, &ioreq->io_Message);
                }
                else
                {
                    ioreq->io_Actual = 0;
                    ret = TDERR_DriveInUse;
                }
            }
            UNLOCK_REGION_SHARED(sbp2_unit);
            break;

            /* NOPs */
        case CMD_CLEAR:
        case CMD_UPDATE:
        case TD_MOTOR:
            ret = RC_OK;
            break;

        case TD_ADDCHANGEINT:
            LOCK_REGION_SHARED(base->dv_SBP2ClassBase);
            ret = sbp2_io_addchangeint(base, sbp2_unit, ioreq);
            UNLOCK_REGION_SHARED(base->dv_SBP2ClassBase);
            break;

        case TD_REMCHANGEINT:
            LOCK_REGION(base->dv_SBP2ClassBase);
            ret = sbp2_io_remchangeint(base, sbp2_unit, ioreq);
            UNLOCK_REGION(base->dv_SBP2ClassBase);
            ioreq->io_Flags &= ~IOF_QUICK;
            break;

        case TD_CHANGESTATE:
            LOCK_REGION_SHARED(sbp2_unit);
            ioreq->io_Actual = sbp2_unit->u_Flags.Ready ? 0:~0;
            UNLOCK_REGION_SHARED(sbp2_unit);
            ret = RC_OK;
            break;

        case TD_CHANGENUM:
            LOCK_REGION_SHARED(sbp2_unit);
            ioreq->io_Actual = sbp2_unit->u_ChangeCounter;
            UNLOCK_REGION_SHARED(sbp2_unit);
            ret = RC_OK;
            break;

        case TD_PROTSTATUS:
            LOCK_REGION_SHARED(sbp2_unit);
            {
                if (sbp2_unit->u_Flags.Ready)
                {
                    ioreq->io_Actual = sbp2_unit->u_Flags.WriteProtected ? ~0 : 0;
                    ret = RC_OK;
                }
                else
                    ret = TDERR_DiskChanged;
            }
            UNLOCK_REGION_SHARED(sbp2_unit);
            break;

        default:
            kprintf("[ERR] SBP2 > Not supported IO cmd: %u\n", ioreq->io_Command);
            ioreq->io_Actual = 0;
            ret = IOERR_NOCMD;
            break;
    }

    if (ret != RC_DONTREPLY)
    {
        if (ret != RC_OK)
        {
            ioreq->io_Error = ret & 0xff;
            if (IOERR_NOCMD != ret)
                _ERR("IO cmd %d failed: ret=%ld\n", ioreq->io_Command, ret);
        }
        
        /* If not quick I/O, reply the message */
        if(!(ioreq->io_Flags & IOF_QUICK))
            ReplyMsg(&ioreq->io_Message);
    }
}

static void devAbortIO(void)
{
    struct IOStdReq *ioreq = (APTR) REG_A1;
    //SBP2Device *base = (APTR) REG_A6;
    SBP2Unit *sbp2_unit = (APTR)ioreq->io_Unit;

    /* We suppose that caller design is such
     * this function will not be called in a concurent way.
     */

    if (ioreq->io_Message.mn_Node.ln_Type == NT_MESSAGE)
    {
        ioreq->io_Error = IOERR_ABORTED;
        ioreq->io_Actual = 0;
        sbp2_cancel_orbs(sbp2_unit);
    }
}

static int devQuery(void)
{
    SBP2Device *base = (APTR) REG_A6;
    ULONG *data = (ULONG *)REG_A0;
	ULONG attr = REG_D0;
    struct TagItem *ti;

    _INFO_LIB("base=%p, data=$%08x, attr=$%08x\n", base, data, attr);

	if ((NULL != UtilityBase) && (NULL != data))
	{
        /* Device only queries */
		if (QUERYINFOATTR_DEVICE_UNITS == attr)
		{
            LOCK_REGION(base->dv_SBP2ClassBase);
            *data = base->dv_SBP2ClassBase->hc_MaxUnitNo + 1;
            UNLOCK_REGION(base->dv_SBP2ClassBase);
			
            _INFO("QUERYINFOATTR_DEVICE_UNITS %lu\n", *data);
			return TRUE;
		}
        else if (NULL != (ti = FindTagItem(attr, QueryTags)))
		{
			_INFO("$%08x -> $%08x\n", attr, ti->ti_Data);
			*data = ti->ti_Data;
			return TRUE;
		}
        else if (QUERYINFOATTR_DEVICE_MASK == attr)
        {
            *data = 0xfffffffe;
            _INFO("QUERYINFOATTR_DEVICE_MASK $%08x\n", *data);
			return TRUE;
        }
        else if (QUERYINFOATTR_DEVICE_MAXTRANSFER == attr)
        {
            *data = (1ul<<21)-1;
            _INFO("QUERYINFOATTR_DEVICE_MAXTRANSFER $%08x\n", *data);
			return TRUE;
        }
        else
        {
            struct QueryUnitID *MyUnitID = (struct QueryUnitID *)data;
            SBP2Unit *sbp2_unit = (APTR)MyUnitID->Unit;

            _INFO("SBP2 unit @ %p\n", sbp2_unit);

            /* Unit queries */
            if (QUERYINFOATTR_DEVICE_UNIT_UNIT == attr)
            {
                MyUnitID->ID = sbp2_unit->u_UnitNo;
                _INFO("QUERYINFOATTR_DEVICE_UNIT_UNIT %lu\n", MyUnitID->ID);
    			return TRUE;
            }
            else if (QUERYINFOATTR_DEVICE_UNIT_LUN == attr)
            {
                MyUnitID->ID = sbp2_unit->u_LUN;
                _INFO("QUERYINFOATTR_DEVICE_UNIT_LUN %u\n", MyUnitID->ID);
    			return TRUE;
            }
        }

        _WARN("Unmanaged attr: $%08lx (QUERYINFOATTR_Dummy+%lu)\n", attr, attr-QUERYINFOATTR_Dummy);
	}

    return FALSE;
}

static struct DiskPort *devCreateDiskPort(void)
{
    //SBP2Device *base = (APTR) REG_A6;

    /* TODO */
    DB_NotImplemented();

    return NULL;
}

static void devDeleteDiskPort(void)
{
    //SBP2Device *base = (APTR) REG_A6;
    //struct DiskPort *MyDiskPort = (struct DiskPort *) REG_A1;

    /* TODO */
    DB_NotImplemented();
}

static ULONG devInquiry(void)
{
    //SBP2Device *base = (APTR) REG_A6;
    //ULONG UnitNo = (ULONG) REG_D0;
    //struct DiskPort *MyDiskPort = (struct DiskPort *) REG_A1;

    /* TODO */
    DB_NotImplemented();

    return 0;
}

static ULONG devMount(void)
{
    SBP2Device *base = (APTR) REG_A6;
    struct IOStdReq *ioreq = (APTR) REG_A0;
    SBP2Unit *sbp2_unit = (APTR)ioreq->io_Unit;

    _INFO("Mount unit %p\n", sbp2_unit);
    return !MountMountTags(sbp2_unit->u_NotifyUnit, TAG_END);
}

static void devDisMount(void)
{
    //SBP2Device *base = (APTR) REG_A6;
    struct IOStdReq *ioreq = (APTR) REG_A0;
    SBP2Unit *sbp2_unit = (APTR)ioreq->io_Unit;
    
    sbp2_unmount_all(sbp2_unit);
}

