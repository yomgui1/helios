/* Copyright 2008-2013, 2018 Guillaume Roguez

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

/* $Id$
** This file is copyrights 2008-2013 by Guillaume ROGUEZ.
**
** Dummy device driver implemention.
*/

#define DEBUG_SYSTEM_LIBRARY
#define STATIC_DEVICE static /* for utils.h */

#include "dummy.device.h"
#include "dummy.class.h"

#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <devices/timer.h>
#include <libraries/query.h>
#include <proto/exec.h>

/*============================================================================*/
/*--- LOCAL STRUCTURES AND VARIABLES -----------------------------------------*/
/*============================================================================*/
static struct TagItem QueryTags[] =
{
	{QUERYINFOATTR_NAME, (ULONG) DEVNAME},
	{QUERYINFOATTR_IDSTRING, (ULONG) VSTRING},
	{QUERYINFOATTR_DESCRIPTION, (ULONG) "DUMMY class for Helios"},
	{QUERYINFOATTR_COPYRIGHT, (ULONG) "(c) 2013 by Guillaume ROGUEZ"},
	{QUERYINFOATTR_AUTHOR, (ULONG) "Guillaume ROGUEZ"},
	{QUERYINFOATTR_RELEASETAG, (ULONG) "aplha"},
	{QUERYINFOATTR_VERSION, VERSION},
	{QUERYINFOATTR_REVISION, REVISION},
	{QUERYINFOATTR_CODETYPE, MACHINE_PPC},
	{QUERYINFOATTR_SUBTYPE, QUERYSUBTYPE_DEVICE},
	{QUERYINFOATTR_DEVICE_UNITS, 1},
	{QUERYINFOATTR_DEVICE_LUNS, 1},
	/*{QUERYINFOATTR_CLASS, QUERYCLASS_STORAGE},*/
	/*{QUERYINFOATTR_SUBCLASS, QUERYSUBCLASS_STORAGE_FIREWIRE},*/
	{TAG_DONE, 0}
};
/*============================================================================*/
/*--- PUBLIC STRUCTURES AND VARIABLES ----------------------------------------*/
/*============================================================================*/
const ULONG devFuncTable[] =
{
	FUNCARRAY_BEGIN,
	
	FUNCARRAY_32BIT_NATIVE,

	(ULONG) DEV_OPEN_FNAME,
	(ULONG) DEV_CLOSE_FNAME,
	(ULONG) DEV_EXPUNGE_FNAME,
	(ULONG) DEV_QUERY_FNAME,
	(ULONG) DEV_BEGINIO_FNAME,
	(ULONG) DEV_ABORTIO_FNAME,
	(ULONG) RESERVED_FNAME, //devCreateDiskPort
	(ULONG) RESERVED_FNAME, //devDeleteDiskPort
	(ULONG) RESERVED_FNAME, //devInquiry
	//(ULONG) devMount,
	//(ULONG) devDisMount,
	-1,
	
	FUNCARRAY_END
};
/*============================================================================*/
/*--- LOCAL CODE -------------------------------------------------------------*/
/*============================================================================*/

/*============================================================================*/
/*--- PUBLIC CODE ------------------------------------------------------------*/
/*============================================================================*/
BPTR
devCleanup(DummyDevice *base)
{
	BPTR seglist;

	base->dv_Library.lib_Flags |= LIBF_DELEXP;
	if (base->dv_Library.lib_OpenCnt != 0)
		return 0;

	seglist = base->dv_SegList;

	_DBG_DEV("Remove from system...\n");
	Forbid();
	REMOVE(&base->dv_Library.lib_Node);
	Permit();

	_DBG_DEV("Free resources...\n");
	/* NOTHING */
	
	FreeMem((char *)base - base->dv_Library.lib_NegSize,
			base->dv_Library.lib_NegSize + base->dv_Library.lib_PosSize);

	return seglist;
}
/*============================================================================*/
/*--- LIBRARY CODE -----------------------------------------------------------*/
/*============================================================================*/
DEVINIT(DummyDevice, base, seglist)
{
	LOCK_INIT(base);
	INIT_LOCKED_LIST(base->dv_Units);
	base->dv_UnitCount = 0;
	return (struct Device *)base;
}
ENDFUNC
DEVOPEN(DummyDevice, base, ioreq, unitno)
{
	ULONG flags = REG_D1;
	DummyDevice *ret = NULL;
	DummyUnit *dummy_unit;
	LONG err;

	/* Sanity checks */
	if (ioreq->io_Message.mn_Length < sizeof(struct IOStdReq))
	{
		err = IOERR_BADLENGTH;
		_ERR_DEV("Bad IO length\n");
		goto on_return;
	}
	
	if ((unitno < 0) || (unitno > base->dv_UnitCount))
	{
		err = IOERR_OPENFAIL;
		_ERR_DEV("Invalid unit number: %ld\n", unitno);
		goto on_return;
	}
	
	/* Search for the given unitno */
	READ_LOCK_LIST(base->dv_Units);
	{
		ioreq->io_Unit = NULL;
		ForeachNode(&base->dv_Units, dummy_unit)
		{
			if (unitno == dummy_unit->u_UnitNo)
			{
				ioreq->io_Unit = &dummy_unit->u_SysUnit;
				ioreq->io_Device = (struct Device *)base;
				ioreq->io_Flags = flags;
				break;
			}
		}
	}
	UNLOCK_LIST(base->dv_Units);

	if (!ioreq->io_Unit)
	{
		err = IOERR_OPENFAIL;
		_ERR_DEV("UnitNo %ld not found\n", unitno);
		goto on_return;
	}

	/* successfull */
	++base->dv_Library.lib_OpenCnt;
	base->dv_Library.lib_Flags &= ~LIBF_DELEXP;
	ret = (APTR)base;
	err = 0;
	
	_INF_DEV("open done\n");
	
on_return:
	if (err)
		base->dv_Library.lib_Flags |= LIBF_DELEXP;
	ioreq->io_Error = err;
	return (struct Device *)ret;
}
ENDFUNC
DEVCLOSE(DummyDevice, base, ioreq)
{
	DummyUnit *dummy_unit = (APTR)ioreq->io_Unit;

	/* In case if re-user */
	ioreq->io_Unit   = (APTR) 0;
	ioreq->io_Device = (APTR) 0;

	--base->dv_Library.lib_OpenCnt;
	return 0;
}
ENDFUNC
DEVEXPUNGE(DummyDevice, base)
{
	base->dv_Library.lib_Flags &= ~LIBF_DELEXP;
	return 0;
}
ENDFUNC
DEVBEGINIO(DummyDevice, base, ioreq)
{
	DummyUnit *dummy_unit = (APTR)ioreq->io_Unit;
	LONG ret;

	ioreq->io_Error = 0;

	switch (ioreq->io_Command)
	{
		default:
			_ERR_DEV("Not supported IO cmd: %u\n", ioreq->io_Command);
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
				_ERR_DEV("IO cmd %d failed: ret=%ld\n", ioreq->io_Command, ret);
		}
		
		if(!(ioreq->io_Flags & IOF_QUICK))
			ReplyMsg(&ioreq->io_Message);
	}
}
ENDFUNC
DEVABORTIO(DummyDevice, base, ioreq)
{
	DummyUnit *dummy_unit = (APTR)ioreq->io_Unit;

	if (ioreq->io_Message.mn_Node.ln_Type == NT_MESSAGE)
	{
		ioreq->io_Error = IOERR_ABORTED;
		ioreq->io_Actual = 0;
	}
}
ENDFUNC
DEVQUERY(DummyDevice, base, data, attr)
{
	struct TagItem *ti;

	/* Device only queries */
	if (QUERYINFOATTR_DEVICE_UNITS == attr)
	{
		SHARED_PROTECT_BEGIN(base);
		*data = base->dv_UnitCount + 1;
		SHARED_PROTECT_END(base);
		
		_INF_DEV("QUERYINFOATTR_DEVICE_UNITS %lu\n", *data);
		return TRUE;
	}
	else if (NULL != (ti = FindTagItem(attr, QueryTags)))
	{
		_INF_DEV("$%08x -> $%08x\n", attr, ti->ti_Data);
		*data = ti->ti_Data;
		return TRUE;
	}
	else
	{
		struct QueryUnitID *MyUnitID = (struct QueryUnitID *)data;
		DummyUnit *dummy_unit = (APTR)MyUnitID->Unit;

		_DBG_DEV("unit @ %p\n", dummy_unit);
		if (dummy_unit)
		{
			/* Unit queries */
			if (QUERYINFOATTR_DEVICE_UNIT_UNIT == attr)
			{
				MyUnitID->ID = dummy_unit->u_UnitNo;
				_INF_DEV("QUERYINFOATTR_DEVICE_UNIT_UNIT %u\n", MyUnitID->ID);
				return TRUE;
			}
			else if (QUERYINFOATTR_DEVICE_UNIT_LUN == attr)
			{
				MyUnitID->ID = dummy_unit->u_LUN;
				_INF_DEV("QUERYINFOATTR_DEVICE_UNIT_LUN %u\n", MyUnitID->ID);
				return TRUE;
			}
		}
	}

	_WRN_DEV("Unmanaged attr: $%08lx (QUERYINFOATTR_Dummy+%lu)\n", attr, attr-QUERYINFOATTR_Dummy);
	return FALSE;
}
ENDFUNC
/*=== EOF ====================================================================*/