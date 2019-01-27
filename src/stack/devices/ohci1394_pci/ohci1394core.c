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
** Low-level software support to OHCI 1394 compliant brigdes.
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
*/

/* TODO:
   - Handling broadcast transactions (only write).
   - ISO
   - better handling of dead DMA programs
*/
#ifndef NDEBUG
//#define DEBUG_CTX
//#define DEBUG_AT_DMA_CTX
//#define DEBUG_AR_DMA_CTX
//#define DEBUG_IR_DMA_CTX
//#define DEBUG_IT_DMA_CTX
//#define DEBUG_MEM
#define NIRQDEBUG
#endif

#include "ohci1394core.h"
#include "ohci1394trans.h"

#include "proto/helios.h"

#include <exec/errors.h>
#include <libraries/pcix.h>
#include <hardware/atomic.h>
#include <hardware/byteswap.h>
#include <clib/macros.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/pcix.h>
#include <proto/alib.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#ifdef DEBUG_AT_DMA_CTX
	#define _DBG_ATDMA_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
		_DBG("[unit %u, AT-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
	#define _ERR_ATDMA_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
		_ERR("[unit %u, AT-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
#else
	#define _DBG_ATDMA_CTX(c, fmt, args...)
	#define _ERR_ATDMA_CTX(c, fmt, args...)
#endif

#ifdef DEBUG_AR_DMA_CTX
	#define _DBG_ARDMA_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
		_DBG("[unit %u, AR-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
	#define _ERR_ARDMA_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
		_ERR("[unit %u, AR-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
#else
	#define _DBG_ARDMA_CTX(c, fmt, args...)
	#define _ERR_ARDMA_CTX(c, fmt, args...)
#endif

#ifdef DEBUG_IR_DMA_CTX
#define _INFO_IRDMA_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
	_INFO("[unit %u, IR-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
#define _ERR_IRDMA_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
	_ERR("[unit %u, IR-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
#else
#define _INFO_IRDMA_CTX(c, fmt, args...)
#define _ERR_IRDMA_CTX(c, fmt, args...)
#endif

#ifdef DEBUG_IT_DMA_CTX
#define _INFO_ITDMA_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
	_INFO("[unit %u, IR-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
#define _ERR_ITDMA_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
	_ERR("[unit %u, IR-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
#else
#define _INFO_ITDMA_CTX(c, fmt, args...)
#define _ERR_ITDMA_CTX(c, fmt, args...)
#endif

#ifdef DEBUG_CTX
	#define _INF_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
		_INF("[unit %u, Ctx-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
	#define _DBG_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
		_DBG("[unit %u, Ctx-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
	#define _WRN_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
		_WRN("[unit %u, Ctx-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
	#define _ERR_CTX(c, fmt, args...) ({ OHCI1394Context *_c = (OHCI1394Context *)(c); \
		_ERR("[unit %u, Ctx-$%x] " fmt, _c->ctx_Unit->hu_UnitNo, _c->ctx_RegOffset ,##args); })
#else
	#define _INF_CTX(c, fmt, args...)
	#define _DBG_CTX(c, fmt, args...)
	#define _WRN_CTX(c, fmt, args...)
	#define _ERR_CTX(c, fmt, args...)
#endif

#define SELF_ID_BUF_SIZE 0x800

#define PCIXCONFIG_PROGINTERFACE_OHCI 0x10
#define HELIOS_PCI_OWNER DEVNAME

#define PCI_IRQ_HANDLER_PRIORITY	10
#define TASK_PRIO_BUSRESET			5
#define TASK_PRIO_ARCTX				0
#define TASK_PRIO_ATCTX				0
#define TASK_PRIO_IRCTX				5
#define TASK_PRIO_SPLITTIMEOUT		5

#define MAXLOOP                 100
#define MAX_BAD_TOPO            10

#define OHCI_PHY_UPPERBOUND     (0x00010000)
#define AT_DMA_BUFFER_SIZE      (1024*64)   /* 64 KB to store all AT DMA buffers */

/* AR buffers size: to handle packet split across pages, page size requires to be
 * large enough to support the maximal packet size possible.
 * This size is 2068: 20 bytes for header + trailer, 2048 bytes of payload max at S400.
 * The maximal size is limited by the reqCount DMA program field: 65532.
 * Finally the number shall be aligned on quadlet (4-bytes).
 * More information about that at last paragraph of chapter 3.3.1 in OHCI-1.1 doc.
 */
#define ARBUFFER_PAGE_SIZE      65532 /* 2068 <= n <= 65532 */
#define ARBUFFER_PAGE_COUNT     10 /* per AR ctx */

#define OHCI1394_FLAGS_GUID_ROM (1<<0)

#define OHCI1394_MAX_AT_REQ_RETRIES     (0x2)
#define OHCI1394_MAX_AT_RESP_RETRIES    (0x2)
#define OHCI1394_MAX_PHYS_RESP_RETRIES  (0x8)

#define CTX_CTRL_SET(o)      ((o)+0x0)
#define CTX_CTRL_CLEAR(o)    ((o)+0x4)
#define CTX_CTRL_CMDPTR(o)   ((o)+0xC)

#define CTX_BUFFER_FILL        (1<<31)
#define CTX_ISOCH_HEADER       (1<<30)
#define CTX_CYCLE_MATCH_ENABLE (1<<29)
#define CTX_MULTI_CHAN_MODE    (1<<28)
#define CTX_DUAL_BUFFER_MODE   (1<<27)

static void ohci_ATContext_ATCompleteHandler(OHCI1394Context *);
static void ohci_CheckAndScheduleIsoList(struct MinList *list, ULONG events);

typedef struct {
	HeliosOffset offset;
	const STRPTR label;
} OHCI1394RegDescription;

static const OHCI1394RegDescription ohci_reg_names[] = {
	{ OHCI1394_REG_VERSION ,                    "version" },
	{ OHCI1394_REG_GUID_ROM,                    "guid rom" },
	{ OHCI1394_REG_AT_RETRIES,                  "at retries" },
	{ OHCI1394_REG_CSR_READ_DATA,               "csr read data" },
	{ OHCI1394_REG_CSR_COMPARE_DATA,            "csr compare data" },
	{ OHCI1394_REG_CSR_CONTROL,                 "csr control" },
	{ OHCI1394_REG_CONFIG_ROM_HDR,              "config rom hdr" },
	{ OHCI1394_REG_BUS_ID,                      "bus id" },
	{ OHCI1394_REG_BUS_OPTIONS,                 "bus options" },
	{ OHCI1394_REG_GUID_HI,                     "guid hi" },
	{ OHCI1394_REG_GUID_LO,                     "guid lo" },
	{ OHCI1394_REG_CONFIG_ROM_MAP,              "config rom map" },
	{ OHCI1394_REG_PWRITE_ADDR_LO,              "pwrite addr lo" },
	{ OHCI1394_REG_PWRITE_ADDR_HI,              "pwrite addr hi" },
	{ OHCI1394_REG_VENDOR_ID,                   "vendor id" },
	{ OHCI1394_REG_HC_CONTROL,                  "hc control" },
	{ OHCI1394_REG_SELFID_BUFFER,               "selfid buffer" },
	{ OHCI1394_REG_SELFID_COUNT,                "selfid count" },
	{ OHCI1394_REG_IR_MULTICHAN_MASK_HI,        "ir multichan mask hi" },
	{ OHCI1394_REG_IR_MULTICHAN_MASK_LO,        "ir multichan mask lo" },
	{ OHCI1394_REG_INT_EVENT,                   "int event" },
	{ OHCI1394_REG_INT_MASK,                    "int mask" },
	{ OHCI1394_REG_ISO_XMIT_INT_EVENT,          "iso xmit int event" },
	{ OHCI1394_REG_ISO_XMIT_INT_MASK,           "iso xmit int mask" },
	{ OHCI1394_REG_ISO_RECV_INT_EVENT,          "iso recv int event" },
	{ OHCI1394_REG_ISO_RECV_INT_MASK,           "iso recv int mask" },
	{ OHCI1394_REG_INITIAL_BW_AVAILABLE,        "initial bw available" },
	{ OHCI1394_REG_INITIAL_CHAN_AVAILABLE_HI,   "initial chan available hi" },
	{ OHCI1394_REG_INITIAL_CHAN_AVAILABLE_LO,   "initial chan available lo" },
	{ OHCI1394_REG_FAIRNESS_CONTROL,            "fairness control" },
	{ OHCI1394_REG_LINK_CONTROL,                "link control" },
	{ OHCI1394_REG_NODE_ID,                     "node id" },
	{ OHCI1394_REG_PHY_CONTROL,                 "phy control" },
	{ OHCI1394_REG_ISOCHRONOUS_CYCLE_TIMER,     "isochronous cycle timer" },
	{ OHCI1394_REG_ASYNCH_REQ_FILTER_HI,        "asynch req filter hi" },
	{ OHCI1394_REG_ASYNCH_REQ_FILTER_LO,        "asynch req filter lo" },
	{ OHCI1394_REG_PHYREQ_REQ_FILTER_HI,        "phyreq req filter hi" },
	{ OHCI1394_REG_PHYREQ_REQ_FILTER_LO,        "phyreq req filter lo" },
	{ OHCI1394_REG_PHYSICAL_UPPER_BOUND,        "physical upper bound" },
	{ OHCI1394_REG_AREQT_CONTEXT_CONTROL,       "areqt context control" },
	{ OHCI1394_REG_AREQT_COMMANDPTR,            "areqt commandptr" },
	{ OHCI1394_REG_AREST_CONTEXT_CONTROL,       "arest context control" },
	{ OHCI1394_REG_AREST_COMMAND_PTR,           "arest command ptr" },
	{ OHCI1394_REG_AREQR_CONTEXT_CONTROL,       "areqr context control" },
	{ OHCI1394_REG_AREQR_COMMAND_PTR,           "areqr command ptr" },
	{ OHCI1394_REG_ARESR_CONTEXT_CONTROL,       "aresr context control" },
	{ OHCI1394_REG_ARESR_COMMAND_PTR,           "aresr command ptr" },
};
const char *evt_strings[] = {
	[0x00] = "evt_no_status",   [0x01] = "-reserved-",
	[0x02] = "evt_long_packet", [0x03] = "evt_missing_ack",
	[0x04] = "evt_underrun",    [0x05] = "evt_overrun",
	[0x06] = "evt_descriptor_read", [0x07] = "evt_data_read",
	[0x08] = "evt_data_write",  [0x09] = "evt_bus_reset",
	[0x0a] = "evt_timeout",     [0x0b] = "evt_tcode_err",
	[0x0c] = "-reserved-",      [0x0d] = "-reserved-",
	[0x0e] = "evt_unknown",     [0x0f] = "evt_flushed",
	[0x10] = "-reserved-",      [0x11] = "ack_complete",
	[0x12] = "ack_pending",     [0x13] = "-reserved-",
	[0x14] = "ack_busy_X",      [0x15] = "ack_busy_A",
	[0x16] = "ack_busy_B",      [0x17] = "-reserved-",
	[0x18] = "-reserved-",      [0x19] = "-reserved-",
	[0x1a] = "-reserved-",      [0x1b] = "ack_tardy",
	[0x1c] = "-reserved-",      [0x1d] = "ack_data_error",
	[0x1e] = "ack_type_error",  [0x1f] = "-reserved-",
	[0x20] = "pending/cancelled",
};
#ifndef NDEBUG
static const char *speed_st[] = {
	[0] = "S100", [1] = "S200", [2] = "S400",    [3] = "beta",
};
static const char *power[] = {
	[0] = "+0W",  [1] = "+15W", [2] = "+30W",    [3] = "+45W",
	[4] = "-3W",  [5] = " ?W",  [6] = "-3..-6W", [7] = "-3..-10W",
};
#endif
static const char port[] = { '.', '-', 'p', 'c', };

/*============================================================================*/
/*--- LOCAL CODE -------------------------------------------------------------*/
/*============================================================================*/
/*=== Logging ================================================================*/
static void
log_IrqEvents(QUADLET events)
{
	/* CAUTION: called from IRQ context */

	IRQDB("%08x%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n", events,
		  events & OHCI1394_INTF_SELFIDCOMPLETE     ? " SelfID"            : "",
		  events & OHCI1394_INTF_SELFIDCOMPLETE2    ? " SelfID2"           : "",
		  events & OHCI1394_INTF_RQPKT              ? " AR_req"            : "",
		  events & OHCI1394_INTF_RSPKT              ? " AR_resp"           : "",
		  events & OHCI1394_INTF_REQTXCOMPLETE      ? " AT_req"            : "",
		  events & OHCI1394_INTF_RESPTXCOMPLETE     ? " AT_resp"           : "",
		  events & OHCI1394_INTF_ISOCHRX            ? " IR"                : "",
		  events & OHCI1394_INTF_ISOCHTX            ? " IT"                : "",
		  events & OHCI1394_INTF_POSTEDWRITEERR     ? " PostedWriteErr"    : "",
		  events & OHCI1394_INTF_CYCLETOOLONG       ? " CycleTooLong"      : "",
		  events & OHCI1394_INTF_CYCLE64SECONDS     ? " Cycle64Seconds"    : "",
		  events & OHCI1394_INTF_REGACCESSFAIL      ? " RegAccessFail"     : "",
		  events & OHCI1394_INTF_BUSRESET           ? " BusReset"          : "",
		  events & OHCI1394_INTF_UNRECOVERABLEERROR ? " UnrecoverableError": "",
		  events & ~(OHCI1394_INTF_SELFIDCOMPLETE
					 | OHCI1394_INTF_SELFIDCOMPLETE2
					 | OHCI1394_INTF_RQPKT
					 | OHCI1394_INTF_RSPKT
					 | OHCI1394_INTF_REQTXCOMPLETE
					 | OHCI1394_INTF_RESPTXCOMPLETE
					 | OHCI1394_INTF_ISOCHRX
					 | OHCI1394_INTF_ISOCHTX
					 | OHCI1394_INTF_POSTEDWRITEERR
					 | OHCI1394_INTF_CYCLETOOLONG
					 | OHCI1394_INTF_CYCLE64SECONDS
					 | OHCI1394_INTF_REGACCESSFAIL
					 | OHCI1394_INTF_BUSRESET
					 | OHCI1394_INTF_UNRECOVERABLEERROR)  ? " ?"             : "");
}
static void
log_IrqIsoRecvEvents(QUADLET events)
{
	/* CAUTION: called from IRQ context */

	IRQDB("iso receive events received: $%08x\n", events);
}
static void
log_IrqIsoXmitEvents(QUADLET events)
{
	/* CAUTION: called from IRQ context */

	IRQDB("iso transmit events received: $%08x\n", events);
}
static void
log_IrqError(const char *msg, QUADLET events)
{
	/* CAUTION: called from IRQ context */

	IRQDB("error events received: $%08x \"%s\"\n", events, msg);
}
static inline char
pn(QUADLET *s, ULONG shift)
{
	return port[*s >> shift & 3];
}
static void
log_SelfIDs(UBYTE nodeID, UBYTE generation, ULONG selfIDCount, QUADLET *s)
{
	int i;
	
	_INF("%u SelfID pkts, Gen=%u, LocalNodeID=%u\n", selfIDCount, generation, nodeID);

	for (i=0; i < selfIDCount; i++, s++)
	{
		if ((*s & (1 << 23)) == 0)
			_INF("SelfID#%u (0x%08x): P0 phy %d [%c%c%c] "
				"%s gc=%d %s %s%s%s\n", i,
				*s, *s >> 24 & 63, pn(s, 6), pn(s, 4), pn(s, 2),
				speed_st[*s >> 14 & 3], *s >> 16 & 63,
				power[*s >> 8 & 7], *s >> 22 & 1 ? "L" : "",
				*s >> 11 & 1 ? "c" : "", *s & 2 ? "i" : "");
		else
			_INF("SelfID#%u (0x%08x): Pn phy %d [%c%c%c%c%c%c%c%c]\n",
				i, *s, *s >> 24 & 63,
				pn(s, 16), pn(s, 14), pn(s, 12), pn(s, 10),
				pn(s,  8), pn(s,  6), pn(s,  4), pn(s,  2));
	}
}
/*=== OHCI low-level register access =========================================*/
static QUADLET
ohci_RegRead(OHCI1394Unit *unit, LONG offset)
{
	register QUADLET ret;
	register volatile QUADLET *address = (APTR)unit->hu_PCI_RegBase + offset;

#ifdef __PPC__
	/* bytes swap on BE => PCI is little-endian */
	__asm volatile ("lwbrx %0,0,%1; sync" : "=r" (ret) : "r" (address));
#else
	ret = BE_SWAPLONG(*address);
#endif

	return ret;
}
static void
ohci_RegWrite(OHCI1394Unit *unit, LONG offset, QUADLET value)
{
	register volatile QUADLET *address = (APTR)unit->hu_PCI_RegBase + offset;

#ifdef __PPC__
	/* bytes swap on BE => PCI is little-endian */
	__asm volatile ("stwbrx %0,0,%1; sync" : : "r" (value), "r" (address));
#else
	*address = BE_SWAPLONG(value);
#endif
}
static void
ohci_OnUnrecoverableError(OHCI1394Unit *unit, STRPTR msg)
{
	_ERR("UnrecoverableError: unit #%lu shall be reset now! Reason: '%s'\n",
		unit->hu_UnitNo, msg);

	EXCLUSIVE_PROTECT_BEGIN(unit);
	unit->hu_Flags.UnrecoverableError = TRUE;
	EXCLUSIVE_PROTECT_END(unit);

	/* TODO: unit full reset? */
	DB_NotFinished();
}
/* WARNING: this function delays */
static BOOL
ohci_WritePHY(
	OHCI1394Unit *unit,
	UBYTE addr,
	UBYTE bits_clear,
	UBYTE bits_set)
{
	ULONG loop;
	QUADLET data;
	BOOL pending;

	/* Check that the PHY register is not busy */
	if (0 == ((data = ohci_RegRead(unit, OHCI1394_REG_PHY_CONTROL)) & OHCI1394_PCF_WR_REG))
	{
		/* Send a read request to PHY */
		ohci_RegWrite(unit, OHCI1394_REG_PHY_CONTROL, OHCI1394_PCF_READ(addr));

		/* Poll PHY ctrl until read request completion */
		loop = MAXLOOP;
		do
		{
			data = ohci_RegRead(unit, OHCI1394_REG_PHY_CONTROL);
			pending = data & OHCI1394_PCF_RD_REG;
			if (pending)
				Helios_DelayMS(5); /* 5 ms delay */
		}
		while (pending && --loop);

		if (!pending)
		{
			if (OHCI1394_PC_READ_ADDR(data) == addr)
			{
				QUADLET q;

				/* modify data and write back */
				q = OHCI1394_PCF_WRITE(addr, (OHCI1394_PC_READ_DATA(data) & ~bits_clear) | bits_set);
				ohci_RegWrite(unit, OHCI1394_REG_PHY_CONTROL, q);

				/* Wait for write request completion */
				loop = MAXLOOP;
				do
				{
					pending = ohci_RegRead(unit, OHCI1394_REG_PHY_CONTROL) & OHCI1394_PCF_WR_REG;
					if (pending)
						Helios_DelayMS(5); /* 5 ms delay */
				}
				while (pending && --loop);

				if (!pending)
					return TRUE;
				else
					_ERR_UNIT(unit, "PHY write timeout\n");
			}
			else
				_ERR_UNIT(unit, "PHY read addr mismatch: get %02x, %02x waited (PHY=%x)\n",
					OHCI1394_PC_READ_ADDR(data), addr, data);
		}
		else
			_ERR_UNIT(unit, "PHY read timeout\n");
	}
	else
		_ERR_UNIT(unit, "PHY busy\n");

	return FALSE;
}
/*=== PCI level handling =======================================================*/
static ULONG
ohci_PCI_IrqHandler(APTR data)
{
	OHCI1394Unit *unit = (APTR)data;
	QUADLET events, iso_events;

	/* read masked version of interrupt events */
	events = ohci_RegRead(unit, OHCI1394_REG_INT_EVENT_CLEAR);
	if (!events || !~events) return 0; /* spurious interrupt? */

	/* OHCI 1.1, chapter 7.2.3.2 => don't clear BusReset int (done by the BusReset handler) */
	ohci_RegWrite(unit, OHCI1394_REG_INT_EVENT_CLEAR, events & ~OHCI1394_INTF_BUSRESET);
	
	log_IrqEvents(events);

	/* Handle 64 seconds cycle timeout */
	if (events & OHCI1394_INTF_CYCLE64SECONDS)
	{
		QUADLET value;

		value = ohci_RegRead(unit, OHCI1394_REG_ISOCHRONOUS_CYCLE_TIMER);
		if (0 == (value & 0x80000000))
			ATOMIC_ADD(&unit->hu_BusSeconds, 1);
	}

	/* Handle errors (mostly just log them) */
	if (events & OHCI1394_INTF_CYCLETOOLONG)
	{
		log_IrqError("isochronous cycle too long", events);
		ohci_RegWrite(unit, OHCI1394_REG_LINK_CONTROL_SET, OHCI1394_LCF_CYCLEMASTER);
	}
	
	if (events & OHCI1394_INTF_UNRECOVERABLEERROR)
	{
		log_IrqError("Unrecoverable error", events);
		ohci_OnUnrecoverableError(unit, "IRQ UnrecoverableError received!");
	}
	
	if (events & OHCI1394_INTF_REGACCESSFAIL)
		log_IrqError("Register access failure", events);

	if (events & OHCI1394_INTF_POSTEDWRITEERR)
		log_IrqError("PCI posted write error", events);

	/* Handle Asynchronous events */
	if (events & OHCI1394_INTF_SELFIDCOMPLETE) Helios_SignalSubTask(unit->hu_BusResetTask, unit->hu_BusResetSignal);
	if (events & OHCI1394_INTF_REQTXCOMPLETE) Helios_SignalSubTask(unit->hu_ATRequestCtx.atc_Context.ctx_SubTask, unit->hu_ATRequestCtx.atc_Context.ctx_Signal);
	if (events & OHCI1394_INTF_RESPTXCOMPLETE) Helios_SignalSubTask(unit->hu_ATResponseCtx.atc_Context.ctx_SubTask, unit->hu_ATResponseCtx.atc_Context.ctx_Signal);
	if (events & OHCI1394_INTF_RQPKT) Helios_SignalSubTask(unit->hu_ARRequestCtx.arc_Context.ctx_SubTask, unit->hu_ARRequestCtx.arc_Context.ctx_Signal);
	if (events & OHCI1394_INTF_RSPKT) Helios_SignalSubTask(unit->hu_ARResponseCtx.arc_Context.ctx_SubTask, unit->hu_ARResponseCtx.arc_Context.ctx_Signal);

#if 0
	/* Isochronous RX events? */
	iso_events = ohci_RegRead(unit, OHCI1394_REG_ISO_RECV_INT_EVENT_CLEAR);
	ohci_RegWrite(unit, OHCI1394_REG_ISO_RECV_INT_EVENT_CLEAR, iso_events);
	if (iso_events)
	{
		log_IrqIsoRecvEvents(iso_events);

		/* Signal corresponding RX iso handlers */
		SHARED_PROTECT_BEGIN(unit);
		ohci_CheckAndScheduleIsoList(&unit->hu_IRCtxList, iso_events);
		SHARED_PROTECT_END(unit);
	}

	/* Isochronous TX events? */
	iso_events = ohci_RegRead(unit, OHCI1394_REG_ISO_XMIT_INT_EVENT_CLEAR);
	ohci_RegWrite(unit, OHCI1394_REG_ISO_XMIT_INT_EVENT_CLEAR, iso_events);
	if (iso_events)
	{
		log_IrqIsoXmitEvents(iso_events);

		/* Signal corresponding TX iso handlers */
		LOCK_REGION_SHARED(unit);
		ohci_CheckAndScheduleIsoList(&unit->hu_ITCtxList, iso_events);
		UNLOCK_REGION_SHARED(unit);
	}
#endif

	return 0;
}
static BOOL
ohci_PCI_InstallIRQ(OHCI1394Unit *unit)
{
	if (!unit->hu_PCI_IRQHandlerObject)
	{
		_DBG_UNIT(unit, "Installing IRQ handler\n");
		
		unit->hu_PCI_IRQHandlerObject = PCIXCreateIntObjectTags(
			unit->hu_PCI_BoardObject,
			(ULONG(*)(void))ohci_PCI_IrqHandler, unit,
			PCIXINTTAG_MACHINE, MACHINE_PPC,
			PCIXINTTAG_PRI, PCI_IRQ_HANDLER_PRIORITY,
			TAG_DONE);
		if (!unit->hu_PCI_IRQHandlerObject)
		{
			_ERR_UNIT(unit, "Failed to install IRQ handler\n");
			return FALSE;
		}
	}
	
	return TRUE;
}
static void
ohci_PCI_RemoveIRQ(OHCI1394Unit *unit)
{
	if (unit->hu_PCI_IRQHandlerObject)
	{
		_DBG_UNIT(unit, "Removing IRQ handler\n");
		
		ohci_RegWrite(unit, OHCI1394_REG_INT_MASK_CLEAR, ~0);
		PCIXDeleteIntObject(unit->hu_PCI_IRQHandlerObject);
		unit->hu_PCI_IRQHandlerObject = NULL;
	}
}
static LONG
ohci_PCI_OpenUnit(OHCI1394Unit *unit)
{
	APTR board = unit->hu_PCI_BoardObject;
	STRPTR owner;
	ULONG size;

	/* Set us as board owner into PCIX stack.
	 * Handle case where an owner is already set.
	 */
	
	_DBG_UNIT(unit, "Trying PCI board %p...\n", board);
	if (PCIXAttemptBoard(board))
	{
		owner = (STRPTR) PCIXGetBoardAttr(board, PCIXTAG_OWNER);
		if (PCIXSetBoardAttr(board, PCIXTAG_OWNER, (ULONG) HELIOS_PCI_OWNER)
			|| !strcmp(owner, HELIOS_PCI_OWNER))
		{
			PCIXReleaseBoard(board);

			/* We are owner!
			 * Get & check OHCI registers space size and address
			 */
			size = PCIXGetBoardAttr(board, PCIXTAG_BASESIZE0);
			if (size >= OHCI1394_REGISTERS_SPACE_SIZE) {
				unit->hu_PCI_RegBase = (APTR) PCIXGetBoardAttr(board, PCIXTAG_BASEADDRESS0);

				/* Check for alignement requirement */
				if (!(((ULONG) unit->hu_PCI_RegBase) % OHCI1394_REGISTERS_SPACE_SIZE))
				{
					/* Put PowerManagement in D0 state */
					PCIXWriteConfigWord(board, 0x54, PCIXReadConfigWord(board, 0x54) & ~3);

					/* Enable bridge and set it as bus master */
					if((PCIXReadConfigWord(board, PCIXCONFIG_COMMAND) & 7) == 0)
						PCIXWriteConfigWord(board, PCIXCONFIG_COMMAND, 7);

					return TRUE;
				}
				else
					_ERR_UNIT(unit, "OHCI registers space is not aligned on %u bytes!\n",
						OHCI1394_REGISTERS_SPACE_SIZE);
			}
			else
				_ERR_UNIT(unit, "Too small OHCI registers space: %u byte(s) found, should be >= %u\n",
					size, OHCI1394_REGISTERS_SPACE_SIZE);

			PCIXSetBoardAttr(board, PCIXTAG_OWNER, NULL);
		}
		else
			_ERR_UNIT(unit, "PCI board %p already owned by '%s' (%p)\n",
				board, owner, owner);

		PCIXReleaseBoard(board);
	}
	else
		_ERR_UNIT(unit, "Access failed on PCI board %p\n", board);

	return FALSE;
}
static void
ohci_PCI_CloseUnit(OHCI1394Unit *unit)
{
	APTR board = unit->hu_PCI_BoardObject;

	_DBG_UNIT(unit, "Closing PCI board %p\n", board);

	PCIXObtainBoard(board);
	
	/* Disable the board */
	PCIXWriteConfigWord(board, PCIXCONFIG_COMMAND, 0);

	/* Put PowerManagement in D0 state */
	PCIXWriteConfigWord(board, 0x54, PCIXReadConfigWord(board, 0x54) & ~3);

	/* Remove the PCI board owning */
	PCIXSetBoardAttr(board, PCIXTAG_OWNER, NULL);

	PCIXReleaseBoard(board);
}
/*=== Basic OHCI API ===========================================================*/
static BOOL
ohci_SoftReset(OHCI1394Unit *unit)
{
	ULONG loop = MAXLOOP;
	BOOL pending;

	_INF_UNIT(unit, "soft reset...\n");
	
	ohci_RegWrite(unit, OHCI1394_REG_HC_CONTROL_SET, OHCI1394_HCCF_SOFTRESET);

	/* loop until SoftReset signal returns to zero (10ms polling) */
	do
	{
		pending = 0 != (ohci_RegRead(unit, OHCI1394_REG_HC_CONTROL) & OHCI1394_HCCF_SOFTRESET);
		if (pending)
			Helios_DelayMS(10);
	} while (pending && --loop);

	if (pending)
		_ERR_UNIT(unit, "soft reset failed\n");

	return !pending;
}
static void
ohci_EnableInterrupts(OHCI1394Unit *unit)
{
	_DBG_UNIT(unit, "Enabling interrupts...\n");
	ohci_RegWrite(unit, OHCI1394_REG_INT_MASK_SET,
		OHCI1394_INTF_MASTERINTENABLE

		/* --- */

		| OHCI1394_INTF_SELFIDCOMPLETE
		| OHCI1394_INTF_SELFIDCOMPLETE2
		| OHCI1394_INTF_REGACCESSFAIL
		| OHCI1394_INTF_UNRECOVERABLEERROR
		| OHCI1394_INTF_CYCLE64SECONDS
		| OHCI1394_INTF_CYCLETOOLONG
		| OHCI1394_INTF_CYCLEINCONSISTENT
		| OHCI1394_INTF_POSTEDWRITEERR

		/* --- */

		/*| OHCI1394_INTF_ARRQ*/
		/*| OHCI1394_INTF_ARRS*/
		| OHCI1394_INTF_RQPKT
		| OHCI1394_INTF_RSPKT
		| OHCI1394_INTF_REQTXCOMPLETE
		| OHCI1394_INTF_RESPTXCOMPLETE
		| OHCI1394_INTF_ISOCHTX
		| OHCI1394_INTF_ISOCHRX);
}
static void
ohci_DisableInterrupts(OHCI1394Unit *unit)
{
	_DBG_UNIT(unit, "Disabling interrupts...\n");
	ohci_RegWrite(unit, OHCI1394_REG_INT_MASK_CLEAR, -1);
}
/*=== Generic context API ====================================================*/
static LONG
ohci_Context_SubTask(HeliosSubTask *self, struct TagItem *tags)
{
	OHCI1394Context *ctx;
	struct MsgPort *taskport;
	ULONG signal, sigset;
	BOOL run;

	taskport = (APTR) GetTagData(HA_MsgPort, 0, tags);
	ctx = (APTR) GetTagData(HA_UserData, 0, tags);

	if (!taskport|| !ctx)
	{
		_ERR("Badly called\n");
		return 0;
	}

	signal = AllocSignal(-1);
	if (-1 == signal)
	{
		_ERR_CTX(ctx, "AllocSignal(-1) failed\n");
		return 0;
	}

	ctx->ctx_Signal = 1 << signal;

	sigset = ctx->ctx_Signal | (1 << taskport->mp_SigBit);
	run = TRUE;
	do
	{
		HeliosMsg *msg;
		ULONG sigs;

		sigs = Wait(sigset);

		while (NULL != (msg = (APTR) GetMsg(taskport)))
		{
			switch (msg->hm_Type)
			{
				
				case HELIOS_MSGTYPE_INIT:
					_DBG_CTX(ctx, "CTX_Task-%p: init msg rcv\n", FindTask(NULL));
					break;
					
				case HELIOS_MSGTYPE_DIE:
					_WRN_CTX(ctx, "CTX_Task-%p: die msg rcv\n", FindTask(NULL));
					run = FALSE;
					FreeMem(msg, msg->hm_Msg.mn_Length);
					continue;
			}

			ReplyMsg((struct Message *) msg);
		}

		if (run && (sigs & ctx->ctx_Signal))
			ctx->ctx_Handler(ctx);
	}
	while (run);

	FreeSignal(signal);
	return 0;
}
static BOOL
ohci_Context_Init(
	OHCI1394Unit *		unit,
	OHCI1394Context *	ctx,
	ULONG				reg_offset,
	STRPTR				task_name,
	OHCI1394CtxHandler	handler,
	LONG				task_priority)
{
	HeliosMsg msg;
	
	LOCK_INIT(ctx);

	ctx->ctx_Unit = unit;
	ctx->ctx_RegOffset = reg_offset;
	ctx->ctx_Handler = handler;
	
	ctx->ctx_SubTask = Helios_CreateSubTask(
		task_name,
		ohci_Context_SubTask,
		HA_MemPool, (ULONG)unit->hu_MemPool,
		TASKTAG_PRI, task_priority,
		HA_UserData, (ULONG)ctx,
		TAG_DONE);
	
	if (!ctx->ctx_SubTask)
		return FALSE;

	msg.hm_Msg.mn_Length = sizeof(msg);
	msg.hm_Msg.mn_Node.ln_Type = NT_MESSAGE;
	msg.hm_Type = HELIOS_MSGTYPE_INIT;
	
	if (Helios_DoMsgToSubTask(ctx->ctx_SubTask, &msg.hm_Msg, NULL) != HERR_NOERR)
		Helios_KillSubTask(ctx->ctx_SubTask, TRUE);
	
	return TRUE;
}
static void
ohci_Context_Term(OHCI1394Context *ctx)
{
	Helios_KillSubTask(ctx->ctx_SubTask, TRUE);
}
static BOOL
ohci_Context_IsDead(OHCI1394Context *ctx)
{
	return ohci_RegRead(ctx->ctx_Unit, CTX_CTRL_SET(ctx->ctx_RegOffset)) & CTX_DEAD;
}
static BOOL
ohci_Context_Stop(OHCI1394Context *ctx)
{
	OHCI1394Unit *unit = ctx->ctx_Unit;
	ULONG regoff = ctx->ctx_RegOffset;
	ULONG loop = 2;

	ohci_RegWrite(unit, CTX_CTRL_CLEAR(regoff), CTX_RUN);

	/* Wait about DMA safe state */
	do
	{
		QUADLET ctrl = ohci_RegRead(unit, CTX_CTRL_SET(regoff));
		if ((ctrl & CTX_ACTIVE) == 0)
			return TRUE;

		_DBG_CTX(ctx, "context %08x still active\n", regoff);
		Helios_DelayMS(25);
	} while (loop--);

	/* Timeout */
	_ERR_CTX(ctx, "TIMEOUT, context %08x still active 50ms after STOP request\n", regoff);

	return FALSE;
}
/*=== AT Context API =========================================================*/
/* WARNING: All AT context functions shall be locked before calling */
static void
ohci_ATContext_InitDMABuffers(OHCI1394ATCtx *ctx)
{
	const static ULONG cnt = AT_DMA_BUFFER_SIZE / sizeof(OHCI1394ATBuffer);
	ULONG i;

	NEWLIST((struct List *)&ctx->atc_BufferList);
	NEWLIST((struct List *)&ctx->atc_UsedBufferList);
	NEWLIST((struct List *)&ctx->atc_DeadBufferList);
	for (i=0; i<cnt; i++)
		ADDTAIL((struct List *)&ctx->atc_BufferList, (struct Node *)&ctx->atc_CpuDMABuffers[i]);
}
static OHCI1394ATBuffer *
ohci_ATContext_GetBuffer(OHCI1394ATCtx *ctx)
{
	OHCI1394ATBuffer *buf;

	buf = (OHCI1394ATBuffer *)REMHEAD((struct List *)&ctx->atc_BufferList);
	ADDTAIL((struct List *)&ctx->atc_UsedBufferList, (struct Node *)buf);
	ctx->atc_BufferUsage--;

	_DBG_CTX(ctx, "DMA usage: %lu\n", ctx->atc_BufferUsage);
	
	return buf;
}
static void
ohci_ATContext_ReleaseBuffer(OHCI1394ATCtx *ctx, OHCI1394ATBuffer *buf)
{
	/* Tail when removed, head when get: rotate buffers usage */
	REMOVE((struct Node *)buf);
	ADDTAIL((struct List *)&ctx->atc_BufferList, (struct Node *)buf);
	ctx->atc_BufferUsage++;
}
static ULONG
ohci_ATContext_GetPhyAddress(OHCI1394ATCtx *ctx, APTR ptr)
{
	return ctx->atc_PhyDMABuffers + (ptr - (APTR)ctx->atc_CpuDMABuffers);
}
static ULONG
ohci_ATContext_GetCPUAddress(OHCI1394ATCtx *ctx, ULONG ptr)
{
	return ((ULONG)ctx->atc_CpuDMABuffers) + (ptr - ctx->atc_PhyDMABuffers);
}
static void
ohci_ATContext_Run(OHCI1394ATCtx *ctx)
{
	OHCI1394Unit *unit = ctx->atc_Context.ctx_Unit;
	ULONG regoff = ctx->atc_Context.ctx_RegOffset;
	QUADLET ctrl;

	ctrl = ohci_RegRead(unit, CTX_CTRL_SET(regoff));

	/* CommandPtr context value records the first descriptor of a DMA program.
	 * Some rules shall be respected to change this value:
	 * - The value shall always be valid and not 0 when it written here.
	 * - The software put descriptors in a FIFO queue, but the DMA may handle them in
	 * a out-of-order way. So the value shall always represent the oldest not acknowledged
	 * descriptor. All previous descriptors are known as parsed and acknownledged.
	 * - The value is 0 if no previous DMA program is pending.
	 * - As the oldest descriptor can be fetched and acknowledged during the time we change
	 * the value, we never change the OHCI CommandPtr register until the current DMA program
	 * is finished, i.e. the value is set to 0.
	 */

	/* The CommandPtr register shall only be written if RUN & ACTIVE bits are 0 */
	if (0 == (ctrl & (CTX_RUN | CTX_ACTIVE)))
	{
		_DBG_ATDMA_CTX(ctx, "CmdPtr to %p\n", ctx->atc_CommandPtr);
		ohci_RegWrite(unit, CTX_CTRL_CMDPTR(regoff), ctx->atc_CommandPtr);
		ohci_RegWrite(unit, CTX_CTRL_CLEAR(regoff), ~0);
		ohci_RegWrite(unit, CTX_CTRL_SET(regoff), CTX_RUN);

		/* Note: if the context dead, this cas will be handled later by the DeadIRQ.
		 * So the code should always assumes that its run request is pending.
		 */

		_DBG_ATDMA_CTX(ctx, "ctrl=$%08x\n", ohci_RegRead(unit, CTX_CTRL_SET(regoff)));
	}
}
static void
ohci_ATContext_AppendBuffer(
	OHCI1394ATCtx *		ctx,
	OHCI1394ATBuffer *	buffer,
	ULONG				z)
{
	OHCI1394Unit *unit = ctx->atc_Context.ctx_Unit;
	ULONG regoff = ctx->atc_Context.ctx_RegOffset;
	OHCI1394Descriptor *d;
	ULONG d_phy_addr;

	d = &buffer->atb_Descriptors[0];
	d_phy_addr = ohci_ATContext_GetPhyAddress(ctx, d); /* normally aligned on 16-bytes */

	_DBG_CTX(ctx, "CmdPtr=$%p, Reg=$%p\n", ctx->atc_CommandPtr,
		ohci_RegRead(unit, CTX_CTRL_CMDPTR(regoff)));

	/* DMA already programmed ? */
	if (NULL != ctx->atc_LastBuffer)
	{
		_DBG_CTX(ctx, "Last buffer of ctx %p: %p\n", regoff, ctx->atc_LastBuffer);

		/* Attach this new buffer to an existant DMA program */
		ctx->atc_LastBuffer->atb_LastDescriptor->d_BranchAddress = BE_SWAPLONG( d_phy_addr | z);

		if (0 == ctx->atc_CommandPtr)
			ohci_ATContext_ReleaseBuffer(ctx, ctx->atc_LastBuffer);
	}

	if (0 == ctx->atc_CommandPtr)
		ctx->atc_CommandPtr = d_phy_addr | z;

	/* This buffer becomes the last buffer in the DMA program */
	ctx->atc_LastBuffer = buffer;

	_DBG_CTX(ctx, "%u descriptor(s) filled: d=%p, last=#%u, ba=%x\n",
			   z, d, (ctx->atc_LastBuffer->atb_LastDescriptor - d) / sizeof(OHCI1394Descriptor),
			   d_phy_addr);

	log_DumpMem(d, sizeof(OHCI1394Descriptor)*z, TRUE, "Dumping context descriptors:\n");

	/* Force a DMA wake-up */
	_DBG_CTX(ctx, "wake-up ... (status = %08x)\n", ohci_RegRead(unit, CTX_CTRL_SET(regoff)));
	ohci_RegWrite(unit, CTX_CTRL_SET(regoff), CTX_WAKE);
}
static BOOL
ohci_ATContext_Init(
	OHCI1394Unit *		unit,
	OHCI1394ATCtx *		ctx,
	ULONG				regoffset,
	STRPTR				task_name)
{
	const static ULONG cnt = AT_DMA_BUFFER_SIZE / sizeof(OHCI1394ATBuffer);
	const static ULONG size = cnt * sizeof(OHCI1394ATBuffer);

	_INF_UNIT(unit, "AT context $%X: DMA buffer size=%lu bytes\n", regoffset, size);

	/* Allocate DMA buffers space, 16-bytes aligned */
	ctx->atc_AllocDMABuffers = AllocVecDMA(size + 15, MEMF_PUBLIC | MEMF_CLEAR);
	if (NULL == ctx->atc_AllocDMABuffers) {
		_ERR_UNIT(unit, "AT-DMA buffer allocation failed\n");
		return FALSE;
	}

	ctx->atc_BufferUsage = cnt;

	ctx->atc_CpuDMABuffers = (APTR)(16 + (((ULONG)ctx->atc_AllocDMABuffers - 1) & ~15));
	ctx->atc_PhyDMABuffers = (ULONG)PCIXDMAGetPhysical(unit->hu_PCI_BoardObject, ctx->atc_CpuDMABuffers);
	_INF_UNIT(unit, "AT-DMA buffers: cpu@%p, phy@%p\n", ctx->atc_CpuDMABuffers, ctx->atc_PhyDMABuffers);

	/* Init. all AT buffers */
	ohci_ATContext_InitDMABuffers(ctx);
	
	if (!ohci_Context_Init(unit, &ctx->atc_Context, regoffset,
		task_name, ohci_ATContext_ATCompleteHandler,
		TASK_PRIO_ATCTX))
	{
		FreeVecDMA(ctx->atc_AllocDMABuffers);
		return FALSE;
	}

	return TRUE;
}
static void
ohci_ATContext_Term(OHCI1394ATCtx *ctx)
{
	LOCK_CTX(ctx);
	ohci_Context_Stop(&ctx->atc_Context);
	UNLOCK_CTX(ctx);

	ohci_Context_Term(&ctx->atc_Context);
	FreeVecDMA(ctx->atc_AllocDMABuffers);
}
static BOOL
ohci_ATContexts_Init(OHCI1394Unit *unit)
{
	if (ohci_ATContext_Init(unit, &unit->hu_ATRequestCtx,
							OHCI1394_REG_AREQT_CONTEXT_CONTROL,
							"["DEVNAME"] AT Requests Handler")) {
		_INF_UNIT(unit, "AT request context %p created!\n", &unit->hu_ATRequestCtx);
		if (ohci_ATContext_Init(unit, &unit->hu_ATResponseCtx,
								OHCI1394_REG_AREST_CONTEXT_CONTROL,
								"["DEVNAME"] AT Responses Handler")) {
			_INF_UNIT(unit, "AT response context %p created!\n", &unit->hu_ATResponseCtx);
			return TRUE;
		} else
			_ERR_UNIT(unit, "AT response context creation failed\n");

		ohci_ATContext_Term(&unit->hu_ATRequestCtx);
	} else
		_ERR_UNIT(unit, "AT request context creation failed\n");
	return FALSE;
}
static void
ohci_ATContexts_Term(OHCI1394Unit *unit)
{
	ohci_ATContext_Term(&unit->hu_ATRequestCtx);
	ohci_ATContext_Term(&unit->hu_ATResponseCtx);
}
static void
ohci_ATContexts_Stop(OHCI1394Unit *unit)
{
	LOCK_CTX(&unit->hu_ATRequestCtx); 
	ohci_Context_Stop(&unit->hu_ATRequestCtx.atc_Context);
	UNLOCK_CTX(&unit->hu_ATRequestCtx);         
	
	LOCK_CTX(&unit->hu_ATResponseCtx);                    
	ohci_Context_Stop(&unit->hu_ATResponseCtx.atc_Context);
	UNLOCK_CTX(&unit->hu_ATResponseCtx);
}
static void
ohci_ATContext_ATCompleteHandler(OHCI1394Context *_ctx)
{
	OHCI1394ATCtx *ctx = (APTR)_ctx;
	OHCI1394Unit *unit = _ctx->ctx_Unit;
	OHCI1394ATBuffer *buf, *next, *last=NULL;
	BOOL dead;

	/* The AT DMA program is recorded by the software in a FIFO queue.
	 * But depending on the DMA used, a out-of-order fetching method may be used.
	 * So when this procedure is called (when an AT-complete interrupt is raised), we
	 * need to scan this queue to find which DMA block has been acknowledged and
	 * remove them from the queue. We need also to maintain the context CommandPtr
	 * value to permit software to append new descriptors.
	 */

	LOCK_CTX(ctx);
	{
		dead = ohci_Context_IsDead(_ctx);

		if (dead)
		{
			QUADLET phy = ohci_RegRead(unit, CTX_CTRL_CMDPTR(_ctx->ctx_RegOffset));

			last = (OHCI1394ATBuffer *)ohci_ATContext_GetCPUAddress(ctx, phy & 7);
			_DBG_ATDMA_CTX(ctx, "Dead ctx, last fetched=$%p", last);
		}

		ForeachNodeSafe(&ctx->atc_UsedBufferList, buf, next)
		{
			OHCI1394Descriptor *d_last = buf->atb_LastDescriptor;

			/* Processed descriptors ? */
			if (d_last->d_TransferStatus)
			{
				REMOVE(buf);
				ADDTAIL(&ctx->atc_DeadBufferList, buf);
			}
			else if (dead)
			{ /* Not processed, but may be the cause of dead context */
				d_last->d_TransferStatus = OHCI1394_EVENT_MISSING_ACK;

				REMOVE(buf);
				ADDTAIL(&ctx->atc_DeadBufferList, buf);
				
				/* remove all blocks until the last fetched DMA block is reached */
				if (buf == last)
					break;
			}
			else
			{
				last = buf;
				break;
			}
		}

		/* Completly finished or not ? */
		if (NULL == last)
			ctx->atc_CommandPtr = 0;
		else if (dead)
		{ /* Setup correctly the CommandPtr to the next valid descriptor block */
			OHCI1394Descriptor *d = &last->atb_Descriptors[0];
			int z = ((last->atb_LastDescriptor - d) / sizeof(OHCI1394Descriptor)) + 1;
			
			ctx->atc_CommandPtr = ohci_ATContext_GetPhyAddress(ctx, d) | z;
		}
		else
		{ /* Update the context CommandPtr in case of a future stop-start */
			OHCI1394Descriptor *d = &last->atb_Descriptors[0];
			int z = ((last->atb_LastDescriptor - d) / sizeof(OHCI1394Descriptor)) + 1;

			ctx->atc_CommandPtr = ohci_ATContext_GetPhyAddress(ctx, d) | z;
		}

		_DBG_ATDMA_CTX(ctx, "New CommandPtr reg: $%p\n", ctx->atc_CommandPtr);
	}
	UNLOCK_CTX(ctx);

	if (dead)
	{
		/* Remove the unrecoverable error signal */
		EXCLUSIVE_PROTECT_BEGIN(unit);
		unit->hu_Flags.UnrecoverableError = 0;
		EXCLUSIVE_PROTECT_END(unit);

		/* Clear ctx ctrl reg and re-run */
		LOCK_CTX(ctx);
		{
			int regoff = _ctx->ctx_RegOffset;

			ohci_RegWrite(unit, CTX_CTRL_CLEAR(regoff), ~0);
			ohci_RegWrite(unit, CTX_CTRL_SET(regoff), CTX_RUN);

			/* Note: if the context dead, this cas will be handled later by the DeadIRQ.
			 * So the code should always assumes that its run request is pending.
			 */
			_DBG_ATDMA_CTX(ctx, "Dead ctx re-run: ctrl=$%X\n", ohci_RegRead(unit, CTX_CTRL_SET(regoff)));
		}
		UNLOCK_CTX(ctx);
	}

	/* Process the ok DMA blocks list */
	ForeachNodeSafe(&ctx->atc_DeadBufferList, buf, next)
	{
		OHCI1394ATData *atd;
		UBYTE event;
		BYTE ack;
		UWORD ts, status;

		/* Transfer status decoding */
		status = BE_SWAPWORD(buf->atb_LastDescriptor->d_TransferStatus);
		event = status & 0x1f;

		/* transform DMA events into ack code */
		switch (event)
		{ 
			case OHCI1394_EVENT_MISSING_ACK: /* Packet transmitted -> subaction gap detected or ack parity error */
				ack = HELIOS_ACK_MISSING;
				break;
				
			case OHCI1394_EVENT_FLUSHED:     /* Packet not transmitted (but in DMA-FIFO) -> BusReset */
				ack = HELIOS_ACK_GENERATION;
				break;
				
			case OHCI1394_EVENT_TIMEOUT:     /* (Response packet only) Packet not transmitted, timeout expired */
				ack = HELIOS_ACK_TIMEOUT;
				break;

			/* OHCI ACK code (packet transmitted) */
			case HELIOS_ACK_COMPLETE + 0x10:
			case HELIOS_ACK_PENDING + 0x10:
			case HELIOS_ACK_BUSY_X + 0x10:
			case HELIOS_ACK_BUSY_A + 0x10:
			case HELIOS_ACK_BUSY_B + 0x10:
			case HELIOS_ACK_DATA_ERROR + 0x10:
			case HELIOS_ACK_TYPE_ERROR + 0x10:
				ack = event - 0x10;
				break;
				
			default:
				ack = HELIOS_ACK_OTHER; /* Should not happend with OHCI 1.1 devices */
		}
		
		ts = BE_SWAPWORD(buf->atb_LastDescriptor->d_TimeStamp);

		/* Obtain associated AT packet data if not cancelled */
		LOCK_CTX(ctx);
		{
			atd = buf->atb_PacketData;
			if (NULL != atd)
			{
				atd->atd_Buffer = NULL;
				buf->atb_PacketData = NULL;
			}
			
			/* release buffer if not the last of the current DMA program */
			if (buf != ctx->atc_LastBuffer)
				ohci_ATContext_ReleaseBuffer(ctx, buf);
		}
		UNLOCK_CTX(ctx);
		
		if (NULL != atd)
		{
			_DBG_ATDMA_CTX(ctx, "pd@%p ack'ed: Ack=%d [Evt=$%x (%s), XferSt=$%08x]\n",
				atd, ack, event, evt_strings[event], status);
					
			/* Let upper layers know the ack */
			atd->atd_AckCallback(&unit->hu_TL, ack, ts, atd);
		}
		else
			_DBG_ATDMA_CTX(ctx, "atd@%p ack'ed but was cancelled\n", atd, ack);
	}
}
/*=== AR Context API =========================================================*/
/* WARNING: All AR context functions shall be locked before calling */
static void
ohci_ARContext_Wake(OHCI1394ARCtx *ctx)
{
	OHCI1394Unit *unit = ctx->arc_Context.ctx_Unit;

	ohci_RegWrite(unit, CTX_CTRL_SET(ctx->arc_Context.ctx_RegOffset), CTX_WAKE);
	_DBG_ARDMA_CTX(ctx, "wake-up: Ctrl=%08x\n", ohci_RegRead(unit, CTX_CTRL_SET(ctx->arc_Context.ctx_RegOffset)));
}
static QUADLET *
ohci_ARContext_ParsePacket(
	OHCI1394ARCtx *	ctx,
	OHCI1394Unit *	unit,
	QUADLET *		data)
{
#ifdef WARN_DOUBLE_TL
	static ULONG _prev_tl=-1;
#endif

	HeliosPacket p;
	ULONG len;
	QUADLET trailer;
	UBYTE event;
	BOOL isrequest = ctx == &unit->hu_ARRequestCtx;

	/* Copy and convert to the good endianess the Packet Header */
	p.Header[0] = BE_SWAPLONG(data[0]);
	p.Header[1] = BE_SWAPLONG(data[1]);
	p.Header[2] = BE_SWAPLONG(data[2]);
	
	/* Partial packet decoding based on read TCode */
	/* Notes for quadlet data in read/write quadlet req/resp. tcodes:
	 * BE: no sw byteswap needed, done by HW OHCI FIFO.
	 * LE: PCI is LE, BE-OHCI -> LE-PCI HW convertion used.
	 */
	p.TCode = AT_GET_HEADER_TCODE(p.Header[0]);
	switch (p.TCode)
	{
		case TCODE_READ_QUADLET_REQUEST:
		case TCODE_WRITE_RESPONSE:
		case TCODE_WRITE_PHY: /* For BusReset PHY packet */
			p.HeaderLength = 12;
			p.PayloadLength = 0;
			break;

		case TCODE_READ_BLOCK_REQUEST:
			p.Header[3] = BE_SWAPLONG(data[3]);
		case TCODE_WRITE_QUADLET_REQUEST:
			p.HeaderLength = 16;
			p.PayloadLength = 0;
			break;

		case TCODE_READ_QUADLET_RESPONSE:
			p.QuadletData = p.Header[3] = data[3];
#ifdef WARN_DOUBLE_TL
			if (AT_GET_HEADER_TLABEL(BE_SWAPLONG(data[0])) == _prev_tl)
				dprintf("[WRN] Double TL detected (DestID=$%04x)\n", AT_GET_HEADER_DEST_ID(BE_SWAPLONG(data[0])));
			_prev_tl = AT_GET_HEADER_TLABEL(BE_SWAPLONG(data[0]));
#endif
			p.HeaderLength = 16;
			p.PayloadLength = 0;
			break;

		case TCODE_LOCK_REQUEST:
		case TCODE_WRITE_BLOCK_REQUEST:
		case TCODE_LOCK_RESPONSE:
		case TCODE_READ_BLOCK_RESPONSE:
			p.HeaderLength = 16;
			p.Header[3] = BE_SWAPLONG(data[3]);

			/* LE/BE: data are already HW byteswapped to be read correctly on the LE PCI bus */
			p.Payload = &data[4];
			p.PayloadLength = AT_GET_HEADER_LEN(p.Header[3]);
			break;

		default: /* unknown tcode */
			_ERR_ARDMA_CTX(ctx, "Unsupported TCode received ($%x)\n", p.TCode);
			return NULL; /* return an error */
	}

	
	/* header + payload length (without trailer) rounded to quadlet */
	len = (p.HeaderLength + p.PayloadLength + 3) / 4; 
	_DBG_ARDMA_CTX(ctx, "Packet length: %lu quadlet(s) (%u for header)\n", len, p.HeaderLength/4);

	/* Trailer packet (XFertStatus & TimeStamp) */
	trailer = BE_SWAPLONG(data[len]);
	p.TimeStamp = trailer & 0xffff;
	trailer >>= 16;
	event = (trailer & 0x1f);
	p.Speed = (trailer >> 5) & 7;
	_DBG_ARDMA_CTX(ctx, "Event=$%02x (%s), TS=$%04x, Speed=%u, tcode=%u, payload=$%p (%lu byte(s))\n",
	event, evt_strings[event], p.TimeStamp, p.Speed, p.TCode, p.Payload, p.PayloadLength);

	/* handle event code first */
	if (event == OHCI1394_EVENT_BUS_RESET)
	{
		EXCLUSIVE_PROTECT_BEGIN(unit);
		{
			unit->hu_OHCI_LastBRGeneration = (p.Header[2] >> 16) & 0xff;
			_DBG_ARDMA_CTX(ctx, "BusReset packet received for generation %u\n",
				(p.Header[2] >> 16) & 0xff);
		}
		EXCLUSIVE_PROTECT_END(unit);
	}
	else if (TCODE_WRITE_PHY == p.TCode)
	{
		//_DBG_ARDMA_CTX(ctx, "PHY packet received, data=$%08x\n", p.Header[1]);
		dprintf("PHY packet received, data=$%08x\n", p.Header[1]);
	}
	else /* XXX: other evt_*? Currently considering that everything is ok */
	{
		UBYTE packet_gen;
		BOOL gen_ok;
		
		p.TLabel	= AT_GET_HEADER_TLABEL(p.Header[0]);
		p.DestID	= AT_GET_HEADER_DEST_ID(p.Header[0]);
		p.SourceID	= AT_GET_HEADER_SOURCE_ID(p.Header[1]);
		p.Ack		= event - 16; /* IEEE1394 ack code */

		SHARED_PROTECT_BEGIN(unit);
		{
			/* use the generation recorded in the DMA buffer,
			 * the generation in SelfID may not be the one when the packet has been received
			 * if a busreset has occured between the packet reception and now.
			 */
			packet_gen = unit->hu_OHCI_LastBRGeneration;

			/* Outdated packet (before a busreset packet) ? */
			gen_ok = packet_gen == unit->hu_OHCI_LastGeneration;
			_DBG_ARDMA_CTX(ctx, "pkt gen:%u, ohci gen: %u\n", packet_gen, unit->hu_OHCI_LastGeneration);
		}
		SHARED_PROTECT_END(unit);

		/* drop outdated incomming packets */
		if (!gen_ok) goto bye;

		if (isrequest)
		{
			p.Offset = ((HeliosOffset)(p.Header[1] & 0xffff) << 32) | p.Header[2];
			ohci_TL_HandleRequest(&unit->hu_TL, &p);
		}
		else
		{
			/* Finish the packet decoding */
			p.RCode =  AT_GET_HEADER_RCODE(p.Header[1]);
			ohci_TL_HandleResponse(&unit->hu_TL, &p);
		}
	}

bye:
	/* Return the possible next packet address */
	return data + len + 1;
}
static void
ohci_ARContext_PktHandler(OHCI1394Context *_ctx)
{
	OHCI1394ARCtx *ctx = (APTR)_ctx;
	OHCI1394Unit *unit = _ctx->ctx_Unit;
	OHCI1394ARBuffer *buf;
	UWORD resCount;
	ULONG len;
	APTR start, end;

	/* Take the first page descriptor to see what happened */
	buf = ctx->arc_FirstBuffer;
	start = ctx->arc_FirstQuadlet;
	_DBG_ARDMA_CTX(ctx, "FB: $%p, FQ=$%x\n", buf, start);

	/* Byte-swapping notes: see notes in the ohci_ATContext_Send function.
	 * Note that only descriptor data are byteswapped by the software.
	 * Incoming data from the IEEE1394 bus are byteswapped using
	 * the OHCI hardware FIFO byteswap feature (noByteSwapData HCC flags cleared).
	 */

	log_DumpMem(&buf->arb_Descriptor, sizeof(OHCI1394Descriptor), TRUE, "Dumping AR descriptor #0:\n");

	/* If errors occure during the DMA write into AR buffers, packet is 'backed-out' and
	 * neither XferStatus or resCount are upated.
	 */

	/* amount of remaining space in current receive buffer */
	resCount = BE_SWAPWORD(buf->arb_Descriptor.d_ResCount);

	/* amount of data that can be read on the current buffer */
	len = ARBUFFER_PAGE_SIZE - resCount - ((APTR)ctx->arc_FirstQuadlet - (APTR)buf->arb_Page);
	_DBG_ARDMA_CTX(ctx, "resCount=%lu (already read=%lu)\n", resCount, (APTR)ctx->arc_FirstQuadlet - (APTR)buf->arb_Page);

	/* Split packet may occure in buffer-fill mode when the resCount is 0 (some data may reside
	 * on the next buffer).
	 * As all pages has been allocated as a contiguous buffer, so the split is trivial to handle
	 * except when it occures between the last bytes and the first bytes of this big buffer.
	 * If the global pages buffer is large enough this not occures often and we could do some
	 * more cpu intensive stuffs in this case (like using the current full buffer to merge the packet).
	 */
	if (!resCount)
	{
		BOOL at_end;

		_DBG_ARDMA_CTX(ctx, "buffer full: $%p\n", buf);

		/* Reset this first buffer as its fully filled */
		buf->arb_Descriptor.d_ResCount = BE_SWAPWORD_C(ARBUFFER_PAGE_SIZE);
		buf->arb_Descriptor.d_TransferStatus = 0;

		/* Next buffer as current buffer */
		at_end = buf == ctx->arc_RealLastBuffer;
		if (at_end)
			ctx->arc_FirstBuffer = ctx->arc_CpuDMABuffers;
		else
			ctx->arc_FirstBuffer = buf+1;

		/* Next buffer contains data ? */
		if (ctx->arc_FirstBuffer->arb_Descriptor.d_TransferStatus)
		{
			ULONG rest;

			resCount = BE_SWAPWORD(ctx->arc_FirstBuffer->arb_Descriptor.d_ResCount);
			rest = ARBUFFER_PAGE_SIZE - resCount;

			ctx->arc_FirstQuadlet = (APTR)ctx->arc_FirstBuffer->arb_Page + rest;

			/* Packet split between the end of pages buffer and its start ? */
			if (at_end)
			{
				/* We're going to reuse the current buffer to reassembling the packet */
				_DBG_ARDMA_CTX(ctx, "Split packet detected: len=%lu, rest=%lu\n", len, rest);

				memmove(buf->arb_Page, start, len);
				CopyMem(ctx->arc_FirstBuffer->arb_Page, (APTR)buf->arb_Page + len, rest);

				/* Note: as page size is equals or bigger than the maximal packet size,
				 * our reassembled buffer contains at least one packet and shall not contains
				 * a split packet anymore.
				 */

				start = buf->arb_Page;
			}

			len += rest;
		}
		else
			ctx->arc_FirstQuadlet = ctx->arc_FirstBuffer->arb_Page;

		_DBG_ARDMA_CTX(ctx, "data len=%lu\n", len);
		log_DumpMem(start, len, TRUE, "Dumping AR data:\n");
		end = start + len;
		while (start < end)
		{
			start = ohci_ARContext_ParsePacket(ctx, unit, start);
			
			/* if errors, we drop the rest of data */
			if (NULL == start)
				break;
		}

		_DBG_ARDMA_CTX(ctx, "New FQ: $%p, new FB: $%p\n", ctx->arc_FirstQuadlet, ctx->arc_FirstBuffer);

		/* This full DMA block becomes the last DMA block in the program */
		ctx->arc_LastBuffer->arb_Descriptor.d_BranchAddress |= BE_SWAPLONG_C(1); /* Z=1 */
		_DBG_ARDMA_CTX(ctx, "old last buffer: $%p (z=%u)\n",
						ctx->arc_LastBuffer,
						BE_SWAPLONG(ctx->arc_LastBuffer->arb_Descriptor.d_BranchAddress) & 15);
		buf->arb_Descriptor.d_BranchAddress &= BE_SWAPLONG_C(~15); /* Z=0 */
		ctx->arc_LastBuffer = buf;

		/* Inform the DMA context of this change */
		ohci_ARContext_Wake(ctx);
	}
	else
	{
		/* Note: as page size is equals or bigger than the maximal packet size,
		 * our reassembled buffer contains at least one packet and shall not contains
		 * a split packet anymore.
		 */

		_DBG_ARDMA_CTX(ctx, "data len=%lu\n", len);
		log_DumpMem(start, len, TRUE, "Dumping AR pkt:\n");
		end = start + len;
		while (start < end)
		{
			start = ohci_ARContext_ParsePacket(ctx, unit, start);
				
			/* if errors, we drop the rest of data */
			if (NULL == start)
				break;
		}

		ctx->arc_FirstQuadlet = (APTR)ctx->arc_FirstQuadlet + len;
		_DBG_ARDMA_CTX(ctx, "New FQ: $%p\n", ctx->arc_FirstQuadlet);
	}

	/* Note: after some tests, looping if an AR packet has received during processing
	 * doesn't give better perf.
	 */
}
static ULONG
ohci_ARContext_GetPhyAddress(OHCI1394ARCtx *ctx, APTR ptr)
{
	return ctx->arc_PhyDMABuffers + (ptr - (APTR)ctx->arc_CpuDMABuffers);
}
static void
ohci_ARContext_SetupBuffer(
	OHCI1394ARCtx *		ctx,
	OHCI1394ARBuffer *	buffer,
	OHCI1394ARBuffer *	next,
	ULONG				z)
{
	OHCI1394Descriptor *d = &buffer->arb_Descriptor;
	ULONG phy_page;

	_DBG_ARDMA_CTX(ctx, "buf=%p, next=%p, z=%u\n", buffer, next, z);

	d->d_Control = BE_SWAPWORD_C(0x280c); /* INPUT_MORE + no IntEvent.ARRx */
	phy_page = ohci_ARContext_GetPhyAddress(ctx, buffer->arb_Page);
	d->d_DataAddress = BE_SWAPLONG(phy_page);

	if (NULL != next)
		d->d_BranchAddress = BE_SWAPLONG(ohci_ARContext_GetPhyAddress(ctx, &next->arb_Descriptor) | z);
	else
		d->d_BranchAddress = 0;

	d->d_TransferStatus = 0;
	d->d_ResCount = d->d_ReqCount = BE_SWAPWORD(ARBUFFER_PAGE_SIZE);

	log_DumpMem(d, sizeof(OHCI1394Descriptor), TRUE, "Dumping AR descriptor:\n");
}
static void
ohci_ARContext_Reset(OHCI1394ARCtx *ctx)
{
	OHCI1394ARBuffer *buf, *next;
	ULONG i, z = 1;
	UBYTE *page = (APTR)ctx->arc_Pages;

	ctx->arc_FirstBuffer = buf = ctx->arc_CpuDMABuffers;
	for (i=0; i < ARBUFFER_PAGE_COUNT; i++, buf++)
	{
		if (i < (ARBUFFER_PAGE_COUNT-1))
			next = buf + 1;
		else {
			next = ctx->arc_CpuDMABuffers;
			z = 0;
		}

		buf->arb_Page = (QUADLET *)page;
		page += ARBUFFER_PAGE_SIZE;

		ohci_ARContext_SetupBuffer(ctx, buf, next, z);
	}

	ctx->arc_LastBuffer = ctx->arc_RealLastBuffer;
	ctx->arc_FirstQuadlet = ctx->arc_Pages;
}
static void
ohci_ARContext_Start(OHCI1394ARCtx *ctx)
{
	OHCI1394Unit *unit = ctx->arc_Context.ctx_Unit;
	OHCI1394ARBuffer *buf = ctx->arc_FirstBuffer;
	ULONG bus = ohci_ARContext_GetPhyAddress(ctx, &buf->arb_Descriptor) | 1;
	ULONG regoff = ctx->arc_Context.ctx_RegOffset;
	QUADLET reg;

	/* Already running ? */
	reg = ohci_RegRead(unit, CTX_CTRL_SET(regoff));
	_DBG_ARDMA_CTX(ctx, "ctrl=$%08x\n", reg);
	if (reg & CTX_RUN) return;

	_DBG_ARDMA_CTX(ctx, "Running AR context $%x: CmdPtr: %p\n", bus);

	ohci_RegWrite(unit, CTX_CTRL_CMDPTR(regoff), bus);
	ohci_RegWrite(unit, CTX_CTRL_CLEAR(regoff), ~0);
	ohci_RegWrite(unit, CTX_CTRL_SET(regoff), CTX_RUN);

	/* Note: if the context dead, this cas will be handled later by the DeadIRQ.
	 * So the code should always assumes that its run request is pending.
	 */

	_DBG_ARDMA_CTX(ctx, "ctrl=$%08x\n", ohci_RegRead(unit, CTX_CTRL_SET(regoff)));
}

static BOOL
ohci_ARContext_Init(
	OHCI1394Unit *		unit,
	OHCI1394ARCtx *		ctx,
	ULONG				regoffset,
	STRPTR				task_name)
{
	const static ULONG blocksize = ARBUFFER_PAGE_COUNT * sizeof(OHCI1394ARBuffer);
	const static ULONG pagessize = ARBUFFER_PAGE_COUNT * ARBUFFER_PAGE_SIZE;

	_DBG_UNIT(unit, "AR context $%X: DMA buffers size=%lu bytes, Pages buffer size=%lu bytes\n",
		regoffset, blocksize, pagessize);

	/* Allocate DMA buffers space, 16-bytes aligned */
	ctx->arc_AllocDMABuffers = AllocVecDMA(blocksize + 15, MEMF_PUBLIC);
	if (NULL == ctx->arc_AllocDMABuffers)
	{
		_ERR_UNIT(unit, "AT-DMA buffer allocation failed\n");
		return FALSE;
	}

	ctx->arc_CpuDMABuffers = (APTR)(16 + (((ULONG)ctx->arc_AllocDMABuffers - 1) & ~15));
	ctx->arc_PhyDMABuffers = (ULONG)PCIXDMAGetPhysical(unit->hu_PCI_BoardObject, ctx->arc_CpuDMABuffers);
	_DBG_UNIT(unit, "AR-DMA buffers: cpu@%p, phy@%p\n", ctx->arc_CpuDMABuffers, ctx->arc_PhyDMABuffers);

	/* Alloc pages buffer */
	ctx->arc_Pages = AllocVecDMA(pagessize, MEMF_PUBLIC);
	if (NULL == ctx->arc_Pages)
	{
		_ERR_UNIT(unit, "AT-DMA pages allocation failed\n");
		FreeVecDMA(ctx->arc_AllocDMABuffers);
		return FALSE;
	}

	_DBG_UNIT(unit, "AR-DMA pages buffer: %p\n", ctx->arc_Pages);

	if (!ohci_Context_Init(unit, &ctx->arc_Context, regoffset,
						   task_name, ohci_ARContext_PktHandler,
						   TASK_PRIO_ARCTX))
	{
		FreeVecDMA(ctx->arc_AllocDMABuffers);
		FreeVecDMA(ctx->arc_Pages);
		return FALSE;
	}

	/* Reset AR context descriptors */
	ctx->arc_RealLastBuffer = &ctx->arc_CpuDMABuffers[ARBUFFER_PAGE_COUNT-1];
	ohci_ARContext_Reset(ctx);

	return TRUE;
}
static void
ohci_ARContext_Term(OHCI1394ARCtx *ctx)
{
	LOCK_CTX(ctx);
	ohci_Context_Stop(&ctx->arc_Context);
	UNLOCK_CTX(ctx);

	ohci_Context_Term(&ctx->arc_Context);
	FreeVecDMA(ctx->arc_AllocDMABuffers);
	FreeVecDMA(ctx->arc_Pages);
}
static void
ohci_ARContexts_Start(OHCI1394Unit *unit)
{
	LOCK_CTX(&unit->hu_ARRequestCtx);
	ohci_ARContext_Start(&unit->hu_ARRequestCtx);
	UNLOCK_CTX(&unit->hu_ARRequestCtx);
	
	LOCK_CTX(&unit->hu_ARResponseCtx);
	ohci_ARContext_Start(&unit->hu_ARResponseCtx);
	UNLOCK_CTX(&unit->hu_ARResponseCtx);
}
static void
ohci_ARContexts_Stop(OHCI1394Unit *unit)
{
	LOCK_CTX(&unit->hu_ARRequestCtx);
	ohci_Context_Stop(&unit->hu_ARRequestCtx.arc_Context);
	UNLOCK_CTX(&unit->hu_ARRequestCtx);

	LOCK_CTX(&unit->hu_ARResponseCtx);
	ohci_Context_Stop(&unit->hu_ARResponseCtx.arc_Context);
	UNLOCK_CTX(&unit->hu_ARResponseCtx);
}
static BOOL
ohci_ARContexts_Init(OHCI1394Unit *unit)
{
	if (ohci_ARContext_Init(unit, &unit->hu_ARRequestCtx,
							OHCI1394_REG_AREQR_CONTEXT_CONTROL,
							"["DEVNAME"] AR Requests Handler")) {
		_DBG_UNIT(unit, "AR request context %p created!\n", &unit->hu_ARRequestCtx);
		if (ohci_ARContext_Init(unit, &unit->hu_ARResponseCtx,
								OHCI1394_REG_ARESR_CONTEXT_CONTROL,
								"["DEVNAME"] AR Responses Handler")) {
			_DBG_UNIT(unit, "AR response context %p created!\n", &unit->hu_ARResponseCtx);
			return TRUE;
		} else
			_ERR_UNIT(unit, "AR response context creation failed\n");

		ohci_ARContext_Term(&unit->hu_ARRequestCtx);
	} else
		_ERR_UNIT(unit, "AR request context creation failed\n");

	return FALSE;
}
static void
ohci_ARContexts_Term(OHCI1394Unit *unit)
{
	ohci_ARContext_Term(&unit->hu_ARRequestCtx);
	ohci_ARContext_Term(&unit->hu_ARResponseCtx);
}
/*=== Bus Reset handling =====================================================*/
static void
ohci_HandleSelfIDComplete(OHCI1394Unit *unit)
{
	QUADLET q;
	ULONG packets_nbr;
	ULONG i, j, nodeid, gen;

	/* The allocated SelfID buffer given to the OHCI hardware
	 * is not cached and no global arbitration mechanism exists
	 * to protect a change of this buffer during its parsing.
	 * The parsing process takes this in account by validating
	 * the resulted tree when all the SelfID stream is read.
	 */

	/* 1) Identification phase:               */
	/* => Read and check Phy ID of local node */

	/* Read the NodeID register and check if it's valid */
	q = ohci_RegRead(unit, OHCI1394_REG_NODE_ID);
	if (0 == (q & OHCI1394_NODEIDF_IDVALID))
	{
		_WRN_UNIT(unit, "BusReset in progress, try later.\n");
		return;
	}

	/* XXX: should be needed to check that the bus number
	 * is not 0x3ff (bus reset phase)?
	 */
	nodeid = OHCI1394_NODEID_NODENUMBER(q);
	if (63 == nodeid) {
		_ERR_UNIT(unit, "Bad node id number\n");
		return;
	}

	_DBG_UNIT(unit, "Local NodeId: %lu\n", nodeid);
	
	/* Report bus reset event with generation as data */
	q = OHCI1394_SELFIDCOUNT_SELFIDGENERATION(q);
	Helios_SendEvent(&unit->hu_Listeners, HEVTF_HARDWARE_BUSRESET, q);
	
	/* 2) SelfID's stream handling phase */
	/* => parse SelfID data linearly */
	
	/* Check for errors and obtain the number of packets written */
	q = ohci_RegRead(unit, OHCI1394_REG_SELFID_COUNT);
	_DBG("SelfID Count reg = 0x%08X\n", q);
	if (q & OHCI1394_SELFIDCOUNTF_SELFIDERROR)
	{
		_ERR_UNIT(unit, "Bad self id count\n");
		return;
	}
	
	/* 8 bytes/pkt: 2 quadlets per packets and 4 bytes per quadlets.
	 * limited to 256 packets.
	 * Note: First quadlet of SelfID buffer contains generation number.
	 */
	packets_nbr = (q >> 3) & 0xff;
	if (!packets_nbr)
	{
		_WRN_UNIT(unit, "empty SelfID stream\n");
		return;
	}
	
	unit->hu_SelfIdPktCnt = packets_nbr;

	/* OHCI 1.1 § 11.3: multiple bus reset can occures, so multiple SelfID generation
	 * sessions can occure also. So to be sure of coherency of data,
	 * the strict order of operations is the following:
	 *  1 - read the SelfID generation counter inside the stream
	 *  2 - read the rest of SelfID packet data
	 *  3 - read the SelfID count register
	 *
	 * Packet data have a sense only if the generation counter in the stream is the same as
	 * in the count register.
	 *
	 * Note: as this stack provide Isochronous Resource Manager and Bus Manager features,
	 * during the Seld-ID scan all IEEE 1394a-1995 § 8.4.2.3 requirements shall be checked.
	 * If anyone is not satisfied, a bus reset is raised.
	 */
				
	_DBG_UNIT(unit, "Reading %u SelfID packets (%u quadlet(s))...\n",
		packets_nbr, (q >> 2) & 0x1ff);

	/* Step 1 - Get the generation of the stream */

	BE_SWAPLONG_P(&unit->hu_SelfIdBufferCpu[0]);
	gen = (unit->hu_SelfIdBufferCpu[0] >> 16) & 0xff;
	_DBG_UNIT(unit, "SelfID stream generation: %u (Pkt#0 = 0x%08X)\n",
		gen, unit->hu_SelfIdBufferCpu[0]);

	/* Step 2 - Checking data integrity of the self-IDs stream and cache the result */

	for (i=1, j=0; j < packets_nbr; i+=2, j++)
	{
		q = unit->hu_SelfIdBufferCpu[i];
		if (q != ~unit->hu_SelfIdBufferCpu[i+1])
		{
			_ERR_UNIT(unit, "SelfID stream data error: 0x%08x != 0x%08x (packet #%u)",
				q, ~unit->hu_SelfIdBufferCpu[i+1], j);
			goto raise_bus_reset;
		}
		unit->hu_SelfIdArray[j] = BE_SWAPLONG(q);
	}
				
	/* Step 3 - Validate our cached copy (= no generation change) */

	q = ohci_RegRead(unit, OHCI1394_REG_SELFID_COUNT);
	q = OHCI1394_SELFIDCOUNT_SELFIDGENERATION(q);
	if (gen != q)
	{
		_ERR_UNIT(unit, "Incoherent generation numbers: stream=%u, PHY=%u", gen, q);
		goto raise_bus_reset;
	}

	/* Our SelfIDs buffer is now consistent,
	 * stop async contexts, clear BusRest interrupt bit
	 */

	EXCLUSIVE_PROTECT_BEGIN(unit);
	{
		unit->hu_OHCI_LastGeneration = gen;
		unit->hu_LocalNodeId = nodeid | HELIOS_LOCAL_BUS; /* Local bus ID (0x3ff) enforced */

		/* Stop AT contexts (see OHCI 1.1 § 7.2.3.2) before clearing BusReset signal */
		ohci_ATContexts_Stop(unit);

		/* Clear BusResetDone interrupt bit */
		ohci_RegWrite(unit, OHCI1394_REG_INT_EVENT_CLEAR, OHCI1394_INTF_BUSRESET);

		/* Swap ROM data buffers in case of update requested.
		 * Free the old one.
		 */
		if (unit->hu_NextROMData)
		{
			if (unit->hu_NextROMData != unit->hu_ROMData)
				Helios_FreeROM(unit->hu_MemPool, unit->hu_ROMData);

			unit->hu_ROMData = unit->hu_NextROMData;
			unit->hu_NextROMData = NULL;

			ohci_RegWrite(unit, OHCI1394_REG_CONFIG_ROM_HDR, unit->hu_ROMData[0]);
			ohci_RegWrite(unit, OHCI1394_REG_BUS_OPTIONS, unit->hu_ROMData[2]);

			/* Readback bus options register and update our ROM buffer
			 * (in case of forced bits)
			 */
			q = ohci_RegRead(unit, OHCI1394_REG_BUS_OPTIONS);
			unit->hu_ROMData[2] = unit->hu_BusOptions.quadlet = q;
		}

		ohci_RegWrite(unit, OHCI1394_REG_PHYREQ_REQ_FILTER_HI_SET, ~0);
		ohci_RegWrite(unit, OHCI1394_REG_PHYREQ_REQ_FILTER_LO_SET, ~0);

		/* Flush pending transactions */
		ohci_TL_FlushAll(&unit->hu_TL);
	}
	EXCLUSIVE_PROTECT_END(unit);
	
	log_SelfIDs(nodeid, gen, packets_nbr, unit->hu_SelfIdArray);

	/* SelfID contents and nodes topology construction
	 * are not normalized by the OHCI itself.
	 * The job is done, let upper layers play.
	 */
	Helios_SendEvent(&unit->hu_Listeners, HEVTF_HARDWARE_SELFID, gen);
	return;

raise_bus_reset: /* Error => Short reset */
	_ERR_UNIT(unit, "SelfID parsing error: raising short bus reset...\n");
	if (!ohci_WritePHY(unit, 5, 0, PHYF_SHORT_BUS_RESET))
	{
		/* Gash, it's worst! Put the unit in UnrecoverableError state */
		ohci_OnUnrecoverableError(unit, "Cannot raise a bus reset!");
	}
}
static void
ohci_BusResetTask(HeliosSubTask *subtask, struct TagItem *tags)
{
	OHCI1394Unit *unit;
	struct MsgPort *taskport;
	ULONG sigbit, sigset;
	BOOL run;

	taskport = (APTR) GetTagData(HA_MsgPort, 0, tags);
	unit = (APTR) GetTagData(HA_UserData, 0, tags);
	
	_DBG("BusResetTask: taskport: %p, unit: %p\n", taskport, unit);
	if (!taskport || !unit)
	{
		_ERR("Badly called\n");
		return;
	}

	sigbit = AllocSignal(-1);
	if (sigbit < 0)
	{
		_ERR("AllocSignal() failed\n");
		return;
	}
	
	unit->hu_BusResetSignal = 1 << sigbit;

	sigset = (1 << taskport->mp_SigBit) | unit->hu_BusResetSignal;
	run = TRUE;
	while (run)
	{
		HeliosMsg *msg;
		ULONG sigs;

		sigs = Wait(sigset);
		
		while (NULL != (msg = (APTR)GetMsg(taskport)))
		{
			switch (msg->hm_Type) {
				case HELIOS_MSGTYPE_DIE:
					_WRN_UNIT(unit, "die msg rcv\n");
					FreeMem(msg, msg->hm_Msg.mn_Length);
					run = FALSE;
					continue;
					
				case HELIOS_MSGTYPE_INIT:
					_DBG_UNIT(unit, "BusResetTask ready\n");
					break;
			}

			ReplyMsg((struct Message *) msg);
		}

		if (run)
		{
			if (sigs & unit->hu_BusResetSignal)
				ohci_HandleSelfIDComplete(unit);
		}
	}

	FreeSignal(sigbit);
}
/*=== SplitTimeout Task ======================================================*/
static LONG
ohci_SplitTimeoutTask(HeliosSubTask *subtask, struct TagItem *tags)
{
	OHCI1394Unit *unit;
	struct MsgPort *taskport, *timerport;
	
	taskport = (APTR) GetTagData(HA_MsgPort, 0, tags);
	unit = (APTR) GetTagData(HA_UserData, 0, tags);

	if (taskport && unit)
	{
		timerport = CreateMsgPort();
		if (timerport)
		{
			unit->hu_SplitTimeoutIOReq = Helios_OpenTimer(timerport, UNIT_MICROHZ);
			if (unit->hu_SplitTimeoutIOReq)
			{
				BOOL run = TRUE;
				ULONG timerPortSignal = (1ul << timerport->mp_SigBit);
				ULONG sigset = (1 << taskport->mp_SigBit) | timerPortSignal;
				
				do
				{
					HeliosMsg *msg;
					ULONG sigs;

					sigs = Wait(sigset);

					/* Process task msg first */
					while (NULL != (msg = (APTR) GetMsg(taskport)))
					{
						switch (msg->hm_Type)
						{
							
							case HELIOS_MSGTYPE_INIT:
								_DBG("STO_Task-%p: init msg rcv\n", FindTask(NULL));
								break;
								
							case HELIOS_MSGTYPE_DIE:
								_WRN("STO_Task-%p: die msg rcv\n", FindTask(NULL));
								run = FALSE;
								FreeMem(msg, msg->hm_Msg.mn_Length);
								continue;
						}

						ReplyMsg((struct Message *) msg);
					}

					/* Flush transactions in SplitTimeout MsgPort */
					if (sigs & timerPortSignal)
					{
						OHCI1394Transaction *t;
						
						while (NULL != (t = (OHCI1394Transaction *) GetMsg(timerport)))
							ohci_TL_Finish(&unit->hu_TL, t, HELIOS_RCODE_TIMEOUT);
					}
				}
				while (run);

				Helios_CloseTimer(unit->hu_SplitTimeoutIOReq);
			}
			else
				_ERR_UNIT(unit, "Failed to open timer for SPLIT-TIMEOUT\n");
			
			DeleteMsgPort(timerport);
		}
		else
			_ERR_UNIT(unit, "Failed to create SPLIT-TIMEOUT TimerPort\n");
	}
	else
		_ERR("Invalid tags values\n");

	return 0;
}

/*=== Local requests API =====================================================*/
static BYTE
ohci_HandleLocalROMRequest(
	OHCI1394Unit *unit,
	ULONG csr,
	HeliosPacket *req,
	HeliosPacket *resp)
{
	ULONG csrIdx = csr - CSR_CONFIG_ROM_OFFSET*sizeof(QUADLET);
	BYTE rcode;

	if (csr > CSR_CONFIG_ROM_END*sizeof(QUADLET))
		rcode = HELIOS_RCODE_ADDRESS_ERROR;
	else if ((req->TCode != TCODE_READ_QUADLET_REQUEST) &&
			 (req->TCode != TCODE_READ_BLOCK_REQUEST))
		rcode = HELIOS_RCODE_TYPE_ERROR;
	else
	{
		rcode = HELIOS_RCODE_COMPLETE;

		if (req->TCode == TCODE_READ_QUADLET_REQUEST)
		{
			resp->Header[3] = resp->QuadletData = unit->hu_ROMData[csrIdx/4];
			_DBG_UNIT(unit, "ROM[%u]=0x%08X\n", csrIdx, resp->QuadletData);
		}
		else
		{
			resp->Payload = &unit->hu_ROMData[csrIdx/4];
			resp->PayloadLength = MIN(req->PayloadLength, CSR_CONFIG_ROM_SIZE*sizeof(QUADLET)-csrIdx);
			
			_DBG_UNIT(unit, "ROM[%u]: payload=%p (len=%u)\n",
				csrIdx, rcode, resp->Payload, resp->PayloadLength);
		}
	}

	return rcode;
}
static BYTE
ohci_HandleLocalCSRLock(
	OHCI1394Unit *unit,
	ULONG csr,
	HeliosPacket *req,
	HeliosPacket *resp)
{
	QUADLET data, arg=0;

	resp->ExtTCode = req->ExtTCode;

	_DBG_UNIT(unit, "csr=$%x, extcode=%u\n", csr, req->ExtTCode);

	if ((TCODE_LOCK_REQUEST == req->TCode) &&
		(EXTCODE_COMPARE_SWAP == req->ExtTCode))
	{
		resp->PayloadLength = req->PayloadLength;
		if (resp->PayloadLength == 8)
			resp->Payload = req->Payload;
		else
			return HELIOS_RCODE_DATA_ERROR;

		arg  = req->Payload[0];
		data = req->Payload[1];
	}
	else if (TCODE_READ_QUADLET_REQUEST == req->TCode)
		data = arg;
	else
		return HELIOS_RCODE_TYPE_ERROR;

	if (CSR_BUS_MANAGER_ID == csr)
	{
		data = OHCI1394_NODEID_NODENUMBER(data);
		arg = OHCI1394_NODEID_NODENUMBER(arg);
	}
	else if (CSR_BANDWIDTH_AVAILABLE == csr)
	{
		data &= 0x1fff;
		arg &= 0x1fff;
	}

	/* Swap register to data if old value == compare */
	_DBG_UNIT(unit, "Lock data: data=$%x, arg=$%x, sel=%u\n", data, arg, (csr - CSR_BUS_MANAGER_ID) / 4);
	ohci_RegWrite(unit, OHCI1394_REG_CSR_WRITE_DATA,   data);
	ohci_RegWrite(unit, OHCI1394_REG_CSR_COMPARE_DATA, arg);
	ohci_RegWrite(unit, OHCI1394_REG_CSR_CONTROL,      (csr - CSR_BUS_MANAGER_ID) / 4);

	while (0 == (ohci_RegRead(unit, OHCI1394_REG_CSR_CONTROL) & OHCI1394_CSRCTRLF_DONE))
		Helios_DelayMS(5);

	data = ohci_RegRead(unit, OHCI1394_REG_CSR_READ_DATA);
	_DBG_UNIT(unit, "Lock old data=$%x\n", data);

	if (TCODE_READ_QUADLET_REQUEST == req->TCode)
		resp->QuadletData = data;
	else
	{
		resp->Payload[0] = data;
		resp->PayloadLength = sizeof(QUADLET);
	}
	
	return HELIOS_RCODE_COMPLETE;
}
#if 0
/*--- Isochrone API ---*/
static void ohci_IRContext_Run(OHCI1394IRCtx *ctx)
{
	_INFO_IRDMA_CTX(ctx, "[%u]: run (CmdPtr=$%08x)\n", ctx->irc_Base.ic_Index,
					ohci_RegRead(ctx->irc_Base.ic_Context.ctx_Unit, OHCI1394_REG_IRECV_COMMAND_PTR(ctx->irc_Base.ic_Index)));

	ohci_RegWrite(ctx->irc_Base.ic_Context.ctx_Unit,
				  OHCI1394_REG_IRECV_CONTEXT_CONTROL_SET(ctx->irc_Base.ic_Index),
				  CTX_RUN);
}
static void ohci_IRContext_Wake(OHCI1394IRCtx *ctx)
{
	ohci_RegWrite(ctx->irc_Base.ic_Context.ctx_Unit,
				  OHCI1394_REG_IRECV_CONTEXT_CONTROL_SET(ctx->irc_Base.ic_Index),
				  CTX_WAKE);
}
static ULONG ohci_IRContext_PacketPerBuffer_InitDMABuffers(OHCI1394IRCtx *ctx, ULONG buf_size)
{
	OHCI1394Unit *unit = ctx->irc_Base.ic_Context.ctx_Unit;
	UWORD dma_payload_size = BE_SWAPWORD(ctx->irc_PayloadLength);
	UWORD dma_header_size = BE_SWAPWORD(ctx->irc_HeaderLength);
	ULONG i, cmd_ptr, dma_phy, page_phy;
	APTR page = ctx->irc_AlignedPageBuffer;
	OHCI1394IRBuffer *buf;

	/* cmd_ptr should be 16-bytes aligned */
	cmd_ptr = dma_phy = (ULONG)PCIXDMAGetPhysical(unit->hu_PCI_BoardObject, ctx->irc_AlignedDMABuffer);
	page_phy =  (ULONG)PCIXDMAGetPhysical(unit->hu_PCI_BoardObject, page);

	ctx->irc_FirstDMABuffer = buf = ctx->irc_AlignedDMABuffer;

	_INFO_IRDMA_CTX(ctx, "BufAddr: DMA=%p, Pages=%p\n", ctx->irc_AlignedDMABuffer, page);
	_INFO_IRDMA_CTX(ctx, "PhyAddr: DMA=%p, Pages=[%p, %p]\n", dma_phy, page_phy, page_phy+buf_size*ctx->irc_DMABufferCount);

	/* Initialize all descriptors, 2 descriptors per DMA block.
	 * These 2 descriptors split one data buffer into 2 part: header (hlen) and data.
	 */
	for (i=0; i<ctx->irc_DMABufferCount; i++, page+=buf_size, page_phy+=buf_size, buf++)
	{
		OHCI1394Descriptor *d = buf->irb_Descriptors;
		OHCI1394Descriptor *dn;
		ULONG z = 2;

		buf->irb_BufferData.Header = page;
		buf->irb_BufferData.Payload = page + ctx->irc_HeaderLength + ctx->irc_HeaderPadding;

		if (i < (ctx->irc_DMABufferCount-1))
		{
			buf->irb_Next = buf + 1;
			dn = &buf[1].irb_Descriptors[0];
		}
		else
		{
			ctx->irc_LastDMABuffer = ctx->irc_RealLastDMABuffer = buf;
			buf->irb_Next = ctx->irc_FirstDMABuffer;
			dn = &ctx->irc_FirstDMABuffer->irb_Descriptors[0];
			z = 0;
		}

		/* First descriptor (header data) */
		d[0].d_Control = BE_SWAPWORD_C(DESCRIPTOR_INPUT_MORE | DESCRIPTOR_STATUS | DESCRIPTOR_NO_IRQ);
		d[0].d_ReqCount = d[0].d_ResCount = dma_header_size;
		d[0].d_DataAddress = BE_SWAPLONG(page_phy);
		d[0].d_BranchAddress = 0; /* Not used */
		d[0].d_TransferStatus = 0;

		/* Last descriptor (stream data) */
		d[1].d_Control = BE_SWAPWORD_C(DESCRIPTOR_INPUT_LAST | DESCRIPTOR_STATUS | DESCRIPTOR_IRQ_ALWAYS | DESCRIPTOR_BRANCH_ALWAYS);
		d[1].d_ReqCount = d[1].d_ResCount = dma_payload_size;
		d[1].d_DataAddress = BE_SWAPLONG(page_phy + ctx->irc_HeaderLength + ctx->irc_HeaderPadding);
		d[1].d_BranchAddress = BE_SWAPLONG(dma_phy + ((APTR) dn - (APTR) ctx->irc_AlignedDMABuffer) + z);
		d[1].d_TransferStatus = 0;
	}

	return cmd_ptr;
}
static void ohci_IRContext_PacketPerBufferCallback(OHCI1394Context *_ctx)
{
	OHCI1394IRCtx *ctx = (OHCI1394IRCtx *)_ctx;
	register OHCI1394IRBuffer *buf, *last;

	/* Scan desciptors status bits from first to last.
	 *
	 * Each completed buffer triggers the user callback call.
	 * If a status reports an error, the callback is called with a zero length.
	 * In this case, it's up to the user to reset the iso context.
	 *
	 * If all desciptors are completed, the DMA RUN bit should be zero, so we reset the CommandPtr
	 * register to the first descriptor, let the Z=0 on the last one and finally running the context again.
	 *
	 * If only a part of desciptors are filled, we can change the last filled buffer Z value to 0,
	 * set  the current last desciptor Z value to 2 and wake-up the context.
	 */

	/* for buffer from FirstDMABuffer to LastDMABuffer */
	last = buf = ctx->irc_FirstDMABuffer;
	do
	{
		register ULONG status;
		register ULONG event;

		/* 1 Buffer = 2 descriptors (HEADER + PAYLOAD), but only one is set, the other is zero */
		status = BE_SWAPWORD(buf->irb_Descriptors[0].d_TransferStatus | buf->irb_Descriptors[1].d_TransferStatus);

		/* No events ? */
		if (0 == status)
		{
			/* Something received ? */
			if (buf != ctx->irc_FirstDMABuffer)
			{
				/* DMA program update phase.
				 * Use the last filled buffer as the new last buffer (Z=0), then update last buffer Z value to 2.
				 */
				ctx->irc_FirstDMABuffer = buf;
				last->irb_Descriptors[1].d_BranchAddress &= BE_SWAPLONG_C(~15); /* Z=0 */
				ctx->irc_LastDMABuffer->irb_Descriptors[1].d_BranchAddress |= BE_SWAPLONG_C(2); /* Z= 2 */
				ctx->irc_LastDMABuffer = last;

				/* All data read and DMA program is ok, wake-up the context */
				ohci_IRContext_Wake(ctx);
			}

			return;
		}

		/* Checking for event errors, if not compute filled data sizes, else 0 them */
		event = status & 0xff;
		if ((0 == (CTX_ACTIVE & status)) ||
			((OHCI1394_EVENT_ACK_COMPLETE != event) && (OHCI1394_EVENT_LONG_PACKET != event)))
		{
			static const HeliosIRBuffer empty_buf = {0,0,0,0};

			_ERR_IRDMA_CTX(ctx, "Event error in IR context %u (stopped): $%02x-'%s'",
						   ctx->irc_Base.ic_Index, event, evt_strings[event]);

			ctx->irc_Callback((HeliosIRBuffer *)&empty_buf, event, ctx->irc_UserData);

			/* stop and flush the context */
			ohci_IRContext_Stop(ctx);
			//ohci_IRContext_Flush(ctx);

			return;
		}

		buf->irb_BufferData.HeaderLength = ctx->irc_HeaderLength;

		if (0 != buf->irb_Descriptors[0].d_TransferStatus)
		{
			buf->irb_BufferData.HeaderLength -= BE_SWAPWORD(buf->irb_Descriptors[0].d_ResCount);
			buf->irb_BufferData.PayloadLength = 0;
			buf->irb_Descriptors[0].d_ResCount = buf->irb_Descriptors[0].d_ReqCount;
			buf->irb_Descriptors[0].d_TransferStatus = 0;
		}
		else
		{
			buf->irb_BufferData.PayloadLength = ctx->irc_PayloadLength - BE_SWAPWORD(buf->irb_Descriptors[1].d_ResCount);
			buf->irb_Descriptors[1].d_ResCount = buf->irb_Descriptors[1].d_ReqCount;
			buf->irb_Descriptors[1].d_TransferStatus = 0;
		}

		/* Call user callback if required */
		if ((buf->irb_BufferData.PayloadLength > 0) || !ctx->irc_Flags.DropEmpty)
			ctx->irc_Callback(&buf->irb_BufferData, event, ctx->irc_UserData);

		last = buf; /* Record the last filled buffer */

		/* Next buffer in DMA program */
		buf = buf->irb_Next;
	} while (buf != ctx->irc_FirstDMABuffer);

	/* if buf != FirstBuffer, buf status is 0 and this buffer is the new first buffer.
	 * if buf == FirstBuffer, all buffers have been filled, the DMA context is stop and maybe some data has been lost
	 * as not more descriptors are available => we restart the ctx.
	 */

	dprintf("IR #%u buffers full! (reset and run)", ctx->irc_Base.ic_Index);

	/* Protected because someone else can call the stop function */
	LOCK_CTX(ctx);
	{
		//ohci_IRContext_Flush(ctx);
		//ohci_IRContext_Run(ctx);
	}
	UNLOCK_CTX(ctx);
}
static void ohci_ITContext_Run(OHCI1394ITCtx *ctx)
{
	_INFO_ITDMA_CTX(ctx, "[%u]: run (CmdPtr=$%08x)\n", ctx->itc_Base.ic_Index,
					ohci_RegRead(ctx->itc_Base.ic_Context.ctx_Unit, OHCI1394_REG_IXMIT_COMMAND_PTR(ctx->itc_Base.ic_Index)));

	ohci_RegWrite(ctx->itc_Base.ic_Context.ctx_Unit,
				  OHCI1394_REG_IXMIT_CONTEXT_CONTROL_SET(ctx->itc_Base.ic_Index),
				  CTX_RUN);
}
static void ohci_ITContext_Wake(OHCI1394ITCtx *ctx)
{
	ohci_RegWrite(ctx->itc_Base.ic_Context.ctx_Unit,
				  OHCI1394_REG_IXMIT_CONTEXT_CONTROL_SET(ctx->itc_Base.ic_Index),
				  CTX_WAKE);
}
static BOOL ohci_ITContext_Stop(OHCI1394ITCtx *ctx)
{
	OHCI1394Unit *unit = ctx->itc_Base.ic_Context.ctx_Unit;
	ULONG loop = 20;
	ULONG index_mask = 1ul << ctx->itc_Base.ic_Index;

	/* Disable interruption line */
	ohci_RegWrite(unit, OHCI1394_REG_ISO_XMIT_INT_MASK_CLEAR, index_mask);

	/* Request the DMA stop of context */
	ohci_RegWrite(unit, OHCI1394_REG_IXMIT_CONTEXT_CONTROL_CLEAR(ctx->itc_Base.ic_Index), CTX_RUN);

	/* Wait about DMA safe state */
	do
	{
		QUADLET reg;

		reg = ohci_RegRead(unit, OHCI1394_REG_IXMIT_CONTEXT_CONTROL(ctx->itc_Base.ic_Index));
		if (0 == (reg & CTX_ACTIVE))
			return TRUE;

		_INFO_CTX(&ctx->itc_Base.ic_Context, "IT context #%u still active\n", ctx->itc_Base.ic_Index);
		Helios_DelayMS(25); // wait 25 ms
	} while (loop--);

	/* Timeout */
	_INFO_CTX(&ctx->itc_Base.ic_Context,
			  "TIMEOUT, IR context #%u still active 500ms after STOP request\n",
			  ctx->itc_Base.ic_Index);

	return FALSE;
}
static void ohci_IRContext_Append(OHCI1394IRCtx *ctx, ULONG cmd_ptr)
{
	OHCI1394Unit *unit = ctx->irc_Base.ic_Context.ctx_Unit;

	LOCK_REGION(unit);
	{
		/* Clear the iso context config */
		_INFO_IRDMA_CTX(ctx, "[%u]: Clearing context %p CTRL\n", ctx->irc_Base.ic_Index);
		ohci_RegWrite(unit, OHCI1394_REG_IRECV_CONTEXT_CONTROL_CLEAR(ctx->irc_Base.ic_Index), ~0);

		/* Setup the Iso CommandPtr register on the first descriptor */
		_INFO_IRDMA_CTX(ctx, "[%u]: Setting CommandPtr of context %p with value $%08x\n",
						ctx->irc_Base.ic_Index, cmd_ptr | 2);
		ohci_RegWrite(unit, OHCI1394_REG_IRECV_COMMAND_PTR(ctx->irc_Base.ic_Index), cmd_ptr | 2);

		/* Register to unit */
		ADDHEAD(&unit->hu_IRCtxList, ctx);
	}
	UNLOCK_REGION(unit);
}
static LONG ohci_AllocIsoCtx(OHCI1394Unit *unit, ULONG type, LONG index)
{
	LONG max;

	if (HELIOS_ISO_RX_CTX == type)
		max = unit->hu_MaxIsoReceiveCtx;
	else
		max = unit->hu_MaxIsoTransmitCtx;

	if (index >= max)
	{
		_ERR("Out bound ISO Context requested (%u), max=%u\n", index, max);
		return -1;
	}

	LOCK_REGION(unit);
	{
		ULONG mask;

		/* Get current reservation mask */
		if (HELIOS_ISO_RX_CTX == type)
			mask = unit->hu_IRCtxMask;
		else
			mask = unit->hu_ITCtxMask;

		/* Next available? */
		if (index < 0)
		{
			ULONG i;
			
			index = -1;
			for (i=0; i<32; i++)
			{
				if (0 != (mask & (1ul << i)))
				{
					index = i;
					mask &= ~(1ul << i);
					break;
				}
			}
		}
		else
		{
			/* free? */
			if (0 == (mask & (1ul << index)))
				mask |= 1ul << index;
			else
				index = -1;
		}

		if (index >= 0)
		{
			/* Save the new reservation mask */
			if (HELIOS_ISO_RX_CTX == type)
				unit->hu_IRCtxMask = mask;
			else
				unit->hu_ITCtxMask = mask;
		}
		else
			_ERR_UNIT(unit, "No free iso contexts (requested=%d)\n", index);
	}
	UNLOCK_REGION(unit);

	return index;
}
static void ohci_FreeIsoCtx(OHCI1394Unit *unit, ULONG type, ULONG index)
{
	LOCK_REGION(unit);
	{
		_INFO_UNIT(unit, "Freeing iso ctx %p (#%ld)\n", index);

		if (HELIOS_ISO_RX_CTX == type)
			unit->hu_IRCtxMask |= 1ul << index;
		else
			unit->hu_ITCtxMask |= 1ul << index;
	}
	UNLOCK_REGION(unit);
}
static void ohci_IsoCtx_Remove(OHCI1394Context *ctx)
{
	LOCK_REGION(ctx->ctx_Unit);
	REMOVE(ctx);
	UNLOCK_REGION(ctx->ctx_Unit);
}
static void ohci_IsoCtx_StopAll(OHCI1394Unit *unit)
{
	LOCK_REGION_SHARED(unit);
	{
		OHCI1394IRCtx *ir_ctx;
		OHCI1394ITCtx *it_ctx;

		ForeachNode(&unit->hu_IRCtxList, ir_ctx)
			ohci_IRContext_Stop(ir_ctx);

		ForeachNode(&unit->hu_ITCtxList, it_ctx)
			ohci_ITContext_Stop(it_ctx);
	}
	UNLOCK_REGION_SHARED(unit);
}
static void ohci_CheckAndScheduleIsoList(struct MinList *list, ULONG events)
{
	OHCI1394IsoCtxBase *ctx;

	/* WARNING: unprotected list access */
	ForeachNode(list, ctx)
	{
		if (events & (1 << ctx->ic_Index))
			Helios_SignalSubTask(ctx->ic_Context.ctx_SubTask, ctx->ic_Context.ctx_Signal);
	}
}
#endif
/*============================================================================*/
/*--- PUBLIC API CODE --------------------------------------------------------*/
/*============================================================================*/
/*=== IO API =================================================================*/
void
ohci_CancelATProcess(OHCI1394ATData *atd)
{
	/* Really send? */
	if (atd->atd_Ctx)
	{
		LOCK_CTX(atd->atd_Ctx);
		{
			/* Not already Ack'ed? */
			if (atd->atd_Buffer)
			{
				atd->atd_Buffer->atb_PacketData = NULL;
				atd->atd_Buffer = NULL;
			}
		}
		UNLOCK_CTX(atd->atd_Ctx);
		atd->atd_Ctx = NULL;
	}
}
/* WARNING: must be called with locked unit. Request packet modified */
void
ohci_HandleLocalRequest(
	OHCI1394Unit *unit,
	HeliosPacket *req,
	HeliosPacket *resp)
{
	_DBG_UNIT(unit, "LocReq: TL=%u, TC=$%02X, Payload=%p (%lu)\n",
		req->TLabel, req->TCode, req->Payload, req->PayloadLength);

	req->Ack = HELIOS_ACK_COMPLETE;
	req->TimeStamp = ohci_GetTimeStamp(unit);
	
	/* This function supposes that response headers are copy of req headers */
	resp->TLabel = req->TLabel;
	resp->DestID = req->DestID;
	resp->Generation = resp->Generation;
	
	/* Check req's TCode, compute resp's one and header length */
	resp->HeaderLength = 12;
	switch (req->TCode)
	{
		case TCODE_WRITE_BLOCK_REQUEST:
		case TCODE_WRITE_QUADLET_REQUEST:
			resp->TCode = TCODE_WRITE_RESPONSE;
			break;
			
		case TCODE_LOCK_REQUEST:
		case TCODE_READ_BLOCK_REQUEST:
		case TCODE_READ_QUADLET_REQUEST:
			resp->HeaderLength += 4;
			resp->TCode = req->TCode + 2;
			break;

		default:
			_ERR_UNIT(unit, "Unsupported TCode ($%X)\n", req->TCode);
			resp->RCode = HELIOS_RCODE_TYPE_ERROR;
			return;
	}
	
	/* Fill response packet */
	resp->Header[0] = req->Header[0] & ~AT_HEADER_TCODE_MSK;
	resp->Header[0] |= resp->TCode << 4;
	resp->Speed = AT_GET_HEADER_SPEED(req->Header[0]);

	/* Offset is in CSR space? */
	if ((req->Offset >= CSR_BASE_LO) && (req->Offset <= CSR_BASE_HI))
	{
		ULONG csr = (ULONG)(req->Offset - CSR_BASE_LO);

		_DBG_UNIT(unit, "csr=$%lx\n", csr);

		/* CSR offset not in ROM area ? */
		if ((csr < CSR_CONFIG_ROM_OFFSET*sizeof(QUADLET)) || (csr > CSR_CONFIG_ROM_END*sizeof(QUADLET)))
		{
			switch (csr)
			{
				/* CSR's under lock req */
				case CSR_BUS_MANAGER_ID:
				case CSR_BANDWIDTH_AVAILABLE:
				case CSR_CHANNELS_AVAILABLE_HI:
				case CSR_CHANNELS_AVAILABLE_LO:
					resp->RCode = ohci_HandleLocalCSRLock(unit, csr, req, resp);
					break;

				default: /* others CSR offset */
					_WRN_UNIT(unit, "LocReq in non-supported CSR space: offset=0x%llX\n",
						req->Offset);
					resp->RCode = HELIOS_RCODE_ADDRESS_ERROR;
			}
		}
		else
			resp->RCode = ohci_HandleLocalROMRequest(unit, csr, req, resp);
	}
	else /* Other addresses are not accessible (TODO: request handler?) */
		resp->RCode = HELIOS_RCODE_ADDRESS_ERROR;

	resp->SourceID = AT_GET_HEADER_DEST_ID(req->Header[1]);
	resp->Header[1] = (AT_GET_HEADER_DEST_ID(req->Header[0]) << 16) | (resp->RCode << 12);
	resp->TimeStamp = ohci_ComputeResponseTS(ohci_GetTimeStamp(unit), 4000);
}
/* Put an ohci raw packet on the given AT ctx.
 * optional parameters:
 * timestamp: (resp. packet only) indicate the packet timeout.
 * tlabel: (req. packet only) overwrite the tlabel data into the packet.
 */
LONG
ohci_ATContext_Send(
	OHCI1394ATCtx *		ctx,
	QUADLET *			p,
	QUADLET *			payload,
	OHCI1394ATData *	atd,
	UBYTE 				tlabel,
	UWORD 				timestamp)
{
	OHCI1394Unit *unit = ctx->atc_Context.ctx_Unit;
	OHCI1394ATBuffer *buffer;
	OHCI1394Descriptor *d, *last_d;
	QUADLET p0;
	UBYTE z, hl, tcode;
	ULONG length;
	LONG err;
	BOOL request;

	_DBG_CTX(ctx, "ATD=$%p, tl=$%x, ts=$%x\n", atd, tlabel, timestamp);

	tcode = AT_GET_HEADER_TCODE(p[0]);

	/* BusReset occured? */
	if ((tcode != TCODE_WRITE_PHY) &&
		(OHCI1394_INTF_BUSRESET & ohci_RegRead(unit, OHCI1394_REG_INT_EVENT_CLEAR)))
	{
		/* Simulate a FIFO flush after busreset */
		atd->atd_AckCallback(&unit->hu_TL, HELIOS_ACK_GENERATION, ohci_GetTimeStamp(unit), atd);
		return HHIOERR_NO_ERROR;
	}

	/* To build the DMA descriptor block we'll arrange
	 * a couple of DMA descriptors like this:
	 * Our transaction layer built packets with an header of 3, 4 or
	 * more quadlets.
	 * If more than 4 quadlets need to be sent (like in the case of a payload
	 * to send), the packet contains a full header filled (4 quadlets).
	 *
	 * As the IEEE 1394 norm enforces also the usage of an immediate
	 * descriptor in first, in term of descriptor usage we'll have:
	 *
	 *  - first descriptor used to sent the header, placed in the 4 last quadlets.
	 *  - a second one used to send payload data if any.
	 *
	 * The trick is descriptors are inside a more complexe structure, a node,
	 * where the packet address is stored. This node is used to track allocations
	 * in a big pre-allocated buffer.
	 *
	 * Byte-swapping notes: The IEEE 1394 bus specifies that data are treated as big-endian.
	 * But we place our data in a memory block read through the little-endian PCI bus.
	 * So were going to byte-swap all data if we are on a big-endian environment.
	 * That's the reason of all BE_SWAPxxx macros.
	 */

	LOCK_CTX(ctx);
	{
		buffer = ohci_ATContext_GetBuffer(ctx);
		_DBG_CTX(ctx, "buffer: $%p %p-%p\n", buffer);
	}
	UNLOCK_CTX(ctx);

	if (!buffer)
	{
		_ERR_CTX(ctx, "no free buffers\n");
		return IOERR_UNITBUSY;
	}

	length = 0;
	switch (tcode)
	{
		/* Request w/o payload */
		case TCODE_READ_BLOCK_REQUEST:
		case TCODE_WRITE_QUADLET_REQUEST:
			hl = 16;
			payload = NULL;
			z = 2;
			request = TRUE;
			break;

		case TCODE_READ_QUADLET_REQUEST:
			hl = 12;
			payload = NULL;
			z = 2;
			request = TRUE;
			break;

		case TCODE_WRITE_PHY:
			hl = 12;
			payload = NULL;
			z = 2;
			request = TRUE;
			break;

		/* Request w/ payload */
		case TCODE_LOCK_REQUEST:
		case TCODE_WRITE_BLOCK_REQUEST:
			hl = 16;
			if (NULL == payload)
				payload = &p[4];
			length = AT_GET_HEADER_LEN(p[3]);
			z = 3;
			request = TRUE;
			break;

		case TCODE_WRITE_STREAM:
			hl = 8;
			if (NULL == payload)
				payload = &p[2];
			length = AT_GET_HEADER_LEN(p[1]);
			z = 3;
			request = TRUE;
			break;

		/* Response w/o payload */
		case TCODE_READ_QUADLET_RESPONSE:
			hl = 16;
			payload = NULL;
			z = 2;
			request = FALSE;
			break;

		case TCODE_WRITE_RESPONSE:
			hl = 12;
			payload = NULL;
			z = 2;
			request = FALSE;
			break;

		/* Response w/ payload */
		case TCODE_READ_BLOCK_RESPONSE:
		case TCODE_LOCK_RESPONSE:
			hl = 16;
			if (NULL == payload)
				payload = &p[4];
			length = AT_GET_HEADER_LEN(p[3]);
			z = 3;
			request = FALSE;
			break;

		default: /* not the responsability of this function, should not happen */
			LOCK_CTX(ctx);
			ohci_ATContext_ReleaseBuffer(ctx, buffer);
			UNLOCK_CTX(ctx);

			atd->atd_AckCallback(&unit->hu_TL, HELIOS_ACK_TYPE_ERROR, ohci_GetTimeStamp(unit), atd);
			return HHIOERR_NO_ERROR;
	}

	/* z = number of needed descriptors
	 * 2 descriptors used => no payload data
	 * 3 descriptors used => payload data
	 */
	d = buffer->atb_Descriptors;
	bzero(d, z * sizeof(OHCI1394Descriptor));

	/* Prepare header descriptor */
	d[0].d_Control = BE_SWAPWORD_C(DESCRIPTOR_KEY_IMMEDIATE);

	/* quadlets count for this descriptor */
	d[0].d_ReqCount = BE_SWAPWORD_C(hl);

	/* TimeStamp (AT response packet only, or let it to 0) */
	if (!request)
		d[0].d_TimeStamp = BE_SWAPWORD(timestamp);

	/* More descriptors? */
	if (payload)
	{
		if (TCODE_WRITE_STREAM == tcode)
			last_d = &d[1];
		else
			last_d = &d[2];

		/* Prepare the payload descriptor */
		last_d->d_ReqCount = BE_SWAPWORD(length);
		last_d->d_DataAddress = BE_SWAPLONG((ULONG)PCIXDMAGetPhysical(unit->hu_PCI_BoardObject, payload));
	}
	else
		last_d = &d[0];

	/* Overwrite tlabel for request packet */
	if (request && (TCODE_WRITE_STREAM != tcode) && (TCODE_WRITE_PHY != tcode))
		p0 = (p[0] & ~(AT_HEADER_TLABEL_MSK << AT_HEADER_TLABEL_SHIFT)) | (tlabel << AT_HEADER_TLABEL_SHIFT);
	else
		p0 = p[0];

	/* Prepare the Header data area */
	((QUADLET *)&d[1])[0] = BE_SWAPLONG(p0);
	((QUADLET *)&d[1])[1] = BE_SWAPLONG(p[1]);

	if (TCODE_WRITE_STREAM != tcode)
	{
		((QUADLET *)&d[1])[2] = BE_SWAPLONG(p[2]);
		if (16 == hl)
		{
			/* Data are swapped by the HW */
			if ((tcode == TCODE_WRITE_QUADLET_REQUEST) &&
				(tcode == TCODE_READ_QUADLET_RESPONSE))
				((QUADLET *)&d[1])[3] = LE_SWAPLONG(p[3]);
			else
				((QUADLET *)&d[1])[3] = BE_SWAPLONG(p[3]);
		}
	}

	/* Finish the last descriptor */
	last_d->d_Control |= BE_SWAPWORD_C(	DESCRIPTOR_OUTPUT_LAST |
										DESCRIPTOR_IRQ_ALWAYS |
										DESCRIPTOR_BRANCH_ALWAYS);
	buffer->atb_LastDescriptor = last_d;

	/* buffer / pd dual link (no lock needed until put in AT-DMA FIFO) */
	atd->atd_Ctx = ctx;
	atd->atd_Buffer = buffer;
	buffer->atb_PacketData = atd;

	/* Lock the unit to be sure to not send during a bus-reset */
	SHARED_PROTECT_BEGIN(unit);
	{
		if (unit->hu_Flags.Enabled)
		{
			if ((tcode == TCODE_WRITE_PHY) ||
				!(OHCI1394_INTF_BUSRESET & ohci_RegRead(unit, OHCI1394_REG_INT_EVENT_CLEAR)))
			{
				LOCK_CTX(ctx);
				{
					/* Append the descriptor block at the end of the current context program */
					ohci_ATContext_AppendBuffer(ctx, buffer, z);

					/* And finish by running the context if not done yet */
					ohci_ATContext_Run(ctx);
				}
				UNLOCK_CTX(ctx);
				
				err = HHIOERR_NO_ERROR;
			}
			else
				err = HHIOERR_ACK; /* ack callback will be called */
		}
		else
			err = HHIOERR_UNIT_DISABLED;
	}
	SHARED_PROTECT_END(unit);
	
	if (err != HHIOERR_NO_ERROR)
	{
		LOCK_CTX(ctx);
		ohci_ATContext_ReleaseBuffer(ctx, buffer);
		UNLOCK_CTX(ctx);
		
		/* Simulate a FIFO flush after busreset */
		if (err == HHIOERR_ACK)
		{
			atd->atd_AckCallback(&unit->hu_TL, HELIOS_ACK_GENERATION, ohci_GetTimeStamp(unit), atd);
			err = HHIOERR_NO_ERROR;
		}
	}
	
	return err;
}
LONG
ohci_SendPHYPacket(
	OHCI1394Unit *unit,
	HeliosSpeed speed,
	QUADLET phy_data,
	OHCI1394ATData *atd)
{
	QUADLET p[3];

	/* Construct an OHCI PHY packet */
	p[0] = (speed << 16) | (TCODE_WRITE_PHY << 4);
	p[1] = phy_data;
	p[2] = ~p[1];

	return ohci_ATContext_Send(&unit->hu_ATRequestCtx, p, NULL, atd, 0, 0);
}
/*=== MISC ====================================================================*/
void
ohci_DumpRegisters(OHCI1394Unit *unit)
{
	ULONG i;

	DB_Raw("\n\tOHCI Reg 0x%08x ========\n", unit->hu_PCI_RegBase);
	for (i=0; i < ARRAY_SIZE(ohci_reg_names); i++)
	{
		QUADLET q;
		ULONG offset = ohci_reg_names[i].offset;

		q = ohci_RegRead(unit, offset);
		DB_Raw("$%03x - %25s : 0x%08x\n", offset, ohci_reg_names[i].label, q);
	}
	DB_Raw("\n");
}
BOOL
ohci_RaiseBusReset(OHCI1394Unit *unit, BOOL shortreset)
{
	if (shortreset)
	{
		_INF_UNIT(unit, "Raising short busreset\n");
		return ohci_WritePHY(unit, 5, 0, PHYF_SHORT_BUS_RESET);
	}
	else
	{
		_INF_UNIT(unit, "Raising long busreset\n");
		return ohci_WritePHY(unit, 1, 0, PHYF_BUS_RESET);
	}
}
/* atomic */
UWORD
ohci_GetTimeStamp(OHCI1394Unit *unit)
{
	return ohci_RegRead(unit, OHCI1394_REG_ISOCHRONOUS_CYCLE_TIMER) >> 12;
}
UWORD
ohci_ComputeResponseTS(UWORD req_timestamp, UWORD offset)
{
	UWORD timestamp;

	timestamp = (req_timestamp & 0x1fff) + offset;
	/* 13bit overflow => add one second */
	if (timestamp >= 8000)
	{
		timestamp -= 8000;
		timestamp += (req_timestamp & ~0x1fff) + 0x2000;
	}
	else
		timestamp += (req_timestamp & ~0x1fff);
	return timestamp;
}
#if 0
/* atomic (only if the task is not delayed more than 1 seconds per register read) */
UQUAD ohci_UpTime(OHCI1394Unit *unit)
{
	QUADLET cycle_time, seconds_0, seconds_1;

	/* Reading BusSecond twice to be sure it has not changed during CycleTimer reg read.
	 * If not, the cycle_time value doesn't correspond to the first seconds read.
	 */
	seconds_0 = ATOMIC_FETCH(&unit->hu_BusSeconds);
	cycle_time = ohci_RegRead(unit, OHCI1394_REG_ISOCHRONOUS_CYCLE_TIMER);
	/* BusSeconds may has changed here */
	seconds_1 = ATOMIC_FETCH(&unit->hu_BusSeconds);

	if (seconds_0 != seconds_1)
	{
		/* Read again the cycle_time register
		 * We suppose to do that in less than 1 second ;-)
		 */
		cycle_time = ohci_RegRead(unit, OHCI1394_REG_ISOCHRONOUS_CYCLE_TIMER);
	}

	return (((UQUAD)seconds_1) << 32) | cycle_time;
}
LONG ohci_GenerationOK(OHCI1394Unit *unit, UBYTE generation)
{
	return (generation == unit->hu_OHCI_LastGeneration) &&
		   (0 == (OHCI1394_INTF_BUSRESET & ohci_RegRead(unit, OHCI1394_REG_INT_EVENT_CLEAR)));
}
LONG ohci_SetROM(OHCI1394Unit *unit, QUADLET *data)
{
	QUADLET *next_rom;
	LONG err;

	/* make internal copy of data */
	next_rom = Helios_CreateROM(unit->hu_MemPool, HA_Rom, (ULONG)data, TAG_DONE);
	if (NULL == next_rom)
		return HHIOERR_NOMEM;

	LOCK_REGION(unit);
	{
		/* Previous ROM update process finished ? */
		if (NULL == unit->hu_NextROMData)
		{
			QUADLET *rom_phy;

			rom_phy = PCIXDMAGetPhysical(unit->hu_PCI_BoardObject, next_rom);
			_INFO_UNIT(unit, "ROM: CPU=%p, PHY=%p\n", next_rom, rom_phy);

			unit->hu_NextROMData = next_rom;
			ohci_RegWrite(unit, OHCI1394_REG_CONFIG_ROM_MAP, (QUADLET)rom_phy);

			err = HHIOERR_NO_ERROR;
		}
		else /* BUSY */
			err = HHIOERR_FAILED;
	}
	UNLOCK_REGION(unit);
	
	if (!err)
	{
		/* Now finish the process by a bus reset */
		ohci_RaiseBusReset(unit, TRUE);
	}
	else
		Helios_FreeROM(unit->hu_MemPool, next_rom);

	return err;
}
void ohci_IRContext_Destroy(OHCI1394IRCtx *ctx)
{
	OHCI1394Unit *unit = ctx->irc_Base.ic_Context.ctx_Unit;

	ohci_IRContext_Stop(ctx);
	Helios_KillSubTask(ctx->irc_Base.ic_Context.ctx_SubTask);
	ohci_IsoCtx_Remove(&ctx->irc_Base.ic_Context);
	ohci_FreeIsoCtx(unit, HELIOS_ISO_RX_CTX, ctx->irc_Base.ic_Index);

	FreeVecDMA(ctx->irc_PageBuffer);
	FreeVecDMA(ctx->irc_DMABuffer);
	FreePooled(unit->hu_MemPool, ctx, sizeof(*ctx));
}
OHCI1394IRCtx *ohci_IRContext_Create(OHCI1394Unit *     unit,
									 LONG               index,
									 UWORD              ibuf_size,
									 UWORD              ibuf_count,
									 UWORD              hlen,
									 UBYTE              payload_align,
									 APTR               callback,
									 APTR               udata,
									 OHCI1394IRCtxFlags flags)
{
	OHCI1394IRCtx *ctx = NULL;
	ULONG buf_size = ibuf_size + payload_align - 1;

	index = ohci_AllocIsoCtx(unit, HELIOS_ISO_RX_CTX, index);
	if (index >= 0)
	{
		ctx = AllocPooled(unit->hu_MemPool, sizeof(*ctx));
		if (NULL != ctx)
		{
			ctx->irc_Base.ic_Index = index;
			ctx->irc_HeaderLength = hlen;
			ctx->irc_PayloadLength = ibuf_size - hlen;
			ctx->irc_Callback = callback;
			ctx->irc_UserData = udata;
			ctx->irc_Flags = flags;

			/* Data buffer allocation */
			ctx->irc_PageBuffer = AllocVecDMA(buf_size * ibuf_count + payload_align - 1, MEMF_PUBLIC | MEMF_CLEAR);
			if (NULL != ctx->irc_PageBuffer)
			{
				/* Align the start of the data buffer */
				ctx->irc_AlignedPageBuffer = ctx->irc_PageBuffer + ((payload_align - (((ULONG) ctx->irc_PageBuffer) % payload_align)) % payload_align);

				/* Compute the header padding from requested payload alignement and the allocated data buffer address */
				ctx->irc_HeaderPadding = (payload_align - ((((ULONG) ctx->irc_AlignedPageBuffer) + hlen) % payload_align)) % payload_align;

				/* DMA programs allocation */
				ctx->irc_DMABufferCount = ibuf_count;
				ctx->irc_DMABuffer = AllocVecDMA(sizeof(OHCI1394IRBuffer)*ibuf_count+15, MEMF_PUBLIC | MEMF_CLEAR);
				if (NULL != ctx->irc_DMABuffer)
				{
					ULONG cmd_ptr;
					char buf[64];

					/* Align on 16-bytes DMA buffers */
					ctx->irc_AlignedDMABuffer = (APTR) (16 + (((ULONG) ctx->irc_DMABuffer - 1) & ~15));

					/* Prepare the DMA program */
					buf_size = ctx->irc_HeaderLength + ctx->irc_HeaderPadding + ctx->irc_PayloadLength;

					_INFO_IRDMA_CTX(ctx, "DMA@%p, Pages@%p, DMA buf size=%lu\n",
									ctx->irc_AlignedDMABuffer, ctx->irc_AlignedPageBuffer, buf_size);

					cmd_ptr = ohci_IRContext_PacketPerBuffer_InitDMABuffers(ctx, buf_size);

					utils_SPrintF(buf, "["DEVNAME"] IR Handler #%u", index);
					if (ohci_Context_Init(unit, &ctx->irc_Base.ic_Context,
										  OHCI1394_REG_IRECV_CONTEXT_CONTROL(index),
										  buf, ohci_IRContext_PacketPerBufferCallback,
										  TASK_PRIO_IRCTX))
					{
						ohci_IRContext_Append(ctx, cmd_ptr);
						return ctx;
					}

					FreeVecDMA(ctx->irc_DMABuffer);
				}
				else
					_ERR_UNIT(unit, "IR #%u: DMABuffer alloc failed\n", index);

				FreeVecDMA(ctx->irc_PageBuffer);
			}
			else
				_ERR_UNIT(unit, "IR #%u: PageBuffer alloc failed\n", index);

			FreePooled(unit->hu_MemPool, ctx, sizeof(*ctx));
		}
		else
			_ERR_UNIT(unit, "IR #%u: ctx alloc failed\n", index);

		ohci_FreeIsoCtx(unit, HELIOS_ISO_RX_CTX, index);
	}

	return NULL;
}
void ohci_IRContext_Start(OHCI1394IRCtx *ctx, ULONG channel, ULONG tags)
{
	LOCK_CTX(ctx);
	{
		OHCI1394Unit *unit = ctx->irc_Base.ic_Context.ctx_Unit;
		QUADLET reg = ohci_RegRead(unit, OHCI1394_REG_IRECV_CONTEXT_CONTROL(ctx->irc_Base.ic_Index));

		_INFO_IRDMA_CTX(ctx, "[%u]: CTRL=$%08x\n", ctx->irc_Base.ic_Index, reg);
		if (0 == (reg & CTX_RUN))
		{
			ULONG index_mask = 1ul << ctx->irc_Base.ic_Index;

			/* Clearing and enable interruption line */
			ohci_RegWrite(unit, OHCI1394_REG_ISO_RECV_INT_EVENT_CLEAR, index_mask);
			ohci_RegWrite(unit, OHCI1394_REG_ISO_RECV_INT_MASK_SET, index_mask);

			/* Setup IR contextMatch register */
			reg = (tags << 28) | channel;
			ohci_RegWrite(unit, OHCI1394_REG_IRECV_COMMAND_MATCH(ctx->irc_Base.ic_Index), reg);

			/* Run context */
			ohci_IRContext_Run(ctx);

			_INFO_IRDMA_CTX(ctx, "[%u]: CTRL=$%08x\n", ctx->irc_Base.ic_Index,
							ohci_RegRead(unit, OHCI1394_REG_IRECV_CONTEXT_CONTROL(ctx->irc_Base.ic_Index)));
		}
	}
	UNLOCK_CTX(ctx);
}
BOOL ohci_IRContext_Stop(OHCI1394IRCtx *ctx)
{
	OHCI1394Unit *unit = ctx->irc_Base.ic_Context.ctx_Unit;
	ULONG loop = 20;
	ULONG index_mask = 1ul << ctx->irc_Base.ic_Index;
	BOOL res = FALSE;

	LOCK_CTX(ctx);
	{
	   /* Disable interruption line */
	   ohci_RegWrite(unit, OHCI1394_REG_ISO_RECV_INT_MASK_CLEAR, index_mask);

	   /* Request the DMA stop of context */
	   ohci_RegWrite(unit, OHCI1394_REG_IRECV_CONTEXT_CONTROL_CLEAR(ctx->irc_Base.ic_Index), CTX_RUN);

	   /* Wait about DMA safe state */
	   do
	   {
		   QUADLET reg;

		   reg = ohci_RegRead(unit, OHCI1394_REG_IRECV_CONTEXT_CONTROL(ctx->irc_Base.ic_Index));
		   if (0 == (reg & CTX_ACTIVE))
		   {
			   res = TRUE;
			   goto out;
		   }

		   _INFO_CTX(&ctx->irc_Base.ic_Context, "IR context %u still active\n", ctx->irc_Base.ic_Index);
		   Helios_DelayMS(25); // wait 25 ms
	   } while (loop--);

	   /* Timeout */
	   _ERR_CTX(&ctx->irc_Base.ic_Context,
				"TIMEOUT, IR context #%u still active 500ms after STOP request\n",
				ctx->irc_Base.ic_Index);
	}
	UNLOCK_CTX(ctx);

out:
	return res;
}
#endif
/*=== Hi-level API ==========================================================*/
/* WARNING: does unprotected base's data accesses */
LONG
ohci_ScanPCI(OHCI1394Device *base)
{
	APTR board = NULL;

	base->hd_UnitCount = 0;
	NEWLIST(&base->hd_Units);

	/* Search about all OHCI 1394 boards on the PCI bus */
	do
	{
		/* Filter on Firewire and not owned boards */
		board = PCIXFindBoardTags(board,
			PCIXFINDTAG_FULLCLASS, PCI_CLASS_SERIAL_FIREWIRE,
			PCIXFINDTAG_IGNOREOWNED, TRUE,
			TAG_DONE);
		if (board)
		{
			_DBG("FW board found: %p\n", board);

			/* OHCI interface? */
			if (PCIXReadConfigByte(board, PCIXCONFIG_PROGINTERFACE) == PCIXCONFIG_PROGINTERFACE_OHCI)
			{
				OHCI1394Unit *unit;

				/* Create a new unit, fill its PCI info and append it to the global list */
				unit = AllocPooled(base->hd_MemPool, sizeof(*unit));
				if (NULL != unit)
				{
					LOCK_INIT(unit);

					unit->hu_UnitNo = base->hd_UnitCount++;
					unit->hu_Device = (struct Device *)base;
					unit->hu_PCI_BoardObject = board;
					unit->hu_PCI_VID = PCIXReadConfigWord(board, PCIXCONFIG_VENDOR);
					unit->hu_PCI_DID = PCIXReadConfigWord(board, PCIXCONFIG_DEVICE);

					_INF("unit-%u: PCI board %p, VID: %04x, DID: %04x\n",
						unit->hu_UnitNo, board, unit->hu_PCI_VID, unit->hu_PCI_DID);
					ADDTAIL(&base->hd_Units, unit);
				}
				else
				{
					_ERR("Failed to allocate OHCI unit\n");
					return FALSE;
				}
			}
		}
	}
	while (NULL != board);

	_DBG("%u FW boards found on PCI bus\n", base->hd_UnitCount);
	return TRUE;
}
/* WARNING: does unprotected base's data accesses */
LONG
ohci_OpenUnit(OHCI1394Device *base, struct IORequest *ioreq, ULONG index)
{
	OHCI1394Unit *unit, *node;
	LONG err = IOERR_OPENFAIL;

	/* Search unit index in list filled by the PCI scan */
	unit = NULL;
	ForeachNode(&base->hd_Units, node)
	{
		if (node->hu_UnitNo == index)
		{
			unit = node;
			break;
		}
	}
	
	if (unit)
	{
		/* Not already opened? */
		if (!unit->hu_Flags.Initialized)
		{
			if (ohci_ResetUnit(unit))
			{
				ioreq->io_Unit = (struct Unit *) unit;
				err = HHIOERR_NO_ERROR;
			}
		}
		else
		{
			_ERR("Unit %lu already initialized.\n", index);
			err = IOERR_UNITBUSY;
		}
	}
	else
		_ERR("Invalid unit number %u!\n", index);

	return err;
}
/* WARNING: does unprotected base's data accesses */
void
ohci_CloseUnit(OHCI1394Device *base, struct IORequest *ioreq)
{
	OHCI1394Unit *unit = (OHCI1394Unit *) ioreq->io_Unit;

	if (unit->hu_Flags.Initialized)
	{
		ohci_Term(unit);
		unit->hu_Flags.Initialized = FALSE;
	}
}
/* WARNING: does unprotected base's data accesses */
LONG
ohci_Init(OHCI1394Unit *unit)
{
	LONG ret = FALSE;
	QUADLET q;
	HeliosMsg msg;

	NEWLIST(&unit->hu_Listeners.ell_SysList);
	LOCK_INIT(&unit->hu_Listeners);
	ohci_TL_Init(unit, &unit->hu_TL);

	msg.hm_Msg.mn_Length = sizeof(msg);
	msg.hm_Msg.mn_Node.ln_Type = NT_MESSAGE;

	if (ohci_PCI_OpenUnit(unit))
	{
		/* XXX: does sem protection needed as we have own protection system? */
		unit->hu_MemPool = CreatePool(MEMF_PUBLIC|MEMF_CLEAR|MEMF_SEM_PROTECTED, 16384, 4096);
		if (unit->hu_MemPool)
		{
			/* Task to handle async requests in split-timeout */
			unit->hu_SplitTimeoutTask = Helios_CreateSubTask(
				"["DEVNAME"] SplitTimeout task",
				ohci_SplitTimeoutTask,
				HA_MemPool, (ULONG)unit->hu_MemPool,
				TASKTAG_PRI, TASK_PRIO_SPLITTIMEOUT,
				HA_UserData, (ULONG)unit,
				TAG_DONE);
			if (unit->hu_SplitTimeoutTask)
			{
				msg.hm_Type = HELIOS_MSGTYPE_INIT;
				if (Helios_DoMsgToSubTask(unit->hu_SplitTimeoutTask, &msg.hm_Msg, NULL) == HERR_NOERR)
				{
					_DBG_UNIT(unit, "Reset HW registers...\n");
					if (ohci_SoftReset(unit))
					{
						/* Check for OHCI protocol version */
						q = ohci_RegRead(unit, OHCI1394_REG_VERSION);
						unit->hu_OHCI_Version = q & 0x00ff00ff;

						/* Only OHCI v1.0 and V1.1 are supported */
						if ((OHCI1394_VERSION_1_0 == unit->hu_OHCI_Version) ||
							(OHCI1394_VERSION_1_1 == unit->hu_OHCI_Version))
						{
							/* Store static OHCI data */
							unit->hu_OHCI_VendorID = ohci_RegRead(unit, OHCI1394_REG_VENDOR_ID);
							_DBG_UNIT(unit, "VendorID: 0x%08x\n", unit->hu_OHCI_VendorID);

							unit->hu_BusOptions.quadlet = ohci_RegRead(unit, OHCI1394_REG_BUS_OPTIONS);
							_DBG_UNIT(unit, "BusOption: 0x%08x = [MaxRec: %lu]\n",
								unit->hu_BusOptions.quadlet,
								2 << unit->hu_BusOptions.MaxRec);

							unit->hu_BusSeconds = 0;

							/* 1394 static data */
							unit->hu_GUID  = ((UQUAD) ohci_RegRead(unit, OHCI1394_REG_GUID_HI)) << 32;
							unit->hu_GUID |= ohci_RegRead(unit, OHCI1394_REG_GUID_LO);

							_INF_UNIT(unit, "GUID: <%016llx>\n", unit->hu_GUID);

							/* Iso channels stuffs */
							NEWLIST(&unit->hu_IRCtxList);
							ohci_RegWrite(unit, OHCI1394_REG_ISO_RECV_INT_MASK_SET, ~0);
							unit->hu_IRCtxMask = ohci_RegRead(unit, OHCI1394_REG_ISO_RECV_INT_MASK_SET);
							ohci_RegWrite(unit, OHCI1394_REG_ISO_RECV_INT_MASK_CLEAR, ~0);
							unit->hu_MaxIsoReceiveCtx = utils_CountBits32(unit->hu_IRCtxMask, 1);

							NEWLIST(&unit->hu_ITCtxList);
							ohci_RegWrite(unit, OHCI1394_REG_ISO_XMIT_INT_MASK_SET, ~0);
							unit->hu_ITCtxMask = ohci_RegRead(unit, OHCI1394_REG_ISO_XMIT_INT_MASK_SET);
							ohci_RegWrite(unit, OHCI1394_REG_ISO_XMIT_INT_MASK_CLEAR, ~0);
							unit->hu_MaxIsoTransmitCtx = utils_CountBits32(unit->hu_IRCtxMask, 1);

							_DBG_UNIT(unit, "Available ISO Contexts: Rx=%u, Tx=%u\n",
								unit->hu_MaxIsoReceiveCtx, unit->hu_MaxIsoTransmitCtx);

							/* Allocation of self-id DMA buffer (aligned on SELF_ID_BUF_SIZE) */
							unit->hu_SelfIdBufferAlloc = AllocVecDMA(SELF_ID_BUF_SIZE * 2, MEMF_PUBLIC | MEMF_CLEAR);
							if (NULL != unit->hu_SelfIdBufferAlloc)
							{
								unit->hu_SelfIdBufferCpu = (APTR) GET_ALIGNED2(unit->hu_SelfIdBufferAlloc, SELF_ID_BUF_SIZE);
								unit->hu_SelfIdBufferPhy = PCIXDMAGetPhysical(unit->hu_PCI_BoardObject, unit->hu_SelfIdBufferCpu);

								_DBG_UNIT(unit, "SelfID buffers: raw=%p, cpu=%p, phy=%p\n",
									unit->hu_SelfIdBufferAlloc,
									unit->hu_SelfIdBufferCpu,
									unit->hu_SelfIdBufferPhy);

								/* Use this special dma buffer now */
								ohci_RegWrite(unit, OHCI1394_REG_SELFID_BUFFER, (ULONG)unit->hu_SelfIdBufferPhy);

								/* Subtask creation for BusReset interruptions */
								unit->hu_BusResetTask = Helios_CreateSubTask("["DEVNAME"] BusReset",
									(APTR)ohci_BusResetTask,
									HA_MemPool, (ULONG)unit->hu_MemPool,
									TASKTAG_PRI, TASK_PRIO_BUSRESET,
									HA_UserData, (ULONG)unit,
									TAG_DONE);
								if (unit->hu_BusResetTask)
								{
									msg.hm_Type = HELIOS_MSGTYPE_INIT;
									if (Helios_DoMsgToSubTask(unit->hu_BusResetTask, &msg.hm_Msg, NULL) == HERR_NOERR)
									{
										/* Set upper bound of direct physical accesses */
										ohci_RegWrite(unit, OHCI1394_REG_PHYSICAL_UPPER_BOUND, OHCI_PHY_UPPERBOUND);

										/* Others OHCI setup */
										ohci_RegWrite(unit, OHCI1394_REG_AT_RETRIES,
											OHCI1394_MAX_AT_REQ_RETRIES |
											(OHCI1394_MAX_AT_RESP_RETRIES << 4) |
											(OHCI1394_MAX_PHYS_RESP_RETRIES << 8));

										/* Asynchronous handlers creation */
										if (ohci_ATContexts_Init(unit))
										{
											if (ohci_ARContexts_Init(unit))
											{
												ULONG lps, i;

												/* HCC setup:
												 * - disable posted write
												 * - set Link Power Status to on (SCLK start)
												 * - BE environment: swap packet data (see document OHCI 1.1 Ch. 5.7.1 for more information)
												 */

#if BYTE_ORDER == BIG_ENDIAN
												ohci_RegWrite(unit, OHCI1394_REG_HC_CONTROL_CLEAR, OHCI1394_HCCF_NOBYTESWAPDATA);
#else
												ohci_RegWrite(unit, OHCI1394_REG_HC_CONTROL_SET, OHCI1394_HCCF_NOBYTESWAPDATA);
#endif
												ohci_RegWrite(unit, OHCI1394_REG_HC_CONTROL_CLEAR, OHCI1394_HCCF_POSTEDWRITEENABLE);
												ohci_RegWrite(unit, OHCI1394_REG_HC_CONTROL_SET, OHCI1394_HCCF_LPS);

												/* Waiting for LPS startup: 50ms delay, 3 retries */
												for (lps=0, i=0; !lps && (i < 3); i++)
												{
													Helios_DelayMS(50);
													lps = ohci_RegRead(unit, OHCI1394_REG_HC_CONTROL) & OHCI1394_HCCF_LPS;
												}

												if (lps)
												{
													/* We can setup some extra registers now... */
													_DBG_UNIT(unit, "Link Power Status set\n");

													/* Setup LinkControl register:
													 * - Enable PHY packet receipt
													 * - Enable SelfID packet receipt
													 * - Enable Cycle Timer counter
													 * - Enable CycleMaster when root.
													 */
													//ohci_RegWrite(unit, OHCI1394_REG_LINK_CONTROL_CLEAR, OHCI1394_LCF_RCVPHYPKT);
													ohci_RegWrite(unit, OHCI1394_REG_LINK_CONTROL_SET,
														OHCI1394_LCF_RCVSELFID |
														OHCI1394_LCF_CYCLETIMERENABLE |
														OHCI1394_LCF_RCVPHYPKT |
														OHCI1394_LCF_CYCLEMASTER);

													/* Clear events */
													ohci_RegWrite(unit, OHCI1394_REG_INT_EVENT_CLEAR, ~0);

													/* Set L and C bits in our SelfID packets (cf. IEEE1394a-1995 Ch. 8.4.2.3) */
													if (ohci_WritePHY(unit, 4, 0, PHYF_LINK_ACTIVE | PHYF_CONTENDER))
													{
														/* Create a new ROM with default values based on HW default ROM */
														unit->hu_ROMData = Helios_CreateROM(unit->hu_MemPool,
															HA_GUID_Hi,			(ULONG)(unit->hu_GUID >> 32),
															HA_GUID_Lo,			(ULONG)(unit->hu_GUID),
															HA_BusOptions,		unit->hu_BusOptions.quadlet,
															HA_CSR_VendorID,	unit->hu_OHCI_VendorID,
															TAG_DONE);
														if (unit->hu_ROMData)
														{
															QUADLET *rom_phy;

															unit->hu_NextROMData = NULL;
															
															rom_phy = PCIXDMAGetPhysical(unit->hu_PCI_BoardObject, unit->hu_ROMData);
															_INF_UNIT(unit, "ROM: CPU=%p, PHY=%p\n", unit->hu_ROMData, rom_phy);

															/* ROM set/udpate procedure is explained in section 5.5.6 in the OHCI specification.
															 * Here we are in a state where HCControl.linkEnable = 0 and HCControl.BIBimageValid = 0.
															 * So the atomic update using the OHCI rom shadow register doesn't occure.
															 * When the Config ROM Header register is set, the change occures immediately.
															 * So OHCI requires that the Config ROM Header and the Bus Options registers
															 * are valid BEFORE set HCControl.linkEnable.
															 */

															_INF_UNIT(unit, "ROM before update: ROM Hdr=0x%08x, BusOption=$%08x\n",
																ohci_RegRead(unit, OHCI1394_REG_CONFIG_ROM_HDR),
																ohci_RegRead(unit, OHCI1394_REG_BUS_OPTIONS));
															ohci_RegWrite(unit, OHCI1394_REG_CONFIG_ROM_MAP, (QUADLET)rom_phy);
															_INF_UNIT(unit, "ROM after update: ROM Hdr=0x%08x, BusOption=$%08x\n",
																ohci_RegRead(unit, OHCI1394_REG_CONFIG_ROM_HDR),
																ohci_RegRead(unit, OHCI1394_REG_BUS_OPTIONS));

															ohci_RegWrite(unit, OHCI1394_REG_CONFIG_ROM_HDR, unit->hu_ROMData[0]);
															ohci_RegWrite(unit, OHCI1394_REG_BUS_OPTIONS, unit->hu_ROMData[2]);
															
															/* Accept PHY request packet from all nodes id */
															ohci_RegWrite(unit, OHCI1394_REG_ASYNCH_REQ_FILTER_HI_SET, 0x80000000);

															/* Init transaction layer */
															unit->hu_SplitTimeoutCSR = 0x800; /* Set SPLIT-TIMEOUT default to 100ms */

															/* Enable the Link */
															ohci_RegWrite(unit, OHCI1394_REG_HC_CONTROL_SET,
																		  OHCI1394_HCCF_LINKENABLE | OHCI1394_HCCF_BIBIMAGEVALID);

															_DBG_UNIT(unit, "HCC: 0x%08x\n", ohci_RegRead(unit, OHCI1394_REG_HC_CONTROL_SET));

															/* Install PCI IRQ handler */
															ret = ohci_PCI_InstallIRQ(unit);
															if (ret)
															{
																unit->hu_Flags.Enabled = FALSE;
																
																_INF_UNIT(unit, "init done\n");
																ret = TRUE;
																goto end;
															}
														}
														else
															_ERR_UNIT(unit, "ROM creation failed\n");
													}
													else
														_ERR_UNIT(unit, "Failed to set L&C SelfID bits in PHY\n");
												}
												else
													_ERR_UNIT(unit, "Failed to set Link Power Status\n");
													
												ohci_ARContexts_Term(unit);
											}

											ohci_ATContexts_Term(unit);
										}
									}
									else
										_ERR_UNIT(unit, "BusReset task init msg failed\n");
									
									Helios_KillSubTask(unit->hu_BusResetTask, TRUE);
								}
								else
									_ERR_UNIT(unit, "Failed to create BusReset task\n");

								FreeVecDMA(unit->hu_SelfIdBufferAlloc);
							}
							else
								_ERR_UNIT(unit, "AllocVecDMA() for %lu bytes failed\n", SELF_ID_BUF_SIZE * 2);
						}
						else
							_ERR_UNIT(unit, "Unsupported OHCI version (0x%08x)\n", unit->hu_OHCI_Version);

						ohci_DumpRegisters(unit);
						ohci_SoftReset(unit);
					}
				}
				else
					_ERR_UNIT(unit, "SplitTimeout task init msg failed\n");
					
				Helios_KillSubTask(unit->hu_SplitTimeoutTask, TRUE);
			}

			DeletePool(unit->hu_MemPool);
		}
		else
			_ERR_UNIT(unit, "CreatePool() failed!\n");
	}

end:
	return ret;
}
/* WARNING: does unprotected base's data accesses */
void
ohci_Term(OHCI1394Unit *unit)
{
	APTR iso_ctx, next;

	_WRN_UNIT(unit, "terminating...\n");
	
	ohci_Disable(unit);
	ohci_PCI_RemoveIRQ(unit);
	
#if 0
	ForeachNodeSafe(&unit->hu_IRCtxList, iso_ctx, next)
		ohci_IRContext_Destroy(iso_ctx);

	ForeachNodeSafe(&unit->hu_ITCtxList, iso_ctx, next)
		iso_ITContext_Destroy(iso_ctx);
#endif

	ohci_SoftReset(unit);

	ohci_ATContexts_Term(unit);
	ohci_ARContexts_Term(unit);
	
	_DBG_UNIT(unit, "Killing tasks...\n");
	Helios_KillSubTask(unit->hu_BusResetTask, TRUE);
	Helios_KillSubTask(unit->hu_SplitTimeoutTask, TRUE);

	Helios_FreeROM(unit->hu_MemPool, unit->hu_ROMData);
	if (NULL != unit->hu_NextROMData)
		Helios_FreeROM(unit->hu_MemPool, unit->hu_NextROMData);

	FreeVecDMA(unit->hu_SelfIdBufferAlloc);

	DeletePool(unit->hu_MemPool);
	unit->hu_MemPool = NULL;

	ohci_PCI_CloseUnit(unit);

	/* Erase data */
	memset(&unit->hu_MemPool, 0, sizeof(*unit) - offsetof(OHCI1394Unit, hu_MemPool));
}
/* WARNING: does unprotected base's data accesses */
LONG
ohci_Enable(OHCI1394Unit *unit)
{
	if (unit->hu_Flags.Enabled)
		return TRUE;

	_INF_UNIT(unit, "enabling...\n");
	ohci_EnableInterrupts(unit);

	/* Start Receive DMA contexts */
	ohci_ARContexts_Start(unit);
	//ohci_IsoContexts_Start(unit);

	/* force short bus reset */
	ohci_RaiseBusReset(unit, TRUE);
	
	unit->hu_Flags.Enabled = TRUE;
	return TRUE;
}
/* WARNING: does unprotected base's data accesses */
void
ohci_Disable(OHCI1394Unit *unit)
{
	if (!unit->hu_Flags.Enabled) return;

	_WRN_UNIT(unit, "disabling...\n");
	ohci_DisableInterrupts(unit);

	/* Stop all DMA contexts */
	ohci_ATContexts_Stop(unit);
	ohci_ARContexts_Stop(unit);
	///ohci_IsoCtx_StopAll(unit);
	
	/* Flush all pending transactions */
	ohci_TL_FlushAll(&unit->hu_TL);

	unit->hu_Flags.Enabled = FALSE;
}

/* WARNING: does unprotected base's data accesses */
LONG
ohci_ResetUnit(OHCI1394Unit *unit)
{
	if (unit->hu_Flags.Initialized)
		ohci_Term(unit);
		
	if (ohci_Init(unit))
	{
		if (ohci_Enable(unit))
		{
			unit->hu_Flags.Initialized = TRUE;
			return TRUE;
		}
		else
			ohci_Term(unit);
	}
	
	return FALSE;
}
#if 0
//+ ohci_HandleDeadContexts
void ohci_HandleDeadContexts(_HELIOS_Bus *bus)
{
	_OHCI1394_Chipset *ohci = (APTR) bus;

	LOCK_REGION(ohci);
	{
		/* Scan all contexts to find which ones are dead */
		if (ohci_Context_IsDead(&ohci->ARResponseCtx.Context)) {
			log_Error("AR Response context dead");
			// TODO
		}
		if (ohci_Context_IsDead(&ohci->ARRequestCtx.Context)) {
			log_Error("AR Request context dead");
			// TODO
		}
		if (ohci_Context_IsDead(&ohci->ATResponseCtx.Context)) {
			log_Error("AT Response context dead");
			// TODO
		}
		if (ohci_Context_IsDead(&ohci->ATRequestCtx.Context)) {
			log_Error("AT Request context dead");
			// TODO
		}

		// TODO: Isochronous contexts to check
	}
	UNLOCK_REGION(ohci);
}
//-
//+ Helios_DumpOHCI
void Helios_DumpOHCI(HeliosBus *_bus)
{
	int i;
	_HELIOS_Bus *bus = (APTR) _bus;
	_OHCI1394_Chipset *ohci = (APTR) _bus;

	dprintf("\t\n\t\nOHCI Reg 0x%08x ========\n", ohci->Registers);

	LOCK_REGION(ohci);
	{
		APTR ir_ctx;

		for (i=0; i < ARRAY_SIZE(ohci_reg_names); i++) {
			QUADLET q;
			ULONG offset = ohci_reg_names[i].Offset;


			q = ohci_RegRead((APTR)bus, offset);
			dprintf("$%03x - %25s : 0x%08x\n", offset, ohci_reg_names[i].Name, q);
		}

		dprintf("\t\nDumping IR contexts\n");
		ForeachNode(&ohci->IRCtxList, ir_ctx) {
			if (NULL != ir_ctx) {
				dprintf("IR Ctx %u-%p:\n", i, ir_ctx);
				dprintf("\tCtrl   : $%08x\n", ohci_RegRead(ohci, OHCI1394_REG_IRECV_CONTEXT_CONTROL(i)));
				dprintf("\tCmdPtr : $%08x\n", ohci_RegRead(ohci, OHCI1394_REG_IRECV_COMMAND_PTR(i)));
				dprintf("\tMatch  : $%08x\n", ohci_RegRead(ohci, OHCI1394_REG_IRECV_COMMAND_MATCH(i)));
				dprintf("----------------\n\t\n");

				iso_IRContext_Dump(ir_ctx, ohci->PciBoardObject);
			}
		}
	}
	UNLOCK_REGION(ohci);

	dprintf("\t\n");

	LOCK_REGION(bus);
	{
		ULONG tlabel;
		_HELIOS_Transaction **ptr_t;

		dprintf("Bus TLabels mask : $%016llx\n", bus->TLabelBitmap);
		dprintf("Dumping bus pending transactions array:\n");
		ptr_t = bus->PendingTransactions;
		for (tlabel=0; tlabel < 64; tlabel++, ptr_t++) {
			if (NULL == *ptr_t) continue;
			dprintf("[%02u] %p, ack %d, rcode %d\n", tlabel, *ptr_t, (*ptr_t)->RequestSA.Ack, (*ptr_t)->Response.RCode);
		}
		dprintf("\t\n");
	}
	UNLOCK_REGION(bus);

	dprintf("ATRequestCtx   usecount : %lu\n", ohci_Context_UseCount(&ohci->ATRequestCtx.Context));
	dprintf("ATResponseCtx  usecount : %lu\n", ohci_Context_UseCount(&ohci->ATResponseCtx.Context));
	dprintf("ARRequestCtx   usecount : %lu\n", ohci_Context_UseCount(&ohci->ARRequestCtx.Context));
	dprintf("ARResponseCtx  usecount : %lu\n", ohci_Context_UseCount(&ohci->ARResponseCtx.Context));

	LOCK_REGION(&ohci->ATRequestCtx.Context);
	{
		_OHCI1394_ATBuffer *buffer, *next;

		dprintf("\t\nDumping ATRequestCtx in-use buffers...");

		buffer = (APTR) ATOMIC_FETCH((ULONG *) &ohci->ATRequestCtx.LastBufferDone);
		if (NULL != buffer) {
			ULONG j, i, *mem;

			dprintf("\nDescriptors of the last done buffer %p:\n", buffer);

			for (j=0; j < 3; j++) {
				mem = (APTR) &buffer->Descriptors[j];
				for (i=0; i < sizeof(_OHCI1394_Descriptor); i += 4, mem++)
					dprintf("$%08x: %08x\n", PCIXDMAGetPhysical(ohci->PciBoardObject, mem), SWAPLONG(*mem));
			}
			dprintf("\t\n");
		}

		ForeachNodeSafe(&ohci->ATRequestCtx.Context.UsedList, buffer, next) {
			ULONG j, i, *mem;

			dprintf("\t\nDescriptors of buffer %p:\n", buffer);

			for (j=0; j < 3; j++) {
				mem = (APTR) &buffer->Descriptors[j];
				for (i=0; i < sizeof(_OHCI1394_Descriptor); i += 4, mem++)
					dprintf("$%08x: %08x\n", PCIXDMAGetPhysical(ohci->PciBoardObject, mem), SWAPLONG(*mem));
			}
		}
		dprintf("\t\n");
	}
	UNLOCK_REGION(&ohci->ATRequestCtx.Context);

	LOCK_REGION(&ohci->ARResponseCtx.Context);
	{
		_OHCI1394_ARBuffer *buf;
		ULONG i;

		buf = ohci->ARResponseCtx.Context.AlignedBigBufferStart;
		dprintf("Checking AR buffers... (already read: %u)\n",
			(APTR) ohci->ARResponseCtx.FirstQuadlet - (APTR) ohci->ARResponseCtx.FirstBuffer->Page);
		for (i=0; i < ARBUFFER_PAGE_COUNT; i++, buf++)
			dprintf("[%03u]: $%08x -> $%08x, len=%u, Status: $%04x %s%s\n", i,
				ohci_GetPhyAddress(&ohci->ARResponseCtx.Context, &buf->Descriptor),
				BE_SWAPLONG(buf->Descriptor.d_BranchAddress),
				ARBUFFER_PAGE_SIZE - BE_SWAPWORD(buf->Descriptor.d_ResCount),
				BE_SWAPWORD(buf->Descriptor.d_TransferStatus),
				buf == ohci->ARResponseCtx.FirstBuffer?"First ":"",
				buf == ohci->ARResponseCtx.LastBuffer?"Last":"");
	}
	UNLOCK_REGION(&ohci->ARResponseCtx.Context);

	dprintf("Done ===========================\n");
}
//-
#endif /* 0 */
/*=== EOF ====================================================================*/
