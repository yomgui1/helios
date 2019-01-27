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
*/

#ifndef OHCI1394_TRANS_H
#define OHCI1394_TRANS_H

#include "ohci1394core.h"

/*============================================================================*/
/*--- PROTOTYPES -------------------------------------------------------------*/
/*============================================================================*/

extern void ohci_TL_Init(OHCI1394Unit *unit, OHCI1394TLayer *tl);
extern OHCI1394Transaction * ohci_TL_Register(
	OHCI1394TLayer *				tl,
	HeliosPacket *					packet,
	OHCI1394TransactionCompleteFunc	func,
	APTR							udata);
extern void ohci_TL_FlushAll(OHCI1394TLayer *tl);
extern void ohci_TL_Finish(OHCI1394TLayer *tl, OHCI1394Transaction *t, BYTE rcode);
extern LONG ohci_TL_SendRequest(OHCI1394Unit *unit, OHCI1394Transaction *t);
extern LONG ohci_TL_DoPHYPacket(OHCI1394Unit *unit, UBYTE sigbit, QUADLET value);
extern void ohci_TL_CancelPacket(OHCI1394TLayer *tl, HeliosPacket *packet);
extern void ohci_TL_HandleResponse(OHCI1394TLayer *tl, HeliosPacket *resp);
extern void ohci_TL_HandleRequest(OHCI1394TLayer *tl, HeliosPacket *req);
extern LONG ohci_TL_AddReqHandler(OHCI1394TLayer *tl, HeliosRequestHandler *reqh);
extern LONG ohci_TL_RemReqHandler(OHCI1394TLayer *tl, HeliosRequestHandler *reqh);

#endif /* OHCI1394_TRANS_H */
