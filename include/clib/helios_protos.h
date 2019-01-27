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

/* Timing API */
extern struct timerequest *Helios_OpenTimer(struct MsgPort *port, ULONG unit);
extern void Helios_CloseTimer(struct timerequest *tr)  ;
extern void Helios_DelayMS(ULONG milli);

/* Subtask API */
extern HeliosSubTask *Helios_CreateSubTaskA(CONST_STRPTR name, HeliosSubTaskEntry entry, struct TagItem *tags);
extern LONG Helios_KillSubTask(HeliosSubTask *ctx, LONG mustwait);
extern LONG Helios_SendMsgToSubTask(HeliosSubTask *ctx, struct Message *msg);
extern LONG Helios_DoMsgToSubTask(HeliosSubTask *ctx, struct Message *msg, struct MsgPort *replyport);
extern LONG Helios_SignalSubTask(HeliosSubTask *ctx, ULONG sigmask);
extern BOOL Helios_GetSubTaskRC(HeliosSubTask *ctx, LONG *rc_ptr);

/* Event API */
extern void Helios_AddEventListener(HeliosEventListenerList *list, HeliosEventMsg *node);
extern void Helios_RemoveEventListener(HeliosEventListenerList *list, HeliosEventMsg *node);
extern void Helios_SendEvent(HeliosEventListenerList *list, ULONG event, ULONG result);

/* Objects API */
extern void Helios_ObjectInitA(APTR obj, LONG type, struct TagItem *tags);
extern APTR Helios_FindObjectA(LONG type, APTR obj, struct TagItem *tags);
extern LONG Helios_SetAttrsA(APTR obj, struct TagItem *tags);
extern LONG Helios_GetAttrsA(APTR obj, struct TagItem *tags);
extern LONG Helios_ObtainObject(APTR obj);
extern void Helios_ReleaseObject(APTR obj);

/* ROM functions */
extern QUADLET *Helios_CreateROMTagList(APTR pool, CONST struct TagItem *tags);
extern void Helios_FreeROM(APTR pool, QUADLET *rom);
extern LONG Helios_ReadROM(
	HeliosHardware *hh,
	UWORD nodeID,
	UBYTE gen,
	HeliosSpeed maxSpeed,
	QUADLET *storage, ULONG *length);

extern void Helios_InitRomIterator(HeliosRomIterator *ri, const QUADLET *rom);
extern LONG Helios_RomIterate(HeliosRomIterator *ri, QUADLET *key, QUADLET *value);
extern LONG Helios_ReadTextualDescriptor(const QUADLET *dir, STRPTR buffer, ULONG length);
extern UWORD Helios_ComputeCRC16(const QUADLET *data, ULONG len);

/* Hardwares API */
extern void Helios_FlushA(struct TagItem *tags);
extern struct MsgPort *Helios_AddHardware(STRPTR name, LONG unit);
extern void Helios_SendIO(HeliosHardware *hh, struct IORequest *ioreq);
extern LONG Helios_DoIO(HeliosHardware *hh, struct IORequest *ioreq);

/* Classes API */
extern HeliosClass *Helios_AddClass(STRPTR name, ULONG version);
extern void Helios_RemoveClass(HeliosClass *hc);

/* VarArgs APIs */
#if !defined(USE_INLINE_STDARG)
extern void Helios_Flush(...);
extern void Helios_ObjectInit(APTR obj, LONG type, ...);
extern APTR Helios_FindObject(LONG type, APTR obj, ...);
extern APTR Helios_SetAttrs(APTR obj, ...);
extern APTR Helios_GetAttrs(APTR obj, ...);
extern HeliosSubTask *Helios_CreateSubTask(CONST_STRPTR name, HeliosSubTaskEntry entry, ...);
extern QUADLET *Helios_CreateROM(APTR pool, ...);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CLIB_HELIOS_PROTOS_H */
