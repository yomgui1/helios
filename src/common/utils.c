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
** Some usefull routines.
**
*/

#include "utils.h"

#include <proto/exec.h>
#include <stdarg.h>
#include <string.h>

APTR utils_PutChProc(APTR data, UBYTE c)
{
    STRPTR *string_p = data;
    ((*string_p)++)[0] = c;
    return data;
}

APTR utils_SafePutChProc(APTR data, UBYTE c)
{
    struct safe_buf *sb = data;

    if (sb->size)
    {
        (sb->buf++)[0] = c;
        sb->size--;
    }

    return data;
}

APTR utils_SPrintF(STRPTR buf, STRPTR fmt, ...)
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    STRPTR s = buf;
    va_list va;

    va_start(va, fmt);
    VNewRawDoFmt(fmt, utils_PutChProc, (APTR)&s, va);
    va_end(va);

    return buf;
}

APTR utils_SafeSPrintF(STRPTR buf, ULONG size, STRPTR fmt, ...)
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    struct safe_buf sb = {buf, size};
    va_list va;

    va_start(va, fmt);
    VNewRawDoFmt(fmt, utils_SafePutChProc, (APTR)&sb, va);
    va_end(va);

    return buf;
}

STRPTR utils_DupStr(APTR pool, CONST_STRPTR src)
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    ULONG len = strlen(src) + 1;
    STRPTR dest;

    if (NULL != pool)
        dest = AllocVecPooled(pool, len);
    else
        dest = AllocVec(len, MEMF_PUBLIC);

    if (NULL != dest)
        CopyMem((STRPTR)src, dest, len);

    return dest;
}

ULONG utils_GetPhyAddress(APTR addr)
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    ULONG len = 16; // Hardware field len
    APTR busaddr = CachePreDMA(addr, &len, 0L);

    return (ULONG) busaddr;
}

