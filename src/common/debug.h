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
** Header file for loggin facilities.
*/

#ifndef DEBUG_H
#define DEBUG_H

#include <clib/debug_protos.h>

#include <stdio.h>
#include <stdarg.h>

extern void dprintf();
extern void vdprintf();

#ifndef NDEBUG

#define XDBFN(_h, _f, _a...) dprintf(_h "%6s:%4lu:%-30s: " _f,DBNAME,__LINE__,__FUNCTION__,##_a)

#define DB(x, a...) dprintf("OHCI1394: "x , ##a)
#ifndef NIRQDEBUG
	#define IRQDB(x, a...) dprintf("{IRQ} "x , ##a)
#else
	#define IRQDB(x, a...)
#endif
#define DB_Raw(x, a...) dprintf(x , ## a)
#define DB_NotImplemented() DB("%s: %s() not implemented\n",__FILE__,__FUNCTION__)
#define DB_NotFinished() DB("%s: %s() not finished\n",__FILE__,__FUNCTION__)
#define DB_IN(fmt, a...) DB("+ %s + "fmt"\n",__FUNCTION__,##a)
#define DB_OUT() DB("- %s -\n",__FUNCTION__)
#define log_Debug(m, a...) dprintf("OHCI1394-Debug: "m"\n" ,##a)
#else

#define XDBFN(h, x, a...) ({;})

#define DB(x, a...)
#define IRQDB(x, a...)
#define DB_Raw(x, a...)
#define DB_NotImplemented()
#define DB_NotFinished()
#define DB_IN(fmt, a...)
#define DB_OUT()
#define log_Debug(x,a...)
#endif

#ifndef DBNAME
#error "DBNAME shall be defined before including debug.h file."
#endif

#define _ERR(_fmt, _a...) XDBFN("[E]", _fmt, ##_a)
#define _WRN(_fmt, _a...) XDBFN("[W]", _fmt, ##_a)
#define _INF(_fmt, _a...) XDBFN("[I]", _fmt, ##_a)
#define _DBG(_fmt, _a...) XDBFN("[D]", _fmt, ##_a)

#ifdef DEBUG_SYSTEM_LIBRARY
#define DH_LIB "<lib> "
#define DH_DEV "<dev> "

#define _ERR_LIB(_fmt, _a...) _ERR(DH_LIB _fmt, ##_a)
#define _WRN_LIB(_fmt, _a...) _WRN(DH_LIB _fmt, ##_a)
#define _INF_LIB(_fmt, _a...) _INF(DH_LIB _fmt, ##_a)
#define _DBG_LIB(_fmt, _a...) _DBG(DH_LIB _fmt, ##_a)

#define _ERR_DEV(_fmt, _a...) _ERR(DH_DEV _fmt, ##_a)
#define _WRN_DEV(_fmt, _a...) _WRN(DH_DEV _fmt, ##_a)
#define _INF_DEV(_fmt, _a...) _INF(DH_DEV _fmt, ##_a)
#define _DBG_DEV(_fmt, _a...) _DBG(DH_DEV _fmt, ##_a)
#endif

#define log_APIError(m) _log_APIError(__FUNCTION__, m)
static inline void _log_APIError(CONST_STRPTR fname, CONST_STRPTR msg)
{
    DB("  Function '%s' failed: \"%s\"\n", fname, msg);
}
#define log_Error(m, a...) dprintf(DBNAME"-Error: "m"\n" ,##a)

#ifdef DEBUG_MEM
#include <hardware/byteswap.h>
static inline void log_DumpMem(APTR mem, ULONG len, BOOL swap, const char *title)
{
    ULONG i;

    if (NULL != title) {
        DB(""); DB_Raw(title);
    }

    if (len > 2068)
        len = 2068;

    for (i=0; i < len; i+=4, mem+=4) {
        ULONG v = *(ULONG *)mem;
        if (swap) {v = SWAPLONG(v);}
        DB_Raw("0x%08x: %08x\n", mem, v);
    }
}
#else
#define log_DumpMem(...)
#endif

#endif /* DEBUG_H */
