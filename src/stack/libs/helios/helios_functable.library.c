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
**
*/

#include "helios_base.library.h"
#include "clib/helios_protos.h"

extern void LIB_Open(void);
extern void LIB_Close(void);
extern void LIB_Expunge(void);
extern void LIB_Reserved(void);
extern LONG Helios_RomStop(void);

ULONG LibFuncTable[]=
{
    FUNCARRAY_BEGIN,

    FUNCARRAY_32BIT_NATIVE,
    (ULONG) &LIB_Open,
    (ULONG) &LIB_Close,
    (ULONG) &LIB_Expunge,
    (ULONG) &LIB_Reserved,
    -1,

    FUNCARRAY_32BIT_SYSTEMV,
    (ULONG) &Helios_DBG_DumpDevices,
    (ULONG) &Helios_WriteLockBase,
    (ULONG) &Helios_ReadLockBase,
    (ULONG) &Helios_UnlockBase,
    (ULONG) &Helios_VReportMsg,
    (ULONG) &Helios_ReportMsg,
    (ULONG) &Helios_GetNextReportMsg,
    (ULONG) &Helios_FreeReportMsg,
    (ULONG) &Helios_AddEventListener,
    (ULONG) &Helios_RemoveEventListener,
    (ULONG) &Helios_SendEvent,
    (ULONG) &Helios_OpenTimer,
    (ULONG) &Helios_CloseTimer,
    (ULONG) &Helios_DelayMS,
    (ULONG) &Helios_CreateSubTaskA,
    (ULONG) &Helios_KillSubTask,
    (ULONG) &Helios_IsSubTaskKilled,
    (ULONG) &Helios_SendMsgToSubTask,
    (ULONG) &Helios_DoMsgToSubTask,
    (ULONG) &Helios_SignalSubTask,
    (ULONG) &Helios_WaitTaskReady,
    (ULONG) &Helios_TaskReady,
    (ULONG) &Helios_GetNextHardware,
    (ULONG) &Helios_AddHardware,
    (ULONG) &Helios_RemoveHardware,
    (ULONG) &Helios_ReleaseHardware,
    (ULONG) &Helios_DisableHardware,
    (ULONG) &Helios_BusReset,
    (ULONG) &Helios_CreateROMTagList,
    (ULONG) &Helios_FreeROM,
    (ULONG) &Helios_ReadROM,
    (ULONG) &Helios_InitRomIterator,
    (ULONG) &Helios_RomIterate,
    (ULONG) &Helios_SetAttrsA,
    (ULONG) &Helios_GetAttrsA,
    (ULONG) &Helios_AddDevice,
    (ULONG) &Helios_RemoveDevice,
    (ULONG) &Helios_UpdateDevice,
    (ULONG) &Helios_GetNextDeviceA,
    (ULONG) &Helios_ObtainDevice,
    (ULONG) &Helios_ReleaseDevice,
    (ULONG) &Helios_ScanDevice,
    (ULONG) &Helios_ReadLockDevice,
    (ULONG) &Helios_WriteLockDevice,
    (ULONG) &Helios_UnlockDevice,
    (ULONG) &Helios_ObtainUnit,
    (ULONG) &Helios_ReleaseUnit,
    (ULONG) &Helios_GetNextUnitA,
    (ULONG) &Helios_BindUnit,
    (ULONG) &Helios_UnbindUnit,
    (ULONG) &Helios_AddClass,
    (ULONG) &Helios_RemoveClass,
    (ULONG) &Helios_ObtainClass,
    (ULONG) &Helios_ReleaseClass,
    (ULONG) &Helios_GetNextClass,
    (ULONG) &Helios_InitIO,
    (ULONG) &Helios_DoIO,
    (ULONG) &Helios_ComputeCRC16,
    (ULONG) &Helios_ReadTextualDescriptor,
    -1,

    FUNCARRAY_END
};
