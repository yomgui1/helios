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

#ifndef  CLIB_AVC1394_PROTOS_H
#define  CLIB_AVC1394_PROTOS_H

/*
**
** AVC1394 C prototypes.
**
*/

#include <libraries/avc1394.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern void                 AVC1394_FreeServerMsg( AVC1394_ServerMsg *msg );
extern AVC1394_ServerMsg *  AVC1394_GetUnitInfo( HeliosBus *bus, UWORD nodeid );
extern LONG                 AVC1394_GetSubUnitInfo( HeliosBus *bus, UWORD nodeid, QUADLET *table );
extern LONG                 AVC1394_CheckSubUnitType( HeliosBus *bus, UWORD nodeid, LONG type );

extern LONG AVC1394_VCR_IsPlaying( HeliosBus *bus, UWORD nodeid );
extern LONG AVC1394_VCR_IsRecording( HeliosBus *bus, UWORD nodeid );
extern LONG AVC1394_VCR_Eject( HeliosBus *bus, UWORD nodeid, QUADLET *status );
extern LONG AVC1394_VCR_Stop( HeliosBus *bus, UWORD nodeid, QUADLET *status );
extern LONG AVC1394_VCR_Play( HeliosBus *bus, UWORD nodeid, QUADLET *status );
extern LONG AVC1394_VCR_Reverse( HeliosBus *bus, UWORD nodeid, QUADLET *status );
extern LONG AVC1394_VCR_Pause( HeliosBus *bus, UWORD nodeid, QUADLET *status );

#if 0
/* ##### Ask the device for the status ##### */
QUADLET AVC1394_VCR_Status(HeliosBus bus, ULONG generation, ULONG id);

/* ##### Check to see if the device is recording ##### */
int
avc1394_vcr_is_recording(raw1394handle_t handle, nodeid_t node);

/* ##### Tell the device to PLAY FORWARD ##### */
/* issue twice in a row to play forward at the slowest rate */
void
avc1394_vcr_play(raw1394handle_t handle, nodeid_t node);

/* ##### Tell the device to PLAY in REVERSE ##### */
/* issue twice in a row to play in reverse at the slowest rate */
void
avc1394_vcr_reverse(raw1394handle_t handle, nodeid_t node);

/* ##### exercise the trick play modes! ##### */
/* speed is a signed percentage value from -14 to +14 */
void
avc1394_vcr_trick_play(raw1394handle_t handle, nodeid_t node, int speed);

/* ##### Tell the device to REWIND ##### */
void
avc1394_vcr_rewind(raw1394handle_t handle, nodeid_t node);

/* ##### Tell the device to PAUSE ##### */
void
avc1394_vcr_pause(raw1394handle_t handle, nodeid_t node);

/* ##### Tell the device to FAST FORWARD ##### */
void
avc1394_vcr_forward(raw1394handle_t handle, nodeid_t node);

/* ##### Tell the device goto NEXT frame ##### */
void
avc1394_vcr_next(raw1394handle_t handle, nodeid_t node);

/* ##### Tell the device goto NEXT index point ##### */
void
avc1394_vcr_next_index(raw1394handle_t handle, nodeid_t node);

/* ##### Tell the device goto PREVIOUS frame ##### */
void
avc1394_vcr_previous(raw1394handle_t handle, nodeid_t node);

/* ##### Tell the device goto PREVIOUS index point ##### */
void
avc1394_vcr_previous_index(raw1394handle_t handle, nodeid_t node);

/* ##### Tell the device to RECORD ##### */
void
avc1394_vcr_record(raw1394handle_t handle, nodeid_t node);

/* Get a textual description of the status */
char *
avc1394_vcr_decode_status(quadlet_t response);

/* Get the time code on tape in format HH:MM:SS:FF */
/* This version allocates memory for the string, and
   the caller is required to free it. */
char *
avc1394_vcr_get_timecode(raw1394handle_t handle, nodeid_t node)
__attribute__ ((deprecated));
/* This version requires a pre-allocated output string of at least 12
   characters. */
int
avc1394_vcr_get_timecode2(raw1394handle_t handle, nodeid_t node, char *output);

/* Go to the time code on tape in format HH:MM:SS:FF */
void
avc1394_vcr_seek_timecode(raw1394handle_t handle, nodeid_t node, char *timecode);
#endif

/* VarArgs API */
#if !defined(USE_INLINE_STDARG)
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CLIB_AVC1394_PROTOS_H */
