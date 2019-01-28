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
** Header file for topology API
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
**
*/

#ifndef OHCI1394_TOPO_H
#define OHCI1394_TOPO_H

#include "ohci1394core.h"

#include <exec/nodes.h>

extern void ohci_FreeTopology(OHCI1394Unit *unit);
extern void ohci_ResetTopology(OHCI1394Unit *unit);
extern void ohci_InvalidTopology(OHCI1394Unit *unit);
extern BOOL ohci_UpdateTopologyMapping(OHCI1394Unit *unit, UBYTE gen, UBYTE local_phyid);

#endif /* OHCI1394_TOPO_H */
