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
** Header file to define public OHCI global definitions.
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
*/

#ifndef OHCI1394_CORE_H
#define OHCI1394_CORE_H

#include "ohci1394.device.h"

#include <devices/timer.h>

#define OHCI1394_REGISTERS_SPACE_SIZE 2048

/* OHCI Register Mapping */
#define OHCI1394_REG_VERSION                     (0x000)
#define OHCI1394_REG_GUID_ROM                    (0x004)
#define OHCI1394_REG_AT_RETRIES                  (0x008)
#define OHCI1394_REG_CSR_READ_DATA               (0x00C)
#define OHCI1394_REG_CSR_WRITE_DATA              (0x00C)
#define OHCI1394_REG_CSR_COMPARE_DATA            (0x010)
#define OHCI1394_REG_CSR_CONTROL                 (0x014)
#define OHCI1394_REG_CONFIG_ROM_HDR              (0x018)
#define OHCI1394_REG_BUS_ID                      (0x01C)
#define OHCI1394_REG_BUS_OPTIONS                 (0x020)
#define OHCI1394_REG_GUID_HI                     (0x024)
#define OHCI1394_REG_GUID_LO                     (0x028)
//          --- RESERVED ---                     (0x02C)
//          --- RESERVED ---                     (0x030)
#define OHCI1394_REG_CONFIG_ROM_MAP              (0x034)
#define OHCI1394_REG_PWRITE_ADDR_LO              (0x038)
#define OHCI1394_REG_PWRITE_ADDR_HI              (0x03C)
#define OHCI1394_REG_VENDOR_ID                   (0x040)
//          --- RESERVED ---                     (0x044)
//          --- RESERVED ---                     (0x048)
//          --- RESERVED ---                     (0x04C)
#define OHCI1394_REG_HC_CONTROL                  (0x050)
#define OHCI1394_REG_HC_CONTROL_SET              (0x050)
#define OHCI1394_REG_HC_CONTROL_CLEAR            (0x054)
//          --- RESERVED ---                     (0x058)
//          --- RESERVED ---                     (0x05C)
//          --- RESERVED ---                     (0x060)
#define OHCI1394_REG_SELFID_BUFFER               (0x064)
#define OHCI1394_REG_SELFID_COUNT                (0x068)
//          --- RESERVED ---                     (0x06C)
//          --- RESERVED ---                     (0x06D)
//          --- RESERVED ---                     (0x06E)
//          --- RESERVED ---                     (0x06F)
#define OHCI1394_REG_IR_MULTICHAN_MASK_HI        (0x070)
#define OHCI1394_REG_IR_MULTICHAN_MASK_HI_SET    (0x070)
#define OHCI1394_REG_IR_MULTICHAN_MASK_HI_CLEAR  (0x074)
#define OHCI1394_REG_IR_MULTICHAN_MASK_LO        (0x078)
#define OHCI1394_REG_IR_MULTICHAN_MASK_LO_SET    (0x078)
#define OHCI1394_REG_IR_MULTICHAN_MASK_LO_CLEAR  (0x07C)
#define OHCI1394_REG_INT_EVENT                   (0x080)
#define OHCI1394_REG_INT_EVENT_SET               (0x080)
#define OHCI1394_REG_INT_EVENT_CLEAR             (0x084)
#define OHCI1394_REG_INT_MASK                    (0x088)
#define OHCI1394_REG_INT_MASK_SET                (0x088)
#define OHCI1394_REG_INT_MASK_CLEAR              (0x08C)
#define OHCI1394_REG_ISO_XMIT_INT_EVENT          (0x090)
#define OHCI1394_REG_ISO_XMIT_INT_EVENT_SET      (0x090)
#define OHCI1394_REG_ISO_XMIT_INT_EVENT_CLEAR    (0x094)
#define OHCI1394_REG_ISO_XMIT_INT_MASK           (0x098)
#define OHCI1394_REG_ISO_XMIT_INT_MASK_SET       (0x098)
#define OHCI1394_REG_ISO_XMIT_INT_MASK_CLEAR     (0x09C)
#define OHCI1394_REG_ISO_RECV_INT_EVENT          (0x0A0)
#define OHCI1394_REG_ISO_RECV_INT_EVENT_SET      (0x0A0)
#define OHCI1394_REG_ISO_RECV_INT_EVENT_CLEAR    (0x0A4)
#define OHCI1394_REG_ISO_RECV_INT_MASK           (0x0A8)
#define OHCI1394_REG_ISO_RECV_INT_MASK_SET       (0x0A8)
#define OHCI1394_REG_ISO_RECV_INT_MASK_CLEAR     (0x0AC)
#define OHCI1394_REG_INITIAL_BW_AVAILABLE        (0x0B0)
#define OHCI1394_REG_INITIAL_CHAN_AVAILABLE_HI   (0x0B4)
#define OHCI1394_REG_INITIAL_CHAN_AVAILABLE_LO   (0x0B8)
//          --- RESERVED ---                     (0x0BC)
//          --- RESERVED ---                     (0x0D0)
//          --- RESERVED ---                     (0x0D4)
//          --- RESERVED ---                     (0x0D8)
#define OHCI1394_REG_FAIRNESS_CONTROL            (0x0DC)
#define OHCI1394_REG_LINK_CONTROL                (0x0E0)
#define OHCI1394_REG_LINK_CONTROL_SET            (0x0E0)
#define OHCI1394_REG_LINK_CONTROL_CLEAR          (0x0E4)
#define OHCI1394_REG_NODE_ID                     (0x0E8)
#define OHCI1394_REG_PHY_CONTROL                 (0x0EC)
#define OHCI1394_REG_ISOCHRONOUS_CYCLE_TIMER     (0x0F0)
//          --- RESERVED ---                     (0x0F4)
//          --- RESERVED ---                     (0x0F8)
//          --- RESERVED ---                     (0x0FC)
#define OHCI1394_REG_ASYNCH_REQ_FILTER_HI        (0x100)
#define OHCI1394_REG_ASYNCH_REQ_FILTER_HI_SET    (0x100)
#define OHCI1394_REG_ASYNCH_REQ_FILTER_HI_CLEAR  (0x104)
#define OHCI1394_REG_ASYNCH_REQ_FILTER_LO        (0x108)
#define OHCI1394_REG_ASYNCH_REQ_FILTER_LO_SET    (0x108)
#define OHCI1394_REG_ASYNCH_REQ_FILTER_LO_CLEAR  (0x10C)
#define OHCI1394_REG_PHYREQ_REQ_FILTER_HI        (0x110)
#define OHCI1394_REG_PHYREQ_REQ_FILTER_HI_SET    (0x110)
#define OHCI1394_REG_PHYREQ_REQ_FILTER_HI_CLEAR  (0x114)
#define OHCI1394_REG_PHYREQ_REQ_FILTER_LO        (0x118)
#define OHCI1394_REG_PHYREQ_REQ_FILTER_LO_SET    (0x118)
#define OHCI1394_REG_PHYREQ_REQ_FILTER_LO_CLEAR  (0x11C)
#define OHCI1394_REG_PHYSICAL_UPPER_BOUND        (0x120)
//          --- RESERVED ---                     (0x124)
//                                               (...)
//          --- RESERVED ---                     (0x17C)
#define OHCI1394_REG_AREQT_CONTEXT_CONTROL       (0x180)
#define OHCI1394_REG_AREQT_CONTEXT_CONTROL_SET   (0x180)
#define OHCI1394_REG_AREQT_CONTEXT_CONTROL_CLEAR (0x184)
//          --- RESERVED ---                     (0x188)
#define OHCI1394_REG_AREQT_COMMANDPTR            (0x18C)
//          --- RESERVED ---                     (0x190)
//                                               (...)
//          --- RESERVED ---                     (0x19C)
#define OHCI1394_REG_AREST_CONTEXT_CONTROL       (0x1A0)
#define OHCI1394_REG_AREST_CONTEXT_CONTROL_SET   (0x1A0)
#define OHCI1394_REG_AREST_CONTEXT_CONTROL_CLEAR (0x1A4)
//          --- RESERVED ---                     (0x1A8)
#define OHCI1394_REG_AREST_COMMAND_PTR           (0x1AC)
//          --- RESERVED ---                     (0x1B0)
//          --- RESERVED ---                     (0x1B4)
//          --- RESERVED ---                     (0x1B8)
//          --- RESERVED ---                     (0x1BC)
#define OHCI1394_REG_AREQR_CONTEXT_CONTROL       (0x1C0)
#define OHCI1394_REG_AREQR_CONTEXT_CONTROL_SET   (0x1C0)
#define OHCI1394_REG_AREQR_CONTEXT_CONTROL_CLEAR (0x1C4)
//          --- RESERVED ---                     (0x1C8)
#define OHCI1394_REG_AREQR_COMMAND_PTR           (0x1CC)
//          --- RESERVED ---                     (0x1D0)
//          --- RESERVED ---                     (0x1D4)
//          --- RESERVED ---                     (0x1D8)
//          --- RESERVED ---                     (0x1DC)
#define OHCI1394_REG_ARESR_CONTEXT_CONTROL       (0x1E0)
#define OHCI1394_REG_ARESR_CONTEXT_CONTROL_SET   (0x1E0)
#define OHCI1394_REG_ARESR_CONTEXT_CONTROL_CLEAR (0x1E4)
//          --- RESERVED ---                     (0x1E8)
#define OHCI1394_REG_ARESR_COMMAND_PTR           (0x1EC)
//          --- RESERVED ---                     (0x1F0)
//          --- RESERVED ---                     (0x1F4)
//          --- RESERVED ---                     (0x1F8)
//          --- RESERVED ---                     (0x1FC)

/* Isochronous transmit context registers */
#define OHCI1394_REG_IXMIT_CONTEXT_CONTROL(n)       (0x200 + 16 * (n))
#define OHCI1394_REG_IXMIT_CONTEXT_CONTROL_SET(n)   (0x200 + 16 * (n))
#define OHCI1394_REG_IXMIT_CONTEXT_CONTROL_CLEAR(n) (0x204 + 16 * (n))
//          --- RESERVED ---                        (0x208 + 16 * (n))
#define OHCI1394_REG_IXMIT_COMMAND_PTR(n)           (0x20C + 16 * (n))

/* Isochronous receive context registers */
#define OHCI1394_REG_IRECV_CONTEXT_CONTROL(n)       (0x400 + 32 * (n))
#define OHCI1394_REG_IRECV_CONTEXT_CONTROL_SET(n)   (0x400 + 32 * (n))
#define OHCI1394_REG_IRECV_CONTEXT_CONTROL_CLEAR(n) (0x404 + 32 * (n))
//          --- RESERVED ---                        (0x408 + 32 * (n))
#define OHCI1394_REG_IRECV_COMMAND_PTR(n)           (0x40C + 32 * (n))
#define OHCI1394_REG_IRECV_COMMAND_MATCH(n)         (0x410 + 32 * (n))

/* HC Control register bits */
#define OHCI1394_HCCB_BIBIMAGEVALID      31
#define OHCI1394_HCCB_NOBYTESWAPDATA     30
#define OHCI1394_HCCB_ACKTARDYENABLE     29
#define OHCI1394_HCCB_PROGRAMPHYENABLE   23
#define OHCI1394_HCCB_APHYENHANCEENABLE  22
#define OHCI1394_HCCB_LPS                19
#define OHCI1394_HCCB_POSTEDWRITEENABLE  18
#define OHCI1394_HCCB_LINKENABLE         17
#define OHCI1394_HCCB_SOFTRESET          16

#define OHCI1394_HCCF_BIBIMAGEVALID      (1<<OHCI1394_HCCB_BIBIMAGEVALID)
#define OHCI1394_HCCF_NOBYTESWAPDATA     (1<<OHCI1394_HCCB_NOBYTESWAPDATA)
#define OHCI1394_HCCF_ACKTARDYENABLE     (1<<OHCI1394_HCCB_ACKTARDYENABLE)
#define OHCI1394_HCCF_PROGRAMPHYENABLE   (1<<OHCI1394_HCCB_PROGRAMPHYENABLE)
#define OHCI1394_HCCF_APHYENHANCEENABLE  (1<<OHCI1394_HCCB_APHYENHANCEENABLE)
#define OHCI1394_HCCF_LPS                (1<<OHCI1394_HCCB_LPS)
#define OHCI1394_HCCF_POSTEDWRITEENABLE  (1<<OHCI1394_HCCB_POSTEDWRITEENABLE)
#define OHCI1394_HCCF_LINKENABLE         (1<<OHCI1394_HCCB_LINKENABLE)
#define OHCI1394_HCCF_SOFTRESET          (1<<OHCI1394_HCCB_SOFTRESET)

/* Interrupts Mask/Events registers bits */
#define OHCI1394_INTB_REQTXCOMPLETE          0
#define OHCI1394_INTB_RESPTXCOMPLETE         1
#define OHCI1394_INTB_ARRQ                   2
#define OHCI1394_INTB_ARRS                   3
#define OHCI1394_INTB_RQPKT                  4
#define OHCI1394_INTB_RSPKT                  5
#define OHCI1394_INTB_ISOCHTX                6
#define OHCI1394_INTB_ISOCHRX                7
#define OHCI1394_INTB_POSTEDWRITEERR         8
#define OHCI1394_INTB_LOCKRESPERR            9
#define OHCI1394_INTB_SELFIDCOMPLETE2       15
#define OHCI1394_INTB_SELFIDCOMPLETE        16
#define OHCI1394_INTB_BUSRESET              17
#define OHCI1394_INTB_REGACCESSFAIL         18
#define OHCI1394_INTB_PHY                   19
#define OHCI1394_INTB_CYCLESYNCH            20
#define OHCI1394_INTB_CYCLE64SECONDS        21
#define OHCI1394_INTB_CYCLELOST             22
#define OHCI1394_INTB_CYCLEINCONSISTENT     23
#define OHCI1394_INTB_UNRECOVERABLEERROR    24
#define OHCI1394_INTB_CYCLETOOLONG          25
#define OHCI1394_INTB_PHYREGRCVD            26
#define OHCI1394_INTB_ACKTARDY              27
#define OHCI1394_INTB_SOFTINTERRUPT         29
#define OHCI1394_INTB_VENDORSPECIFIC        30
#define OHCI1394_INTB_MASTERINTENABLE       31

#define OHCI1394_INTF_REQTXCOMPLETE         (1<<OHCI1394_INTB_REQTXCOMPLETE)
#define OHCI1394_INTF_RESPTXCOMPLETE        (1<<OHCI1394_INTB_RESPTXCOMPLETE)
#define OHCI1394_INTF_ARRQ                  (1<<OHCI1394_INTB_ARRQ)
#define OHCI1394_INTF_ARRS                  (1<<OHCI1394_INTB_ARRS)
#define OHCI1394_INTF_RQPKT                 (1<<OHCI1394_INTB_RQPKT)
#define OHCI1394_INTF_RSPKT                 (1<<OHCI1394_INTB_RSPKT)
#define OHCI1394_INTF_ISOCHTX               (1<<OHCI1394_INTB_ISOCHTX)
#define OHCI1394_INTF_ISOCHRX               (1<<OHCI1394_INTB_ISOCHRX)
#define OHCI1394_INTF_POSTEDWRITEERR        (1<<OHCI1394_INTB_POSTEDWRITEERR)
#define OHCI1394_INTF_LOCKRESPERR           (1<<OHCI1394_INTB_LOCKRESPERR)
#define OHCI1394_INTF_SELFIDCOMPLETE2       (1<<OHCI1394_INTB_SELFIDCOMPLETE2)
#define OHCI1394_INTF_SELFIDCOMPLETE        (1<<OHCI1394_INTB_SELFIDCOMPLETE)
#define OHCI1394_INTF_BUSRESET              (1<<OHCI1394_INTB_BUSRESET)
#define OHCI1394_INTF_REGACCESSFAIL         (1<<OHCI1394_INTB_REGACCESSFAIL)
#define OHCI1394_INTF_PHY                   (1<<OHCI1394_INTB_PHY)
#define OHCI1394_INTF_CYCLESYNCH            (1<<OHCI1394_INTB_CYCLESYNCH)
#define OHCI1394_INTF_CYCLE64SECONDS        (1<<OHCI1394_INTB_CYCLE64SECONDS)
#define OHCI1394_INTF_CYCLELOST             (1<<OHCI1394_INTB_CYCLELOST)
#define OHCI1394_INTF_CYCLEINCONSISTENT     (1<<OHCI1394_INTB_CYCLEINCONSISTENT)
#define OHCI1394_INTF_UNRECOVERABLEERROR    (1<<OHCI1394_INTB_UNRECOVERABLEERROR)
#define OHCI1394_INTF_CYCLETOOLONG          (1<<OHCI1394_INTB_CYCLETOOLONG)
#define OHCI1394_INTF_PHYREGRCVD            (1<<OHCI1394_INTB_PHYREGRCVD)
#define OHCI1394_INTF_ACKTARDY              (1<<OHCI1394_INTB_ACKTARDY)
#define OHCI1394_INTF_SOFTINTERRUPT         (1<<OHCI1394_INTB_SOFTINTERRUPT)
#define OHCI1394_INTF_VENDORSPECIFIC        (1<<OHCI1394_INTB_VENDORSPECIFIC)
#define OHCI1394_INTF_MASTERINTENABLE       (1<<OHCI1394_INTB_MASTERINTENABLE)

/* LinkControl register bits */
#define OHCI1394_LCB_TAG1SYNCFILTERLOCK  6
#define OHCI1394_LCB_RCVSELFID           9
#define OHCI1394_LCB_RCVPHYPKT          10
#define OHCI1394_LCB_CYCLETIMERENABLE   20
#define OHCI1394_LCB_CYCLEMASTER        21
#define OHCI1394_LCB_CYCLESOURCE        22

#define OHCI1394_LCF_TAG1SYNCFILTERLOCK (1<<OHCI1394_LCB_TAG1SYNCFILTERLOCK)
#define OHCI1394_LCF_RCVSELFID          (1<<OHCI1394_LCB_RCVSELFID)
#define OHCI1394_LCF_RCVPHYPKT          (1<<OHCI1394_LCB_RCVPHYPKT)
#define OHCI1394_LCF_CYCLETIMERENABLE   (1<<OHCI1394_LCB_CYCLETIMERENABLE)
#define OHCI1394_LCF_CYCLEMASTER        (1<<OHCI1394_LCB_CYCLEMASTER)
#define OHCI1394_LCF_CYCLESOURCE        (1<<OHCI1394_LCB_CYCLESOURCE)

/* Phy Control register bits */
#define OHCI1394_PCB_WR_DATA     0
#define OHCI1394_PCB_REG_ADDR    8
#define OHCI1394_PCB_WR_REG     14
#define OHCI1394_PCB_RD_REG     15
#define OHCI1394_PCB_RD_DATA    16
#define OHCI1394_PCB_RD_ADDR    24
#define OHCI1394_PCB_RD_DONE    31

#define OHCI1394_PCF_WR_REG     (1<<OHCI1394_PCB_WR_REG)
#define OHCI1394_PCF_RD_REG     (1<<OHCI1394_PCB_RD_REG)
#define OHCI1394_PCF_RD_DONE    (1<<OHCI1394_PCB_RD_DONE)
#define OHCI1394_PCF_READ(a)    ((((a) & 0x0f) << OHCI1394_PCB_REG_ADDR) | OHCI1394_PCF_RD_REG)
#define OHCI1394_PCF_WRITE(a,d) ((((a) & 0x0f) << OHCI1394_PCB_REG_ADDR) | ((d) << OHCI1394_PCB_WR_DATA) | OHCI1394_PCF_WR_REG)

#define OHCI1394_PC_READ_DATA(r)    (((r) >> OHCI1394_PCB_RD_DATA) & 0xff)
#define OHCI1394_PC_READ_ADDR(r)    (((r) >> OHCI1394_PCB_RD_ADDR) & 0x0f)

/* NodeID register bits */
#define OHCI1394_NODEIDB_NODENUMBER          0
#define OHCI1394_NODEIDB_BUSNUMBER           6
#define OHCI1394_NODEIDB_CABLEPOWERSTATUS   27
#define OHCI1394_NODEIDB_ROOT               30
#define OHCI1394_NODEIDB_IDVALID            31

#define OHCI1394_NODEIDF_CABLEPOWERSTATUS   (1<<OHCI1394_NODEIDB_CABLEPOWERSTATUS)
#define OHCI1394_NODEIDF_ROOT               (1<<OHCI1394_NODEIDB_ROOT)
#define OHCI1394_NODEIDF_IDVALID            (1<<OHCI1394_NODEIDB_IDVALID)

#define OHCI1394_NODEID_NODENUMBER_MASK     (0x3f << OHCI1394_NODEIDB_NODENUMBER)
#define OHCI1394_NODEID_BUSNUMBER_MASK      (0x3ff << OHCI1394_NODEIDB_BUSNUMBER)

#define OHCI1394_NODEID_NODENUMBER(r)       (((r) & OHCI1394_NODEID_NODENUMBER_MASK) >> OHCI1394_NODEIDB_NODENUMBER)
#define OHCI1394_NODEID_BUSNUMBER(r)        (((r) & OHCI1394_NODEID_BUSNUMBER_MASK) >> OHCI1394_NODEIDB_BUSNUMBER)

#define OHCI1394_LOCAL_BUS                  OHCI1394_NODEID_BUSNUMBER_MASK

/* SelfID Count register bits */
#define OHCI1394_SELFIDCOUNTB_SELFIDSIZE             2
#define OHCI1394_SELFIDCOUNTB_SELFIDGENERATION      16
#define OHCI1394_SELFIDCOUNTB_SELFIDERROR           31

#define OHCI1394_SELFIDCOUNTF_SELFIDERROR           (1<<OHCI1394_SELFIDCOUNTB_SELFIDERROR)

#define OHCI1394_SELFIDCOUNT_SELFIDSIZE_MASK        (0x1ff << OHCI1394_SELFIDCOUNTB_SELFIDSIZE)
#define OHCI1394_SELFIDCOUNT_SELFIDGENERATION_MASK  (0xff << OHCI1394_SELFIDCOUNTB_SELFIDGENERATION)

#define OHCI1394_SELFIDCOUNT_SELFIDSIZE(r)          (((r) & OHCI1394_SELFIDCOUNT_SELFIDSIZE_MASK) >> OHCI1394_SELFIDCOUNTB_SELFIDSIZE)
#define OHCI1394_SELFIDCOUNT_SELFIDGENERATION(r)    (((r) & OHCI1394_SELFIDCOUNT_SELFIDGENERATION_MASK) >> OHCI1394_SELFIDCOUNTB_SELFIDGENERATION)

/* PHY register bits */
#define PHYF_LINK_ACTIVE        (0x80)
#define PHYF_CONTENDER          (0x40)
#define PHYF_BUS_RESET          (0x40)
#define PHYF_SHORT_BUS_RESET    (0x40)

#define OHCI1394_CSRCTRLB_DONE 31
#define OHCI1394_CSRCTRLF_DONE (1 << OHCI1394_CSRCTRLB_DONE)

#define OHCI1394_EVENT_NO_STATUS        (0x00) // AT AR IT IR
#define OHCI1394_EVENT_LONG_PACKET      (0x02) //          IR
#define OHCI1394_EVENT_MISSING_ACK      (0x03) // AT
#define OHCI1394_EVENT_UNDERRUN         (0x04) // AT
#define OHCI1394_EVENT_OVERRUN          (0x05) //          IR
#define OHCI1394_EVENT_DESCRIPTOR_READ  (0x06) // AT AR IT IR
#define OHCI1394_EVENT_DATA_READ        (0x07) // AT    IT
#define OHCI1394_EVENT_DATA_WRITE       (0x08) //    AR IT IR
#define OHCI1394_EVENT_BUS_RESET        (0x09) //    AR
#define OHCI1394_EVENT_TIMEOUT          (0x0a) // AT    IT
#define OHCI1394_EVENT_TCODE_ERR        (0x0b) // AT    IT
#define OHCI1394_EVENT_UNKNOWN          (0x0e) // AT AR IT IR
#define OHCI1394_EVENT_FLUSHED          (0x0f) // AT
#define OHCI1394_EVENT_ACK_COMPLETE     (0x11)

/* Descriptors */
#define DESCRIPTOR_OUTPUT_MORE      (0)
#define DESCRIPTOR_OUTPUT_LAST      (1 << 12)
#define DESCRIPTOR_INPUT_MORE       (2 << 12)
#define DESCRIPTOR_INPUT_LAST       (3 << 12)
#define DESCRIPTOR_STATUS           (1 << 11)
#define DESCRIPTOR_KEY_IMMEDIATE    (2 << 8)
#define DESCRIPTOR_PING             (1 << 7)
#define DESCRIPTOR_YY               (1 << 6)
#define DESCRIPTOR_NO_IRQ           (0 << 4)
#define DESCRIPTOR_IRQ_ERROR        (1 << 4)
#define DESCRIPTOR_IRQ_ALWAYS       (3 << 4)
#define DESCRIPTOR_BRANCH_ALWAYS    (3 << 2)
#define DESCRIPTOR_WAIT             (3 << 0)

#define CTX_RUN     (1<<15)
#define CTX_WAKE    (1<<12)
#define CTX_DEAD    (1<<11)
#define CTX_ACTIVE  (1<<10)

#define AT_HEADER_TCODE_SHIFT 4
#define AT_HEADER_TCODE_MSK (0xf)

#define AT_HEADER_TLABEL_SHIFT 10
#define AT_HEADER_TLABEL_MSK (0x3f)

#define AT_HEADER_RT_SHIFT 8
#define AT_HEADER_RT_MSK (0xf)

#define AT_HEADER_SPEED_SHIFT 16
#define AT_HEADER_SPEED_MSK (0x7)

#define AT_HEADER_DEST_ID_SHIFT 16
#define AT_HEADER_DEST_ID_MSK (0xffff)

#define AT_HEADER_SOURCE_ID_SHIFT 16
#define AT_HEADER_SOURCE_ID_MSK (0xffff)

#define AT_HEADER_EXTCODE_SHIFT 0
#define AT_HEADER_EXTCODE_MSK (0xffff)

#define AT_HEADER_LEN_SHIFT 16
#define AT_HEADER_LEN_MSK (0xffff)

#define AT_HEADER_RCODE_SHIFT 12
#define AT_HEADER_RCODE_MSK (0xf)

#define AT_GET_HEADER_TCODE(x)          (((QUADLET)(x) >> AT_HEADER_TCODE_SHIFT) & AT_HEADER_TCODE_MSK)
#define AT_GET_HEADER_TLABEL(x)         (((QUADLET)(x) >> AT_HEADER_TLABEL_SHIFT) & AT_HEADER_TLABEL_MSK)
#define AT_GET_HEADER_RT(x)             (((QUADLET)(x) >> AT_HEADER_RT_SHIFT) & AT_HEADER_RT_MSK)
#define AT_GET_HEADER_SPEED(x)          (((QUADLET)(x) >> AT_HEADER_SPEED_SHIFT) & AT_HEADER_SPEED_MSK)
#define AT_GET_HEADER_DEST_ID(x)        (((QUADLET)(x) >> AT_HEADER_DEST_ID_SHIFT) & AT_HEADER_DEST_ID_MSK)
#define AT_GET_HEADER_EXTCODE(x)        (((QUADLET)(x) >> AT_HEADER_EXTCODE_SHIFT) & AT_HEADER_EXTCODE_MSK)
#define AT_GET_HEADER_LEN(x)            (((QUADLET)(x) >> AT_HEADER_LEN_SHIFT) & AT_HEADER_LEN_MSK)
#define AT_GET_HEADER_SOURCE_ID(x)      (((QUADLET)(x) >> AT_HEADER_SOURCE_ID_SHIFT) & AT_HEADER_SOURCE_ID_MSK)
#define AT_GET_HEADER_RCODE(x)          (((QUADLET)(x) >> AT_HEADER_RCODE_SHIFT) & AT_HEADER_RCODE_MSK)

#define AT_HEADER_TLABEL(x)  (((QUADLET)(x) & AT_HEADER_TLABEL_MSK) << AT_HEADER_TLABEL_SHIFT)
#define AT_HEADER_DEST_ID(x) (((QUADLET)(x) & AT_HEADER_DEST_ID_MSK) << AT_HEADER_DEST_ID_SHIFT)
#define AT_HEADER_LEN(x)     (((QUADLET)(x) & AT_HEADER_LEN_MSK) << AT_HEADER_LEN_SHIFT)
#define AT_HEADER_SPEED(x)   (((QUADLET)(x) & AT_HEADER_SPEED_MSK) << AT_HEADER_SPEED_SHIFT)
#define AT_HEADER_RT(x)      (((QUADLET)(x) & AT_HEADER_RT_MSK) << AT_HEADER_RT_SHIFT)
#define AT_HEADER_TCODE(x)   (((QUADLET)(x) & AT_HEADER_TCODE_MSK) << AT_HEADER_TCODE_SHIFT)
#define AT_HEADER_EXTCODE(x) (((QUADLET)(x) & AT_HEADER_EXTCODE_MSK) << AT_HEADER_EXTCODE_SHIFT)

#define TLABEL_MAX 64 /* TLabel is on 6bits */
#define MAX_NODES 63 /* Max nodes per bus, NodeID=0x3f is reserved as broadcast id */

#define OHCI1394_PKTF_REQUEST (1<<0)
#define OHCI1394_PKTF_4QH     (1<<1)

#define LOCK_CTX(c) EXCLUSIVE_PROTECT_BEGIN((OHCI1394Context *)(c))
#define UNLOCK_CTX(c) EXCLUSIVE_PROTECT_END((OHCI1394Context *)(c))

#include "debug.h"

#define DH_UNIT "<ohci-%d> "

#define _ERR_UNIT(_unit, _fmt, _a...) _ERR(DH_UNIT _fmt, (_unit)->hu_UnitNo, ##_a)
#define _WRN_UNIT(_unit, _fmt, _a...) _WRN(DH_UNIT _fmt, (_unit)->hu_UnitNo, ##_a)
#define _INF_UNIT(_unit, _fmt, _a...) _INF(DH_UNIT _fmt, (_unit)->hu_UnitNo, ##_a)
#define _DBG_UNIT(_unit, _fmt, _a...) _DBG(DH_UNIT _fmt, (_unit)->hu_UnitNo, ##_a)

/*============================================================================*/
/*--- STRUCTURES -------------------------------------------------------------*/
/*============================================================================*/

struct OHCI1394Unit;
struct OHCI1394Transaction;
struct OHCI1394ATCtx;
struct OHCI1394ATData;
struct OHCI1394ATBuffer;
struct OHCI1394IRBuffer;
struct OHCI1394Context;
struct OHCI1394TLayer;

typedef void (*OHCI1394ATCompleteCallback)(
	struct OHCI1394TLayer *tl,
	BYTE ack, UWORD timestamp,
	struct OHCI1394ATData *pd);
	
typedef void (*OHCI1394TransactionCompleteFunc)(
	HeliosPacket *request,
	HeliosPacket *response,
	APTR udata);

typedef union HCC {
	QUADLET value;

	/* Keep following structure as it */
	struct {
		/* bits from 31 to 0 */
		QUADLET BIBimageValid	: 1;
		QUADLET NoByteSwapData	: 1;
		QUADLET AckTardyEnable	: 1;
		QUADLET Reserved0		: 5;
		QUADLET PrgPhyEnable	: 1;
		QUADLET APhyEhcEnable	: 1;
		QUADLET Reserved1		: 2;
		QUADLET LinkPowerStatus	: 1;
		QUADLET PWriteEnable	: 1;
		QUADLET LinkEnable		: 1;
		QUADLET SoftReset		: 1;
		QUADLET Reserved2		: 16;
	};
} HCC;

typedef struct OHCI1394ATData
{
	struct OHCI1394ATCtx *		atd_Ctx;
	struct OHCI1394ATBuffer *	atd_Buffer;
	OHCI1394ATCompleteCallback	atd_AckCallback;
	APTR						atd_UData;
} OHCI1394ATData;

/* Packet TLabel field is encoded on 6 bits, this gives 64 differents packets
 * handled on one bus in same time (TLABEL_MAX).
 * Following structure is used in unit structure as entries of array limited
 * to 64 items.
 * Each transaction is fully re-usable. When the system run out of items
 * the Transaction Layer issues an error to user.
 */
typedef struct OHCI1394Transaction {
	struct timerequest				t_TReq;				/* Base: used for timeout */
	LOCK_PROTO;
	OHCI1394ATData					t_ATData;			/* Data used by AT Ack phase */
	HeliosPacket *					t_Packet;			/* Given by user */
	OHCI1394TransactionCompleteFunc	t_CompleteFunc;		/* Called when transaction is complet */
	APTR							t_UserData;
} OHCI1394Transaction;

typedef struct OHCI1394TLayer {
	LOCK_PROTO;
	struct OHCI1394Unit *	tl_Unit;						/* Up link */
	OHCI1394Transaction		tl_Transactions[TLABEL_MAX];	/* Async transactions array */
	UQUAD					tl_TLabelMap;					/* Transactions array usage mapping */
	struct
	{
		ULONG tl_LastTLabel:6;	/* Last used TLabel */
	};
	LOCKED_MINLIST_PROTO(tl_ReqHandlerList);
} OHCI1394TLayer;

typedef struct OHCI1394Descriptor {
	u_int16_t	d_ReqCount;
	u_int16_t	d_Control;
	u_int32_t	d_DataAddress;
	u_int32_t	d_BranchAddress;
	u_int16_t	d_ResCount;
	u_int16_t	d_TransferStatus;
} OHCI1394Descriptor __attribute__((aligned(16)));

#define d_TimeStamp d_ResCount

typedef struct OHCI1394ATBuffer
{
	struct MinNode				atb_Node;
	OHCI1394ATData *			atb_PacketData;
	OHCI1394Descriptor			atb_Descriptors[3];
	OHCI1394Descriptor *		atb_LastDescriptor;
} OHCI1394ATBuffer __attribute__((aligned(16)));

/* The real size of this buffer is known only after the OHCI init */
typedef struct OHCI1394ARBuffer {
	struct MinNode				arb_Node;
	QUADLET *					arb_Page;
	ULONG						arb_Pad0;
	OHCI1394Descriptor			arb_Descriptor; /* DMA INPUT_MORE descriptor */
} OHCI1394ARBuffer __attribute__((aligned(16)));


typedef struct OHCI1394IRBuffer {
	OHCI1394Descriptor			irb_Descriptors[2];
	struct OHCI1394IRBuffer *	irb_Next;
	HeliosIRBuffer				irb_BufferData;
} OHCI1394IRBuffer __attribute__((aligned(16)));

/* Common Context structure */
typedef void (*OHCI1394CtxHandler) (struct OHCI1394Context *ctx);

typedef struct OHCI1394Context {
	struct MinNode			ctx_SysNode;
	LOCK_PROTO;

	struct OHCI1394Unit *	ctx_Unit;
	ULONG					ctx_RegOffset;
	OHCI1394CtxHandler		ctx_Handler;
	HeliosSubTask *			ctx_SubTask;
	ULONG					ctx_Signal;
} OHCI1394Context;

typedef struct OHCI1394ATCtx {
	OHCI1394Context		atc_Context;
	ULONG				atc_CommandPtr;

	APTR				atc_AllocDMABuffers;
	OHCI1394ATBuffer *	atc_CpuDMABuffers;
	ULONG				atc_PhyDMABuffers;
	struct MinList		atc_BufferList;
	ULONG				atc_BufferUsage;
	struct MinList		atc_UsedBufferList;
	struct MinList		atc_DeadBufferList;
	OHCI1394ATBuffer *	atc_LastBuffer;
} OHCI1394ATCtx;

typedef struct OHCI1394ARCtx {
	OHCI1394Context		arc_Context;

	APTR				arc_AllocDMABuffers;
	OHCI1394ARBuffer *	arc_CpuDMABuffers;
	ULONG				arc_PhyDMABuffers;
	OHCI1394ARBuffer *	arc_RealLastBuffer;		// Last buffer in the block of ARBuffer.

	OHCI1394ARBuffer *	arc_FirstBuffer;		// First DMA block that contains the first AR DMA descriptors of the AR DMA program
	OHCI1394ARBuffer *	arc_LastBuffer;			// Last buffer that contains the last AR DMA descriptors of the AR DMA program (Z=0)

	QUADLET *			arc_Pages;
	QUADLET *			arc_FirstQuadlet;		// first quadlet to read inside the first page of the current DMA block
} OHCI1394ARCtx;

typedef struct OHCI1394IsoCtxBase {
	OHCI1394Context		ic_Context;
	ULONG				ic_Index;
} OHCI1394IsoCtxBase;

typedef struct OHCI1394IRCtxFlags {
	ULONG DropEmpty:1;
} OHCI1394IRCtxFlags;

typedef struct OHCI1394IRCtx {
	OHCI1394IsoCtxBase		irc_Base;
	UBYTE					irc_HeaderPadding;
	UBYTE					irc_Channel;
	UBYTE					irc_Reserved[2];

	OHCI1394IRCtxFlags		irc_Flags;
	HeliosIRCallback		irc_Callback;
	APTR					irc_UserData;
	ULONG					irc_HeaderLength;
	ULONG					irc_PayloadLength;

	ULONG					irc_DMABufferCount;
	APTR					irc_DMABuffer;
	OHCI1394IRBuffer *		irc_AlignedDMABuffer;
	OHCI1394IRBuffer *		irc_FirstDMABuffer;
	OHCI1394IRBuffer *		irc_LastDMABuffer;
	OHCI1394IRBuffer *		irc_RealLastDMABuffer;

	APTR					irc_PageBuffer;
	APTR					irc_AlignedPageBuffer;
} OHCI1394IRCtx;

typedef struct OHCI1394ITCtx {
	OHCI1394IsoCtxBase itc_Base;
} OHCI1394ITCtx;

typedef struct OHCI1394Flags {
	ULONG Initialized		:1;
	ULONG Enabled			:1;
	ULONG UnrecoverableError:1;
} OHCI1394Flags;

typedef struct OHCI1394Unit {
	struct Unit				hu_Unit;					/* Base */
	LONG					hu_UnitNo;					/* Unit logical number */
	struct Device *			hu_Device;					/* Owner device */
	LOCK_PROTO;

	/* PCI data */
	APTR					hu_PCI_BoardObject;			/* PCIX BoardObject */
	APTR					hu_PCI_IRQHandlerObject;	/* PCIX IRQ object */
	UWORD					hu_PCI_VID;					/* PCI bridge VendorID */
	UWORD					hu_PCI_DID;					/* PCI bridge DeviceID */
	UBYTE *					hu_PCI_RegBase;				/* PCI Base address of OHCI registers (size >= 2048) */

	/* Everything are zero'ed from this point during ohci_Term() */
	APTR					hu_MemPool;					/* Unit private memory pool */
	OHCI1394Flags			hu_Flags;
	HeliosEventListenerList hu_Listeners;

	/* 1394 variables */
	UQUAD					hu_GUID;
	QUADLET *				hu_ROMData;					/* Current ROM configuration */
	QUADLET *				hu_NextROMData;				/* Next ROM config to use, set to NULL after the BusReset process */
	ULONG					hu_BusSeconds;				/* Counter of bus second events */
	HeliosBusOptions		hu_BusOptions;				/* Simple register copy */

	/* Transaction Layer */
	OHCI1394TLayer			hu_TL;
	HeliosSubTask *			hu_SplitTimeoutTask;			/* Task to flush transactions in timeout */
	struct timerequest *	hu_SplitTimeoutIOReq;			/* Base timerequest for copy */
	ULONG					hu_SplitTimeoutCSR;				/* SPLIT-TIMEOUT CSR register value */

	/* OHCI static stuff (never change) */
	ULONG					hu_OHCI_Version;			/* Implemented OHCI version */
	ULONG					hu_OHCI_VendorID;
	UBYTE					hu_OHCI_LastGeneration;		/* Last generation from SelfID packets */
	UBYTE					hu_OHCI_LastBRGeneration;	/* Last generation from BusReset response packet */
	UWORD					hu_Reserved1;

	/* BusReset and Self-ID fields */
	HeliosSubTask *			hu_BusResetTask;			/* This task handles BusReset/SelfID events */
	ULONG					hu_BusResetSignal;
	QUADLET *				hu_SelfIdBufferAlloc;		/* Non-aligned buffer pointer for SelfId stream, directly from OS alloc functions */
	QUADLET *				hu_SelfIdBufferCpu;			/* Aligned version of hu_SelfIdBufferAlloc */
	QUADLET *				hu_SelfIdBufferPhy;			/* hu_SelfIdBufferCpu seen from DMA */
	QUADLET					hu_SelfIdArray[HELIOS_SELFID_LENGHT]; /* Then self-id data will be copied in this one after a bus reset */
	ULONG					hu_SelfIdPktCnt;			/* Number of QUADLET really filled in hu_SelfIdArray */
	UWORD 					hu_LocalNodeId;
	UWORD					hu_Reserved2;

	/* Devices management */
	HeliosSubTask *			hu_GCTask;

	/* Isochronous stuffs */
	struct MinList			hu_IRCtxList;				/* List of created Iso Receive contexts */
	struct MinList			hu_ITCtxList;				/* List of created Iso Transmit contexts */
	ULONG					hu_ITCtxMask;
	ULONG					hu_IRCtxMask;
	UBYTE					hu_MaxIsoReceiveCtx;
	UBYTE					hu_MaxIsoTransmitCtx;
	UWORD					hu_Reserved3;

	/* Asynchronous stuffs */
	OHCI1394ATCtx			hu_ATRequestCtx;
	OHCI1394ATCtx			hu_ATResponseCtx;
	OHCI1394ARCtx			hu_ARRequestCtx;
	OHCI1394ARCtx			hu_ARResponseCtx;
} OHCI1394Unit;

/*----------------------------------------------------------------------------*/
/*--- EXPORTED API -----------------------------------------------------------*/

extern LONG ohci_ScanPCI(OHCI1394Device *base);
extern LONG ohci_OpenUnit(OHCI1394Device *base, struct IORequest *ioreq, ULONG index);
extern void ohci_CloseUnit(OHCI1394Device *base, struct IORequest *ioreq);
extern LONG ohci_Init(OHCI1394Unit *unit);
extern void ohci_Term(OHCI1394Unit *unit);
extern LONG ohci_Enable(OHCI1394Unit *unit);
extern void ohci_Disable(OHCI1394Unit *unit);
extern LONG ohci_ResetUnit(OHCI1394Unit *unit);
extern BOOL ohci_RaiseBusReset(OHCI1394Unit *unit, BOOL shortreset);
extern UWORD ohci_GetTimeStamp(OHCI1394Unit *unit);
extern UWORD ohci_ComputeResponseTS(UWORD req_timestamp, UWORD offset);
extern UQUAD ohci_UpTime(OHCI1394Unit *unit);
extern LONG ohci_SendPHYPacket(
	OHCI1394Unit *unit,
	HeliosSpeed speed,
	QUADLET phy_data,
	OHCI1394ATData *atd);
extern LONG ohci_ATContext_Send(
	OHCI1394ATCtx *ctx,
	QUADLET *p,
	QUADLET *payload,
	OHCI1394ATData *atd,
	UBYTE tlabel,
	UWORD timestamp);
extern void ohci_HandleLocalRequest(
	OHCI1394Unit *unit,
	HeliosPacket *req,
	HeliosPacket *resp);
extern void ohci_CancelATProcess(OHCI1394ATData *atd);
extern LONG ohci_GenerationOK(OHCI1394Unit *unit, UBYTE generation);
extern LONG ohci_SetROM(OHCI1394Unit *unit, QUADLET *data);
extern void ohci_IRContext_Destroy(OHCI1394IRCtx *ctx);
extern OHCI1394IRCtx *ohci_IRContext_Create(
	OHCI1394Unit *		unit,
	LONG				index,
	UWORD				ibuf_size,
	UWORD				ibuf_count,
	UWORD				hlen,
	UBYTE				payload_align,
	APTR				callback,
	APTR				udata,
	OHCI1394IRCtxFlags	flags);
extern void ohci_IRContext_Start(OHCI1394IRCtx *ctx, ULONG channel, ULONG tags);
extern BOOL ohci_IRContext_Stop(OHCI1394IRCtx *ctx);

#endif /* OHCI1394_CORE_H */
