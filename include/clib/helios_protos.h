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

#ifndef CLIB_HELIOS_PROTOS_H
#define CLIB_HELIOS_PROTOS_H

/*
**
** Helios C prototypes.
**
*/

#include <libraries/helios.h>
#include <devices/helios.h>
#include <devices/timer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Base API */
extern void Helios_WriteLockBase(void);
extern void Helios_ReadLockBase(void);
extern void Helios_UnlockBase(void);
extern LONG Helios_VReportMsg(ULONG type, CONST_STRPTR label, CONST_STRPTR fmt, va_list args);
extern LONG Helios_ReportMsg(ULONG type, CONST_STRPTR label, CONST_STRPTR fmt, ...);
extern HeliosReportMsg *Helios_GetNextReportMsg(void);
extern void Helios_FreeReportMsg(HeliosReportMsg *msg);
extern UWORD Helios_ComputeCRC16(const QUADLET *data, ULONG len);

/* Event API */
extern void Helios_AddEventListener(HeliosEventListenerList *list, HeliosEventMsg *node);
extern void Helios_RemoveEventListener(HeliosEventListenerList *list, HeliosEventMsg *node);
extern void Helios_SendEvent(HeliosEventListenerList *list, ULONG event, ULONG result);

/* Subtask API */
extern HeliosSubTask *Helios_CreateSubTaskA(CONST_STRPTR name, HeliosSubTaskEntry entry, struct TagItem *tags);
extern LONG Helios_KillSubTask(HeliosSubTask *ctx);
extern LONG Helios_SendMsgToSubTask(HeliosSubTask *ctx, struct Message *msg);
extern LONG Helios_DoMsgToSubTask(HeliosSubTask *ctx, struct Message *msg, struct MsgPort *replyport);
extern ULONG Helios_SignalSubTask(HeliosSubTask *ctx, ULONG sigmask);
extern LONG Helios_WaitTaskReady(HeliosSubTask *ctx, LONG sigs);
extern void Helios_TaskReady(HeliosSubTask *ctx, BOOL success);
extern LONG Helios_IsSubTaskKilled(HeliosSubTask *ctx);

/* Timing API */
extern struct timerequest *Helios_OpenTimer(struct MsgPort *port, ULONG unit);
extern void Helios_CloseTimer(struct timerequest *tr)  ;
extern void Helios_DelayMS(ULONG milli);

/* ROM functions */
extern QUADLET *Helios_CreateROMTagList(APTR pool, CONST struct TagItem *tags);
extern void Helios_FreeROM(APTR pool, QUADLET *rom);
extern LONG Helios_ReadROM(HeliosDevice *dev, QUADLET *storage, ULONG *length);

extern void Helios_InitRomIterator(HeliosRomIterator *ri, const QUADLET *rom);
extern LONG Helios_RomIterate(HeliosRomIterator *ri, QUADLET *key, QUADLET *value);
extern LONG Helios_ReadTextualDescriptor(const QUADLET *dir, STRPTR buffer, ULONG length);

/* Objects API */
extern LONG Helios_SetAttrsA(ULONG type, APTR obj, struct TagItem *tags);
extern LONG Helios_GetAttrsA(ULONG type, APTR obj, struct TagItem *tags);
extern void Helios_InitIO(ULONG type, APTR obj, IOHeliosHWReq *ioreq);
extern LONG Helios_DoIO(ULONG type, APTR obj, IOHeliosHWReq *ioreq);

/* Hardwares API */
extern HeliosHardware *Helios_GetNextHardware(HeliosHardware *hw);
extern HeliosHardware *Helios_AddHardware(STRPTR name, LONG unit);
extern LONG Helios_RemoveHardware(HeliosHardware *hw);
extern LONG Helios_DisableHardware(HeliosHardware *hw, BOOL state);
extern LONG Helios_BusReset(HeliosHardware *hw, BOOL short_br);
extern void Helios_ReleaseHardware(HeliosHardware *hw);

/* Devices API */
extern HeliosDevice *Helios_AddDevice(HeliosHardware *hw,
                                      HeliosNode *node,
                                      ULONG topogen);
extern void Helios_RemoveDevice(HeliosDevice *dev);
extern void Helios_UpdateDevice(HeliosDevice *dev, HeliosNode *node, ULONG topogen);
extern HeliosDevice *Helios_GetNextDeviceA(HeliosDevice *dev, struct TagItem *tags);
extern LONG Helios_ObtainDevice(HeliosDevice *dev);
extern void Helios_ReleaseDevice(HeliosDevice *dev);
extern void Helios_ScanDevice(HeliosDevice *dev);
extern void Helios_ReadLockDevice(HeliosDevice *dev);
extern void Helios_WriteLockDevice(HeliosDevice *dev);
extern void Helios_UnlockDevice(HeliosDevice *dev);

/* Units API */
extern LONG Helios_ObtainUnit(HeliosUnit *unit);
extern void Helios_ReleaseUnit(HeliosUnit *unit);
extern HeliosUnit *Helios_GetNextUnitA(HeliosUnit *unit, struct TagItem *tags);
extern LONG Helios_BindUnit(HeliosUnit *unit, HeliosClass *hc, APTR udata);
extern LONG Helios_UnbindUnit(HeliosUnit *unit);

/* Classes API */
extern HeliosClass *Helios_AddClass(STRPTR name, ULONG version);
extern void Helios_RemoveClass(HeliosClass *hc);
extern LONG Helios_ObtainClass(HeliosClass *hc);
extern void Helios_ReleaseClass(HeliosClass *hc);
extern HeliosClass *Helios_GetNextClass(HeliosClass *hc);

/* DBG */
extern void Helios_DBG_DumpDevices(HeliosHardware *hw);

/* VarArgs APIs */
#if !defined(USE_INLINE_STDARG)
extern HeliosSubTask *Helios_CreateSubTask(CONST_STRPTR name, HeliosSubTaskEntry entry, ...);
extern QUADLET *Helios_CreateROM(APTR pool, ...);
extern APTR Helios_SetAttrs(ULONG type, APTR obj, ...);
extern APTR Helios_GetAttrs(ULONG type, APTR obj, ...);
extern HeliosDevice *Helios_GetNextDevice(HeliosDevice *dev, ...);
extern HeliosUnit *Helios_GetNextUnit(HeliosUnit *dev, ...);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CLIB_HELIOS_PROTOS_H */
