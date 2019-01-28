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
** SBP2 Class core file.
*/

#define DEBUG_SYSTEM_LIBRARY

#include "sbp2.class.h"
#include "sbp2.device.h"

extern ULONG libFuncTable[];
extern void DEV_INIT_FNAME(void);

struct ExecBase *SysBase=NULL;
struct DosLibrary *DOSBase=NULL;
struct Library *UtilityBase=NULL;
struct Library *QueryBase=NULL;
struct Library *HeliosBase=NULL;

const CONST_STRPTR __abox__=VERSTAG;

/* First function: protection against lib execution */
LONG NoExecute(void) { return -1; }

/*============================================================================*/
/*--- LOCAL API --------------------------------------------------------------*/
/*============================================================================*/
static ULONG 
my_lib_expunge(struct SBP2ClassLib *base)
{
	BPTR seglist;

	if (base->hc_Lib.lib_OpenCnt)
	{
		_DBG_LIB("Set LIBF_DELEXP\n");
		base->hc_Lib.lib_Flags |= LIBF_DELEXP;
		return NULL;
	}

	/* Cleanup underlayered device */
	devCleanup(base->hc_DevBase);
	
	seglist = base->hc_SegList;

	_DBG_LIB("Remove from system...\n");
	Forbid();
	Remove(&base->hc_Lib.lib_Node);
	Permit();

	_DBG_LIB("Free resources...\n");
	CloseLibrary((struct Library *)DOSBase);
	CloseLibrary(UtilityBase);
	CloseLibrary(QueryBase);
	CloseLibrary(HeliosBase);
	
	DeletePool(base->hc_MemPool);
	
	FreeMem((APTR)((ULONG)(base) - (ULONG)(base->hc_Lib.lib_NegSize)),
			base->hc_Lib.lib_NegSize + base->hc_Lib.lib_PosSize);

	return (ULONG)seglist;
}
/*============================================================================*/
/*--- LIBRARY API ------------------------------------------------------------*/
/*============================================================================*/
LIBINIT(struct SBP2ClassLib, base, seglist)
{
	base->hc_SegList = seglist;

	base->hc_MemPool = CreatePool(MEMF_PUBLIC|MEMF_CLEAR|MEMF_SEM_PROTECTED, 16384, 4096);
	if (base->hc_MemPool)
	{
		/* Open needed resources */
		_DBG_LIB("Open resources...\n");
		DOSBase		= utils_SafeOpenLibrary("dos.library", 50);
		UtilityBase	= utils_SafeOpenLibrary("utility.library", 50);
		QueryBase	= utils_SafeOpenLibrary("query.library", 50);
		HeliosBase	= utils_SafeOpenLibrary(HELIOS_LIBNAME, HELIOS_LIBVERSION);

		if (DOSBase && UtilityBase && QueryBase && HeliosBase)
		{
			_DBG_LIB("Loading device %s into memory\n", DEVNAME);
			base->hc_DevBase = (SBP2Device *)NewCreateLibraryTags(
				LIBTAG_FUNCTIONINIT, (ULONG)devFuncTable,
				LIBTAG_LIBRARYINIT, (ULONG)DEV_INIT_FNAME,
				LIBTAG_BASESIZE, sizeof(SBP2Device),
				LIBTAG_MACHINE, MACHINE_PPC,
				LIBTAG_TYPE, NT_DEVICE,
				LIBTAG_FLAGS, LIBF_SUMUSED | LIBF_CHANGED | LIBF_QUERYINFO,
				LIBTAG_NAME, (ULONG)DEVNAME,
				LIBTAG_VERSION, DEV_VERSION,
				LIBTAG_REVISION, DEV_REVISION,
				LIBTAG_IDSTRING, (ULONG)DEV_VERSION_STR,
				LIBTAG_PUBLIC, TRUE,
				LIBTAG_QUERYINFO, TRUE,
				TAG_DONE);
			if (base->hc_DevBase)
			{
				base->hc_DevBase->dv_SBP2ClassBase = base;
				return &base->hc_Lib;
			}
		}

		CLEARLIBBASE(DOSBase);
		CLEARLIBBASE(UtilityBase);
		CLEARLIBBASE(QueryBase);
		CLEARLIBBASE(HeliosBase);

		DeletePool(base->hc_MemPool);
	}
	else
		_ERR_DEV("CreatePool() failed\n");
		
	FreeMem((APTR)((ULONG)(base) - (ULONG)(base->hc_Lib.lib_NegSize)),
			base->hc_Lib.lib_NegSize + base->hc_Lib.lib_PosSize);

	return NULL;
}
ENDFUNC
LIBOPEN(struct SBP2ClassLib, base)
{
	base->hc_Lib.lib_OpenCnt++;
	base->hc_Lib.lib_Flags &= ~LIBF_DELEXP;

	return (APTR)base;
}
ENDFUNC
LIBCLOSE(struct SBP2ClassLib, base)
{
	if ((--base->hc_Lib.lib_OpenCnt) != 0)
		return 0;

	if (base->hc_Lib.lib_Flags & LIBF_DELEXP)
	{
		_DBG_LIB("LIBF_DELEXP set\n");
		return my_lib_expunge(base);
	}

	return 0;
}
ENDFUNC
LIBEXPUNGE(struct SBP2ClassLib, base)
{
	return my_lib_expunge(base);
}
ENDFUNC
LONG
HeliosClass_DoMethodA(struct SBP2ClassLib *base, ULONG methodid, ULONG *data)
{
	switch (methodid)
	{
		/* Mandatory methods */
		case HCM_Initialize: return sbp2_InitClass(base, (HeliosClass *)data[0]);
		case HCM_Terminate: return sbp2_TermClass(base);
		case HCM_ReleaseUnitBinding: return sbp2_ReleaseUnitBinding(base, (HeliosUnit *)data[0]);

		/* Optional methods */
		case HCM_AttemptUnitBinding: return sbp2_AttemptUnitBinding(base, (HeliosUnit *)data[0]); 
	}

	return 0;
}
/*============================================================================*/
/*--- RESIDENT ---------------------------------------------------------------*/
/*============================================================================*/
struct LibInitStruct libInit=
{
	.LibSize	= sizeof(struct SBP2ClassLib),
	.FuncTable	= libFuncTable,
	.DataTable	= NULL,
	.InitFunc	= LIB_INIT_FNAME,
};
struct Resident libResident=
{
	.rt_MatchWord	= RTC_MATCHWORD,
	.rt_MatchTag	= &libResident,
	.rt_EndSkip		= &libResident + 1,
	.rt_Flags		= RTF_PPC | RTF_EXTENDED | RTF_AUTOINIT,
	.rt_Version		= VERSION,
	.rt_Type		= NT_LIBRARY,
	.rt_Pri			= 0,
	.rt_Name		= LIBNAME,
	.rt_IdString	= VSTRING,
	.rt_Init		= &libInit,
	/* V50 */
	.rt_Revision	= REVISION,
	.rt_Tags		= NULL,
};
/*=== EOF ====================================================================*/