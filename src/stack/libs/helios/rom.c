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
** Provide an API to bus a IEEE1394 devices ROM.
*/

#define NDEBUG

#include "private.h"

#include <hardware/byteswap.h>
#include <clib/macros.h>

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

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

#if 0
UBYTE key_types[] = {
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

/*============================================================================*/
/*--- LOCAL CODE -------------------------------------------------------------*/
/*============================================================================*/
void rom_SetDefault(QUADLET *rom, UQUAD guid, QUADLET opts, QUADLET vendor_id)
{
	HeliosBusOptions options;
	UWORD crc;
	ULONG i, j, length;

	_INF("ROM: %p (GUID=%016llX, Opts=%08lX)\n", rom, guid, opts);

	options.quadlet = opts;
	
	options.IsoRsrcMgr = 0;
	options.CycleMaster = 1;
	options.Isochronous = 0;
	options.BusMgr = 0;
	options.CycleClockAcc = 100;

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
	rom[1] = 0x31333934;
	rom[2] = options.quadlet;
	rom[3] = guid >> 32;
	rom[4] = guid;
	
	/* BIB crc */
	crc = utils_GetBlockCRC16(&rom[1], 4);
	_DBG("Header CRC=$%04X\n", crc);
	rom[0] = BIB_INFO_LENGTH(4) | BIB_CRC_LENGTH(4) | BIB_CRC(crc);

	/*--- Root Directory ---*/
	j = i = 5;
	rom[5] = i - 5; /* length + CRC16, filled later */
	i++;
	rom[i++] = 0x03000000 | (vendor_id & 0xffffff);
	rom[i++] = 0x81000000; /* offset filled later */
	rom[i++] = 0x0c0083c0; /* node capabilities, see ISO/IEC 13213:1994 */
	rom[i++] = 0x17000000; /* offset filled later */
	rom[i++] = 0x81000000; /* offset filled later */
	
	rom[j] = (length = i - j - 1) << 16;
	
	j = i;
	rom[7] += i - 7;
	rom[i++] = 0; /* length + CRC16, filled later */
	rom[i++] = 0;
	rom[i++] = 0;
	rom[i++] = MAKE_ID('M', 'o', 'r', 'p');
	rom[i++] = MAKE_ID('h', 'O', 'S', 0x0);

	rom[j] = (length = i - j - 1) << 16;
	rom[j] |= utils_GetBlockCRC16(&rom[j+1], length);

	j = i;
	rom[10] += i - 10;
	rom[i++] = 0; /* length + CRC16, filled later */
	rom[i++] = 0;
	rom[i++] = 0;
	rom[i++] = MAKE_ID('H', 'e', 'l', 'i');
	rom[i++] = MAKE_ID('o', 's', ' ', '-');
	rom[i++] = MAKE_ID(' ', 'O', 'H', 'C');
	rom[i++] = MAKE_ID('I', '1', '3', '9');
	rom[i++] = MAKE_ID('4', 0x0, 0x0, 0x0);

	rom[j] = (length = i - j - 1) << 16;
	rom[j] |= utils_GetBlockCRC16(&rom[j+1], length);

	rom[5] |= utils_GetBlockCRC16(&rom[6], rom[5] >> 16);
}
/*============================================================================*/
/*--- PUBLIC API CODE --------------------------------------------------------*/
/*============================================================================*/
QUADLET *
Helios_CreateROMTagList(APTR pool, CONST struct TagItem *tags)
{
	QUADLET *rom;
	const ULONG size=CSR_CONFIG_ROM_SIZE*sizeof(QUADLET);

	if (NULL != pool)
		rom = AllocPooledAligned(pool, size, size, 0);
	else
		rom = AllocMemAligned(size, MEMF_PUBLIC|MEMF_CLEAR, size, 0);

	if (NULL != rom)
	{
		/* Note: we suppose that data/length are given in Quadlet unit */
		QUADLET *data = (APTR)GetTagData(HA_Rom, 0, tags);
		ULONG len = MIN(sizeof(QUADLET) * GetTagData(HA_RomLength, CSR_CONFIG_ROM_SIZE, tags), size);
		
		if (NULL != data)
		{
			CopyMemQuick(data, rom, len);
			if (len < size)
				bzero((APTR)rom + len, len-size);
		}
		else
			rom_SetDefault(rom, (((UQUAD)GetTagData(HA_GUID_Hi, 0, tags)) << 32)
				| GetTagData(HA_GUID_Lo, 0, tags),
				GetTagData(HA_BusOptions, 0, tags),
				GetTagData(HA_CSR_VendorID, 0, tags));
	}

	return rom;
}
void Helios_FreeROM(APTR pool, QUADLET *rom)
{
	const ULONG size=CSR_CONFIG_ROM_SIZE*sizeof(QUADLET);
	
	if (NULL != pool)
		FreePooled(pool, rom, size);
	else
		FreeMem(rom, size);
}
void
Helios_InitRomIterator(HeliosRomIterator *ri, const QUADLET *rom)
{
	ri->actual = rom + 1;
	ri->end = ri->actual + (rom[0] >> 16);
}
LONG
Helios_RomIterate(HeliosRomIterator *ri, QUADLET *key, QUADLET *value)
{
	*key = *ri->actual >> 24;
	*value = *ri->actual & 0xffffff;

	return ri->actual++ < ri->end;
}
LONG
Helios_ReadTextualDescriptor(const QUADLET *dir, STRPTR buffer, ULONG length)
{
	UWORD crc, len=dir[0] >> 16;
	
	crc = utils_GetBlockCRC16((QUADLET *)&dir[1], len);
	if ((crc == (dir[0] & 0xffff)) && (0 == dir[1]))
	{
		/* Support only English ASCII fixed one-byte characters  */
		if (0 == dir[2])
		{
			len = MIN(len*sizeof(QUADLET), length);
			CopyMem((APTR)&dir[3], buffer, len);
			return len;
		}
		else
			_ERR("Unsupported characters: $%08x\n", dir[0]);
	}
	else
		_ERR("Corrupted Unit textual leaf entry\n");

	return -1;
}
UWORD
Helios_ComputeCRC16(const QUADLET *data, ULONG len)
{
	return utils_GetBlockCRC16((QUADLET *)data, len);
}
LONG
Helios_ReadROM(
	HeliosHardware *hh,
	UWORD nodeID,
	UBYTE gen,
	HeliosSpeed maxSpeed,
	QUADLET *storage, ULONG *length)
{
	struct IOStdReq ioreq;
	HeliosPacket _packet, *p = &_packet;
	HeliosOffset offset;
	ULONG remains, max_payload, info_len, crc_len;
	UWORD crc, rom_crc_value;
	LONG err;
	int i, retry;

	if (*length < CSR_CONFIG_ROM_SIZE)
	{
		_ERR("Destination buffer length too small\n");
		return HERR_BADCALL;
	}
	
	*length = MIN(*length, CSR_CONFIG_ROM_SIZE);

	ioreq.io_Message.mn_Length = sizeof(ioreq);
	ioreq.io_Command = HHIOCMD_SENDREQUEST;
	ioreq.io_Data = p;
	
	p->DestID = nodeID;
	p->Generation = gen;

	offset = CSR_BASE_LO + CSR_CONFIG_ROM_OFFSET*sizeof(QUADLET) + 0;
	remains = *length;
	*length = 0;

	/* Try to read first 5 ROM quadlets with speed = S100 */
	for (i=0; i < 5; i++)
	{
		_DBG("$%04x: ReadQ@$%llX, S100\n", nodeID, offset);
		Helios_FillReadQuadletPacket(p, S100, offset);

		retry = 10;
		for(;;)
		{
			err = Helios_DoIO(hh, (struct IORequest *)&ioreq);
			if (err)
			{
				_ERR("$%04x: ROM[%u] read failed (IOErr=%ld, Ack=%d, RCode=%d)\n",
					nodeID, i, err, p->Ack, p->RCode);
				
				/* BusReset? */
				if ((p->Ack == HELIOS_ACK_GENERATION) ||
					(p->RCode == HELIOS_RCODE_GENERATION))
					return HERR_BUSRESET;
				
				/* Not busy? */
				if ((HELIOS_ACK_BUSY_X != p->Ack) &&
					(HELIOS_ACK_BUSY_A != p->Ack) &&
					(HELIOS_ACK_BUSY_B != p->Ack))
					return HERR_IO;
			}

			/* Retry until first quadlet is readable */
			if ((i > 0) || (p->QuadletData != 0))
				break;

			if (!retry)
				break;
				
			retry--;

			_WRN("$%04x: ROM not ready, retry...\n", nodeID);
			Helios_DelayMS(125);
		}
		
		if (!retry)
			return HERR_TIMEOUT;

		_DBG("$%04x: ROM[%u]=$%08x\n", nodeID, i, p->QuadletData);
		storage[i] = p->QuadletData;
		(*length)++;
		remains--;
		offset += sizeof(QUADLET);

		if (i == 0)
		{
			/* Minimal ROM => 1 QUADLET */
			info_len = storage[0] >> 24;
			if (info_len == 1)
				return HERR_NOERR;
		}
	}

	/* General ROM type */
	crc_len = (storage[0] >> 16) & 0xff;
	rom_crc_value = storage[0] & 0xffff;
	_DBG("$%04x: CRC len=%u, CRC value=%04x\n", nodeID, crc_len, rom_crc_value);

	max_payload = 1ul << (maxSpeed+9);

	/* Read remaining quadlets.
	 *
	 * TechNotes: My first implementation used 2 passes, one using a block read
	 * then many quadlet reads if the first pass failed.
	 * But it seems that some devices don't like this and stop to work
	 * when block read is done.
	 * So now I use always quadlet reads only.
	 */

	while (remains > 0)
	{
		Helios_FillReadQuadletPacket(p, maxSpeed, offset);

		retry = 10;
		do
		{
			_DBG("$%04x: ReadQ@$%llX, S%u\n", nodeID, p->Offset, (1ul << p->Speed)*100);
			err = Helios_DoIO(hh, (struct IORequest *)&ioreq);
			if (!err)
				break;
				
			if (err == HHIOERR_ACK)
			{
				/* Busy? */
				if ((HELIOS_ACK_BUSY_X == p->Ack) ||
					(HELIOS_ACK_BUSY_A == p->Ack) ||
					(HELIOS_ACK_BUSY_B == p->Ack))
				{
					if (!retry)
					{
						_ERR("$%04x: device too busy on ROM[%u], read failed\n", nodeID,
							(p->Offset-(CSR_BASE_LO+CSR_CONFIG_ROM_OFFSET*sizeof(QUADLET)))/sizeof(QUADLET));
						return HERR_IO;
					}
					
					retry--;

					_WRN("$%04x: device busy on ROM[%u], retry #%u...\n", nodeID,
						(p->Offset-(CSR_BASE_LO+CSR_CONFIG_ROM_OFFSET*sizeof(QUADLET)))/sizeof(QUADLET),
						10-retry);
					Helios_DelayMS(125);
					continue;
				}
				
				_ERR("$%04x: ReadQ@$%llx failed, Ack=%d\n", nodeID, p->Offset, p->Ack);
				return HERR_IO;
			}
			else if (err == HHIOERR_RCODE)
			{
				switch (p->RCode)
				{
					case HELIOS_RCODE_GENERATION:
						_WRN("$%04x: ReadQ@$%llx failed, BusReset\n", nodeID, p->Offset);
						return HERR_BUSRESET;

					case HELIOS_RCODE_TYPE_ERROR:
					case HELIOS_RCODE_ADDRESS_ERROR:
						/* Stop here, but considere all previous quadlets read valid */
						_WRN("$%04x: ReadQ@$%llx failed, TYPE/ADDR error\n", nodeID, p->Offset, p->RCode);
						goto check_crc;
						
					default:
						_ERR("$%04x: ReadQ@$%llx failed, RCode=%d\n", nodeID, p->Offset, p->RCode);
						return HERR_IO;
				}
			}
			
			/* other kind of errors */
			_ERR("$%04x: ReadQ@$%llx failed with err %d (Ack=%d, RCode=%d)\n", nodeID, err, p->Offset, p->Ack, p->RCode);
			return HERR_IO;
		}
		while (retry);

		_DBG("$%04x: ROM[%u]=$%08x\n", nodeID, i, p->QuadletData);
		storage[i++] = p->QuadletData;
		(*length)++;
		remains--;
		offset += sizeof(QUADLET);
	}

check_crc:
	for (i=0; i <= crc_len; i++)
		_INF("$%04x: ROM[%u]=$%08x\n", nodeID, i, storage[i]);

	/* Compute the CRC of the protected area */
	crc = utils_GetBlockCRC16(&storage[1], crc_len);
	if (crc != rom_crc_value)
	{
		_ERR("$%04x: CRC error, waited $%x, get $%x\n", nodeID, rom_crc_value, crc);
		return HERR_IO;
	}

	return HERR_NOERR;
}
/*=== EOF ====================================================================*/