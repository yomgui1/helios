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
** SBP2 class functions table definition file.
**
*/

#include "sbp2.class.h"
#include "clib/heliosclsbase_protos.h"

extern void LIB_Open(void);
extern void LIB_Close(void);
extern void LIB_Expunge(void);
extern void LIB_Reserved(void);

ULONG LibFuncTable[]=
{
    FUNCARRAY_BEGIN,

    FUNCARRAY_32BIT_NATIVE,
    (ULONG) &LIB_Open,
    (ULONG) &LIB_Close,
    (ULONG) &LIB_Expunge,
    (ULONG) &LIB_Reserved,
    -1,

    FUNCARRAY_32BIT_SYSTEMV,
    (ULONG) &HeliosClass_DoMethodA,
    -1,

    FUNCARRAY_END
};
