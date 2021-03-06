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
** Helios library private API header file.
**
*/

#ifndef HELIOS_PRIVATE_H
#define HELIOS_PRIVATE_H

#include "helios_base.library.h"
#include "clib/helios_protos.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#if !defined(BUILD_DATE)
#error "BUILD_DATE are not defined but are mandatory"
#endif

#ifndef SCM_REV
#define SCM_REV "private"
#endif

#define COPYRIGHTS "\xa9\x20Guillaume\x20Roguez\x20[" SCM_REV "]"
#define VERSION 52
#define REVISION 1
#define VR_ST "52.1"
#define VERS    LIBNAME" "VR_ST
#define VSTRING LIBNAME" "VR_ST" ("BUILD_DATE") "COPYRIGHTS"\r\n"
#define VTAG "\0$VER: "LIBNAME" "VR_ST" ("BUILD_DATE") "COPYRIGHTS

#define HeliosClassBase (hc->hc_Base)

#define __NOLIBBASE__
#include "proto/heliosclsbase.h"
#undef __NOLIBBASE__

#if defined(USE_INLINE_STDARG) && !defined(__STRICT_ANSI__)
#include <stdarg.h>
#define Helios_SetAttrs(__p0, __p1, ...) \
    ({ULONG _tags[] = { __VA_ARGS__ }; \
    Helios_SetAttrsA(__p0, __p1, (struct TagItem *)_tags);})
#define Helios_CreateROM(__p0, ...) \
    ({ULONG _tags[] = { __VA_ARGS__ }; \
    Helios_CreateROMTagList(__p0, (CONST struct TagItem *)_tags);})
#define Helios_CreateSubTask(__p0, __p1, ...) \
    ({ULONG _tags[] = { __VA_ARGS__ }; \
    Helios_CreateSubTaskA(__p0, __p1, (struct TagItem *)_tags);})
#define Helios_GetAttrs(__p0, __p1, ...) \
    ({ULONG _tags[] = { __VA_ARGS__ }; \
    Helios_GetAttrsA(__p0, __p1, (struct TagItem *)_tags);})
#endif

extern void _Helios_FreeDevice(HeliosDevice *dev);
extern void _Helios_FreeUnit(HeliosUnit *unit);

#endif /* HELIOS_PRIVATE_H */
