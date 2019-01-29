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
** Some macros to easy setup library/device code
*/

#pragma once

#include <exec/resident.h>
#include <exec/nodes.h>

#define DECLARE_LIBINIT_STRUCT(basetype, functable, initfunc)   \
    static const struct                                         \
    {                                                           \
        ULONG   LibSize;                                        \
        APTR    FuncTable;                                      \
        APTR    DataTable;                                      \
        void    (*InitFunc)(void);                              \
    } myLibInit =                                               \
    {                                                           \
        sizeof(basetype),                                       \
        (APTR) functable,                                       \
        NULL,                                                   \
        (void (*)(void)) initfunc,                              \
    }

#define DECLARE_RESIDENT_STRUCT(type, libname, nversion, nrevision, sversion) \
    struct Resident libResident =                                       \
    {                                                                   \
        RTC_MATCHWORD,                                                  \
        &libResident,                                                   \
        &libResident + 1,                                               \
        RTF_PPC | RTF_EXTENDED | RTF_AUTOINIT,                          \
        nversion,                                                       \
        type,                                                           \
        0,                                                              \
        libname,                                                        \
        sversion,                                                       \
        (APTR) &myLibInit,                                              \
        nrevision,                                                      \
        NULL                                                            \
    }

#define DECLARE_LIBRARY(libname, basetype, functable, libinitfunc, nversion, nrevision, sversion, vtag) \
    DECLARE_LIBINIT_STRUCT(basetype, functable, libinitfunc);           \
    DECLARE_RESIDENT_STRUCT(NT_LIBRARY, libname, nversion, nrevision, sversion); \
    const STRPTR __abox__ __attribute__((section (".rodata"))) = vtag;  \
    LONG NoExecute(void) { return -1; }                                 \
    const struct NoExecute { LONG (*NoExecuteFunc)(void); } NoExecuteRef \
    __attribute__((section (".rodata"))) = { &NoExecute }

#define DECLARE_DEVICE(devname, basetype, functable, devinitfunc, nversion, nrevision, sversion, vtag) \
    DECLARE_LIBINIT_STRUCT(basetype, functable, devinitfunc);           \
    DECLARE_RESIDENT_STRUCT(NT_DEVICE, devname, nversion, nrevision, sversion); \
    const STRPTR __abox__ __attribute__((section (".rodata"))) = vtag;  \
    LONG NoExecute(void) { return -1; }                                 \
    const struct NoExecute { LONG (*NoExecuteFunc)(void); } NoExecuteRef \
    __attribute__((section (".rodata"))) = { &NoExecute }

/* END OF FILE */
