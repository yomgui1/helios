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
** Private Helios Structures and Definitions.
**
*/

#ifndef LIBRARIES_HELIOS_INTERNAL_H
#define LIBRARIES_HELIOS_INTERNAL_H

#define HELIOS_INTERNAL 1

#include "libraries/helios.h"
#include "devices/helios.h"
#include "utils.h"
#include "debug.h"

#include <exec/ports.h>

#include <hardware/atomic.h>

#ifndef NDEBUG
#   ifdef DEBUG_REFCNT
#       define _INFO_REFCNT _INFO
#       define _WARN_REFCNT _WARN
#   else
#       define _INFO_REFCNT(x,a...)
#       define _WARN_REFCNT(x,a...)
#   endif /* DEBUG_REFCNT */
#else
#   define _INFO_REFCNT(x,a...)
#   define _WARN_REFCNT(x,a...)
#endif /* !NDEBUG */

#define _SHOBJ_HEAD \
    struct Node     hso_SysNode; \
    LONG            hso_RefCnt;  \
    LOCK_VARIABLE;

typedef struct _HeliosSharedObj
{
    _SHOBJ_HEAD
} _HeliosSharedObj;

struct HeliosEventListenerList
{
    struct MinList ell_SysList;
    LOCK_VARIABLE;
};

typedef union HeliosIds
{
    QUADLET array[4];
    struct
    {
        QUADLET VendorID;
        QUADLET ModelID;
        QUADLET UnitSpecID;
        QUADLET UnitSWVersion;
    } fields __attribute__((packed));
} HeliosIds;

struct HeliosDevice
{
    _SHOBJ_HEAD

    HeliosHardware *        hd_Hardware;
    HeliosEventListenerList hd_Listeners;
    union
    {
        UQUAD               q;
        struct
        {
            ULONG           hi;
            ULONG           lo;
        } w;
    }                       hd_GUID;
    ULONG                   hd_Generation;
    ULONG                   hd_NodeID;
    HeliosNode              hd_NodeInfo;
    struct Task *           hd_RomScanner;
    QUADLET *               hd_Rom;
    ULONG                   hd_RomLength;
    HeliosIds               hd_Ids;
    struct MinList          hd_Units; /* Logical units */
};

#define HELIOS_HW_HEAD                                                  \
    struct Unit             hu_Unit;    /* It's a system device unit */ \
    LONG                    hu_UnitNo;  /* Logical unit # */            \
    struct Device *         hu_Device;  /* up link */                   \
    LOCK_VARIABLE; /* General RW access protection */                   \
    LONG                    hso_RefCnt; /* Keep this field as in _SHO_HEAD */ \
    HeliosSubTask *         hu_HWTask;                                  \
    struct MinNode          hu_HWNode;                                  \
    HeliosEventListenerList hu_Listeners;                               \
    struct MinList          hu_Devices;                                 \
    UWORD                   hu_LocalNodeId; /* Known after successfull Self-ID process */ \
    UWORD                   hu_Pad0;

struct HeliosHardware
{
    HELIOS_HW_HEAD
};

struct HeliosUnit
{
    _SHOBJ_HEAD

    /* Following fields are set to NULL when unit is removed */

    LONG             hu_UnitNo;       /* unit number given during device's ROM parsing */
    HeliosDevice *   hu_Device;       /* up link */
    HeliosHardware * hu_Hardware;     /* up link */
    const QUADLET *  hu_RomDirectory; /* Pointer inside hu_Device->hd_Rom */
    HeliosIds        hu_Ids;          /* Useful to for bind classes */

    /* Binding field */
    HeliosClass *    hu_BindClass; /* driver class */
    APTR             hu_BindUData; /* udata for driver implementor */
};

struct HeliosClass
{
    _SHOBJ_HEAD

    struct Library *        hc_Base;
    STRPTR                  hc_LoadedName;
    HeliosEventListenerList hc_Listeners;
};

#define _SHOBJ_DECREF(n, o) ({ \
            LONG _x = ATOMIC_FETCH(&o->hso_RefCnt); \
            _x = ATOMIC_NCMPXCHG(&o->hso_RefCnt, 0, _x-1); \
            _INFO_REFCNT("["n":%p] ctn was %d\n", o, _x); _x; })

#define _SHOBJ_INCREF(n, o) ({ \
            LONG _x = ATOMIC_FETCH(&o->hso_RefCnt); \
            _x = ATOMIC_NCMPXCHG(&o->hso_RefCnt, 0, _x+1); \
            _INFO_REFCNT("["n":%p] ctn was %d\n", o, _x); _x; })

#define CLS_DECREF(o) _SHOBJ_DECREF("CLS", o)
#define CLS_INCREF(o) _SHOBJ_INCREF("CLS", o)
#define HW_DECREF(o) _SHOBJ_DECREF("HW", o)
#define HW_INCREF(o) _SHOBJ_INCREF("HW", o)
#define DEV_DECREF(o) _SHOBJ_DECREF("DEV", o)
#define DEV_INCREF(o) _SHOBJ_INCREF("DEV", o)
#define UNIT_DECREF(o) _SHOBJ_DECREF("UNIT", o)
#define UNIT_INCREF(o) _SHOBJ_INCREF("UNIT", o)

#endif /* LIBRARIES_HELIOS_INTERNAL_H */
