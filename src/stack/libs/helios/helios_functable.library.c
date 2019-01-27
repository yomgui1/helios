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
** Helios Library functions table definition file.
*/

#include "private.h"

ULONG libFuncTable[]=
{
	FUNCARRAY_BEGIN,

	FUNCARRAY_32BIT_NATIVE,
	(ULONG) LIB_OPEN_FNAME,
	(ULONG) LIB_CLOSE_FNAME,
	(ULONG) LIB_EXPUNGE_FNAME,
	(ULONG) RESERVED_FNAME,
	-1,

	FUNCARRAY_32BIT_SYSTEMV,
	(ULONG) Helios_AddHardware,
	(ULONG) Helios_SendIO,
	(ULONG) Helios_DoIO,
	
	(ULONG) Helios_OpenTimer,
	(ULONG) Helios_CloseTimer,
	(ULONG) Helios_DelayMS,
	
	(ULONG) Helios_CreateSubTaskA,
	(ULONG) Helios_KillSubTask,
	(ULONG) Helios_GetSubTaskRC,
	(ULONG) Helios_SendMsgToSubTask,
	(ULONG) Helios_DoMsgToSubTask,
	(ULONG) Helios_SignalSubTask,

	(ULONG) Helios_AddEventListener,
	(ULONG) Helios_RemoveEventListener,
	(ULONG) Helios_SendEvent,
	
	(ULONG) Helios_ObjectInitA,
	(ULONG) Helios_FindObjectA,
	(ULONG) Helios_SetAttrsA,
	(ULONG) Helios_GetAttrsA,
	(ULONG) Helios_ObtainObject,
	(ULONG) Helios_ReleaseObject,

	(ULONG) Helios_ComputeCRC16,
	(ULONG) Helios_CreateROMTagList,
	(ULONG) Helios_FreeROM,
	(ULONG) Helios_ReadROM,
	(ULONG) Helios_InitRomIterator,
	(ULONG) Helios_RomIterate,
	(ULONG) Helios_ReadTextualDescriptor,
	
	(ULONG) Helios_AddClass,
	(ULONG) Helios_RemoveClass,
	
	(ULONG) Helios_FlushA,

	-1,

	FUNCARRAY_END
};
