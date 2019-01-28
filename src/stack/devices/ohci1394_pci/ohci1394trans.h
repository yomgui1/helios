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
** Header file for transaction API
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
**
*/

#ifndef OHCI1394_TRANS_H
#define OHCI1394_TRANS_H

#include "ohci1394core.h"

extern LONG ohci_TL_Register(OHCI1394Unit *unit,
                             HeliosTransaction *t,
                             OHCI1394ATCompleteCallback cb,
                             APTR cb_udata);
extern void ohci_TL_FlushAll(OHCI1394Unit *unit);
extern void ohci_TL_Finish(OHCI1394Unit *unit, HeliosTransaction *t, BYTE rcode);
extern void ohci_TL_Cancel(OHCI1394Unit *unit, HeliosTransaction *t);

extern void ohci_TL_HandleResponse(OHCI1394Unit *unit, HeliosAPacket *resp);
extern void ohci_TL_HandleRequest(OHCI1394Unit *unit,
                                  HeliosAPacket *req,
                                  UBYTE generation);

extern LONG ohci_TL_SendRequest(OHCI1394Unit *unit,
                                HeliosTransaction *t,
                                UWORD destid,
                                HeliosSpeed speed,
                                UBYTE generation,
                                UBYTE tcode,
                                UWORD extcode,
                                HeliosOffset offset,
                                QUADLET *payload,
                                ULONG length);
extern LONG ohci_TL_DoRequest(OHCI1394Unit *unit,
                              UBYTE sigbit,
                              UWORD destid,
                              HeliosSpeed speed,
                              UBYTE generation,
                              UBYTE tcode,
                              UWORD extcode,
                              HeliosOffset offset,
                              QUADLET *payload,
                              ULONG length);
extern LONG ohci_TL_DoPHYPacket(OHCI1394Unit *unit,
                                UBYTE sigbit,
                                QUADLET value);
extern LONG ohci_TL_AddReqHandler(OHCI1394Unit *unit, HeliosHWReqHandler *reqh);
extern LONG ohci_TL_RemReqHandler(OHCI1394Unit *unit, HeliosHWReqHandler *reqh);

#endif /* OHCI1394_TRANS_H */
