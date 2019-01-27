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
** Helios system library API.
*/

#define DEBUG_SYSTEM_LIBRARY

#include "private.h"
#include "fw_libversion.h"

extern ULONG libFuncTable[]; /* from helios_functable.library.c */

struct HeliosBase *	HeliosBase = NULL;
struct ExecBase *	SysBase = NULL;
struct DosLibrary *	DOSBase = NULL;
struct Library *	UtilityBase = NULL;
struct Library *	QueryBase = NULL;

const CONST_STRPTR __abox__ = VERSTAG;

/* First function: protection against lib execution */
LONG NoExecute(void) { return -1; }

/*============================================================================*/
/*--- LOCAL CODE -------------------------------------------------------------*/
/*============================================================================*/
static ULONG 
my_lib_expunge(struct HeliosBase *base)
{
	BPTR seglist;

	if (base->hb_Lib.lib_OpenCnt)
	{
		_DBG_LIB("Set LIBF_DELEXP\n");
		base->hb_Lib.lib_Flags |= LIBF_DELEXP;
		return NULL;
	}

	seglist = base->hb_SegList;

	_DBG_LIB("Remove from system...\n");
	Forbid();
	Remove(&base->hb_Lib.lib_Node);
	Permit();

	_DBG_LIB("Free resources...\n");
	CloseDevice((struct IORequest *)&base->hb_TimeReq);
	CloseLibrary((struct Library *)DOSBase);
	CloseLibrary(UtilityBase);
	CloseLibrary(QueryBase);
	DeletePool(base->hb_MemPool);
	
	FreeMem((APTR)((ULONG)(HeliosBase) - (ULONG)(base->hb_Lib.lib_NegSize)),
			base->hb_Lib.lib_NegSize + base->hb_Lib.lib_PosSize);

	return (ULONG)seglist;
}
/*============================================================================*/
/*--- LIBRARY CODE -----------------------------------------------------------*/
/*============================================================================*/
LIBINIT(struct HeliosBase, base, seglist)
{
	HeliosBase = base;
	base->hb_SegList = seglist;

	/* Open needed resources */
	base->hb_MemPool = CreatePool(MEMF_PUBLIC|MEMF_CLEAR|MEMF_SEM_PROTECTED, 16384, 4096);
	if (NULL != base->hb_MemPool)
	{
		_DBG_LIB("Open resources...\n");
		DOSBase		= utils_SafeOpenLibrary("dos.library", 50);
		UtilityBase	= utils_SafeOpenLibrary("utility.library", 50);
		QueryBase	= utils_SafeOpenLibrary("query.library", 50);
		
		if (DOSBase && UtilityBase && QueryBase)
		{
			LOCK_INIT(base);
			
			INIT_LOCKED_LIST(base->hb_Hardwares);
			INIT_LOCKED_LIST(base->hb_Classes);
			
			NEWLIST(&base->hb_Listeners);
			LOCK_INIT(&base->hb_Listeners);
			
			base->hb_HwCount = 0;
			
			/* Timer used by Helios_DelayMS.
			 * Always use a copy, never directly.
			 */
			base->hb_TimeReq.tr_node.io_Message.mn_Node.ln_Type = NT_MESSAGE;
			if(!OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *) &base->hb_TimeReq, 0))
			{
				_DBG_LIB("init done\n");
				return (APTR)base;
			}
			else
				_ERR_LIB("OpenDevice(\"timer.device\") failed\n");
		}
		
		CLEARLIBBASE(DOSBase);
		CLEARLIBBASE(UtilityBase);
		CLEARLIBBASE(QueryBase);

		DeletePool(base->hb_MemPool);
	}
	else
		_ERR_LIB("CreatePool() failed\n");

	FreeMem((APTR)((ULONG)(base) - (ULONG)(base->hb_Lib.lib_NegSize)),
			base->hb_Lib.lib_NegSize + base->hb_Lib.lib_PosSize);

	_ERR_LIB("LibInit failed\n");
	return NULL;
}
ENDFUNC
LIBOPEN(struct HeliosBase, base)
{
	base->hb_Lib.lib_OpenCnt++;
	base->hb_Lib.lib_Flags &= ~LIBF_DELEXP;
		
	if (base->hb_Lib.lib_OpenCnt > 1)
		return (APTR)base;
		
	_DBG_LIB("open done\n", base);
	return (APTR)base;
}
ENDFUNC
LIBCLOSE(struct HeliosBase, base)
{
	if ((--base->hb_Lib.lib_OpenCnt) != 0)
		return 0;

	if (base->hb_Lib.lib_Flags & LIBF_DELEXP)
	{
		_DBG_LIB("LIBF_DELEXP set\n");
		return my_lib_expunge(base);
	}

	return 0;
}
ENDFUNC
LIBEXPUNGE(struct HeliosBase, base)
{
	return my_lib_expunge(base);
}
ENDFUNC
/*============================================================================*/
/*--- RESIDENT ---------------------------------------------------------------*/
/*============================================================================*/
struct LibInitStruct libInit=
{
	.LibSize	= sizeof(struct HeliosBase),
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