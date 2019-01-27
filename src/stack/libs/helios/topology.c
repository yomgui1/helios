/* Copyright 2008-2013, 2018 Guillaume Roguez

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

/* $Id$
** This file is copyrights 2008-2012 by Guillaume ROGUEZ.
**
** Low-level API to generate topology mapping.
*/

#include "private.h"
#include "topology.h"

#include <clib/macros.h>
#include <string.h>

#define SELFID_PORT_CHILD	0x3
#define SELFID_PORT_PARENT	0x2
#define SELFID_PORT_NCONN	0x1
#define SELFID_PORT_NOPORT	0x0

/* Bits definition of Self-ID packets
 * Note: BIGENDIAN ordered!
 */
typedef union SelfIDPkt
{
	QUADLET quadlet;

	struct {
		QUADLET PckID:2;		// should be 2
		QUADLET PhyID:6;		// Physical ID
		QUADLET Type:1;			// Packet type: 0 for packets #0, 1 for extended packets (#2, #3, #4)
								// As it's a packet type #0 structure, should always be 0
		QUADLET ActiveLink:1;	// Active Link and Transaction Layer flag
		QUADLET GapCount:6;		// Gap count
		QUADLET PhySpeed:2;		// Speed capabilities
		QUADLET PhyDelay:2;		// Worst-case repeater data delay (only 0 is defined)
		QUADLET Contender:1;	// Is a contender (or IRM)
		QUADLET PowerClass:3;	// Power consumption and source characteristics
		QUADLET P0:2;			// Port Status, port  0
		QUADLET P1:2;			//     ''     , port  1
		QUADLET P2:2;			//     ''     , port  2
		QUADLET InitReset:1;	// This port has initiated the bus reset
		QUADLET More:1;			// Nex packet should be a extended packet for this node
	} Packet0;

	struct {
		QUADLET PckID:2;		// should be 2
		QUADLET PhyID:6;		// Physical ID
		QUADLET Type:1;			// Packet type: 0 for packets #0, 1 for extended packets (#2, #3, #4)
								// As it's a packet type #0 structure, should always be 0
		QUADLET N:3;			// Extended Self-ID packet sequence number (0..2 defined, others are reserved)
		QUADLET Pa:2;			// Port Status, port  3+N*8
		QUADLET Pb:2;			//     ''     , port  4+N*8
		QUADLET Pc:2;			//     ''     , port  5+N*8
		QUADLET Pd:2;			//     ''     , port  6+N*8
		QUADLET Pe:2;			//     ''     , port  7+N*8
		QUADLET Pf:2;			//     ''     , port  8+N*8
		QUADLET Pg:2;			//     ''     , port  9+N*8
		QUADLET Ph:2;			//     ''     , port 10+N*8
		QUADLET InitReset:1;	// This port has initiated the bus reset
		QUADLET Reserved:1;		// A reserved bit (should be 0)
		QUADLET More:1;			// Next packet should be a extended packet for this node
	} PacketN;
} SelfIDPkt;

/*============================================================================*/
/*--- LOCAL CODE -------------------------------------------------------------*/
/*============================================================================*/
static SelfIDPkt *
topo_count_ports(SelfIDPkt *sidPkt, ULONG *portCount, ULONG *childPortCount)
{
	ULONG pc, cpc, shift, seq;

	/* We should start with a 0 packet type */
	if (sidPkt->Packet0.Type != 0)
	{
		_ERR("First SelfID packet of wrong type!\n");
		return NULL;
	}

	pc = 0;
	cpc = 0;
	seq = 0;
	shift = 6; /* P0 first bit */

	/* Loop on Pn */
	for (;;)
	{
		int status = (sidPkt->quadlet >> shift) & 3;

		switch (status)
		{
			case SELFID_PORT_CHILD: cpc++;
			case SELFID_PORT_PARENT:
			case SELFID_PORT_NCONN: pc++;
			case SELFID_PORT_NOPORT:
				break;
		}

		/* Next port */
		shift -= 2;
		if (0 == shift)
		{
			/* No more packets? */
			if (!sidPkt->Packet0.More)
			{
				*portCount = pc;
				*childPortCount = cpc;
				return sidPkt+1;
			}
			
			/* Parse PacketN type */
			sidPkt++;
			shift = 16; /* Pa first bit */

			/* Check for consistency over packets:
			 *  - 3 packets max per node
			 *  - PacketN follows Packet0
			 *  - Packet numbering consistent
			 */
			if ((seq > 2) ||
				(sidPkt->PacketN.Type != 1) ||
				(seq != sidPkt->PacketN.N))
			{
				_ERR("SelfID packet inconsistency\n");
				return NULL;
			}

			seq++;
		}
	}
}
static HeliosNode *
topo_fill_node(HeliosTopology *topo, SelfIDPkt *sidPkt, UBYTE phyID, ULONG portCount)
{
	HeliosNode *node;

	node = &topo->ht_Nodes[phyID];
	
	node->hn_Device					= NULL;
	node->hn_PhyID					= phyID;
	node->hn_PortCount				= portCount;
	node->hn_PhySpeed				= sidPkt->Packet0.PhySpeed;
	node->hn_Flags.IsResetInitiator	= sidPkt->Packet0.InitReset;
	node->hn_Flags.IsLinkOn			= sidPkt->Packet0.ActiveLink;
	
	/* Computed later */
	node->hn_ParentPort				= -1;
	node->hn_MaxHops				= 0;
	node->hn_MaxDepth				= 0;
	
	/* No ports by default */
	memset(node->hn_Ports, -1, sizeof(node->hn_Ports));
	
	return node;
}
static int
topo_get_port_type(QUADLET *sidPkt, ULONG portIdx)
{
	int idx, shift;
	
	idx = (portIdx + 4) / 8;				/* packet index: 0..2 */
	shift = 16 - ((portIdx + 4) & 7) * 2;	/* Px first bit */
	
	return (sidPkt[idx] >> shift) & 3;
}
static void
topo_compute_hop_count(HeliosTopology *topo, HeliosNode *node)
{
	int maxDepths[2] = {-1, -1};
	int i, maxHops = 0;
	HeliosNode *child;

	/* Here we compute the maximum number of connections between any two nodes
	 * in the subtree of the given node. This number is called the maximum hop count.
	 * This number is used to compute the gap count later.
	 */

	for (i=0; i < HELIOS_PORTS_PER_NODE; i++)
	{
		if (node->hn_Ports[i] < 0)
			continue;

		child = &topo->ht_Nodes[node->hn_Ports[i]];

		if (child->hn_MaxHops > maxHops)
			maxHops = child->hn_MaxHops;

		/* get the two maximal depths */
		if (child->hn_MaxDepth > maxDepths[0])
		{
			maxDepths[1] = maxDepths[0];
			maxDepths[0] = child->hn_MaxDepth;
		}
		else if (child->hn_MaxDepth > maxDepths[1])
			maxDepths[1] = child->hn_MaxDepth;
	}

	node->hn_MaxDepth = maxDepths[0] + 1;
	node->hn_MaxHops = MAX(maxHops, maxDepths[0] + maxDepths[1] + 2);
}
/* WARNING: Recursive function, max call depth = 64 */
static void
topo_set_max_speed(HeliosTopology *topo, HeliosNode *current, HeliosNode *parent)
{
	int i;

	if (parent)
		current->hn_MaxSpeed = MIN(parent->hn_MaxSpeed, current->hn_PhySpeed);
	else /* root node (no parent) */
		current->hn_MaxSpeed = current->hn_PhySpeed;
	
	/* TODO: need to take care of beta speeds */
	if (current->hn_MaxSpeed > S400)
		current->hn_MaxSpeed = S400;
	
	_DBG("Node %u: max speed=%u\n", current->hn_PhyID, current->hn_MaxSpeed);

	/* Recurse on children */
	for (i=0; i < HELIOS_PORTS_PER_NODE; i++)
	{
		HeliosNode *child;

		if (current->hn_Ports[i] < 0)
			continue;

		child = &topo->ht_Nodes[current->hn_Ports[i]];
		if (child == parent)
			continue;

		topo_set_max_speed(topo, child, current);
	}
}
static void
topo_for_each_node(
	HeliosHardware *hh,
	HeliosTopology *topo,
	HeliosNode *node,
	HeliosNode *parent,
	void (*func)(HeliosHardware *hh, HeliosNode *node))
{
	int i;

	func(hh, node);

	/* Recurse on children */
	for (i=0; i < HELIOS_PORTS_PER_NODE; i++)
	{
		HeliosNode *child;

		if (node->hn_Ports[i] < 0)
			continue;

		child = &topo->ht_Nodes[node->hn_Ports[i]];
		if (child == parent)
			continue;

		topo_for_each_node(hh, topo, child, node, func);
	}
}
/* WARNING: Recursive function, max call depth = 64 */
static void
topo_compare_trees(
	HeliosHardware *hh,
	HeliosTopology *prevTopo,
	HeliosNode *current,
	HeliosNode *previous,
	HeliosNode *parent)
{
	HeliosNode *nodeArray = hh->hh_Topology->ht_Nodes;
	int i;

	_DBG("CurNode=%u (Link=%u), PrvNode=%u (Link=%u)\n",
		current->hn_PhyID, current->hn_Flags.IsLinkOn,
		previous->hn_PhyID, previous->hn_Flags.IsLinkOn);

	/* Inform the device layer on node Link state change */
	if (current->hn_Flags.IsLinkOn)
	{
		if (!previous->hn_Flags.IsLinkOn)
			_Helios_OnCreatedNode(hh, current);
		else
			_Helios_OnUpdatedNode(hh, previous, current);
	}
	else if (previous->hn_Flags.IsLinkOn)
		_Helios_OnRemovedNode(hh, previous);
	
	/* Loop on all ports of current node */
	for (i=0; i < HELIOS_PORTS_PER_NODE; i++)
	{
		HeliosNode *child;
		BYTE prevID, currID;

		currID = current->hn_Ports[i];

		/* Not connected? */
		if (-1 == currID)
		{
			/* But was connected? */
			prevID = previous->hn_Ports[i];
			if (prevID >= 0)
			{
				HeliosNode *prevChild;
			
				/* Recurse on children */
				prevChild = &prevTopo->ht_Nodes[prevID];
				topo_for_each_node(hh, prevTopo, prevChild, previous, _Helios_OnRemovedNode);
			}
			
			continue;
		}
		
		child = &nodeArray[currID];

		/* Process only real child port, not parent one */
		if (child == parent)
			continue;

		/* Port existed? */
		prevID = previous->hn_Ports[i];
		if (prevID >= 0)
		{
			HeliosNode *prevChild;
			
			/* Recurse on children */
			prevChild = &prevTopo->ht_Nodes[prevID];
			topo_compare_trees(hh, prevTopo, child, prevChild, current);
		}
		else
			topo_for_each_node(hh, hh->hh_Topology, child, current, _Helios_OnCreatedNode);
	}
}
/*============================================================================*/
/*--- EXPORTED API -----------------------------------------------------------*/
/*============================================================================*/
BOOL
_Helios_UpdateTopologyMapping(HeliosHardware *hh)
{
	HeliosTopology *topo;
	SelfIDPkt *sidPkt, *endSidPck;
	HeliosNode *node, *child, *stack[64];
	UBYTE phyID, stackIdx;
	ULONG gapCount, busGapCount;

	topo = AllocPooledAligned(HeliosBase->hb_MemPool, sizeof(HeliosTopology), 16, 0);
	if (NULL == topo)
	{
		_ERR("Topology alloc failed\n");
		return FALSE;
	}

	sidPkt = (SelfIDPkt *)hh->hh_SelfIDStream.hss_Data;
	endSidPck = sidPkt + hh->hh_SelfIDStream.hss_PacketCount;
	
	phyID = 0;
	stackIdx = 0;
	busGapCount = gapCount = sidPkt->Packet0.GapCount;
	
	topo->ht_Generation = hh->hh_SelfIDStream.hss_Generation;
	topo->ht_LocalNodeID = hh->hh_SelfIDStream.hss_LocalNodeID;
	
	_DBG("Starting new topo: gen=%u, LocalNodeID=%u\n",
		topo->ht_Generation, topo->ht_LocalNodeID);
	
	/* -1 as defaults ID = "not set" */
	topo->ht_IRMNodeID = topo->ht_RootNodeID = -1;

	/* Loop on SelfID packets */
	while (sidPkt < endSidPck)
	{
		SelfIDPkt *nextSidPkt;
		ULONG childPortCount=0, portCount=0, parentCount;
		int i;

		/* Compute the number of ports in the current packet.
		 * Note: following function ensures that nextSidPkt
		 * and sidPkt are always 0 type packets.
		 */
		nextSidPkt = topo_count_ports(sidPkt, &portCount, &childPortCount);
		if (NULL == nextSidPkt)
			return FALSE;
			
		_DBG("Node %u: %u port(s), %u connected children\n",
			phyID, portCount, childPortCount);

		/* Keep PhyIDs sequence arithmetic */
		if (phyID != sidPkt->Packet0.PhyID) {
			_ERR("Bad phyID in SelfID packet: get %u, expected %u\n", sidPkt->Packet0.PhyID, phyID);
			return FALSE;
		}

		/* Children nodes stack shall be suffisently filled */
		if (childPortCount > stackIdx) {
			_ERR("Topology stack underflow!\n");
			return FALSE;
		}

		/* Find node structure from phyID and fill it from packet data */
		node = topo_fill_node(topo, sidPkt, phyID, portCount);
		
		/* Loop on ports */
		for (i=0, parentCount=0; i < portCount; i++)
		{
			int pt = topo_get_port_type(&sidPkt->quadlet, i);

			switch (pt)
			{
				case SELFID_PORT_PARENT:
					parentCount++;
					node->hn_ParentPort = i; /* be used if it's a child*/
					break;

				case SELFID_PORT_CHILD:
					/* Consume the last child node put in stack */
					child = stack[--stackIdx];

					/* Associate nodes */
					node->hn_Ports[i] = child->hn_PhyID;
					child->hn_Ports[child->hn_ParentPort] = phyID;
					break;
			}
		}

		/* Parent consistency check */
		if (((nextSidPkt == endSidPck) && (parentCount > 0)) ||
			((nextSidPkt != endSidPck) && (parentCount != 1)))
		{
			_ERR("Parent inconsistency found on node %u (PCnt=%u, Pkt@%p, End@%p)\n",
				phyID, parentCount, nextSidPkt, endSidPck);
			return FALSE;
		}

		/* Stack child node and identify special nodes */
		if (parentCount > 0)
			stack[stackIdx++] = node;
		else /* it's the root node */
			topo->ht_RootNodeID = phyID;

		if (sidPkt->Packet0.Contender)
			topo->ht_IRMNodeID = phyID;

		/* Use an invalid gap count when PHY layer reports differents gap counts */
		if (sidPkt->Packet0.GapCount != busGapCount)
			gapCount = 0;

		/* Computing the maximal hop count for this node */
		topo_compute_hop_count(topo, node);

		sidPkt = nextSidPkt;
		phyID++;
	}

	if (stackIdx > 0)
	{
		_ERR("%lu nodes remain in stack!\n", stackIdx);
		return FALSE;
	}

	topo->ht_NodeCount = phyID;
	topo->ht_GapCount = gapCount;

	/* Starting from the local node, walk the tree and set maximal node speed */
	topo_set_max_speed(topo, &topo->ht_Nodes[topo->ht_LocalNodeID], NULL);

	/* Update current recorded topology */
	EXCLUSIVE_PROTECT_BEGIN(&hh->hh_Base);
	{
		HeliosTopology *prevTopo;

		prevTopo = hh->hh_Topology;
		hh->hh_Topology = topo;

		/* Previous nodes? */
		if (prevTopo && (prevTopo->ht_NodeCount > 0))
		{
			_DBG("Compare with previous topo...\n");
			
			/* Compare old and new topology trees and set the Updated node flag when needed */
			topo_compare_trees(hh, prevTopo,
				&topo->ht_Nodes[topo->ht_LocalNodeID],
				&prevTopo->ht_Nodes[prevTopo->ht_LocalNodeID],
				NULL);
		}
		else
		{
			/* Create devices from the new nodes list */
			topo_for_each_node(hh, topo,
				&topo->ht_Nodes[topo->ht_LocalNodeID],
				NULL, _Helios_OnCreatedNode);
		}
		
		FreePooled(HeliosBase->hb_MemPool, prevTopo, sizeof(HeliosTopology));
	}
	EXCLUSIVE_PROTECT_END(&hh->hh_Base);

	_INF("Topo gen#%u OK: NodeCnt=%u, GapCount=%u, local=%u, IRM=%u, root=%u, MaxHops=%u\n",
		topo->ht_Generation, topo->ht_NodeCount, topo->ht_GapCount, topo->ht_LocalNodeID,
		topo->ht_IRMNodeID, topo->ht_RootNodeID, (ULONG)topo->ht_Nodes[topo->ht_RootNodeID].hn_MaxHops);

	Helios_SendEvent(&hh->hh_Listeners, HEVTF_HARDWARE_TOPOLOGY, topo->ht_Generation);
	
	/* Topo's nodes are now all defined but without any attached devices
	 * Read nodes ROMs and create/re-attach devices
	 */
	
	/* IEEE1394 norm indicates a delay of 1s after busreset before scanning device's ROM */
	Helios_DelayMS(1000);
	
	topo_for_each_node(hh, topo,
		&topo->ht_Nodes[topo->ht_LocalNodeID],
		NULL, _Helios_ScanNode);
			
	return TRUE;
}
/* WARNING: Caller shall lock the HeliosHardware object before calling this function */
void
_Helios_FreeTopology(HeliosHardware *hh)
{
	if (hh->hh_Topology)
	{
		FreePooled(HeliosBase->hb_MemPool, hh->hh_Topology, sizeof(HeliosTopology));
		hh->hh_Topology = NULL;
	}
}
/*=== EOF ====================================================================*/