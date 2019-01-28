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
** Header file for devices API
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
**
*/

#ifndef OHCI1394_DEV_H
#define OHCI1394_DEV_H

#include "ohci1394core.h"

extern void dev_OnNewNode(OHCI1394Unit *unit, HeliosNode *node);
extern void dev_OnUpdatedNode(OHCI1394Unit *unit, HeliosNode *node);
extern void dev_OnRemovedNode(OHCI1394Unit *unit, HeliosNode *node);

#endif /* OHCI1394_DEV_H */
