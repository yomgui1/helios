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

/* 
**
** MUI interface to show 1394 bus and devices properties.
**
*/

#include "libraries/helios.h"
#include "devices/helios.h"
#include "devices/helios/ohci1394.h"

#define USE_INLINE_STDARG
#include "proto/helios.h"
#include <proto/pcix.h>
#undef USE_INLINE_STDARG

#include <libraries/mui.h>
#include <hardware/byteswap.h>
#include <utility/hooks.h>
#include <libraries/pcix.h>

#include <mui/HexEdit_mcc.h>

#include <clib/alib_protos.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/pciids.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#endif

#define MkLabel(ob, l, c, min)                  \
    Child, Label2(l),                           \
    Child, ob = TextObject, StringFrame,        \
        MUIA_Text_PreParse, MUIX_R,             \
        MUIA_Text_Contents, (c),                \
        MUIA_Text_SetMin, min,                  \
        MUIA_Text_SetMax, FALSE,                \
    End

#define DoLabel(l, c, min)                      \
    Child, Label2(l),                           \
    Child, TextObject, StringFrame,             \
        MUIA_Text_PreParse, MUIX_R,             \
        MUIA_Text_Contents, (c),                \
        MUIA_Text_SetMin, min,                  \
        MUIA_Text_SetMax, FALSE,                \
    End

#define BusEvents_ClearLog() DoMethod(objs[OBJ_BUS_EVT_LOG], MUIM_List_Clear)
#define BusEvents_LogMsg(time, type, fmt, a...) ({ char buf[160], *_p=buf; \
            int x, l = sizeof(buf);                                     \
            x = snprintf(_p, l, "%016llx | ", time);                    \
            if (x < l) { l -= x; _p += x;                               \
                if ('\0' != fmt[0]) {                                   \
                    x = snprintf(_p, l, "%-15s : ", type);              \
                    if (x < l) snprintf(_p+x, l-x, fmt ,##a);           \
                } else snprintf(_p, l, "%-15s", type);                  \
                DoMethod(objs[OBJ_BUS_EVT_LOG], MUIM_List_InsertSingle, buf, MUIV_List_Insert_Top); \
            }})

#define MyCheckMark(selected)                                    \
    ImageObject,                                                 \
        ImageButtonFrame,                                        \
        MUIA_InputMode        , MUIV_InputMode_Toggle,           \
        MUIA_Image_Spec       , MUII_CheckMark,                  \
        MUIA_Image_FreeVert   , TRUE,                            \
        MUIA_Selected         , selected,                        \
        MUIA_Background       , MUII_ButtonBack,                 \
        MUIA_ShowSelState     , FALSE,                           \
        MUIA_CycleChain       , TRUE,                            \
        End

#define Image(f) MUI_NewObject(MUIC_Dtpic, MUIA_Dtpic_Name, f, TAG_DONE)

#define INIT_HOOK(h, f) { struct Hook *_h = (struct Hook *)(h); \
    _h->h_Entry = (APTR) HookEntry; \
    _h->h_SubEntry = (APTR) (f); }

#define PCI_VENDORID_VIA        (0x1106)
#define PCI_DEVICEID_VT630X     (0x3044)
#define OUI_VENDOR_VIA          (0x004063ull)
#define PEGASOS_MAGIC_GUID      (0x0011060000004b2full)

#define SBP2_UNIT_SPEC_ID_ENTRY	0x0000609e
#define SBP2_SW_VERSION_ENTRY	0x00010483

typedef struct DeviceListerData
{
    UWORD                    NodeID;
    UWORD                    Pad0;
    HeliosNode               NodeInfo;
    ULONG                    Gen;
    HeliosDevice *           Device;
    HeliosEventListenerList *DevEvtList;
    HeliosEventMsg           DevEvtMsg;
    ULONG                    LastEvent;
    char *                   Type;
    char                     GUID[17];
} DeviceListerData;

enum {
    OBJ_GENERATION,
    OBJ_DEVICE_COUNT,
    OBJ_BUS_RESET,
    OBJ_BUS_ENABLE,
    OBJ_BUS_DISABLE,
    OBJ_BUS_EVT_LOG,
    OBJ_BUS_EVT_SAVE,
    OBJ_BUS_EVT_CLEAR,
    OBJ_BUS_EVT_MSK_BUSRESET,
    OBJ_BUS_EVT_MSK_TOPOLOGY,
    OBJ_BUS_EVT_MSK_DEVICE_ENUM,
    OBJ_DEVICES_LIST,
    OBJ_DUMP_ROM,
    OBJ_READ_QUADLET,
    OBJ_WRITE_QUADLET,
    OBJ_READ_QUADLET_ADDRESS,
    OBJ_READ_QUADLET_CANCEL,
    OBJ_READ_QUADLET_OK,
    OBJ_READ_QUADLET_RESULT,
    OBJ_WRITE_QUADLET_ADDRESS,
    OBJ_WRITE_QUADLET_CANCEL,
    OBJ_WRITE_QUADLET_OK,
    OBJ_WRITE_QUADLET_VALUE,
    OBJ_ROM_WIN,
    OBJ_ROM_GROUP,
    OBJ_ROM_HEX,
    OBJ_VID,
    OBJ_DID,
    OBJ_DUMP_OHCI,
};

enum {
    ID_BUS_RESET=1,
    ID_BUS_ENABLE,
    ID_BUS_DISABLE,
    ID_BUS_EVT_MSK_CHANGED,
    ID_READ_QUADLET,
    ID_WRITE_QUADLET,
    ID_DUMP_ROM,
    ID_DUMP_OHCI,
};

struct Library *PCIXBase = NULL;
struct Library *PCIIDSBase = NULL;
struct Library *HeliosBase = NULL;
static HeliosHardware *gHardware = NULL;
static struct MsgPort *io_port = NULL;
static struct MsgPort *event_port = NULL;
static struct MUI_CustomClass *mcc = NULL;
static ULONG io_port_signal, event_port_signal;
static Object *app, *objs[100] = {0};
static HeliosTopology gLastTopo;
static HeliosEventMsg busreset_msg, topology_msg;
static HeliosEventListenerList *gHWEvtListenerList = NULL;

static struct Hook DeviceListConstructHook;
static struct Hook DeviceListDestructHook;
static struct Hook DeviceListDisplayHook;
static struct Hook DeviceListCompareHook;

static char dev_names[63][3];

static STRPTR rcode_str[] = {
    [HELIOS_RCODE_COMPLETE]         = "Complete",
    [HELIOS_RCODE_CONFLICT_ERROR]   = "Conflict",
    [HELIOS_RCODE_DATA_ERROR]       = "Data",
    [HELIOS_RCODE_TYPE_ERROR]       = "Type",
    [HELIOS_RCODE_ADDRESS_ERROR]    = "Address",

    [0x10-HELIOS_RCODE_CANCELLED]   = "Cancelled",
    [0x10-HELIOS_RCODE_GENERATION]  = "Generation",
    [0x10-HELIOS_RCODE_MISSING_ACK] = "Missing Ack",
    [0x10-HELIOS_RCODE_SEND_ERROR]  = "Send Error",
    [0x10-HELIOS_RCODE_BUSY]        = "Busy",
    [0x10-HELIOS_RCODE_TIMEOUT]     = "Timeout",
};

static STRPTR speed2str[] = {
    [S100] = "S100",
    [S200] = "S200",
    [S400] = "S400",
};

/*==========================================================================================================================*/

//+ LogMsg
static void LogMsg(STRPTR fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    if (NULL == app)
        vprintf(fmt, va);
    else {
        char buf[160];

        vsnprintf(buf, sizeof(buf), fmt, va);
        DoMethod(objs[OBJ_BUS_EVT_LOG], MUIM_List_InsertSingle, buf, MUIV_List_Insert_Top);
    }
    va_end(va);
}
//-
//+ reg_Read
static inline ULONG reg_Read(register ULONG address)
{
    register ULONG ret;

    __asm volatile ("lwbrx %0,0,%1; sync" : "=r" (ret) : "r" (address));

    return ret;
}
//-
//+ reg_Write
static inline void reg_Write(register ULONG address, ULONG value)
{
    __asm volatile ("stwbrx %0,0,%1; sync" : : "r" (value), "r" (address));
}
//-
//+ fw_to_eeprom
static void fw_to_eeprom(ULONG iobase, int cs, int clock, int dataout)
{
    ULONG value = 0x10 /* enable outputs */ | (cs ? 8 : 0) | (clock ? 4 : 0) | (dataout ? 2 : 0);

    //dprintf(stderr, " %c%c%c ", cs ? ' ' : '-', clock ? 'c' : ' ', dataout ? '1' : '0');
    reg_Write(iobase + 0x20, value);
}
//-
//+ send_bits_4w
static void send_bits_4w(ULONG iobase, int bits, ULONG value)
{
    if (bits < 32)
        value <<= 32 - bits;

    while (bits--) {
        fw_to_eeprom(iobase, 1, 0, value & 0x80000000); usleep(10);
        fw_to_eeprom(iobase, 1, 1, value & 0x80000000); usleep(10);
        fw_to_eeprom(iobase, 1, 0, value & 0x80000000); usleep(10);
        value <<= 1;
    }
}
//-
//+ write_4w
static void write_4w(ULONG iobase, UBYTE address, UWORD value)
{
    value = BE_SWAPWORD(value);

    //dprintf("writing 0x%04X at address 0x%02X\n", value, address);

    fw_to_eeprom(iobase, 0, 0, 0); usleep(10); /* Init */

    fw_to_eeprom(iobase, 1, 0, 0); usleep(10);
    send_bits_4w(iobase, 3, 0x4);   /* CMD: EWEN = write enable */
    send_bits_4w(iobase, 6, 0x30);
    send_bits_4w(iobase, 16, 0x0000);
    fw_to_eeprom(iobase, 0, 0, 0); usleep(10);

    fw_to_eeprom(iobase, 1, 0, 0); usleep(10);
    send_bits_4w(iobase, 3, 0x5);   /* CMD: WRITE */
    send_bits_4w(iobase, 6, address);
    send_bits_4w(iobase, 16, value);
    fw_to_eeprom(iobase, 0, 0, 0); usleep(1000);

    fw_to_eeprom(iobase, 1, 0, 0); usleep(10);
    send_bits_4w(iobase, 3, 0x4);   /* CMD: EWDS = write disable */
    send_bits_4w(iobase, 6, 0x00);
    send_bits_4w(iobase, 16, 0x0000);
    fw_to_eeprom(iobase, 0, 0, 0); usleep(10);
}
//-
//+ PimpMyPegasos
static void PimpMyPegasos(APTR board)
{
    UQUAD guid, mac;
    ULONG membase, iobase, macbase, res;

    /* Pegasos machines don't uset a unique GUID: one GUID to rules them all ;-)
     * This GUID is hard coded inside a ATMEL 93c46 4-wire EEPROM.
     * So to fix that, we're going to convert the MAC-48 address of the machine into a EUI-64 value.
     * We need to use undocumented feature of the VIA VT6306 to do that...
     */

    if ((PCI_VENDORID_VIA == PCIXReadConfigWord(board, PCIXCONFIG_VENDOR))
        && (PCI_DEVICEID_VT630X == PCIXReadConfigWord(board, PCIXCONFIG_DEVICE))) {

        /* Power-on this board */
        if((PCIXReadConfigWord(board, PCIXCONFIG_COMMAND) & 7) == 0)
            PCIXWriteConfigWord(board, PCIXCONFIG_COMMAND, 7);

        membase = PCIXGetBoardAttr(board, PCIXTAG_BASEADDRESS0) & ~7;

        /* Obtain current GUID */
        guid   = reg_Read(membase + 0x24);
        guid <<= 32;
        guid  |= reg_Read(membase + 0x28);

        if (PEGASOS_MAGIC_GUID == guid) {
            iobase = PCIXGetBoardAttr(board, PCIXTAG_BASEADDRESS1) & ~7;

            /* Searching for the MAC chipset */
            board = PCIXFindBoardTags(NULL, PCIXFINDTAG_FULLCLASS, PCI_CLASS_NETWORK_ETHERNET,
                                      PCIXFINDTAG_VENDOR, PCI_VENDORID_VIA,
                                      TAG_DONE);
            if (NULL != board) {
                macbase = PCIXGetBoardAttr(board, PCIXTAG_BASEADDRESS0) & ~7;

                /* Get the MAC address from the MAC-Chip */
                mac   = LE_SWAPLONG(*(volatile ULONG *) (macbase + 0x00));
                mac <<= 16;
                mac  |= (LE_SWAPLONG(*(volatile ULONG *) (macbase + 0x04)) >> 16) & 0xffff;

                /* Convert MAC-48 into EUI-64, but use the OHCI vendor id than thus of the ethernet chipset */
                guid = (OUI_VENDOR_VIA << 40) | 0xffff000000ull | (mac & 0xffffffull);

                /* Ask user if we need to make the change */
                res = MUI_Request(NULL, NULL, 0, "Helios Warning!", "_Yes, change!|*"MUIX_B"_No thanks",
                                  MUIX_C MUIX_B MUIX_U"I've detected that my GUID value is not correct!\n\n"MUIX_N MUIX_L
                                  "Currently it's set to: "MUIX_B"$%08lX%08lX"MUIX_N". But this value has been used for all Pegasos!\n\n"
                                  "If you continue with this value you may run into fatal issues if you try to connect\n"
                                  "an other Pegasos machine on this Firewire bus.\n\n"
                                  "To solve this issue, I've generated for you a new GUID value\n"
                                  "using your current MAC-48 address: "MUIX_B"$%04lX%08lX\n\n"MUIX_N
                                  "This new GUID will be: "MUIX_B"$%08lX%08lX\n\n"MUIX_N
                                  "But to record this value I need to modify the hardcoded GUID located in my EEPROM chipset.\n\n"
                                  MUIX_C MUIX_B MUIX_U"THIS CHANGE IS PERMANENT AND MADE AT YOUR OWN RISKS!\n\n"MUIX_N
                                  "So, please, confirm this operation..."MUIX_L,
                                  (ULONG) (PEGASOS_MAGIC_GUID >> 32), (ULONG) PEGASOS_MAGIC_GUID,
                                  (ULONG) (mac >> 32), (ULONG) mac, (ULONG) (guid >> 32), (ULONG) guid);
                if (1 == res) {
                    res = MUI_Request(NULL, NULL, 0, "Helios Warning!", "_Obviously|*"MUIX_B"Oups, _Cancel!",
                                      MUIX_C"Really sure to do this EEPROM change?");
                    if (1 == res) {
                        ULONG cnt;

                        /* enable direct access to pins */
                        reg_Write(iobase, reg_Read(iobase) | 0x80);

                        for (cnt = 0; cnt < 4; cnt++)
                            write_4w(iobase, cnt, (guid >> (16*(3-cnt))) & 0xffff);

                        /* disable direct access to pins */
                        reg_Write(iobase + 0x20, 0x20);

                        MUI_Request(NULL, NULL, 0, "Helios Warning!", "*_Reboot now",
                                    "Modification done!\nI need to reboot now...");
                        ColdReboot();
                        MUI_Request(NULL, NULL, 0, "Helios Warning!", "Wait(0)",
                                    "Hey! Why are you here yet? Reboot yourself so!");
                        Wait(0);
                    }
                }
            }
        }
    }
}
//-
//+ parse_dev_rom
static void parse_dev_rom(DeviceListerData *data)
{
    ULONG len, gen;
    LONG res;
    QUADLET *rom, key, value, id[4]={0};
    HeliosRomIterator ri;

    res = Helios_GetAttrs(HGA_DEVICE, data->Device,
                          HA_Generation, (ULONG)&gen,
                          HA_Rom, (ULONG)&rom,
                          HA_RomLength, (ULONG)&len,
                          TAG_DONE);
    if ((res != 3) || (gen != data->Gen) || (NULL == rom))
        return;
    
    /* Parse the root directory */
    Helios_InitRomIterator(&ri, (APTR)rom + ROM_ROOT_DIRECTORY);
    while (Helios_RomIterate(&ri, &key, &value))
    {
        switch (key)
        {
            case CSR_KEY_MODULE_VENDOR_ID: id[0] = value; break;
            case CSR_KEY_MODEL_ID:		   id[1] = value; break;
            case CSR_KEY_UNIT_SPEC_ID:	   id[2] = value; break;
            case CSR_KEY_UNIT_SW_VERSION:  id[3] = value; break;
		}
    }

    /* Recognize some device types */
    data->Type = "<unknown>";
    if ((SBP2_UNIT_SPEC_ID_ENTRY == id[2]) && (CSR_KEY_UNIT_SW_VERSION == id[3]))
        data->Type = "SBP2";
    else if (0xa02d == id[2])
    {
        switch (id[3])
        {
            case 0x000100:
                data->Type = "DC";
                break;

            case 0x010000:
            case 0x010001:
                data->Type = "AVC";
                break;
        }
    }
}
//-
//+ reset_entry
static void reset_entry(DeviceListerData *data)
{
    if (NULL != data->Device)
    {
        /* Remove our events listener */
        if (NULL != data->DevEvtList)
        {
            Helios_RemoveEventListener(data->DevEvtList, &data->DevEvtMsg);
            data->DevEvtList = NULL;
        }

        /* let device die */
        Helios_ReleaseDevice(data->Device);
        data->Device = NULL;
    }
    data->LastEvent = 0;
    data->Type = "<unknown>";
    data->GUID[0] = '-';
    data->GUID[1] = '\0';
}
//-
//+ DeviceListConstruct
static APTR DeviceListConstruct(struct Hook *hook, APTR pool, HeliosDevice *dev)
{
    HeliosEventListenerList *evtlist = 0;
    ULONG nodeid = 0xffff, gen;
    UQUAD guid = 0;
    HeliosNode nodeinfo;

    LogMsg("New dev: %p\n", dev);

    /* Obtain various information about this device and register an event listener on it */
    if ((Helios_GetAttrs(HGA_DEVICE, dev,
                         HA_Generation, (ULONG)&gen,
                         HA_EventListenerList, (ULONG)&evtlist,
                         HA_GUID, (ULONG)&guid,
                         HA_NodeID, (ULONG)&nodeid,
                         HA_NodeInfo, (ULONG)&nodeinfo,
                         TAG_DONE) > 0)
        && (0xffff != nodeid)
        && (NULL != evtlist))
    {
        DeviceListerData *data;
        
        data = AllocPooled(pool, sizeof(*data));
        if (NULL != data)
        {
            data->Gen = gen;
            data->Device = dev;
            data->NodeID = nodeid;
            data->DevEvtList = evtlist;
            data->Type = "<unknown>";

            CopyMem(&nodeinfo, &data->NodeInfo, sizeof(data->NodeInfo));

            data->DevEvtMsg.hm_Msg.mn_Node.ln_Type = NT_MESSAGE;
            data->DevEvtMsg.hm_Msg.mn_ReplyPort = event_port;
            data->DevEvtMsg.hm_Msg.mn_Length = sizeof(data->DevEvtMsg);
            data->DevEvtMsg.hm_Type = HELIOS_MSGTYPE_EVENT;
            data->DevEvtMsg.hm_EventMask = HEVTF_DEVICE_ALL;
            data->DevEvtMsg.hm_UserData = data;
            Helios_AddEventListener(data->DevEvtList, &data->DevEvtMsg);

            if (guid != 0)
            {
                sprintf(data->GUID, "%016llx", guid);
                parse_dev_rom(data);
                data->LastEvent = HEVTF_DEVICE_SCANNED;
            }
            else
            {
                sprintf(data->GUID, "<unset>");
                data->LastEvent = HEVTF_DEVICE_UPDATED;
            }

            return data;
        }
    }
    
    LogMsg("Failed to add device %p\n", dev);
    return NULL;
}
//-
//+ DeviceListDestruct
static void DeviceListDestruct(struct Hook *hook, APTR pool, DeviceListerData *data)
{
    reset_entry(data);
    FreePooled(pool, data, sizeof(*data));
}
//-
//+ DeviceListDisplay
static LONG DeviceListDisplay(struct Hook *hook, STRPTR output[], DeviceListerData *data)
{
    if (NULL != data)
    {
        *output++ = dev_names[data->NodeInfo.n_PhyID];
        *output++ = data->NodeInfo.n_PhyID == gLastTopo.ht_LocalNodeID ? "o":"";
        *output++ = data->NodeInfo.n_PhyID == gLastTopo.ht_RootNodeID ? "o":"";
        *output++ = data->NodeInfo.n_PhyID == gLastTopo.ht_IRMNodeID ? "o":"";

        if (data->NodeInfo.n_MaxSpeed <= S400)
            *output++ = speed2str[data->NodeInfo.n_MaxSpeed];
        else
            *output++ = "BETA";

        *output++ = "<unknown>"; /* TODO: rom_node_type_str[]; */
        *output++ = data->Type;
        *output++ = data->GUID;
    }
    else
    {
        *output++ = "Id";
        *output++ = "Local";
        *output++ = "Root";
        *output++ = "IRM";
        *output++ = "Speed";
        *output++ = "Label";
        *output++ = "Type";
        *output++ = "GUID";
    }

    return 0;
}
//-
//+ DeviceListCompare
static LONG DeviceListCompare(struct Hook *hook, DeviceListerData *d1, DeviceListerData *d2)
{
    return (int) d2->NodeInfo.n_PhyID - (int) d1->NodeInfo.n_PhyID;
}
//-
//+ fail
static LONG fail(APTR app, char *str)
{
    if (app) MUI_DisposeObject(app);
    if (mcc) MUI_DeleteCustomClass(mcc);
    if (NULL != gHWEvtListenerList)
    {
        Helios_RemoveEventListener(gHWEvtListenerList, &busreset_msg);
        Helios_RemoveEventListener(gHWEvtListenerList, &topology_msg);
    }
    if (NULL != gHardware) Helios_ReleaseHardware(gHardware);
    if (event_port) DeleteMsgPort(event_port);
    if (io_port) DeleteMsgPort(io_port);
    if (HeliosBase) CloseLibrary(HeliosBase);
    if (PCIIDSBase) CloseLibrary(PCIIDSBase);
    if (PCIXBase) CloseLibrary(PCIXBase);
    if (str)
    {
        puts(str);
        exit(20);
    }

    exit(0);
    return 0;
}
//-
//+ Init
LONG Init(void)
{
    PCIXBase = OpenLibrary("pcix.library", 50);
    if (NULL != PCIXBase)
    {
        PCIIDSBase = OpenLibrary("pciids.library", 0);
        if (NULL != PCIIDSBase)
        {
            HeliosBase = OpenLibrary("helios.library", 0);
            if (NULL != HeliosBase)
            {
                io_port = CreateMsgPort();
                if (NULL != io_port)
                {
                    io_port_signal = 1 << io_port->mp_SigBit;

                    event_port = CreateMsgPort();
                    if (NULL != event_port)
                    {
                        event_port_signal = 1 << event_port->mp_SigBit;

                        Helios_ReadLockBase();
                        gHardware = Helios_GetNextHardware(NULL);
                        Helios_UnlockBase();

                        if (NULL != gHardware)
                        {
                            if ((Helios_GetAttrs(HGA_HARDWARE, gHardware,
                                                 HA_EventListenerList, (ULONG)&gHWEvtListenerList,
                                                 TAG_DONE) > 0)
                                && (NULL != gHWEvtListenerList))
                            {
                                // TODO: PimpMyPegasos();

                                /* Register some events */

                                busreset_msg.hm_Msg.mn_Node.ln_Type = NT_MESSAGE;
                                busreset_msg.hm_Msg.mn_ReplyPort = event_port;
                                busreset_msg.hm_Msg.mn_Length = sizeof(busreset_msg);
                                busreset_msg.hm_Type = HELIOS_MSGTYPE_FAST_EVENT;
                                busreset_msg.hm_EventMask = HEVTF_HARDWARE_BUSRESET;
                                busreset_msg.hm_UserData = gHardware;
                                Helios_AddEventListener(gHWEvtListenerList, &busreset_msg);

                                topology_msg.hm_Msg.mn_Node.ln_Type = NT_MESSAGE;
                                topology_msg.hm_Msg.mn_ReplyPort = event_port;
                                topology_msg.hm_Msg.mn_Length = sizeof(topology_msg);
                                topology_msg.hm_Type = HELIOS_MSGTYPE_FAST_EVENT;
                                topology_msg.hm_EventMask = HEVTF_HARDWARE_TOPOLOGY;
                                topology_msg.hm_UserData = gHardware;
                                Helios_AddEventListener(gHWEvtListenerList, &topology_msg);
                            
                                return TRUE;
                            }
                            else
                                LogMsg("Failed to obtain HW event listener list\n");

                            Helios_ReleaseHardware(gHardware);
                            gHardware = NULL;
                        }
                        else
                            LogMsg("Failed to obtain hardware\n");

                        DeleteMsgPort(event_port);
                        event_port = NULL;
                    }
                    else
                        LogMsg("Helios events port creation failed\n");

                    DeleteMsgPort(io_port);
                    io_port = NULL;
                }
                else
                    LogMsg("Helios IOs port creation failed\n");

                CloseLibrary(HeliosBase);
                HeliosBase = NULL;
            }
            else
                LogMsg("can't open helios.library\n");
        }
        else
            LogMsg("can't open pciids.library\n");
    }
    else
        LogMsg("can't open pcix.library\n");

    return NULL;
}
//-
//+ HandleHeliosEvents
BOOL HandleHeliosEvents(void)
{
    HeliosEventMsg *msg;
    BOOL run = TRUE;
    DeviceListerData *data = NULL;
    char buf[4];

    while (NULL != (msg = (APTR) GetMsg(event_port)))
    {
        switch (msg->hm_Type)
        {
            case HELIOS_MSGTYPE_FAST_EVENT:
            {
                /* re-register the event */
                Helios_AddEventListener(gHWEvtListenerList, msg);

                switch (msg->hm_EventMask)
                {
                    case HEVTF_HARDWARE_BUSRESET:
                        set(objs[OBJ_ROM_WIN], MUIA_Window_Open, FALSE);
                        BusEvents_LogMsg(0ull, "BusReset", "-Done-");
                        DoMethod(objs[OBJ_DEVICES_LIST], MUIM_List_Clear);
                        set(objs[OBJ_DEVICES_LIST], MUIA_Disabled, TRUE);
                        break;

                    case HEVTF_HARDWARE_TOPOLOGY:
                    {
                        struct TagItem tags[] = {
                            {HHA_Topology, (ULONG)&gLastTopo},
                            {TAG_DONE, 0}
                        };
                        IOHeliosHWReq ioreq_tmp;

                        ioreq_tmp.iohh_Req.io_Message.mn_Length = sizeof(ioreq_tmp);
                        ioreq_tmp.iohh_Req.io_Command = HHIOCMD_QUERYDEVICE;
                        ioreq_tmp.iohh_Data = tags;

                        if (Helios_DoIO(gHardware, &ioreq_tmp))
                        {
                            BusEvents_LogMsg(0ull, "Topology ready",
                                             "Generation %lu",
                                             gLastTopo.ht_Generation);

                            snprintf(buf, sizeof(buf), "%lu", gLastTopo.ht_Generation);
                            set(objs[OBJ_GENERATION], MUIA_Text_Contents, buf);

                            snprintf(buf, sizeof(buf), "%u", gLastTopo.ht_NodeCount);
                            set(objs[OBJ_DEVICE_COUNT], MUIA_Text_Contents, buf);

                            set(objs[OBJ_DEVICES_LIST], MUIA_List_Quiet, TRUE);
                            Helios_ReadLockBase();
                            {
                                HeliosDevice *dev = NULL;

                                /* Add all devices */
                                while (NULL != (dev = Helios_GetNextDevice(gHardware, dev)))
                                    DoMethod(objs[OBJ_DEVICES_LIST], MUIM_List_InsertSingle, dev, MUIV_List_Insert_Sorted);
                            }
                            Helios_UnlockBase();
                            set(objs[OBJ_DEVICES_LIST], MUIA_List_Quiet, FALSE);

                            set(objs[OBJ_DEVICES_LIST], MUIA_Disabled, FALSE);
                        }
                        else
                            BusEvents_LogMsg(0ull, "Topology ready",
                                             "Error: can't obtain topo info");
                    }
                    break;
                }
            }
            break;

            case HELIOS_MSGTYPE_EVENT:
            {
                //LogMsg("event: %u, result = $%08x\n", msg->hm_EventMask, msg->hm_Result);
                switch (msg->hm_EventMask)
                {
                    case HEVTF_DEVICE_SCANNED:
                    {
                        HeliosDevice *dev = (HeliosDevice *)msg->hm_Result;
                        ULONG gen;
                        UQUAD guid;

                        LogMsg("scanned dev: %p\n", dev);
                        if (2 == Helios_GetAttrs(HGA_DEVICE, dev,
                                                 HA_Generation, (ULONG)&gen,
                                                 HA_GUID, (ULONG)&guid,
                                                 TAG_DONE) && (gen == gLastTopo.ht_Generation))
                        {
                            data = msg->hm_UserData;
                            //printf("gen: %lu-%lu, data: %p, dev=%p\n", gen, gLastTopo.ht_Generation, data, data->Device);

                            sprintf(data->GUID, "%016llx", guid);
                            data->LastEvent = HEVTF_DEVICE_SCANNED;
                            parse_dev_rom(data);
                            
                            DoMethod(objs[OBJ_DEVICES_LIST], MUIM_List_Redraw, MUIV_List_Redraw_Entry, data);
                        }
                    }
                    break;

                    case HEVTF_DEVICE_DEAD:
                        //data = msg->hm_UserData;
                        //sprintf(data->GUID, "<dead>");
                        //data->LastEvent = HEVTF_DEVICE_DEAD;
                        //DoMethod(objs[OBJ_DEVICES_LIST], MUIM_List_Redraw, MUIV_List_Redraw_Entry, data);
                        break;

                    case HEVTF_DEVICE_REMOVED:
                        LogMsg("removed dev %p\n", msg->hm_Result);
                        break;
                }

                /* these messages must be free by user */
                FreeMem(msg, msg->hm_Msg.mn_Length);
            }
            break;
        }
    }

    return run;
}
//-
//+ HandleHeliosIO
void HandleHeliosIO(void)
{
    IOHeliosHWSendRequest *ioreq;
    HeliosAPacket *p;
    LONG err;
    char buf[30];

    while (NULL != (ioreq = (APTR) GetMsg(io_port)))
    {
        switch (ioreq->iohhe_Req.iohh_Req.io_Command)
        {
            case HHIOCMD_SENDREQUEST:
                err = ioreq->iohhe_Req.iohh_Req.io_Error;
                p = &ioreq->iohhe_Transaction.htr_Packet;

                switch (p->RCode)
                {
                    case HELIOS_RCODE_COMPLETE:
                        if (TCODE_READ_QUADLET_REQUEST == p->TCode)
                        {
                            snprintf(buf, sizeof(buf), "$%08x", p->QuadletData);
                            set(objs[OBJ_READ_QUADLET_RESULT], MUIA_Text_Contents, buf);
                            set(objs[OBJ_READ_QUADLET_OK], MUIA_Disabled, FALSE);
                        }
                        else
                            set(objs[OBJ_WRITE_QUADLET_OK], MUIA_Disabled, FALSE);
                        break;

                    case HELIOS_RCODE_CANCELLED:
                        if (TCODE_READ_QUADLET_REQUEST == p->TCode)
                            set(objs[OBJ_READ_QUADLET_RESULT], MUIA_Text_Contents, "Cancelled");
                        else
                            BusEvents_LogMsg(0ull, "Write error", "Cancelled request!");
                        break;

                    default:
                        if (TCODE_READ_QUADLET_REQUEST == p->TCode)
                        {
                            snprintf(buf, sizeof(buf), "%s error", rcode_str[p->RCode]);
                            set(objs[OBJ_READ_QUADLET_RESULT], MUIA_Text_Contents, buf);
                        }
                        else
                            BusEvents_LogMsg(0ull, "Write error", "%s error", rcode_str[p->RCode]);
                }
                
                FreeVec(ioreq);
                break;

            default: /* not handled */ ;
        }
    }
}
//-
//+ main
int main(int argc, char **argv)
{
    Object *win_main;
    Object *win_read_quadlet, *win_write_quadlet, *rom_prop=NULL;
    BOOL run = TRUE;
    LONG sigs, i;
    char buf[80];
    APTR rom = NULL;
    UWORD vid, did;
    ULONG vcid;
    UQUAD local_guid;
    CONST_STRPTR vid_str, did_str;
    struct TagItem tags[] = {
        {OHCI1394A_PciVendorId, (ULONG)&vid},
        {OHCI1394A_PciDeviceId, (ULONG)&did},
        {HHA_VendorCompagnyId, (ULONG)&vcid},
        {HHA_LocalGUID, (ULONG)&local_guid},
        {HHA_Topology, (ULONG)&gLastTopo},
        {TAG_DONE, 0}
    };
    IOHeliosHWReq ioreq_tmp;

    INIT_HOOK(&DeviceListConstructHook, DeviceListConstruct);
    INIT_HOOK(&DeviceListDestructHook, DeviceListDestruct);
    INIT_HOOK(&DeviceListDisplayHook, DeviceListDisplay);
    INIT_HOOK(&DeviceListCompareHook, DeviceListCompare);

    for (i=0; i<63; i++)
        snprintf(&dev_names[i][0], 3, "%2lu", i);

    if (!Init())
        fail(NULL, "Init() failed.");

    ioreq_tmp.iohh_Req.io_Message.mn_Length = sizeof(ioreq_tmp);
    ioreq_tmp.iohh_Req.io_Command = HHIOCMD_QUERYDEVICE;
    ioreq_tmp.iohh_Data = tags;

    if (!Helios_DoIO(gHardware, &ioreq_tmp))
        fail(NULL, "HHIOCMD_QUERYDEVICE failed.");

    vid_str = PCIIDS_GetVendorName(vid);
    did_str = PCIIDS_GetDeviceName(vid, did);

    app = ApplicationObject,
        MUIA_Application_Title      , "FireWire inspector",
        MUIA_Application_Version    , "$VER: FWInspect 0.4 ("__DATE__")",
        MUIA_Application_Copyright  , "\x40 2008-2010, Guillaume ROGUEZ",
        MUIA_Application_Author     , "Guillaume ROGUEZ",
        MUIA_Application_Description, "Inspect local firewire buses",
        MUIA_Application_Base       , "FWInspect",
        MUIA_Application_SingleTask , TRUE,
        MUIA_Application_Iconified  , FALSE,

//+     win_main
        SubWindow, win_main = WindowObject,
            MUIA_Window_Title, "FireWire inspector",
            MUIA_Window_ID   , MAKE_ID('M','A','I','N'),
            WindowContents, VGroup,
                Child, HGroup,
                    Child, VGroup,
                        MUIA_Weight, 1,

                        Child, HCenter(Image("PROGDIR:FWInspect.info")),

                        Child, ColGroup(2),
                            GroupFrameT("Bus Information"),

                            MkLabel(objs[OBJ_VID], "PCI VendorID :", (snprintf(buf, sizeof(buf), "$%04x (%s)", vid, vid_str?vid_str:(CONST_STRPTR)"Unknown"), buf), FALSE),
                            MkLabel(objs[OBJ_DID], "PCI DeviceID :", (snprintf(buf, sizeof(buf), "$%04x (%s)", did, did_str?did_str:(CONST_STRPTR)"Unknown"), buf), FALSE),
                            DoLabel("OHCI VendorID :", (snprintf(buf, sizeof(buf), "$%08lx", vcid), buf), FALSE),
                            DoLabel("OHCI GUID :", (snprintf(buf, sizeof(buf), "$%016llx", local_guid), buf), TRUE),
                            MkLabel(objs[OBJ_GENERATION], "Generation :", (sprintf(buf, "%lu", gLastTopo.ht_Generation), buf), FALSE),
                            MkLabel(objs[OBJ_DEVICE_COUNT], "Nodes # :", (sprintf(buf, "%u", gLastTopo.ht_NodeCount), buf), FALSE),
                        End,

                        Child, VSpace(0),
                    End,

                    Child, BalanceObject, End,

                    Child, VGroup,
                        GroupFrameT("Bus Devices"),
                        MUIA_Weight, 100,

                        Child, objs[OBJ_DEVICES_LIST] = ListviewObject,
                            MUIA_Listview_Input, FALSE,
                            MUIA_Listview_List, ListObject,
                                ReadListFrame,
                                MUIA_List_ConstructHook,    &DeviceListConstructHook,
                                MUIA_List_DestructHook,     &DeviceListDestructHook,
                                MUIA_List_DisplayHook,      &DeviceListDisplayHook,
                                MUIA_List_CompareHook,      &DeviceListCompareHook,
                                MUIA_List_Format,           "P="MUIX_R" BAR,"
                                                            "P="MUIX_C" BAR,"
                                                            "P="MUIX_C" BAR,"
                                                            "P="MUIX_C" BAR,"
                                                            "P="MUIX_C" BAR,"
                                                            "BAR,BAR,BAR",
                                MUIA_List_Title,            TRUE,
                                MUIA_CycleChain,            TRUE,
                            End,
                        End,
                    End,

                    Child, VGroup,
                        GroupFrameT("Device Operations"),
                        MUIA_Weight, 0,

                        Child, objs[OBJ_DUMP_ROM]      = SimpleButton("_Dump ROM"),
                        Child, objs[OBJ_READ_QUADLET]  = SimpleButton("_Read Quadlet"),
                        Child, objs[OBJ_WRITE_QUADLET] = SimpleButton("_Write Quadlet"),

                        Child, VSpace(0),
                    End,
                End,

                Child, HGroup,
                    Child, objs[OBJ_DUMP_OHCI]   = SimpleButton("Dump OHCI registers"),
                    Child, objs[OBJ_BUS_RESET]   = SimpleButton("Reset _bus"),
                    Child, objs[OBJ_BUS_ENABLE]  = SimpleButton("_Enable bus"),
                    Child, objs[OBJ_BUS_DISABLE] = SimpleButton("D_isable bus"),
                End,

                Child, BalanceObject, End,

                Child, HGroup,
                    GroupFrameT("Helios events"),
                    MUIA_Weight, 1,

                    Child, VGroup,
                        Child, objs[OBJ_BUS_EVT_LOG] = ListviewObject,
                            MUIA_Listview_Input, FALSE,
                            MUIA_Listview_List, ListObject,
                                ReadListFrame,      
                                MUIA_Font, MUIV_Font_Fixed,
                                MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
                                MUIA_List_DestructHook, MUIV_List_DestructHook_String,
                            End,
                        End,
                        Child, HGroup,
                            Child, objs[OBJ_BUS_EVT_SAVE] = SimpleButton("_Save log"),
                            Child, objs[OBJ_BUS_EVT_CLEAR] = SimpleButton("_Clear log"),
                        End,
                    End,
                End,
            End,
        End,
//-
//+     OBJ_ROM_WIN
        SubWindow, objs[OBJ_ROM_WIN] = WindowObject,
            MUIA_Window_Title, "ROM Viewer",
            MUIA_Window_ID   , MAKE_ID('R','O','M',' '),
            MUIA_Window_UseRightBorderScroller, TRUE,

            WindowContents, VGroup,
                Child, objs[OBJ_ROM_GROUP] = HGroup,
                End,
            End,
        End,
//-
//+     win_read_quadlet
        SubWindow, win_read_quadlet = WindowObject,
            MUIA_Window_Title, "Read Quadlet",
            MUIA_Window_ID   , MAKE_ID('R','E','A','D'),
            WindowContents, VGroup,
                Child, ColGroup(2),
                    Child, Label2("Address (hex) :"),
                    Child, objs[OBJ_READ_QUADLET_ADDRESS] = StringObject,
                        StringFrame,
                        MUIA_String_Accept, "0123456789abcdefABCDEF",
                        MUIA_String_MaxLen, 13,
                        MUIA_CycleChain, 1,
                    End,

                    Child, Label1("Result :"),
                    Child, objs[OBJ_READ_QUADLET_RESULT] = TextObject,
                        NoFrame,
                        MUIA_Background, 0,
                        MUIA_Text_Contents, "<not set>",
                        MUIA_CycleChain, 1,
                    End,
                End,

                Child, HGroup,
                    Child, objs[OBJ_READ_QUADLET_CANCEL] = SimpleButton("_Cancel"),
                    Child, objs[OBJ_READ_QUADLET_OK] = SimpleButton(MUIX_B"_Read"),
                End,
            End,
        End,
//-
//+     win_write_quadlet
        SubWindow, win_write_quadlet = WindowObject,
            MUIA_Window_Title, "Write Quadlet",
            MUIA_Window_ID   , MAKE_ID('W','R','I','T'),
            WindowContents, VGroup,
                Child, ColGroup(2),
                    Child, Label2("Address (in hex) :"),
                    Child, objs[OBJ_WRITE_QUADLET_ADDRESS] = StringObject,
                        StringFrame,
                        MUIA_String_Accept, "0123456789abcdefABCDEF",
                        MUIA_String_MaxLen, 13,
                        MUIA_CycleChain, 1,
                    End,

                    Child, Label1("Value (in hex) :"),
                    Child, objs[OBJ_WRITE_QUADLET_VALUE] = StringObject,
                        StringFrame,
                        MUIA_String_Accept, "0123456789abcdefABCDEF",
                        MUIA_String_MaxLen, 9,
                        MUIA_CycleChain, 1,
                    End,
                End,

                Child, HGroup,
                    Child, objs[OBJ_WRITE_QUADLET_CANCEL] = SimpleButton("_Cancel"),
                    Child, objs[OBJ_WRITE_QUADLET_OK] = SimpleButton(MUIX_B"_Write"),
                End,
            End,
        End,
//-
    End;

    if (!app)
        fail(app, "Failed to create Application.");

    /* Main window */
    DoMethod(win_main, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    DoMethod(objs[OBJ_BUS_ENABLE], MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_BUS_ENABLE);

    DoMethod(objs[OBJ_BUS_DISABLE], MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_BUS_DISABLE);

    DoMethod(objs[OBJ_BUS_EVT_CLEAR], MUIM_Notify, MUIA_Pressed, FALSE,
             objs[OBJ_BUS_EVT_LOG], 1, MUIM_List_Clear);

    DoMethod(objs[OBJ_BUS_RESET], MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_BUS_RESET);

    DoMethod(objs[OBJ_DUMP_ROM], MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_DUMP_ROM);

    DoMethod(objs[OBJ_READ_QUADLET], MUIM_Notify, MUIA_Pressed, FALSE,
             win_read_quadlet, 3, MUIM_Set, MUIA_Window_Open, TRUE);

    DoMethod(objs[OBJ_WRITE_QUADLET], MUIM_Notify, MUIA_Pressed, FALSE,
             win_write_quadlet, 3, MUIM_Set, MUIA_Window_Open, TRUE);

    DoMethod(objs[OBJ_DEVICES_LIST], MUIM_Notify, MUIA_List_DoubleClick, TRUE,
             app, 2, MUIM_Application_ReturnID, ID_DUMP_ROM);

    DoMethod(objs[OBJ_DUMP_OHCI], MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_DUMP_OHCI);

    if (NULL != vid_str)
        set(objs[OBJ_VID], MUIA_ShortHelp, vid_str);
    if (NULL != did_str)
        set(objs[OBJ_DID], MUIA_ShortHelp, did_str);

//+     Read quadlet window
    DoMethod(win_read_quadlet, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             win_read_quadlet, 3, MUIM_Set, MUIA_Window_Open, FALSE);

    DoMethod(objs[OBJ_READ_QUADLET_CANCEL], MUIM_Notify, MUIA_Pressed, FALSE,
             win_read_quadlet, 3, MUIM_Set, MUIA_Window_Open, FALSE);

    DoMethod(objs[OBJ_READ_QUADLET_OK], MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_READ_QUADLET);
//-
//+     Write quadlet window
    DoMethod(win_write_quadlet, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             win_write_quadlet, 3, MUIM_Set, MUIA_Window_Open, FALSE);

    DoMethod(objs[OBJ_WRITE_QUADLET_CANCEL], MUIM_Notify, MUIA_Pressed, FALSE,
             win_write_quadlet, 3, MUIM_Set, MUIA_Window_Open, FALSE);

    DoMethod(objs[OBJ_WRITE_QUADLET_OK], MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_Application_ReturnID, ID_WRITE_QUADLET);
//-
//+     ROM Window
    DoMethod(objs[OBJ_ROM_WIN], MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             objs[OBJ_ROM_WIN], 3, MUIM_Set, MUIA_Window_Open, FALSE);
//-

    /* Fill the devices list with current devices */
    set(objs[OBJ_DEVICES_LIST], MUIA_List_Quiet, TRUE);
    Helios_ReadLockBase();
    {
        HeliosDevice *dev = NULL;

        /* Add all devices */
        while (NULL != (dev = Helios_GetNextDevice(gHardware, dev)))
            DoMethod(objs[OBJ_DEVICES_LIST], MUIM_List_InsertSingle, dev, MUIV_List_Insert_Sorted);
    }
    Helios_UnlockBase();
    set(objs[OBJ_DEVICES_LIST], MUIA_List_Quiet, FALSE);

    /* CycleChain setup */
    DoMethod(app, MUIM_MultiSet, MUIA_CycleChain, TRUE,
        objs[OBJ_DUMP_OHCI],
        objs[OBJ_DUMP_ROM],
        objs[OBJ_READ_QUADLET],
        objs[OBJ_WRITE_QUADLET],
        objs[OBJ_BUS_RESET],
        objs[OBJ_BUS_ENABLE],
        objs[OBJ_BUS_DISABLE],
        objs[OBJ_BUS_EVT_SAVE],
        objs[OBJ_BUS_EVT_CLEAR],
        objs[OBJ_READ_QUADLET_OK],
        objs[OBJ_WRITE_QUADLET_OK],
        objs[OBJ_READ_QUADLET_CANCEL],
        objs[OBJ_WRITE_QUADLET_CANCEL],
        NULL);

    /* Disable unimplemented features */
    set(objs[OBJ_BUS_EVT_SAVE], MUIA_Disabled, TRUE);

    set(win_main, MUIA_Window_Open, TRUE);

    while (run)
    {
        LONG id = DoMethod(app, MUIM_Application_Input, &sigs);

//+     id
        switch (id)
        {
            case MUIV_Application_ReturnID_Quit:
                run = FALSE;
                break;

            case ID_DUMP_OHCI:
                //TODO: Helios_DumpOHCI(bus);
                break;

            case ID_BUS_RESET:
                Helios_BusReset(gHardware, TRUE);
                break;

            case ID_BUS_ENABLE:
                Helios_DisableHardware(gHardware, FALSE);
                break;

            case ID_BUS_DISABLE:
                Helios_DisableHardware(gHardware, TRUE);
                break;

            case ID_DUMP_ROM:
                {
                    DeviceListerData *data = NULL;
                    Object *obj;
                    LONG x;

                    if (get(objs[OBJ_DEVICES_LIST], MUIA_List_Active, &x) && (x >= 0))
                    {
                        DoMethod(objs[OBJ_DEVICES_LIST], MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &data);
                        if (NULL != data)
                        {
                            /* If scanned, get the read ROM */
                            if (HEVTF_DEVICE_SCANNED == data->LastEvent)
                            {
                                Object *new_rom_prop;
                                ULONG len;
                                QUADLET *tmp;
                                LONG res;

                                Helios_ReadLockDevice(data->Device);
                                {
                                    tmp = NULL;
                                    res = Helios_GetAttrs(HGA_DEVICE, data->Device,
                                                          HA_Rom, (ULONG)&tmp,
                                                          HA_RomLength, (ULONG)&len,
                                                          TAG_DONE);
                                    if ((res > 0) && (NULL != tmp) && (len > 0))
                                    {
                                        QUADLET *tmp2;

                                        /* Duplicate data */
                                        tmp2 = AllocVec(len, MEMF_PUBLIC | MEMF_CLEAR);
                                        if (NULL != tmp2)
                                        {
                                            CopyMem(tmp, tmp2, len);
                                            tmp = tmp2;
                                        }
                                        else
                                            tmp = NULL;
                                    }
                                    else
                                        LogMsg("Failed to get ROM data\n");
                                }
                                Helios_UnlockDevice(data->Device);

                                if (NULL != tmp)
                                {
                                    /* Create a new HexEdit object first before destroying any old one */
                                    obj = HexEditObject,
                                        VirtualFrame,
                                        MUIA_HexEdit_LowBound,          tmp,
                                        MUIA_HexEdit_HighBound,         (APTR)tmp + len - 1,
                                        MUIA_HexEdit_BaseAddressOffset, CSR_CONFIG_ROM_OFFSET - (ULONG)tmp,
                                        MUIA_HexEdit_EditMode,          FALSE,
                                        MUIA_HexEdit_PropObject,        new_rom_prop = ScrollbarObject,
                                        MUIA_Prop_UseWinBorder,         MUIV_Prop_UseWinBorder_Right,
                                        End,
                                        End;

                                    if ((NULL != obj) && (NULL != new_rom_prop))
                                    {
                                        DoMethod(objs[OBJ_ROM_GROUP], MUIM_Group_InitChange);

                                        /* Remove old HexEdit object */
                                        if (NULL != objs[OBJ_ROM_HEX])
                                        {
                                            DoMethod(objs[OBJ_ROM_GROUP], OM_REMMEMBER, objs[OBJ_ROM_HEX]);
                                            DoMethod(objs[OBJ_ROM_GROUP], OM_REMMEMBER, rom_prop);
                                            MUI_DisposeObject(objs[OBJ_ROM_HEX]);
                                        }

                                        /* Delete old ROM data */
                                        if (NULL != rom)
                                            FreeVec(rom);

                                        /* Setup the new ROM data and HexEdit object */
                                        rom = tmp;
                                        rom_prop = new_rom_prop;
                                        DoMethod(objs[OBJ_ROM_GROUP], MUIM_Group_AddHead, rom_prop);
                                        DoMethod(objs[OBJ_ROM_GROUP], MUIM_Group_AddTail, obj);
                                        objs[OBJ_ROM_HEX] = obj;

                                        DoMethod(objs[OBJ_ROM_GROUP], MUIM_Group_ExitChange);
                                        set(objs[OBJ_ROM_WIN], MUIA_Window_Open, TRUE);
                                        break;
                                    }
                                    else
                                    {
                                        if (new_rom_prop) MUI_DisposeObject(new_rom_prop);
                                        if (obj) MUI_DisposeObject(obj);
                                        FreeVec(tmp);
                                    }
                                }
                                else
                                    LogMsg("Unable to obtain ROM data of node $%X\n", data->NodeID);
                            }
                        }
                        else
                            LogMsg("Bad selected device\n");
                    }
                    else
                        LogMsg("No selected device\n");

                    DisplayBeep(NULL);
                }
                break;

            case ID_READ_QUADLET:
            case ID_WRITE_QUADLET:
            {
                DeviceListerData *data;
                IOHeliosHWSendRequest *ioreq;
                HeliosAPacket *p;
                STRPTR addr_str = NULL, value_str = NULL;
                HeliosOffset offset = 0;
                QUADLET values[4] = {0};
                LONG x;

                ioreq = AllocVec(sizeof(*ioreq), MEMF_PUBLIC|MEMF_CLEAR);
                if (NULL != ioreq)
                {
                    if (get(objs[OBJ_DEVICES_LIST], MUIA_List_Active, &x) && (x >= 0))
                    {
                        DoMethod(objs[OBJ_DEVICES_LIST], MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &data);
                        if (NULL != data)
                        {
                            switch (id)
                            {
                                case ID_READ_QUADLET:
                                    get(objs[OBJ_READ_QUADLET_ADDRESS], MUIA_String_Contents, &addr_str);
                                    if (NULL == addr_str)
                                    {
                                        LogMsg("Unable to obtain the address\n");
                                        goto flash;
                                    } else
                                        offset = strtoll(addr_str, NULL, 16);
                                    break;

                                case ID_WRITE_QUADLET:
                                    get(objs[OBJ_WRITE_QUADLET_ADDRESS], MUIA_String_Contents, &addr_str);
                                    if (NULL != addr_str)
                                    {
                                        offset = strtoll(addr_str, NULL, 16);

                                        get(objs[OBJ_WRITE_QUADLET_VALUE], MUIA_String_Contents, &value_str);
                                        if (NULL == value_str)
                                        {
                                            LogMsg("Unable to obtain the value\n");
                                            goto flash;
                                        }
                                        values[0] = strtoul(value_str, NULL, 16);
                                    }
                                    else
                                    {
                                        LogMsg("Unable to obtain the address\n");
                                        goto flash;
                                    }
                                    break;
                            }

                            p = &ioreq->iohhe_Transaction.htr_Packet;
                            switch (id)
                            {
                                case ID_READ_QUADLET:
                                    Helios_FillReadQuadletPacket(p, S100, offset);
                                    set(objs[OBJ_READ_QUADLET_OK], MUIA_Disabled, TRUE);
                                    break;

                                case ID_WRITE_QUADLET:
                                    Helios_FillWriteQuadletPacket(p, S100, offset, values[0]);
                                    set(objs[OBJ_WRITE_QUADLET_OK], MUIA_Disabled, TRUE);
                                    break;
                            }

                            Helios_InitIOReq(gHardware, &ioreq->iohhe_Req);

                            ioreq->iohhe_Req.iohh_Req.io_Message.mn_ReplyPort = io_port;
                            ioreq->iohhe_Req.iohh_Req.io_Message.mn_Length = sizeof(ioreq);
                            ioreq->iohhe_Req.iohh_Req.io_Command = HHIOCMD_SENDREQUEST;
                            ioreq->iohhe_Device = data->Device;
                            ioreq->iohhe_Req.iohh_Data = NULL; /* quadlet read => QuadletData packet field */
                            ioreq->iohhe_Req.iohh_Length = 0;

                            /* BUG: I don't AbortIO at exit! */
                            SendIO(&ioreq->iohhe_Req.iohh_Req);
                        }
                        else
                            LogMsg("Bad selected device\n");
                    }
                    else
                        LogMsg("No selected device\n");
                }
                else
                    LogMsg("Failed to alloc ioreq for sending request\n");

            flash:
                if (NULL != ioreq)
                    FreeVec(ioreq);
                DisplayBeep(NULL);
            }
            break;
        }
//-

        if (run)
        {
            if (sigs)
                sigs = Wait(sigs | io_port_signal | event_port_signal);
            else
                sigs = SetSignal(0, (io_port_signal | event_port_signal));

            if (sigs & event_port_signal)
                run = HandleHeliosEvents();

            if (sigs & io_port_signal)
                HandleHeliosIO();
        }
    }

    set(win_main, MUIA_Window_Open, FALSE);

    if (NULL != rom)
        FreeVec(rom);

    return fail(app, NULL);
}
//-
