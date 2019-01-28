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
** Low-level API to handle HeliosDevice's.
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
**
*/

//#define NDEBUG

#include "ohci1394dev.h"
#include "ohci1394trans.h"

#include "proto/helios.h"

#include <clib/macros.h>
#include <proto/utility.h>

/*----------------------------------------------------------------------------*/
/*--- PUBLIC CODE SECTION ----------------------------------------------------*/

//+ dev_OnNewNode
void dev_OnNewNode(OHCI1394Unit *unit, HeliosNode *node)
{
    HeliosDevice *dev;
    
    dev = Helios_AddDevice((HeliosHardware *)unit, node, unit->hu_Topology->ht_Generation);
    node->n_Device = dev;

    _INFO_UNIT(unit, "node #%u created: device %p\n", node->n_PhyID, node->n_Device);

    if (NULL == dev)
        return;

    Helios_ScanDevice(dev);
}
//-
//+ dev_OnRemovedNode
void dev_OnRemovedNode(OHCI1394Unit *unit, HeliosNode *node)
{
    HeliosDevice *dev;

    _INFO_UNIT(unit, "node #%u removed: device %p\n", node->n_PhyID, node->n_Device);

    dev = node->n_Device;
    if (NULL == dev)
    {
        _ERR_UNIT(unit, "NULL device\n");
        return;
    }

    Helios_RemoveDevice(dev);
    node->n_Device = NULL;
}
//-
//+ dev_OnUpdatedNode
void dev_OnUpdatedNode(OHCI1394Unit *unit, HeliosNode *node)
{
    HeliosDevice *dev;

    _INFO_UNIT(unit, "node #%u updated: device %p\n", node->n_PhyID, node->n_Device);

    dev = node->n_Device;
    if (NULL == dev)
    {
        _ERR_UNIT(unit, "*WARNING* node %u has a NULL device\n", node->n_PhyID);
        return;
    }

    Helios_UpdateDevice(dev, node, unit->hu_Topology->ht_Generation);

    if (0 == dev->hd_GUID.q)
        Helios_ScanDevice(dev);
}
//-
