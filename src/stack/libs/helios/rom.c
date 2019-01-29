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
** Provide an API to bus a IEEE1394 ROM's.
**
** Follow the "1394 Open Host Controller Interface Specifications",
** Release 1.1, Junary 6, 2000.
**
*/

//#define NDEBUG
//#define DEBUG_1394

#include "private.h"
#include "clib/helios_protos.h"

#include <hardware/byteswap.h>
#include <clib/macros.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/utility.h>

#define BIB_CRC(v)          ((((QUADLET)(v))&0xffff) <<  0)
#define BIB_CRC_LENGTH(v)   ((((QUADLET)(v))&255) << 16)
#define BIB_INFO_LENGTH(v)  ((((QUADLET)(v))&255) << 24)

#define BIB_MAX_RECEIVE(v)  ((((QUADLET)(v))&15) << 12)
#define BIB_CYC_CLK_ACC(v)  ((((QUADLET)(v))&255) << 16)
#define BIB_SPEED(v)        ((((QUADLET)(v))&3) << 0)
#define BIB_PMC             ((1) << 27)
#define BIB_BMC             ((1) << 28)
#define BIB_ISC             ((1) << 29)
#define BIB_CMC             ((1) << 30)
#define BIB_IMC             ((1) << 31)

#define KEYTYPE_IMMEDIATE   0
#define KEYTYPE_OFFSET      1
#define KEYTYPE_LEAF        2
#define KEYTYPE_DIRECTORY   3

#define KEYTYPEF_IMMEDIATE  (1<<KEYTYPE_IMMEDIATE)
#define KEYTYPEF_OFFSET     (1<<KEYTYPE_OFFSET)
#define KEYTYPEF_LEAF       (1<<KEYTYPE_LEAF)
#define KEYTYPEF_DIRECTORY  (1<<KEYTYPE_DIRECTORY)

#define KEYTYPEV_IMMEDIATE  (KEYTYPE_IMMEDIATE<<6)
#define KEYTYPEV_OFFSET     (KEYTYPE_OFFSET<<6)
#define KEYTYPEV_LEAF       (KEYTYPE_LEAF<<6)
#define KEYTYPEV_DIRECTORY  (KEYTYPE_DIRECTORY<<6)

#if 0
UBYTE key_types[] =
{
    [KEY_TEXTUAL_DESCRIPTOR]    = KEYTYPEF_LEAF | KEYTYPEF_DIRECTORY,
    [KEY_BUS_DEPENDENT_INFO]    = KEYTYPEF_LEAF | KEYTYPEF_DIRECTORY,
    [KEY_MODULE_VENDOR_ID]      = KEYTYPEF_IMMEDIATE,
    [KEY_MODULE_HW_VERSION]     = KEYTYPEF_IMMEDIATE,
    [KEY_MODULE_SPEC_ID]        = KEYTYPEF_IMMEDIATE,
    [KEY_MODULE_SW_VERSION]     = KEYTYPEF_IMMEDIATE,
    [KEY_MODULE_DEPENDENT_INFO] = KEYTYPEF_LEAF | KEYTYPEF_DIRECTORY,
    [KEY_NODE_VENDOR_ID]        = KEYTYPEF_IMMEDIATE,
    [KEY_NODE_HW_VERSION]       = KEYTYPEF_IMMEDIATE,
    [KEY_NODE_SPEC_ID]          = KEYTYPEF_IMMEDIATE,
    [KEY_NODE_SW_VERSION]       = KEYTYPEF_IMMEDIATE,
    [KEY_NODE_CAPABILITIES]     = KEYTYPEF_IMMEDIATE,
    [KEY_NODE_UNIQUE_ID]        = KEYTYPEF_LEAF,
    [KEY_NODE_UNITS_EXTENT]     = KEYTYPEF_IMMEDIATE | KEYTYPEF_OFFSET,
    [KEY_NODE_MEMORY_EXTENT]    = KEYTYPEF_IMMEDIATE | KEYTYPEF_OFFSET,
    [KEY_NODE_DEPENDENT_INFO]   = KEYTYPEF_LEAF | KEYTYPEF_DIRECTORY,
    [KEY_UNIT_DIRECTORY]        = KEYTYPEF_DIRECTORY,
    [KEY_UNIT_SPEC_ID]          = KEYTYPEF_IMMEDIATE,
    [KEY_UNIT_SW_VERSION]       = KEYTYPEF_IMMEDIATE,
    [KEY_UNIT_DEPENDENT_INFO]   = KEYTYPEF_LEAF | KEYTYPEF_DIRECTORY,
    [KEY_UNIT_LOCATION]         = KEYTYPEF_LEAF,
    [KEY_UNIT_POLL_MASK]        = KEYTYPEF_IMMEDIATE,
};
#endif

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#endif

#define FAIL(fmt, a...) {_ERR(fmt, ##a); return -1;}


/*----------------------------------------------------------------------------*/
/*--- LOCAL CODE SECTION -----------------------------------------------------*/

void rom_SetDefault(QUADLET *rom, UQUAD guid, QUADLET opts, QUADLET vendor_comp_id)
{
    HeliosBusOptions options;
    UWORD crc;
    ULONG i, j, length;

    _INFO("ROM: %p (GUID=%016llX, Opts=%08lX)\n", rom, guid, opts);

    options.value = opts;
    options.r.IsoRsrcMgr = 0;
    options.r.CycleMaster = 1;
    options.r.Isochronous = 0;
    options.r.BusMgr = 0;
    options.r.CycleClockAcc = 100;

    /* Bytes order notes:
     * The ROM data block is accessed in the CPU endianess by Helios
     * when the local node ROM is read.
     * But this block is also accessible by the OHCI DMA for external accesses.
     * As the DMA uses the PCI endianess (little), data may be in a wrong bytes order.
     * This is fixed by asking to the OHCI to byteswap in/out-going data,
     * so DMA IO operations are corrects.
     *
     * So due to the internal accesses, rom data are in the CPU endianess.
     */

    /*--- Bus Info Block ---*/

    /* rom[0] = 0; filled later */
    rom[1]  = 0x31333934;
    rom[2]  = options.value;
    rom[3]  = guid >> 32;
    rom[4]  = guid;

    /* BIB crc */
    crc = utils_GetBlockCRC16(&rom[1], 4);
    _INFO("CRC=$%04X\n", crc);
    rom[0] = BIB_INFO_LENGTH(4) | BIB_CRC_LENGTH(4) | BIB_CRC(crc);

    /*--- Root Directory ---*/
    i = 5;
    rom[i++] = 0; // length + CRC16, filled later
    rom[i++] = 0x03000000 | vendor_comp_id;
    rom[i++] = 0x81000000; // offset filled later
    rom[i++] = 0x0c0083c0; // node capabilities, see ISO/IEC 13213 : 1994
    rom[i++] = 0x17000001;
    rom[i++] = 0x81000000; // offset filled later

    j = i;
    rom[7] += i - 7;
    rom[i++] = 0; // length + CRC16, filled later
    rom[i++] = 0;
    rom[i++] = 0;
    rom[i++] = MAKE_ID('M', 'o', 'r', 'p');
    rom[i++] = MAKE_ID('h', 'O', 'S', ' ');
    rom[i++] = MAKE_ID('-', ' ', 'O', 'H');
    rom[i++] = MAKE_ID('C', 'I', '1', '3');
    rom[i++] = MAKE_ID('9', '4', '\0', '\0');

    rom[j] = (length = i - j - 1) << 16;
    rom[j] |= utils_GetBlockCRC16(&rom[j+1], length);

    j = i;
    rom[10] += i - 10;
    rom[i++] = 0; // length + CRC16, filled later
    rom[i++] = 0;
    rom[i++] = 0;
    rom[i++] = MAKE_ID('H', 'e', 'l', 'i');
    rom[i++] = MAKE_ID('o', 's', '\0', '\0');

    rom[j] = (length = i - j - 1) << 16;
    rom[j] |= utils_GetBlockCRC16(&rom[j+1], length);

    /* Update root directory length and compute its CRC */
    length = 11 - 5 - 1;
    rom[5] = (length << 16) | utils_GetBlockCRC16(&rom[6], length);
}


/*============================================================================*/
/*--- LIBRARY CODE SECTION ---------------------------------------------------*/

QUADLET *Helios_CreateROMTagList(APTR pool, CONST struct TagItem *tags)
{
    QUADLET *rom;

    if (NULL != pool)
    {
        rom = AllocPooledAligned(pool, CSR_CONFIG_ROM_SIZE, CSR_CONFIG_ROM_SIZE, 0);
    }
    else
    {
        rom = AllocMemAligned(CSR_CONFIG_ROM_SIZE, MEMF_PUBLIC|MEMF_CLEAR, CSR_CONFIG_ROM_SIZE, 0);
    }

    if (NULL != rom)
    {
        /* Note: we suppose that data/length must be multiples of 4 */
        QUADLET *data = (APTR)GetTagData(HA_Rom, 0, tags);
        ULONG len = MIN(GetTagData(HA_RomLength, CSR_CONFIG_ROM_SIZE, tags), CSR_CONFIG_ROM_SIZE);

        if (NULL != data)
        {
            CopyMemQuick(data, rom, len);
            if (len < CSR_CONFIG_ROM_SIZE)
            {
                bzero((APTR)rom + len, len-CSR_CONFIG_ROM_SIZE);
            }
        }
        else
            rom_SetDefault(rom, (((UQUAD)GetTagData(HA_GUID_Hi, 0, tags)) << 32)
                           | GetTagData(HA_GUID_Lo, 0, tags),
                           GetTagData(HA_BusOptions, 0, tags),
                           GetTagData(HHA_VendorCompagnyId, 0, tags));
    }

    return rom;
}

void Helios_FreeROM(APTR pool, QUADLET *rom)
{
    if (NULL != pool)
    {
        FreePooled(pool, rom, CSR_CONFIG_ROM_SIZE);
    }
    else
    {
        FreeMem(rom, CSR_CONFIG_ROM_SIZE);
    }
}

LONG Helios_ReadROM(HeliosDevice *dev, QUADLET *storage, ULONG *length)
{
    IOHeliosHWSendRequest ioreq;
    struct MsgPort port;
    HeliosAPacket *p;
    ULONG remains, loop, info_len, crc_len, tgen, tgen2;
    UWORD crc, rom_crc_value;
    LONG err, i;
    HeliosOffset offset;

    if (*length < CSR_CONFIG_ROM_SIZE)
    {
        _ERR("Destination buffer length too small\n");
        return HERR_BADCALL;
    }

    LOCK_REGION_SHARED(dev);
    tgen = dev->hd_Generation;
    UNLOCK_REGION_SHARED(dev);

    /* Reply port setup */
    port.mp_Flags   = PA_SIGNAL;
    port.mp_SigBit  = SIGB_SINGLE;
    port.mp_SigTask = FindTask(NULL);
    NEWLIST(&port.mp_MsgList);

    /* Fill iorequest
     * Note: dev may have be removed, but if caller has take a ref on it
     * the hw pointer remains valid (even if all BeginIO will abord immediately with error).
     */
    ioreq.iohhe_Req.iohh_Req.io_Device = dev->hd_Hardware->hu_Device;
    ioreq.iohhe_Req.iohh_Req.io_Unit = &dev->hd_Hardware->hu_Unit;
    ioreq.iohhe_Req.iohh_Req.io_Message.mn_ReplyPort = &port;
    ioreq.iohhe_Req.iohh_Req.io_Message.mn_Length = sizeof(IOHeliosHWSendRequest);
    ioreq.iohhe_Req.iohh_Req.io_Command = HHIOCMD_SENDREQUEST;
    ioreq.iohhe_Req.iohh_Data = NULL;
    ioreq.iohhe_Req.iohh_Length = 0;
    ioreq.iohhe_Device = dev;

    /* First try to wait for a ROM ready (ROM[0] != 0) */
    offset = CSR_BASE_LO + CSR_CONFIG_ROM_OFFSET + 0;

    /* Fill packet */
    p = &ioreq.iohhe_Transaction.htr_Packet;
    remains = *length;
    *length = 0;

    _INFO_1394("$%04x: ReadQ@$%llX, S100\n", dev->hd_NodeID, offset);
    for (i=0; i<5; i++)
    {
        Helios_FillReadQuadletPacket(p, S100, offset);

        loop = 10;
        for (;;)
        {
            ioreq.iohhe_Req.iohh_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
            err = DoIO(&ioreq.iohhe_Req.iohh_Req);

            if ((HHIOERR_NO_ERROR != err) || (HELIOS_RCODE_COMPLETE != p->RCode))
            {
                _ERR("$%04x: ROM[%u] read failed (IOErr=%ld, RCode=%ld)\n", dev->hd_NodeID, i, err, p->RCode);
            }
            if (HHIOERR_FAILED == err)
            {
                if (HELIOS_RCODE_GENERATION == p->RCode)
                {
                    return HERR_BUSRESET;
                }
                else if (HELIOS_RCODE_BUSY != p->RCode)
                {
                    return HERR_IO;
                }
            }
            else if (HHIOERR_NO_ERROR != err)
            {
                return HERR_IO;
            }
            else
            {
                LOCK_REGION_SHARED(dev);
                tgen2 = dev->hd_Generation;
                UNLOCK_REGION_SHARED(dev);

                if (tgen != tgen2)
                {
                    _WARN("$%04x: ReadQ@$%llx failed, BusReset occured!\n", dev->hd_NodeID, p->Offset);
                    return HERR_BUSRESET;
                }

                if ((i > 0) || (0 != p->QuadletData))
                {
                    break;
                }
            }

            if (0 == --loop)
            {
                break;
            }

            _WARN("$%04x: ROM not ready, try again...\n", dev->hd_NodeID);
            Helios_DelayMS(125);
        }

        if (!loop)
        {
            return HERR_TIMEOUT;
        }

        _INFO_1394("$%04x: ROM[%u]=$%08x\n", dev->hd_NodeID, i, p->QuadletData);
        storage[i] = p->QuadletData;
        *length += sizeof(QUADLET);
        remains -= sizeof(QUADLET);
        offset += sizeof(QUADLET);

        if (0 == i)
        {
            /* Minimal ROM => 1 QUADLET */
            info_len = storage[0] >> 24;
            if (1 == info_len)
            {
                return HERR_NOERR;
            }
        }
    }

    /* General ROM type */
    crc_len = (storage[0] >> 16) & 0xff;
    rom_crc_value = storage[0] & 0xffff;
    _INFO_1394("$%04x: CRC len=%u, CRC value=%04x\n", dev->hd_NodeID, crc_len, rom_crc_value);

    //ULONG max_payload = 1ul << (dev->hd_NodeInfo.n_MaxSpeed+9);

    /* My first implementation used 2 passes, one using a block read
     * then many quadlet reads if the first pass failed.
     * But it seems that some devices don't like this and stop to work
     * when block read is done.
     * So now I use always quadlet reads only.
     */

    while (remains > 0)
    {
        Helios_FillReadQuadletPacket(p, dev->hd_NodeInfo.n_MaxSpeed, offset);

        loop = 10;
        do
        {
            _INFO_1394("$%04x: ReadQ@$%llX, S%u\n", dev->hd_NodeID, p->Offset, (1<<p->Speed)*100);
            ioreq.iohhe_Req.iohh_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
            err = DoIO(&ioreq.iohhe_Req.iohh_Req);
            if ((HHIOERR_FAILED == err) || (HELIOS_RCODE_COMPLETE != p->RCode))
            {
                switch (p->RCode)
                {
                    case HELIOS_RCODE_GENERATION:
                        _WARN("$%04x: ReadQ@$%llx failed, BusReset occured!\n", dev->hd_NodeID, p->Offset);
                        return HERR_BUSRESET;

                    case HELIOS_RCODE_TYPE_ERROR:
                    case HELIOS_RCODE_ADDRESS_ERROR:
                        /* Stop here, but considere all previous quadlets read valid */
                        _WARN("$%04x: ReadQ@$%llx failed, RCode=%ld, stop the scan.\n", dev->hd_NodeID, p->Offset, p->RCode);
                        goto check_crc;

                    case HELIOS_RCODE_BUSY:
                        if (0 == --loop)
                        {
                            _ERR("$%04x: device too busy on ROM[%u], read failed\n", dev->hd_NodeID,
                                 (p->Offset-(CSR_BASE_LO+CSR_CONFIG_ROM_OFFSET))/sizeof(QUADLET));
                            return HERR_IO;
                        }

                        _WARN("$%04x: device busy on ROM[%u], retry #%u...\n", dev->hd_NodeID,
                              (p->Offset-(CSR_BASE_LO+CSR_CONFIG_ROM_OFFSET))/sizeof(QUADLET),
                              10-loop);
                        Helios_DelayMS(125);
                        continue;

                    default:
                        _ERR("$%04x: ReadQ@$%llx failed, RCode=%ld\n", dev->hd_NodeID, p->Offset, p->RCode);
                        return HERR_IO;
                }
            }
            else if (HHIOERR_NO_ERROR != err)
            {
                _ERR("$%04x: ReadQ@$%llx failed, IOErr=%ld\n", dev->hd_NodeID, p->Offset, err);
                return HERR_IO;
            }
            else
            {
                break;
            }
        }
        while (loop);

        LOCK_REGION_SHARED(dev);
        tgen2 = dev->hd_Generation;
        UNLOCK_REGION_SHARED(dev);

        if (tgen != tgen2)
        {
            _WARN("$%04x: ReadQ@$%llx failed, BusReset occured!\n", dev->hd_NodeID, p->Offset);
            return HERR_BUSRESET;
        }

        _INFO_1394("$%04x: ROM[%u]=$%08x\n", dev->hd_NodeID, i, p->QuadletData);
        storage[i++] = p->QuadletData;
        *length += sizeof(QUADLET);
        remains -= sizeof(QUADLET);
        offset += sizeof(QUADLET);
    }

check_crc:
    //for (i=0; i<crc_len; i++)
    //    _INFO("$%04x: ROM[%u]=$%08x\n", dev->hd_NodeID, i, storage[i]);

    /* Compute the CRC of the protected area */
    crc = utils_GetBlockCRC16(&storage[1], crc_len);
    if (crc != rom_crc_value)
    {
        _ERR("$%04x: CRC error, waited $%x, get $%x\n", dev->hd_NodeID, rom_crc_value, crc);
        return HERR_IO;
    }

    return HERR_NOERR;
}

void Helios_InitRomIterator(HeliosRomIterator *ri, const QUADLET *rom)
{
    ri->actual = rom + 1;
    ri->end = ri->actual + (rom[0] >> 16);
}

LONG Helios_RomIterate(HeliosRomIterator *ri, QUADLET *key, QUADLET *value)
{
    *key = *ri->actual >> 24;
    *value = *ri->actual & 0xffffff;

    return ri->actual++ < ri->end;
}

LONG Helios_ReadTextualDescriptor(const QUADLET *dir, STRPTR buffer, ULONG length)
{
    UWORD crc, len = dir[0] >> 16;

    crc = utils_GetBlockCRC16((QUADLET *)&dir[1], len);
    if ((crc == (dir[0] & 0xffff)) && (0 == dir[1]))
    {
        /* Support only English ASCII fixed one-byte characters  */
        if (0 == dir[2])
        {
            len = MIN((len-1)*sizeof(QUADLET), length);
            CopyMem((APTR)&dir[3], buffer, len);

            if (len < length)
            {
                _INFO("Textual contents: %s\n", buffer);
            }

            return len;
        }
        else
        {
            _ERR("Unsupported characters: $%08x\n", dir[0]);
        }
    }
    else
    {
        _ERR("Corrupted Unit textual leaf entry\n");
    }

    return -1;
}

