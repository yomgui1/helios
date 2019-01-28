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
** Helios AV/C 1394 Library functions table definition file.
**
*/

#include "avc1394_libdata.h"
#include "clib/avc1394_protos.h"

extern void LIB_Open(void);
extern void LIB_Close(void);
extern void LIB_Expunge(void);
extern void LIB_Reserved(void);


ULONG LibFuncTable[] = {
    FUNCARRAY_BEGIN,
        FUNCARRAY_32BIT_NATIVE,
        (ULONG) &LIB_Open,
        (ULONG) &LIB_Close,
        (ULONG) &LIB_Expunge,
        (ULONG) &LIB_Reserved,
        -1,

    FUNCARRAY_32BIT_SYSTEMV,
        (ULONG) &AVC1394_FreeServerMsg,
        (ULONG) &AVC1394_GetUnitInfo,
        (ULONG) &AVC1394_GetSubUnitInfo,
        (ULONG) &AVC1394_CheckSubUnitType,
        (ULONG) &AVC1394_VCR_Eject,
        (ULONG) &AVC1394_VCR_Stop,
        (ULONG) &AVC1394_VCR_IsPlaying,
        (ULONG) &AVC1394_VCR_Play,
        (ULONG) &AVC1394_VCR_IsRecording,
        (ULONG) &AVC1394_VCR_Reverse,
        (ULONG) &AVC1394_VCR_Pause,
        -1,
    FUNCARRAY_END
};
