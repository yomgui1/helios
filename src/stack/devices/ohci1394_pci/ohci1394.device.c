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
** This file is copyrights 2008-2012 by Guillaume ROGUEZ.
**
** MorphOS OHCI-1394 device interface implemention.
*/

#define DEBUG_SYSTEM_LIBRARY

#include "ohci1394.device.h"
#include "ohci1394core.h"
#include "ohci1394trans.h"
#include "utils.h"

#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <devices/timer.h>
#include <libraries/query.h>

#include <proto/exec.h>
#include <proto/utility.h>

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *PCIXBase;
struct Library *UtilityBase;
struct Library *HeliosBase;

ULONG __abox__ = 1;
const char *vers = "\0" VSTRING;

static ULONG devFuncTable[] = {
	FUNCARRAY_BEGIN,

	FUNCARRAY_32BIT_NATIVE,

	(ULONG) DEV_OPEN_FNAME,
	(ULONG) DEV_CLOSE_FNAME,
	(ULONG) DEV_EXPUNGE_FNAME,
	(ULONG) DEV_QUERY_FNAME,
	(ULONG) DEV_BEGINIO_FNAME,
	(ULONG) DEV_ABORTIO_FNAME,
	-1,

	FUNCARRAY_END
};

static struct TagItem devQueryTags[] =
{
	{QUERYINFOATTR_NAME, (ULONG) DEVNAME},
	{QUERYINFOATTR_IDSTRING, (ULONG) VSTRING},
	{QUERYINFOATTR_DESCRIPTION, (ULONG) "Generic low-level driver for PCI OHCI 1394 Chipset"},
	{QUERYINFOATTR_COPYRIGHT, (ULONG) COPYRIGHTS},
	{QUERYINFOATTR_AUTHOR, (ULONG) AUTHOR},
	{QUERYINFOATTR_DATE, (ULONG) DATE},
	{QUERYINFOATTR_VERSION, VERSION},
	{QUERYINFOATTR_REVISION, REVISION},
	{QUERYINFOATTR_RELEASETAG, (ULONG) "RC1"},
	{QUERYINFOATTR_CODETYPE, CODETYPE_PPC},
	{QUERYINFOATTR_SUBTYPE, QUERYSUBTYPE_DEVICE},
	{QUERYINFOATTR_CLASS, QUERYCLASS_FIREWIRE},
	{QUERYINFOATTR_SUBCLASS, QUERYSUBCLASS_FIREWIRE_HWDRIVER},
	{TAG_DONE,0}
};

/* First function: protection against lib execution */
LONG NoExecute(void) { return -1; }

/*============================================================================*/
/*--- LOCAL CODE -------------------------------------------------------------*/
/*============================================================================*/
static APTR
my_open_lib(CONST_STRPTR libname, ULONG version)
{
	struct Library *lib;

	lib = (APTR)OpenLibrary(libname, version);
	if (lib == NULL)
		utils_ReportMissingLibrary(libname, version);
	return lib;
}
static BPTR
internalExpunge(OHCI1394Device *base)
{
	BPTR seglist;

	base->hd_Library.lib_Flags |= LIBF_DELEXP;
	if (base->hd_Library.lib_OpenCnt != 0)
		return 0;

	seglist = base->hd_SegList;

	_DBG_DEV("Remove from system...\n");
	Forbid();
	REMOVE(&base->hd_Library.lib_Node);
	Permit();

	_DBG_DEV("Free resources...\n");
	DeletePool(base->hd_MemPool);
	CloseLibrary(UtilityBase);
	CloseLibrary((struct Library *) DOSBase);
	CloseLibrary(PCIXBase);
	CloseLibrary(HeliosBase);

	FreeMem((char *)base - base->hd_Library.lib_NegSize,
			base->hd_Library.lib_NegSize + base->hd_Library.lib_PosSize);

	return seglist;
}
/*============================================================================*/
/*--- LIBRARY CODE -----------------------------------------------------------*/
/*============================================================================*/
DEVINIT(OHCI1394Device, base, seglist)
{
	/* Note: MemPool must clear memory at allocation */
	base->hd_MemPool = CreatePool(MEMF_PUBLIC|MEMF_CLEAR, 16384, 4096);
	if (NULL != base->hd_MemPool)
	{
		_DBG_DEV("Open resources...\n");
		DOSBase		= my_open_lib("dos.library", 50);
		UtilityBase	= my_open_lib("utility.library", 50);
		PCIXBase	= my_open_lib("pcix.library", 50);
		HeliosBase	= my_open_lib(HELIOS_LIBNAME, HELIOS_LIBVERSION);

		if (DOSBase && UtilityBase && PCIXBase && HeliosBase)
		{
			base->hd_UnitCount = 0;
			base->hd_SegList = seglist;

			/* Scan the PCI bus and generate units nodes */
			if (ohci_ScanPCI(base))
			{
				_INF_DEV("init done\n");
				return (APTR)base;
			}
			else
				_ERR_DEV("PCI scan failed!\n");
		}

		if (DOSBase) CloseLibrary((struct Library*)DOSBase);
		if (UtilityBase) CloseLibrary(UtilityBase);
		if (PCIXBase) CloseLibrary(PCIXBase);
		if (HeliosBase) CloseLibrary(HeliosBase);

		DeletePool(base->hd_MemPool);
	}
	else
		_ERR_DEV("CreatePool() failed!\n");

	return NULL;
}
ENDFUNC
DEVOPEN(OHCI1394Device, base, ioreq, unit)
{
	struct Device *ret = NULL;
	LONG err;

	/* Sanity checks */
	if (ioreq->io_Message.mn_Length < sizeof(struct IORequest))
	{
		err = IOERR_BADLENGTH;
		_ERR_DEV("Bad IO length\n");
		goto on_return;
	}

	if ((unit < 0) || (unit > base->hd_UnitCount))
	{
		err = IOERR_OPENFAIL;
		_ERR_DEV("Invalid unit number: %ld\n", unit);
		goto on_return;
	}

	/* Open the unit:
	 * At first open: initialize PCI and 1394 HW
	 * Then: just no-op
	 */
	err = ohci_OpenUnit(base, ioreq, unit);
	if (err)
		goto on_return;

	/* successfull */
	++base->hd_Library.lib_OpenCnt;
	base->hd_Library.lib_Flags &= ~LIBF_DELEXP;
	ret = (APTR)base;
	err = 0;

	_INF_DEV("open done\n");

on_return:
	if (err)
		base->hd_Library.lib_Flags |= LIBF_DELEXP;
	ioreq->io_Error = err;
	return ret;
}
ENDFUNC
DEVCLOSE(OHCI1394Device, base, ioreq)
{
	BPTR seglist = 0;

	ohci_CloseUnit(base, ioreq);

	/* In case if re-user */
	ioreq->io_Unit   = (APTR) 0;
	ioreq->io_Device = (APTR) 0;

	if (--base->hd_Library.lib_OpenCnt == 0)
	{
		if (base->hd_Library.lib_Flags & LIBF_DELEXP)
			seglist = internalExpunge(base);
	}

	return seglist;
}
ENDFUNC
DEVEXPUNGE(OHCI1394Device, base)
{
	return internalExpunge(base);
}

ENDFUNC
DEVQUERY(OHCI1394Device, base, data, attr)
{
	struct TagItem *ti;

	if (QUERYINFOATTR_DEVICE_UNITS == attr)
		*data = base->hd_UnitCount;
	else if (QUERYINFOATTR_DEVICE_UNIT_UNIT == attr)
	{
		struct QueryUnitID *MyUnitID = (APTR)data;
		OHCI1394Unit *unit = (APTR)MyUnitID->Unit;

		MyUnitID->ID = unit->hu_UnitNo;
	}
	else if (NULL != (ti = FindTagItem(attr, devQueryTags)))
		*data = ti->ti_Data;
	else
	{
		HeliosHWQuery *hhq = (APTR)data;
		OHCI1394Unit *unit = (APTR)hhq->hhq_Unit;

		switch (attr)
		{
			case HHA_Standard:
				*(ULONG*)hhq->hhq_Data = HSTD_1394_1995;
				break;

			case HA_EventListenerList:
				*(HeliosEventListenerList**)hhq->hhq_Data = &unit->hu_Listeners;
				break;

			case OHCI1394A_Generation:
				SHARED_PROTECT_BEGIN(unit);
				*(ULONG*)hhq->hhq_Data = unit->hu_OHCI_LastGeneration;
				SHARED_PROTECT_END(unit);
				break;

			case HHA_SelfID:
				{
					HeliosSelfIDStream *dest = hhq->hhq_Data;

					SHARED_PROTECT_BEGIN(unit);
					{
						dest->hss_Generation = unit->hu_OHCI_LastGeneration;
						dest->hss_LocalNodeID = OHCI1394_NODEID_NODENUMBER(unit->hu_LocalNodeId);
						dest->hss_PacketCount = unit->hu_SelfIdPktCnt;
						CopyMem(unit->hu_SelfIdArray, dest->hss_Data, unit->hu_SelfIdPktCnt * 8);
					}
					SHARED_PROTECT_END(unit);
				}
				break;

			case HHA_LocalNodeId:
				SHARED_PROTECT_BEGIN(unit);
				*(UWORD*)hhq->hhq_Data = unit->hu_LocalNodeId;
				SHARED_PROTECT_END(unit);
				break;

			case OHCI1394A_PciVendorId:
				*(ULONG*)hhq->hhq_Data = unit->hu_PCI_VID;
				break;

			case OHCI1394A_PciDeviceId:
				*(ULONG*)hhq->hhq_Data = unit->hu_PCI_DID;
				break;

			case HHA_VendorId:
				*(char*)hhq->hhq_Data = unit->hu_OHCI_VendorID;
				break;

			case HHA_DriverVersion:
				*(ULONG*)hhq->hhq_Data = unit->hu_OHCI_Version;
				break;

			case HHA_LocalGUID:
				*(UQUAD*)hhq->hhq_Data = unit->hu_GUID;
				break;

			case HHA_SplitTimeout:
				SHARED_PROTECT_BEGIN(unit);
				*(ULONG*)hhq->hhq_Data = unit->hu_SplitTimeoutCSR;
				SHARED_PROTECT_END(unit);
				break;

#ifdef NOT_IMPLEMENTED_YET
			case HHA_CycleTime:
				*(ULONG*)hhq->hhq_Data = ohci_GetTimeStamp(unit);
				break;

			case HHA_UpTime:
				*(ULONG*)hhq->hhq_Data = ohci_UpTime(unit);
				break;
#endif

			default:
				_WRN("Unmanaged attr: $%08lx (QUERYINFOATTR_Dummy+%lu)\n",
					attr, attr-QUERYINFOATTR_Dummy);
				return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}
ENDFUNC
DEVBEGINIO(OHCI1394Device, base, ioreq)
{
	OHCI1394Unit *unit = (OHCI1394Unit *)ioreq->io_Unit;
	ULONG err;

	ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
	ioreq->io_Error = 0;

	if (!unit->hu_Flags.Initialized)
	{
		ioreq->io_Error = IOERR_SELFTEST;
		err = 0;
	}
	else
	{
		SHARED_PROTECT_BEGIN(unit);
		err = unit->hu_Flags.UnrecoverableError;
		SHARED_PROTECT_END(unit);

		/* Block any command except CMD_RESET if the unit is in an unrecoverable state */
		if (err && (CMD_RESET != ioreq->io_Command))
		{
			_ERR_DEV("OHCI Unrecoverable Error\n");
			ioreq->io_Error = HHIOERR_UNRECOVERABLE_ERROR;
		}
		else
		{
			//_INF_DEV("cmd = %08x\n", ioreq->iohh_Req.io_Command);
			switch(ioreq->io_Command)
			{
				case CMD_RESET:
					err = cmdReset(ioreq, unit, base);
					break;

				case HHIOCMD_BUSRESET:
					err = cmdBusReset(ioreq, unit, base);
					break;

				case HHIOCMD_ENABLE:
					err = cmdEnable(ioreq, unit, base);
					break;

				case HHIOCMD_DISABLE:
					err = cmdDisable(ioreq, unit, base);
					break;

				case HHIOCMD_SENDREQUEST:
					err = cmdSendRequest(ioreq, unit, base);
					break;

				case HHIOCMD_SENDPHY:
					err = cmdSendPhy(ioreq, unit, base);
					break;

				case HHIOCMD_ADDREQHANDLER:
					err = cmdAddReqHandler(ioreq, unit, base);
					break;

				case HHIOCMD_REMREQHANDLER:
					err = cmdRemReqHandler(ioreq, unit, base);
					break;

#if 0
				case HHIOCMD_SETATTRIBUTES:
					err = cmdSetAttrs(ioreq, unit, base);
					break;

				case HHIOCMD_CREATEISOCONTEXT:
					err = cmdCreateIsoCtx(ioreq, unit, base);
					break;

				case HHIOCMD_DELETEISOCONTEXT:
					err = cmdDelIsoCtx(ioreq, unit, base);
					break;

				case HHIOCMD_STARTISOCONTEXT:
					err = cmdStartIsoCtx(ioreq, unit, base);
					break;

				case HHIOCMD_STOPISOCONTEXT:
					err = cmdStopIsoCtx(ioreq, unit, base);
					break;
#endif

				default:
					_ERR_DEV("Invalid command 0x%X\n", ioreq->io_Command);
					ioreq->io_Error = IOERR_NOCMD;
					err = 0;
					break;
			}
		}
	}

	if (err)
		ioreq->io_Flags &= ~IOF_QUICK;
	else if (!(ioreq->io_Flags & IOF_QUICK))
		ReplyMsg(&ioreq->io_Message);
}
ENDFUNC
DEVABORTIO(OHCI1394Device, base, ioreq)
{
	OHCI1394Unit *unit = (OHCI1394Unit *)ioreq->io_Unit;
	LONG cancelled;

	switch(ioreq->io_Command)
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
		ioreq->io_Error = IOERR_ABORTED;
}
ENDFUNC
/*============================================================================*/
/*--- RESIDENT ---------------------------------------------------------------*/
/*============================================================================*/
static struct LibInitStruct devInit = {
	.LibSize	= sizeof(OHCI1394Device),
	.FuncTable	= devFuncTable,
	.DataTable	= NULL,
	.InitFunc	= (void (*)())DEV_INIT_FNAME,
};
struct Resident devResident=
{
	.rt_MatchWord	= RTC_MATCHWORD,
	.rt_MatchTag	= &devResident,
	.rt_EndSkip		= &devResident + 1,
	.rt_Flags		= RTF_PPC | RTF_EXTENDED | RTF_AUTOINIT,
	.rt_Version		= VERSION,
	.rt_Type		= NT_DEVICE,
	.rt_Pri			= 0,
	.rt_Name		= DEVNAME,
	.rt_IdString	= VSTRING,
	.rt_Init		= &devInit,
	/* V50 */
	.rt_Revision	= REVISION,
	.rt_Tags		= devQueryTags,
};
/*=== EOF ====================================================================*/