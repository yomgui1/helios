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
**
*/

#ifndef AVC1394_LOG_H
#define AVC1394_LOG_H

#include <clib/debug_protos.h>
#include <hardware/atomic.h>
#include <hardware/byteswap.h>

#include <stdio.h>
#include <stdarg.h>

#ifndef NDEBUG

#define DB(x, a...) kprintf("AVC1394: "x , ## a)
#define DB_Raw(x, a...) kprintf(x , ## a)
#define DB_NotImplemented() DB("%s: %s() not implemented\n", __FILE__, __FUNCTION__)
#define DB_NotFinished() DB("%s: %s() not finished\n", __FILE__, __FUNCTION__)
#define DB_IN(fmt, a...) DB("+ "__FUNCTION__" + "fmt"\n", ## a)
#define DB_OUT() DB("- "__FUNCTION__" -\n")

extern void kprintf();
extern void vkprintf();

#else

#define DB(x, a...)
#define DB_Raw(x, a...)
#define DB_NotImplemented()
#define DB_NotFinished()
#define DB_IN(fmt, a...)
#define DB_OUT()

#endif

#define log_APIError(m) DB("Failed "__FILE__":%lu: \"%s\"\n", __LINE__, m);
#define log_Error(m, a...) DB("AVC1394 Error: '"m"'\n" , ## a)

#if !defined(NDEBUG) && defined(DEBUG_MEM)
static inline void log_DumpMem(APTR mem, ULONG len, BOOL swap, const char *title)
{
    ULONG i;

    if (NULL != title) {
        DB(""); DB_Raw(title);
    }

    if (len >= 300)
        len = 300;

    for (i=0; i < len; i+=4, mem+=4) {
        ULONG v = *(ULONG *)mem;
        if (swap) {v = SWAPLONG(v);}
        DB_Raw("0x%08x: %08x\n", mem, v);
    }
}
#else
#define log_DumpMem(x...)
#endif

#endif /* AVC1394_LOG_H */
