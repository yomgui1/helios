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
*/

#ifndef UTILS_H
#define UTILS_H

#include "libraries/helios.h"
#include "debug.h"

#include <proto/exec.h>
#include <proto/intuition.h>

#include <hardware/atomic.h>
#include <exec/semaphores.h>
#include <exec/resident.h>
#include <dos/dos.h>
#include <intuition/intuition.h>

#include <stddef.h> // offsetof()

/* Enable to print debug messages to console */
//#define DEBUG_LOCKS

#ifndef ATOMIC_NCMPXCHG
/* Return the current stored value */
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
	"   mr      %0,%4\n"
	"   bne-    .atomic_ncmpxchg_loop_%=\n"
	".atomic_ncmpxchg_out_%=:\n"
	: "=&r" (ret)
	: "m" (*ptr), "r" (ptr), "r" (old), "r" (new)
	: "memory", "cr0");

	return ret;
};
#define ATOMIC_NCMPXCHG(p,o,n) _ATOMIC_NCMPXCHG(p,o,n)
#endif

/*---- Mutex facilities ----
 *
 * Current implementation uses SignalSemaphore
 *
 * NOTE: if the lock is used only to lock a single long variable
 * to read/write it, it's preferable to use ATOMIC_ macro (faster).
 */

#define LOCK_VAR_NAME _HeliosMutex
#define LOCK_PROTO struct SignalSemaphore LOCK_VAR_NAME
#define LOCK_INIT(_x) InitSemaphore(&(_x)->LOCK_VAR_NAME);

#define _EXCLUSIVE_PROTECT_BEGIN(_x) ObtainSemaphore(&(_x)->LOCK_VAR_NAME)
#define _SHARED_PROTECT_BEGIN(_x) ObtainSemaphoreShared(&(_x)->LOCK_VAR_NAME)
#define _ANY_PROTECT_END(_x) ReleaseSemaphore(&(_x)->LOCK_VAR_NAME)

#define DH_LOCK "<lock> "

#ifdef DEBUG_LOCKS
	#define EXCLUSIVE_PROTECT_BEGIN(_x) ({ \
		_DBG(DH_LOCK " XP?:%s:%lu:P%p:V%p\n", __FUNCTION__, __LINE__, FindTask(NULL), &(_x)->LOCK_VAR_NAME); \
		_EXCLUSIVE_PROTECT_BEGIN(_x); \
		_DBG(DH_LOCK " XP+:%s:%lu:P%p:V%p\n", __FUNCTION__, __LINE__, FindTask(NULL), &(_x)->LOCK_VAR_NAME); })

	#define SHARED_PROTECT_BEGIN(_x) ({ \
		_DBG(DH_LOCK " SP?:%s:%lu:P%p:V%p\n", __FUNCTION__, __LINE__, FindTask(NULL), &(_x)->LOCK_VAR_NAME); \
		_SHARED_PROTECT_BEGIN(_x); \
		_DBG(DH_LOCK " SP+:%s:%lu:P%p:V%p\n", __FUNCTION__, __LINE__, FindTask(NULL), &(_x)->LOCK_VAR_NAME); })

	#define ANY_PROTECT_END(_x) ({ \
		_DBG(DH_LOCK " *P-:%s:%lu:P%p:V%p\n", __FUNCTION__, __LINE__, FindTask(NULL), &(_x)->LOCK_VAR_NAME); \
		_ANY_PROTECT_END(_x); })
#else
	#define EXCLUSIVE_PROTECT_BEGIN		_EXCLUSIVE_PROTECT_BEGIN
	#define SHARED_PROTECT_BEGIN		_SHARED_PROTECT_BEGIN
	#define ANY_PROTECT_END				_ANY_PROTECT_END
#endif

/* Aliases for commodity */
#define EXCLUSIVE_PROTECT_END ANY_PROTECT_END
#define SHARED_PROTECT_END ANY_PROTECT_END

/*---- Lockable List ----*/
#define LOCKED_MINLIST_PROTO(name) \
	struct SignalSemaphore name##_lock; \
	struct MinList name;

#define INIT_LOCKED_LIST(name) ({ \
	NEWLIST(&(name)); \
	InitSemaphore(&(name##_lock)); })

#define WRITE_LOCK_LIST(name) ObtainSemaphore(&(name##_lock))
#define READ_LOCK_LIST(name) ObtainSemaphoreShared(&(name##_lock))
#define UNLOCK_LIST(name) ReleaseSemaphore(&(name##_lock))

#define SAFE_ADDHEAD(l, n) {WRITE_LOCK_LIST(l); ADDHEAD(&l, n); UNLOCK_LIST(l);}
#define SAFE_ADDTAIL(l, n) {WRITE_LOCK_LIST(l); ADDTAIL(&l, n); UNLOCK_LIST(l);}
#define SAFE_REMOVE(l, n) {WRITE_LOCK_LIST(l); REMOVE(n); UNLOCK_LIST(l);}

/*---- Misc ----*/
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*a))
#define ZERO(v) memset(&(v), 0, sizeof(v))
#define IS_BREAK() (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)

#define GET_ALIGNED(offset, align) ({ \
	ULONG _offset = (ULONG)(offset); \
	ULONG _align = (ULONG)(align); \
	_offset + ((_align - (_offset % _align)) % _align); })

#define GET_ALIGNED2(offset, align) ({ \
	ULONG _offset = (ULONG)(offset); \
	ULONG _align = (ULONG)(align); \
	_align + ((_offset - 1) & ~(_align - 1)); })

#define INIT_HOOK(h, f) { struct Hook *_h = (struct Hook *)(h); \
	_h->h_Entry = (APTR) HookEntry; \
	_h->h_SubEntry = (APTR) (f); }

#define CLEARLIBBASE(_b) if (_b) {CloseLibrary((struct Library *)_b); _b = NULL;}

#define RESERVED_FNAME ReservedFunc
#define ENDFUNC }

/*---- Library helpers ----*/
#define LIB_INIT_FNAME LibInit
#define LIBINIT(_type, _base, _seg) \
	struct Library * LIB_INIT_FNAME(_type *_base, BPTR _seg, struct ExecBase *_sysbase) \
	{ SysBase = _sysbase; _DBG_LIB("base=%p\n", _base);

#define LIB_OPEN_FNAME LibOpen
#define LIBOPEN(_type, _base) \
	struct Library* LIB_OPEN_FNAME(void) { \
		_type *_base = (_type *)REG_A6; \
		_DBG_LIB("base %p, ocnt=%ld\n", _base, ((struct Library *)_base)->lib_OpenCnt);

#define LIB_CLOSE_FNAME LibClose
#define LIBCLOSE(_type, _base) \
	ULONG LIB_CLOSE_FNAME(void) { \
		_type *_base = (_type *)REG_A6; \
		_DBG_LIB("base %p, ocnt=%ld\n", _base, ((struct Library *)_base)->lib_OpenCnt);

#define LIB_EXPUNGE_FNAME LibExpunge
#define LIBEXPUNGE(_type, _base) \
	ULONG LIB_EXPUNGE_FNAME(void) { \
		_type *_base = (_type *)REG_A6; \
		_DBG_LIB("base %p, ocnt=%ld\n", _base, ((struct Library *)_base)->lib_OpenCnt);

/*---- Device helpers ----*/
#ifndef STATIC_DEVICE
#define STATIC_DEVICE extern
#endif

#define DEV_INIT_FNAME DevInit
#define DEVINIT(_type, _base, _seg) \
	struct Device * DEV_INIT_FNAME(_type *_base, BPTR _seg, struct ExecBase *_sysbase) \
	{ SysBase = _sysbase; _DBG_DEV("base=%p\n", _base);

#define DEV_OPEN_FNAME DevOpen
#define DEVOPEN(_type, _base, _ioreq, _unit) \
	STATIC_DEVICE struct Device * DEV_OPEN_FNAME(void) { \
		_type *_base = (_type *)REG_A6; \
		struct IORequest *_ioreq = (APTR) REG_A1; \
		LONG _unit = REG_D0; \
		_DBG_DEV("base %p, ocnt=%ld, ioreq %p\n", _base, ((struct Library *)_base)->lib_OpenCnt, ioreq);

#define DEV_CLOSE_FNAME DevClose
#define DEVCLOSE(_type, _base, _ioreq) \
	STATIC_DEVICE BPTR DEV_CLOSE_FNAME(void) { \
		_type *_base = (_type *)REG_A6; \
		struct IORequest *_ioreq = (APTR) REG_A1; \
		_DBG_DEV("base %p, ocnt=%ld\n", _base, ((struct Library *)_base)->lib_OpenCnt);

#define DEV_EXPUNGE_FNAME DevExpunge
#define DEVEXPUNGE(_type, _base) \
	STATIC_DEVICE BPTR DEV_EXPUNGE_FNAME(void) { \
		_type *_base = (_type *)REG_A6; \
		_DBG_DEV("base %p, ocnt=%ld\n", _base, ((struct Library *)_base)->lib_OpenCnt);

#define DEV_QUERY_FNAME DevQuery
#define DEVQUERY(_type, _base, _data, _attr) \
	STATIC_DEVICE LONG DEV_QUERY_FNAME(void) { \
		_type *_base = (_type *)REG_A6; \
		ULONG *_data = (ULONG *)REG_A0; \
		ULONG _attr = REG_D0; \
		_DBG_DEV("base %p, data %p, attr=0x%08X\n", _base, _data, _attr);

#define DEV_BEGINIO_FNAME DevBeginIO
#define DEVBEGINIO(_type, _base, _ioreq) \
	STATIC_DEVICE void DEV_BEGINIO_FNAME(void) { \
		_type *_base = (_type *)REG_A6; \
		struct IOStdReq *_ioreq = (APTR) REG_A1; \
		_DBG_DEV("base %p, ioreq %p, IO Quick=%u\n", _base, _ioreq, ioreq->io_Flags & IOF_QUICK);

#define DEV_ABORTIO_FNAME DevAbortIO
#define DEVABORTIO(_type, _base, _ioreq) \
	STATIC_DEVICE void DEV_ABORTIO_FNAME(void) { \
		_type *_base = (_type *)REG_A6; \
		struct IOStdReq *ioreq = (APTR) REG_A1; \
		_DBG_DEV("base %p, ioreq %p\n", _base, _ioreq);

extern struct Library * LIB_OPEN_FNAME(void);
extern ULONG LIB_CLOSE_FNAME(void);
extern ULONG LIB_EXPUNGE_FNAME(void);

STATIC_DEVICE struct Device * DEV_OPEN_FNAME(void);
STATIC_DEVICE BPTR DEV_CLOSE_FNAME(void);
STATIC_DEVICE BPTR DEV_EXPUNGE_FNAME(void);
STATIC_DEVICE LONG DEV_QUERY_FNAME(void);
STATIC_DEVICE void DEV_BEGINIO_FNAME(void);
STATIC_DEVICE void DEV_ABORTIO_FNAME(void);

static inline ULONG RESERVED_FNAME(void) { return 0; }

struct LibInitStruct
{
	ULONG	LibSize;
	APTR	FuncTable;
	APTR	DataTable;
	APTR	InitFunc;
};

/*---- Misc ----*/
struct safe_buf
{
	UBYTE *buf;
	ULONG size;
};

extern APTR utils_PutChProc(APTR data, UBYTE c);
extern APTR utils_SafePutChProc(APTR data, UBYTE c);
extern APTR utils_SPrintF(STRPTR buf, STRPTR fmt, ...);
extern APTR utils_SafeSPrintF(STRPTR buf, ULONG size, STRPTR fmt, ...);
extern STRPTR utils_DupStr(APTR pool, CONST_STRPTR src);
extern ULONG utils_GetPhyAddress(APTR addr);

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

static inline UWORD utils_GetBlockCRC16(QUADLET *ptr, ULONG length)
{
	UWORD crc = 0;

	for (; length; length--)
		crc = utils_GetQuadletCRC16(*ptr++, crc);

	return crc;
}

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

static inline void utils_ReportMissingLibrary(CONST_STRPTR libname, ULONG version)
{
	struct IntuitionBase *IntuitionBase;

	IntuitionBase = (APTR)OpenLibrary("intuition.library", 39);
	if (IntuitionBase)
	{
		static CONST struct EasyStruct easy = {
			.es_StructSize		= sizeof(struct EasyStruct),
			.es_Flags			= 0,
			.es_Title			= (UBYTE*) "Helios Error",
			.es_TextFormat		= (UBYTE*) "Missing library: %s V%lu (or better)",
			.es_GadgetFormat	= (UBYTE*) "Ok",
		};
		ULONG args[] = {(ULONG)libname, version};

		EasyRequestArgs(NULL, &easy, NULL, args);
		CloseLibrary((struct Library *)IntuitionBase);
	}
	else
		_ERR("OpenLibrary(\"%s\", %lu) failed\n", libname, version);
}

static inline APTR utils_SafeOpenLibrary(CONST_STRPTR libname, ULONG version)
{
	struct Library *lib;

	lib = OpenLibrary(libname, version);
	if (lib == NULL)
		utils_ReportMissingLibrary(libname, version);

	return lib;
}

#endif /* UTILS_H */
