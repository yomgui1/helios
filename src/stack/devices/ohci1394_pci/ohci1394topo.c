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
** Low-level API to generate topology mapping.
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
**
*/


//#define NDEBUG

#include "ohci1394topo.h"
#include "ohci1394dev.h"
#include "proto/helios.h"

#include <clib/macros.h>
#include <proto/exec.h>

#include <string.h>

#define SELFID_PORT_CHILD	0x3
#define SELFID_PORT_PARENT	0x2
#define SELFID_PORT_NCONN	0x1
#define SELFID_PORT_NOPORT	0x0

//+ SelfIDPkt
/* Bits definition of Self-ID packets */
typedef union SelfIDPkt
{
    QUADLET Value;

    struct {
        QUADLET PckID:2;        // should be 2
        QUADLET PhyID:6;        // Physical ID
        QUADLET Type:1;         // Packet type: 0 for packets #0, 1 for extended packets (#2, #3, #4)
                                // As it's a packet type #0 structure, should always be 0
        QUADLET ActiveLink:1;   // Active Link and Transaction Layer flag
        QUADLET GapCount:6;     // Gap count
        QUADLET PhySpeed:2;     // Speed capabilities
        QUADLET PhyDelay:2;     // Worst-case repeater data delay (only 0 is defined)
        QUADLET Contender:1;    // Is a contender (or IRM)
        QUADLET PowerClass:3;   // Power consumption and source characteristics
        QUADLET P0:2;           // Port Status, port  0
        QUADLET P1:2;           //     ''     , port  1
        QUADLET P2:2;           //     ''     , port  2
        QUADLET InitReset:1;    // This port has initiated the bus reset
        QUADLET More:1;         // Nex packet should be a extended packet for this node
    } Packet0;

    struct {
        QUADLET PckID:2;        // should be 2
        QUADLET PhyID:6;        // Physical ID
        QUADLET Type:1;         // Packet type: 0 for packets #0, 1 for extended packets (#2, #3, #4)
                                // As it's a packet type #0 structure, should always be 0
        QUADLET N:3;            // Extended Self-ID packet sequence number (0..2 defined, others are reserved)
        QUADLET Pa:2;           // Port Status, port  3+N*8
        QUADLET Pb:2;           //     ''     , port  4+N*8
        QUADLET Pc:2;           //     ''     , port  5+N*8
        QUADLET Pd:2;           //     ''     , port  6+N*8
        QUADLET Pe:2;           //     ''     , port  7+N*8
        QUADLET Pf:2;           //     ''     , port  8+N*8
        QUADLET Pg:2;           //     ''     , port  9+N*8
        QUADLET Ph:2;           //     ''     , port 10+N*8
        QUADLET InitReset:1;    // This port has initiated the bus reset
        QUADLET Reserved:1;     // A reserved bit (should be 0)
        QUADLET More:1;         // Next packet should be a extended packet for this node
    } PacketN;
} SelfIDPkt;
//-

/*----------------------------------------------------------------------------*/
/*--- LOCAL CODE SECTION -----------------------------------------------------*/

//+ topo_count_ports
static SelfIDPkt * topo_count_ports(SelfIDPkt *sid, ULONG *total_port_count, ULONG *child_port_count)
{
	ULONG shift, seq;

	*total_port_count = 0;
	*child_port_count = 0;

    /* We should start with a Packet #0 */
    if (0 != sid->Packet0.Type)
    {
        _ERR("First SelfID packet of wrong type!\n");
        return NULL;
    }

	shift = 6; /* p0 */
	seq = 0;

    for (;;)
    {
		ULONG status = (sid->Value >> shift) & 3;

		switch (status)
        {
            case SELFID_PORT_CHILD:
                (*child_port_count)++;
            case SELFID_PORT_PARENT:
            case SELFID_PORT_NCONN:
                (*total_port_count)++;
            case SELFID_PORT_NOPORT:
                break;
		}

		shift -= 2;
		if (0 == shift)
        {
            /* No more than 3 ports (i.e. no more packets for one node) ? */
			if (!sid->Packet0.More)
				return sid + 1;

			shift = 16; /* p3 and p11 */
			sid++;

            /* Check for consistency over packets */
			if ((seq > 2) || (1 != sid->PacketN.Type) || (seq != sid->PacketN.N))
            {
                _ERR("SelfID packet inconsistency\n");
				return NULL;
            }

			seq++;
		}
	}
}
//-
//+ topo_fill_node
static HeliosNode *topo_fill_node(HeliosTopology *topo, SelfIDPkt *sid, UBYTE phy_id, ULONG port_count)
{
    HeliosNode *node;

    node = &topo->ht_Nodes[phy_id];

    node->n_PhyID = phy_id;
    node->n_PortCount = port_count;
    node->n_PhySpeed = sid->Packet0.PhySpeed;
    node->n_Flags.ResetInitiator = sid->Packet0.InitReset;
    node->n_Flags.LinkOn = sid->Packet0.ActiveLink;
    node->n_ParentPort = -1;
    node->n_MaxHops = 0;
    node->n_MaxDepth = 0;

    return node;
}
//-
//+ topo_get_port_type
static ULONG topo_get_port_type(QUADLET *sid, ULONG port_index)
{
    ULONG index, shift;

    index = (port_index + 4) / 8;
    shift = 16 - ((port_index + 4) & 7) * 2;
    return (sid[index] >> shift) & 3;
}
//-
//+ topo_compute_hop_count
static void topo_compute_hop_count(HeliosTopology *topo, HeliosNode *node)
{
    LONG max_depths[2] = {-1, -1};
    HeliosNode *child;
    ULONG i, max_hops = 0;

    /* Here we compute the maximum number of connections between any two nodes
     * in the subtree of the given node. This number is called the maximum hop count.
     * This number is used to compute the gap count later.
     */

    for (i=0; i < node->n_PortCount; i++)
    {
        if (node->n_Ports[i] < 0)
            continue;

        child = &topo->ht_Nodes[node->n_Ports[i]];

        if (child->n_MaxHops > max_hops)
            max_hops = child->n_MaxHops;

        /* get the two maximal depths */
        if (child->n_MaxDepth > max_depths[0]) {
            max_depths[1] = max_depths[0];
            max_depths[0] = child->n_MaxDepth;
        }
        else if (child->n_MaxDepth > max_depths[1])
            max_depths[1] = child->n_MaxDepth;
    }

    node->n_MaxDepth = max_depths[0] + 1;
    node->n_MaxHops = MAX(max_hops, max_depths[0] + max_depths[1] + 2);
}
//-
//+ topo_set_max_speed
/* Recursive function, max depth = 64 */
static void topo_set_max_speed(HeliosTopology *topo,
                               HeliosNode *current,
                               HeliosNode *parent)
{
    ULONG i;

    if (NULL != parent)
    {
        current->n_MaxSpeed = MIN(parent->n_MaxSpeed, current->n_PhySpeed);
    }
    else
    { /* root node (no parent) */
        current->n_MaxSpeed = current->n_PhySpeed;
    }

    if (current->n_MaxSpeed > S400)
    {
        /* TODO: need to take care of beta speeds */
        current->n_MaxSpeed = S400;
    }

    _INFO("Node %u: max speed=%u\n", current->n_PhyID, current->n_MaxSpeed);

    /* Recurse on children */
    for (i=0; i < current->n_PortCount; i++)
    {
        HeliosNode *child;

        if (current->n_Ports[i] < 0)
            continue;

        child = &topo->ht_Nodes[current->n_Ports[i]];
        if (child == parent)
            continue;

        topo_set_max_speed(topo, child, current);
    }
}
//-
//+ topo_for_each_node
static void topo_for_each_node(OHCI1394Unit *unit,
                               HeliosNode *node,
                               HeliosNode *parent,
                               void (*func)(OHCI1394Unit *unit, HeliosNode *node))
{
    ULONG i;

    func(unit, node);

    /* Recurse on children */
    for (i=0; i < node->n_PortCount; i++)
    {
        HeliosNode *child;

        if (node->n_Ports[i] < 0)
            continue;

        child = &unit->hu_Topology->ht_Nodes[node->n_Ports[i]];
        if (child == parent)
            continue;

        topo_for_each_node(unit, child, node, func);
    }
}
//-
//+ topo_compare_trees
static void topo_compare_trees(OHCI1394Unit *unit,
                               HeliosNode *current,
                               HeliosNode *previous,
                               HeliosNode *parent)
{
    LONG i;
    HeliosNode *node_array = unit->hu_Topology->ht_Nodes;

    /* Inform the devices layer */
    if (!previous->n_Flags.LinkOn && current->n_Flags.LinkOn)
        dev_OnNewNode(unit, current);
    else
    {
        if (previous->n_Flags.LinkOn && !current->n_Flags.LinkOn)
            dev_OnRemovedNode(unit, previous);
        else
        {
            current->n_Device = previous->n_Device;
            previous->n_Device = NULL;
            dev_OnUpdatedNode(unit, current);
        }
    }

    /* loop on all current device's ports */
    for (i=0; i < current->n_PortCount; i++)
    {
        HeliosNode *child, *prev_child;
        BYTE prev_id, curr_id;

        curr_id = current->n_Ports[i];

        /* NOT CONNECTED */
        if (-1 == curr_id)
            continue;

        child = &node_array[curr_id];

        /* PARENT NODE */
        if (child == parent)
            continue;

        /* PORT WASN'T EXIST BEFORE */
        prev_id = previous->n_Ports[i];
        if (-1 == prev_id)
        {
            _INFO_UNIT(unit, "Node $%X: wasn't exist\n", child->n_PhyID);
            topo_for_each_node(unit, child, current, dev_OnNewNode);
            continue;
        }

        /* PORT HAS BEEN EXISTED */
        prev_child = &unit->hu_OldTopology->ht_Nodes[prev_id];

        /* Recurse on child */
        topo_compare_trees(unit, child, prev_child, current);
    }
}
//-

/*----------------------------------------------------------------------------*/
/*--- PUBLIC CODE SECTION ----------------------------------------------------*/

//+ ohci_FreeTopology
void ohci_FreeTopology(OHCI1394Unit *unit)
{
    if (NULL != unit->hu_Topology)
    {
        FreePooled(unit->hu_MemPool, unit->hu_Topology, sizeof(*unit->hu_Topology));
        unit->hu_Topology = NULL;
    }
    if (NULL != unit->hu_OldTopology)
    {
        FreePooled(unit->hu_MemPool, unit->hu_OldTopology, sizeof(*unit->hu_OldTopology));
        unit->hu_OldTopology = NULL;
    }
}
//-
//+ ohci_ResetTopology
/* WARNING: w-lock unit before call */
void ohci_ResetTopology(OHCI1394Unit *unit)
{
    if (NULL != unit->hu_Topology)
    {
        FreePooled(unit->hu_MemPool, unit->hu_Topology, sizeof(*unit->hu_Topology));
        unit->hu_Topology = NULL;
    }
    if (NULL != unit->hu_OldTopology)
        unit->hu_OldTopology->ht_NodeCount = 0;
}
//-
//+ ohci_InvalidTopology
/* WARNING: w-lock unit before call */
void ohci_InvalidTopology(OHCI1394Unit *unit)
{
    if (NULL != unit->hu_Topology)
    {
        if (NULL != unit->hu_OldTopology)
            FreePooled(unit->hu_MemPool, unit->hu_OldTopology, sizeof(HeliosTopology));

        unit->hu_OldTopology = unit->hu_Topology;        
        unit->hu_Topology = NULL; /* will be regenerated later */
    }
}
//-
//+ ohci_UpdateTopologyMapping
/* Must be called in the context of the BusReset task
 * and after a valid SelfID stream.
 */
BOOL ohci_UpdateTopologyMapping(OHCI1394Unit *unit, UBYTE gen, UBYTE local_phyid)
{
    SelfIDPkt *sid, *next_sid, *end;
    HeliosNode *node, *child;
    HeliosTopology *topo;
    HeliosNode *stack[64];
    UBYTE phy_id, stack_idx;
    ULONG gap_count, bus_gapcount;

    sid = (APTR)unit->hu_SelfIdArray;
    end = sid + unit->hu_SelfIdPktCnt;

    phy_id = 0;
    stack_idx = 0;
    bus_gapcount = gap_count = sid->Packet0.GapCount;

    /* keep topo aligned for fast copy */
    topo = AllocPooledAligned(unit->hu_MemPool, sizeof(HeliosTopology), 8, 0);
    if (NULL == topo)
    {
        _ERR_UNIT(unit, "Topology alloc failed, align=%u\n", sizeof(HeliosTopology));
        return FALSE;
    }

    _INFO("New topo: %p\n", topo);

    topo->ht_LocalNodeID = topo->ht_IRMNodeID = topo->ht_RootNodeID = -1;

    while (sid < end)
    {
        ULONG child_port_count, port_count, i, parent_count;

        /* Compute the number of ports of the current packet.
         * This permits to define the size of the node structure.
         * Note: Check that the first SelfID packet refered by sid is always
         * of a type 0 packet. Then return the next type 0 packet (next node).
         */
        next_sid = topo_count_ports(sid, &port_count, &child_port_count);
        if (NULL == next_sid)
            goto failed;

        /* Check if the PhyIDs sequence is logical */
        if (phy_id != sid->Packet0.PhyID) {
            _ERR_UNIT(unit, "Bad phy_id in SelfID packet: get %u, expected %u\n", sid->Packet0.PhyID, phy_id);
            goto failed;
        }

        /* Children nodes stack shall be suffisently filled */
        if (child_port_count > stack_idx) {
            _ERR_UNIT(unit, "Topology stack underflow!\n");
            goto failed;
        }

        /* Find node from phy_id and fill it.
         * Note: given sid shall always refert to the type 0 SelfID packet
         */
        node = topo_fill_node(topo, sid, phy_id, port_count);

        _INFO_UNIT(unit, "Node %u: %u port(s), %u connected children\n",
                   phy_id, port_count, child_port_count);

        /* Identify special nodes */
        if (phy_id == local_phyid)
			topo->ht_LocalNodeID = phy_id;

		if (sid->Packet0.Contender)
			topo->ht_IRMNodeID = phy_id;

        parent_count = FALSE;
        memset(node->n_Ports, -1, sizeof(node->n_Ports));
        for (i=port_count; i; i--)
        {
            ULONG pt = topo_get_port_type(&sid->Value, i);

            switch (pt)
            {
                case SELFID_PORT_PARENT:
                    parent_count++;
                    node->n_ParentPort = i-1; /* be used if it's a child*/
                    break;

                case SELFID_PORT_CHILD:
                    /* Consume the last child node put in stack */
                    child = stack[--stack_idx];

                    /* Associate nodes */
                    node->n_Ports[i-1] = child->n_PhyID;
                    child->n_Ports[child->n_ParentPort] = phy_id;
                    break;                    
            }
        }

        /* Parent consistency check */
        if (((next_sid == end) && (parent_count > 0)) ||
            ((next_sid != end) && (parent_count != 1)))
        {
            _ERR_UNIT(unit, "Parent inconsistency found on node %u\n", phy_id);
            goto failed;
        }

        if (parent_count > 0)
            stack[stack_idx++] = node;
        else /* it's the root node */
            topo->ht_RootNodeID = phy_id;

        /* Use an invalid gap count when PHY layer reports differents gap counts */
        if (sid->Packet0.GapCount != bus_gapcount)
            gap_count = 0;

        /* Computing the maximal hop count for this node */
        topo_compute_hop_count(topo, node);

        sid = next_sid;
        phy_id++;
    }

    if (stack_idx > 0)
    {
        _ERR_UNIT(unit, "%lu nodes remain in stack!\n", stack_idx);
        goto failed;
    }

    topo->ht_NodeCount = phy_id;

    /* Starting from the local node, walk the tree and set maximal node speed */
    topo_set_max_speed(topo, &topo->ht_Nodes[topo->ht_LocalNodeID], NULL);

    Helios_WriteLockBase();
    {
        LOCK_REGION(unit);
        {
            ULONG old_count;

            /* We suppose here that a old topo may or not exists and current topo is NULL */

            unit->hu_Topology = topo;
            topo->ht_GapCount = gap_count;

            /* Any previous topo ? */
            if (NULL != unit->hu_OldTopology)
            {
                topo->ht_Generation = unit->hu_OldTopology->ht_Generation + 1;

                /* No topo gap ? */
                old_count = unit->hu_OldTopology->ht_NodeCount;
            }
            else
            {
                old_count = 0;
                topo->ht_Generation = 1;
            }

            /* Previous nodes? */
            if (old_count > 0)
            {
                ULONG i;
            
                /* Compare previous and new topology trees and set Updated node flags when needed */
                topo_compare_trees(unit,
                                   &topo->ht_Nodes[topo->ht_LocalNodeID],
                                   &unit->hu_OldTopology->ht_Nodes[unit->hu_OldTopology->ht_LocalNodeID],
                                   NULL);

                /* Cleanup non-used nodes */
                for (i=0; i<unit->hu_OldTopology->ht_NodeCount; i++)
                {
                    HeliosNode *node = &unit->hu_OldTopology->ht_Nodes[i];

                    if (NULL != node->n_Device)
                        dev_OnRemovedNode(unit, node);
                }
            }
            else
            {
                HeliosDevice *dev, *next;

                /* Remove everythings (possible if non-consecutives BR happen) */
                ForeachNodeSafe(&unit->hu_Devices, dev, next)
                    Helios_RemoveDevice(dev);

                /* Create devices from the new nodes list */
                topo_for_each_node(unit, &topo->ht_Nodes[topo->ht_LocalNodeID], NULL, dev_OnNewNode);
            }

            _INFO_UNIT(unit, "Topo #%lu OK: NodeCnt=%u, GapCount=%u, local=%u, IRM=%u, root=%u, MaxHops=%u\n",
                       topo->ht_Generation, topo->ht_NodeCount, gap_count, topo->ht_LocalNodeID,
                       topo->ht_IRMNodeID, topo->ht_RootNodeID, (ULONG)topo->ht_Nodes[topo->ht_RootNodeID].n_MaxHops);

            Helios_SendEvent(&unit->hu_Listeners, HEVTF_HARDWARE_TOPOLOGY, topo->ht_Generation);
        }
        UNLOCK_REGION(unit);
    }
    Helios_UnlockBase();

    return TRUE;

failed:
    ohci_ResetTopology(unit);
    return FALSE;
}
//-
