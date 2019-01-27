/* Copyright 2008-2013, 2018 Guillaume Roguez

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
** Helios FCP device utilities.
**
*/

#ifndef FCP_UTILS_H
#define FCP_UTILS_H

#include "libraries/helios.h"

#include <hardware/atomic.h>
#include <clib/exec_protos.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <stddef.h> // offsetof()

/* Lock region facilities */
#define LOCK_VARIABLE struct SignalSemaphore SpinLock
#define LOCK_INIT(x) InitSemaphore(&(x)->SpinLock);
#ifdef DEBUG_LOCKS
#define LOCK_REGION(x) ({ \
    DB("+ %s: try lock %p\n", __FUNCTION__, x); \
    ObtainSemaphore(&(x)->SpinLock); \
    DB("+ %s: %p locked\n", __FUNCTION__, x); })
#define UNLOCK_REGION(x) ({ DB("- %s: unlock %p\n", __FUNCTION__, x); ReleaseSemaphore(&(x)->SpinLock); })
#else
#define LOCK_REGION(x) ObtainSemaphore(&(x)->SpinLock)
#define UNLOCK_REGION(x) ReleaseSemaphore(&(x)->SpinLock)
#endif

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*a))

#define IS_BREAK() (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)

#endif /* FCP_UTILS_H */
