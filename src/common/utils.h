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
** Header file for utils.c.
**
*/

#ifndef UTILS_H
#define UTILS_H

#include "libraries/helios.h"

#include <hardware/atomic.h>
#include <clib/exec_protos.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <stddef.h> // offsetof()

#ifndef ATOMIC_NCMPXCHG
static inline LONG _ATOMIC_NCMPXCHG(volatile LONG *ptr, LONG old, LONG new)
{
    register LONG ret;

    __asm__ __volatile__ (
    "\n"
    ".atomic_ncmpxchg_loop_%=:\n"
    "   lwarx   %0,0,%2\n"
    "   cmpw    0,%0,%3\n"
    "   beq-    .atomic_ncmpxchg_out_%=\n"
    "   stwcx.  %4,0,%2\n"
    "   bne-    .atomic_ncmpxchg_loop_%=\n"
    ".atomic_ncmpxchg_out_%=:\n"
    : "=&r" (ret)
    : "m" (*ptr), "r" (ptr), "r" (old), "r" (new)
    : "memory", "cr0");

    return ret;
};
#define ATOMIC_NCMPXCHG(p,o,n) _ATOMIC_NCMPXCHG(p,o,n)
#endif

/* Lock region facilities */
#define LOCK_VARIABLE struct SignalSemaphore SpinLock
#define LOCK_INIT(x) InitSemaphore(&(x)->SpinLock);

//#define DEBUG_LOCKS
#ifdef DEBUG_LOCKS
#define LOCK_REGION(x) ({ \
    DB("+ %s[%lu]: %p try lock %p\n", __FUNCTION__, __LINE__, x, FindTask(NULL)); \
    ObtainSemaphore(&(x)->SpinLock); \
    DB("+ %s[%lu]: %p locked %p\n", __FUNCTION__, __LINE__, x, FindTask(NULL)); })
#define UNLOCK_REGION(x) ({ DB("- %s[%lu]: unlock %p %p\n", __FUNCTION__, __LINE__, x, FindTask(NULL)); ReleaseSemaphore(&(x)->SpinLock); })
#else
#define LOCK_REGION(x) ObtainSemaphore(&(x)->SpinLock)
#define UNLOCK_REGION(x) ReleaseSemaphore(&(x)->SpinLock)
#endif

#ifdef DEBUG_LOCKS
#define LOCK_REGION_SHARED(x) ({ \
    DB("+ %s[%lu]: %p try s-lock %p\n", __FUNCTION__, __LINE__, x, FindTask(NULL)); \
    ObtainSemaphoreShared(&(x)->SpinLock); \
    DB("+ %s[%lu]: %p s-locked %p\n", __FUNCTION__, __LINE__, x, FindTask(NULL)); })
#define UNLOCK_REGION_SHARED(x) ({ DB("- %s[%lu]: s-unlock %p %p\n", __FUNCTION__, __LINE__, x, FindTask(NULL)); ReleaseSemaphore(&(x)->SpinLock); })
#else
#define LOCK_REGION_SHARED(x) ObtainSemaphoreShared(&(x)->SpinLock)
#define UNLOCK_REGION_SHARED(x) ReleaseSemaphore(&(x)->SpinLock);
#endif

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*a))
#define IS_BREAK() (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
#define GET_ALIGNED(offset, align) ({                               \
            ULONG _offset = (ULONG)(offset);                        \
            ULONG _align = (ULONG)(align);                          \
            _offset + ((_align - (_offset % _align)) % _align); })
#define GET_ALIGNED2(offset, align) ({                   \
            ULONG _offset = (ULONG)(offset);             \
            ULONG _align = (ULONG)(align);               \
            _align + ((_offset - 1) & ~(_align - 1)); })

#define INIT_HOOK(h, f) { struct Hook *_h = (struct Hook *)(h); \
    _h->h_Entry = (APTR) HookEntry; \
    _h->h_SubEntry = (APTR) (f); }

struct safe_buf
{
    STRPTR buf;
    ULONG size;
};

extern APTR utils_PutChProc(APTR data, UBYTE c);
extern APTR utils_SafePutChProc(APTR data, UBYTE c);
extern APTR utils_SPrintF(STRPTR buf, STRPTR fmt, ...);
extern APTR utils_SafeSPrintF(STRPTR buf, ULONG size, STRPTR fmt, ...);
extern STRPTR utils_DupStr(APTR pool, CONST_STRPTR src);
extern ULONG utils_GetPhyAddress(APTR addr);

//+ utils_GetQuadletCRC16
/* CRC-16 calculation, performed on quadlets at a time
**
** (Taken from the IEEE13213-1994 CSR Architecture documentation)
*/
static inline UWORD utils_GetQuadletCRC16(QUADLET data, QUADLET check)
{
   int shift, sum, next; /* Integers are >= 32 bits */

   for (next = check, shift = 28; shift >= 0; shift -= 4) {
      sum = ((next >> 12) ^ (data >> shift)) & 0xF;
      next = (next << 4) ^ (sum << 12) ^ (sum << 5) ^ (sum);
   }

   return next & 0xFFFF; /* 16-bit CRC value */
}
//-
//+ utils_GetBlockCRC16
static inline UWORD utils_GetBlockCRC16(QUADLET *ptr, ULONG length)
{
    UWORD crc = 0;

    for (; length; length--)
        crc = utils_GetQuadletCRC16(*ptr++, crc);

    return crc;
}
//-
//+ utils_CountBits32
static inline ULONG utils_CountBits32(ULONG value, ULONG state)
{
    ULONG s, cnt;

    state = state ? 1 : 0;
    cnt = 0;
    for (s=32; s; s--, value >>= 1) {
        if (state == (value & 1))
            cnt ++;
    }

    return cnt;
}
//-

#endif /* UTILS_H */
