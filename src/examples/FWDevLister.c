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
*/

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/helios.h>
#include <stdio.h>
#include <string.h>

struct Library *HeliosBase;
static const UBYTE template[] = "HW_UNIT/N";

static struct {
	LONG *unitno;
} args = {NULL};

static CONST_STRPTR keyTypesStr[] =
{
	[KEYTYPE_IMMEDIATE]	= "Immediate",
	[KEYTYPE_OFFSET]	= "Offset",
	[KEYTYPE_LEAF]		= "Leaf",
	[KEYTYPE_DIRECTORY]	= "Directory",
};

static void
read_dev_name(const QUADLET *dir, STRPTR buf, ULONG buflen)
{
	LONG len;

	len = Helios_ReadTextualDescriptor(dir, buf, buflen-1);
	if (len >= 0)
		buf[len] = '\0';
	else
		printf("ERROR: Helios_ReadTextualDescriptor() failed\n");
}

static void
recursive_dump_directory(const QUADLET *dir, STRPTR header, int spaces)
{
	HeliosRomIterator ri;
	QUADLET key, value;
	char buf[80];
	char subheader[30];
	ULONG cnt=0;
	UWORD crc, dirCRC;
	
	/* Note: all arithmetics done on ROM pointer using QUADLET as unit! */
	
	/* Check CRC */
	crc = Helios_ComputeCRC16(&dir[1], ((dir[0] >> 16) & 0xffff));
	dirCRC = dir[0] & 0xffff;

	printf("\n%*s (CRC=$%04X, %s)\n", spaces+strlen(header), header, dirCRC, crc==dirCRC?"OK":"WRONG");
	
	Helios_InitRomIterator(&ri, dir);
	while (Helios_RomIterate(&ri, &key, &value))
	{
		switch (key)
		{
			case KEYTYPEV_IMMEDIATE | CSR_KEY_NODE_CAPABILITIES:
				printf("  %*s : $%X\n", spaces+13, "Capabilites  ", value);
				break;
				
			case KEYTYPEV_IMMEDIATE | CSR_KEY_MODULE_VENDOR_ID:
				printf("  %*s : $%X\n", spaces+13, "VendorID     ", value);
				break;
				
			case KEYTYPEV_IMMEDIATE | CSR_KEY_MODEL_ID:
				printf("  %*s : $%X\n", spaces+13, "ModelID      ", value);
				break;
			
			case KEYTYPEV_IMMEDIATE | CSR_KEY_UNIT_SPEC_ID:
				printf("  %*s : $%X\n", spaces+13, "UnitSpecID   ", value);
				break;
				
			case KEYTYPEV_IMMEDIATE | CSR_KEY_UNIT_SW_VERSION:
				printf("  %*s : $%X\n", spaces+13, "UnitSWVersion", value);
				break;
			
			case KEYTYPEV_LEAF | CSR_KEY_TEXTUAL_DESCRIPTOR:
				read_dev_name(&ri.actual[value - 1], buf, sizeof(buf));
				printf("  %*s : %s\n", spaces+13, "TextualDesc  ", buf);
				break;

			case KEYTYPEV_DIRECTORY | CSR_KEY_UNIT_DIRECTORY:
				snprintf(subheader, sizeof(subheader)-1, "Unit #%lu:", cnt++);
				recursive_dump_directory(&ri.actual[value - 1], subheader, spaces+2);
				break;
				
			case KEYTYPEV_OFFSET | CSR_KEY_UNIT_DEPENDENT_INFO:
				printf("  %*s : Offset=$%X\n", spaces+13, "VendorInfo   ", value);
				break;
				
			case KEYTYPEV_IMMEDIATE | CSR_KEY_UNIT_DEPENDENT_INFO:
				printf("  %*s : $%X\n", spaces+13, "VendorInfo   ", value);
				break;
				
			default:
				printf("  %*s : [$%02X, $%06X] Type: %s\n", spaces+13,
					((key&0x3f) < 0x37) ? "Extra Key    ":"Vendor Key   ",
					key & 0x3f, value, keyTypesStr[key>>6]);
		}
	}
}

int main(int argc, char **argv)
{
	APTR rdargs;
	LONG unitno=0;
	int ret = RETURN_OK;

	rdargs = ReadArgs(template, (APTR) &args, NULL);
	if (NULL != rdargs)
	{
		if (NULL != args.unitno)
			unitno = *args.unitno;
	}
	else
	{
		PrintFault(IoErr(), NULL);
		return RETURN_ERROR;
	}
	
	/* Opening the library before calling its API */
	HeliosBase = OpenLibrary(HELIOS_LIBNAME, HELIOS_LIBVERSION);
	if (HeliosBase)
	{
		HeliosHardware *hh = NULL;
		
		hh = Helios_FindObject(HGA_HARDWARE, NULL,
			HA_UnitNo, unitno,
			TAG_DONE);

		if (hh)
		{
			HeliosDevice *dev=NULL, *oldDev=NULL;
			ULONG count;
			
			for (;;)
			{
				LONG devGen;
				UQUAD devGUID;
				UWORD devNodeID;
				QUADLET devRom[CSR_CONFIG_ROM_SIZE];
				ULONG devRomLength;
				
				/* Obtain pointer on next available device.
				 * Note: Helios check Generation value of given base object (oldDev here).
				 * If mismatching occures, FindObject returns NULL.
				 */
				dev = Helios_FindObject(HGA_DEVICE, oldDev, HA_Hardware, (ULONG)hh, TAG_DONE);
				
				/* Release previous one */
				if (oldDev)
				{
					putchar('\n');
					Helios_ReleaseObject(oldDev);
				}
				
				/* No more devices */
				if (!dev)
					break;
				
				printf("Dev %p", dev);

				/* Obtain some information on device */
				count = Helios_GetAttrs(dev,
					HA_Generation,	(ULONG)&devGen,
					HA_GUID,		(ULONG)&devGUID,
					HA_NodeID,		(ULONG)&devNodeID,
					HA_Rom,			(ULONG)&devRom,			/* Must always be CSR_CONFIG_ROM_SIZE's QUADLET sized or more */
					HA_RomLength,	(ULONG)&devRomLength,
					TAG_DONE);
				
				if (count != 5)
				{
					printf(" *STOP*: Helios_GetAttrs() failed\n");
					break;
				}
				
				printf(", NodeID $%04X:\n", devNodeID);
				printf("  GUID      : $%016llX\n", devGUID);
				printf("  RomLength : %lu quadlet(s)\n", devRomLength);
				
				/* Print ROM Root directory information */
				if (devRomLength >= ROM_ROOT_DIR_OFFSET)
					recursive_dump_directory(devRom + ROM_ROOT_DIR_OFFSET, "Root directory:", 2);
				
				oldDev = dev;
			}
			
			/* Don't forget that! */
			if (dev)
				Helios_ReleaseObject(dev);
			
			/* Returned hardware object has been incref'ed for our usage.
			 * As we don't need anymore, we need to release it.
			 */
			Helios_ReleaseObject(hh);
		}
		else
		{
			printf("No FW hardware found\n");
			ret = RETURN_WARN;
		}
		
		CloseLibrary(HeliosBase);
	}
	else
	{
		printf("%s requires %s v%u\n", argv[0], HELIOS_LIBNAME, HELIOS_LIBVERSION);
		ret = RETURN_ERROR;
	}
	
	return ret;
}
