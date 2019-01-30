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
** Provide an API to handle various internal structures.
*/

/* General note:
 * HeliosBase lock is used to protect objects changes (hw/dev/unit).
 * DOC: call Helios_ReleaseXXX ouside of locked regions.
 */

#include "private.h"

#include <utility/pack.h>
#include <clib/macros.h>
#include <hardware/byteswap.h>

#include <proto/alib.h>
#include <proto/timer.h>

#include <string.h>

#define HELIOSPTASIZE (sizeof(gHeliosPTA)/sizeof(ULONG *))

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

#define PHY_CONFIG_GAP_COUNT(gap_count) (((gap_count) << 16) | (1 << 22))
#define PHY_CONFIG_ROOT_ID(node_id) ((((node_id) & 0x3f) << 24) | (1 << 23))
#define PHY_IDENTIFIER(id) ((id) << 30)

#define PHY_PACKET_CONFIG   0x0
#define PHY_PACKET_LINK_ON  0x1
#define PHY_PACKET_SELF_ID  0x2

static const ULONG gHeliosPTBase[] =
{
    PACK_STARTTABLE(HA_Dummy),
    PACK_ENDTABLE
};

static const ULONG gHeliosPTHardware[] =
{
    PACK_STARTTABLE(HA_Dummy),
    PACK_ENDTABLE
};

static const ULONG gHeliosPTDevice[] =
{
    PACK_STARTTABLE(HA_Dummy),
    PACK_ENTRY(HA_Dummy, HA_NodeID, HeliosDevice, hd_NodeID, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENTRY(HA_Dummy, HA_Rom, HeliosDevice, hd_Rom, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENTRY(HA_Dummy, HA_RomLength, HeliosDevice, hd_RomLength, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENTRY(HA_Dummy, HA_GUID_Hi, HeliosDevice, hd_GUID.w.hi, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENTRY(HA_Dummy, HA_GUID_Lo, HeliosDevice, hd_GUID.w.lo, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENTRY(HA_Dummy, HA_Generation, HeliosDevice, hd_Generation, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    //PACK_ENTRY(HA_Dummy, HA_Hardware, HeliosDevice, hd_Hardware, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENDTABLE
};

static const ULONG gHeliosPTUnit[] =
{
    PACK_STARTTABLE(HA_Dummy),
    //PACK_ENTRY(HA_Dummy, HA_Device, HeliosUnit, hu_Device, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    //PACK_ENTRY(HA_Dummy, HA_Hardware, HeliosUnit, hu_Hardware, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENTRY(HA_Dummy, HA_UnitNo, HeliosUnit, hu_UnitNo, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENTRY(HA_Dummy, HA_CSR_VendorID, HeliosUnit, hu_Ids.fields.VendorID, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENTRY(HA_Dummy, HA_CSR_ModelID, HeliosUnit, hu_Ids.fields.ModelID, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENTRY(HA_Dummy, HA_CSR_UnitSpecID, HeliosUnit, hu_Ids.fields.UnitSpecID, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENTRY(HA_Dummy, HA_CSR_UnitSWVersion, HeliosUnit, hu_Ids.fields.UnitSWVersion, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENTRY(HA_Dummy, HA_UnitRomDirectory, HeliosUnit, hu_RomDirectory, PKCTRL_ULONG|PKCTRL_UNPACKONLY),
    PACK_ENDTABLE
};

static const ULONG *gHeliosPTA[] =
{
    [0] = NULL,
    [HGA_BASE] = gHeliosPTBase,
    [HGA_HARDWARE] = gHeliosPTHardware,
    [HGA_DEVICE] = gHeliosPTDevice,
    [HGA_UNIT] = gHeliosPTUnit,
    [HGA_CLASS] = NULL,
};

static const UBYTE gHeliosGapCountTable[] =
{
    63, 5, 7, 8, 10, 13, 16, 18, 21, 24, 26, 29, 32, 35, 37, 40
};

/*----------------------------------------------------------------------------*/
/*--- LOCAL CODE SECTION -----------------------------------------------------*/

static LONG helios_do_simple_ioreq(HeliosHardware *hw, IOHeliosHWReq *ioreq)
{
    struct MsgPort port;

    port.mp_Flags   = PA_SIGNAL;
    port.mp_SigBit  = SIGB_SINGLE;
    port.mp_SigTask = FindTask(NULL);
    NEWLIST(&port.mp_MsgList);

    Helios_InitIO(HGA_HARDWARE, hw, ioreq);

    ioreq->iohh_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->iohh_Req.io_Message.mn_ReplyPort = &port;

    return DoIO((struct IORequest *)ioreq);
}

static void helios_free_unit(HeliosUnit *unit)
{
    _INFO("freeing unit %p\n", unit);
    FreePooled(HeliosBase->hb_MemPool, unit, sizeof(*unit));
}

static void helios_free_dev(HeliosDevice *dev)
{
    _INFO("freeing dev %p (#%u)\n", dev, dev->hd_NodeInfo.n_PhyID);

    if (NULL != dev->hd_Rom)
    {
        FreePooled(HeliosBase->hb_MemPool, dev->hd_Rom, dev->hd_RomLength);
    }
    FreePooled(HeliosBase->hb_MemPool, dev, sizeof(*dev));
}

/* WARNING: HeliosBase must be w-locked */
static void helios_remove_unit(HeliosUnit *unit)
{
    HeliosHardware *hw;
    HeliosDevice *dev;

    REMOVE(unit);

    LOCK_REGION(unit);
    {
        HeliosClass *hc = unit->hu_BindClass;

        /* Make a copy as erased after */
        hw = unit->hu_Hardware;
        dev = unit->hu_Device;

        /* ask the bind class to release the unit */
        if (NULL != hc)
        {
            HeliosClass_DoMethod(HCM_ReleaseUnitBinding, (ULONG)unit, (ULONG)unit->hu_BindUData);
        }

        /* NOTE: the previous call is asynchrone, the unit may not be released yet.
         * DOC: warn classes implementors that unit data are NULL after HCM_ReleaseUnitBinding.
         */

        /* Invalid unit data */
        bzero((APTR)unit + offsetof(HeliosUnit, hu_UnitNo),
              sizeof(HeliosUnit)-offsetof(HeliosUnit, hu_UnitNo));
    }
    UNLOCK_REGION(unit);

    Helios_ReleaseUnit(unit);
    Helios_ReleaseDevice(dev);
    Helios_ReleaseHardware(hw);
}

/* WARNING: HeliosBase/Device must be w-locked. */
static void helios_dev_set_dead(HeliosDevice *dev)
{
    /* Do nothing if already dead */
    if (dev->hd_Generation > 0)
    {
        HeliosUnit *unit, *next;

        _INFO("Del. dev $%x (%p)\n", dev, dev->hd_NodeID);

        dev->hd_Generation = 0;
        ForeachNodeSafe(&dev->hd_Units, unit, next)
        {
            helios_remove_unit(unit);
        }

        if (NULL != dev->hd_Rom)
        {
            FreePooled(HeliosBase->hb_MemPool, dev->hd_Rom, dev->hd_RomLength);
            dev->hd_Rom = NULL;
            dev->hd_RomLength = 0;
        }

        Helios_SendEvent(&dev->hd_Listeners, HEVTF_DEVICE_DEAD, (ULONG)dev);
    }
}

static void helios_get_ids(const QUADLET *rom, QUADLET *id)
{
    HeliosRomIterator ri;
    QUADLET key, value;

    Helios_InitRomIterator(&ri, rom);
    while (Helios_RomIterate(&ri, &key, &value))
    {
        switch (key)
        {
            case CSR_KEY_MODULE_VENDOR_ID: id[0] = value; break;
            case CSR_KEY_MODEL_ID:         id[1] = value; break;
            case CSR_KEY_UNIT_SPEC_ID:     id[2] = value; break;
            case CSR_KEY_UNIT_SW_VERSION:  id[3] = value; break;
        }
    }
}

/* WARNING: HeliosBase must be w-locked and device must be w-locked */
static void helios_create_unit(HeliosDevice *dev, const QUADLET *directory, LONG unitno)
{
    HeliosUnit *unit;
    HeliosClass *hc;
    QUADLET *id;

    unit = AllocPooled(HeliosBase->hb_MemPool, sizeof(*unit));
    if (NULL == unit)
    {
        _ERR("Failed to alloc unit\n");
        return;
    }

    LOCK_INIT(unit);
    unit->hu_UnitNo = unitno;
    unit->hu_Device = dev;
    unit->hu_Hardware = dev->hd_Hardware;
    unit->hu_RomDirectory = directory;
    unit->hso_RefCnt = 1; /* ADDTAIL */

    DEV_INCREF(dev);
    HW_INCREF(dev->hd_Hardware);

    /* Inherit Ids from device */
    CopyMem(dev->hd_Ids.array, unit->hu_Ids.array, sizeof(unit->hu_Ids.array));

    id = unit->hu_Ids.array;
    helios_get_ids(directory, id);

    ADDTAIL(&dev->hd_Units, unit);
    _INFO("Dev $%04x, new unit: RomDir=%p, id=[%x, %x, %x, %x]\n",
          dev->hd_NodeID, unit->hu_RomDirectory, id[0], id[1], id[2], id[3]);

    /* Try to bind a class on this unit */
    ForeachNode(&HeliosBase->hb_Classes, hc)
    {
        /* DOC: indicate that the HeliosBase/Device are w-locked during this method. */
        if (HeliosClass_DoMethod(HCM_AttemptUnitBinding, (ULONG)unit))
        {
            break;
        }
    }

    /* Let the rest of system know this unit */
    Helios_SendEvent(&dev->hd_Listeners, HEVTF_DEVICE_NEW_UNIT, (ULONG)unit);
}

static void helios_process_bm(HeliosHardware *hw,
                              struct timeval *time,
                              ULONG gen,
                              BOOL is_bm,
                              ULONG bm_gen,
                              ULONG *bm_retry)
{
    HeliosTopology topo;
    struct timeval tv;
    struct Library *TimerBase;

    TimerBase = (struct Library *)HeliosBase->hb_TimeReq.tr_node.io_Device;

retry:
    /* Get topology data */
    Helios_GetAttrs(HGA_HARDWARE, hw,
                    HHA_Topology, (ULONG)&topo,
                    TAG_DONE);

    /* Not too late? */
    if ((0 == bm_gen) || (topo.ht_Generation == gen))
    {
        if (NULL != time)
        {
            GetSysTime(&tv);
            SubTime(&tv, time);
        }

        /* Try to (r)establish itself as BM */
        if ((0 == bm_gen) || is_bm || (tv.tv_secs > 0) || (tv.tv_micro > 125000))
        {
            IOHeliosHWSendRequest ioreq;
            HeliosAPacket *p;
            UBYTE maxhops, gap_count;
            UBYTE new_root_phyid;
            QUADLET data[2];
            LONG ioerr;

            _INFO("Checking for BM...\n");

            if ((topo.ht_IRMNodeID != (UBYTE)-1) && !topo.ht_Nodes[topo.ht_IRMNodeID].n_Flags.LinkOn)
            {
                new_root_phyid = topo.ht_LocalNodeID;
                goto set_config;
            }

            ioreq.iohhe_Req.iohh_Req.io_Message.mn_Length = sizeof(IOHeliosHWSendRequest);
            ioreq.iohhe_Req.iohh_Req.io_Command = HHIOCMD_SENDREQUEST;
            ioreq.iohhe_Req.iohh_Data = data;
            ioreq.iohhe_Req.iohh_Length = 8;
            ioreq.iohhe_Device = NULL; /* using p->DestID */

            p = &ioreq.iohhe_Transaction.htr_Packet;
            p->DestID = HELIOS_LOCAL_BUS | topo.ht_IRMNodeID;

            data[0] = LE_SWAPLONG_C(0x3f);
            data[1] = LE_SWAPLONG(topo.ht_LocalNodeID);
            Helios_FillLockPacket(p, S100, CSR_BASE_LO + CSR_BUS_MANAGER_ID, EXTCODE_COMPARE_SWAP, data, 8);

            ioerr = helios_do_simple_ioreq(hw, &ioreq.iohhe_Req);
            _INFO("ioerr = %u\n", ioerr, data[0], data[1]);

            if (ioerr)
            {
                if (p->RCode == HELIOS_RCODE_GENERATION)
                {
                    Helios_DelayMS(125);
                    _INFO("retry...\n");
                    goto retry;
                }

                new_root_phyid = topo.ht_LocalNodeID;
                goto set_config;
            }

            if (data[0] != LE_SWAPLONG_C(0x3f))
            {
                /* Someone else is the BM, act only as IRM */
                if (topo.ht_LocalNodeID == topo.ht_IRMNodeID)
                {
                    /* TODO: set broadcast channel */
                }

                return;
            }

            bm_gen = gen;
            is_bm = TRUE;

            if (!topo.ht_Nodes[topo.ht_RootNodeID].n_Flags.LinkOn)
            {
                new_root_phyid = topo.ht_LocalNodeID;
            }
            else
            {
                new_root_phyid = topo.ht_RootNodeID;
            }

set_config:
            maxhops = topo.ht_Nodes[topo.ht_RootNodeID].n_MaxHops;
            if (maxhops >= sizeof(gHeliosGapCountTable)/sizeof(gHeliosGapCountTable[0]))
            {
                maxhops = 0;
            }

            gap_count = gHeliosGapCountTable[maxhops];
            if ((*bm_retry++ < 5) &&
                ((gap_count != topo.ht_GapCount) ||
                 (new_root_phyid != topo.ht_RootNodeID)))
            {
                IOHeliosHWReq ioreq;

                Helios_ReportMsg(HRMB_INFO, "BusManager",
                                 "Send PHY cfg <GapCount=%u>", gap_count);

                ioreq.iohh_Req.io_Message.mn_Length = sizeof(ioreq);
                ioreq.iohh_Req.io_Command = HHIOCMD_SENDPHY;
                ioreq.iohh_Data = (APTR)(PHY_IDENTIFIER(PHY_PACKET_CONFIG) |
                                         PHY_CONFIG_ROOT_ID(new_root_phyid) |
                                         PHY_CONFIG_GAP_COUNT(gap_count));
                helios_do_simple_ioreq(hw, &ioreq);
                Helios_BusReset(hw, TRUE);
            }
            else
            {
                bm_retry = 0;

                if (topo.ht_LocalNodeID == topo.ht_IRMNodeID)
                {
                    /* TODO: set broadcast channel */
                }
            }
        }
        else
        {
            Helios_DelayMS(125);
            _INFO("retry...\n");
            goto retry;
        }
    }
    else
    {
        _WARN("Too late\n");
    }
}

static void helios_hardware_task(HeliosSubTask *self, struct TagItem *tags)
{
    struct MsgPort *taskport, *hwport, *evtport;
    HeliosHardware *hw, **hw_ptr;
    STRPTR name;
    ULONG all_sigs, bm_gen=0, bm_retry=0;
    LONG unit;
    IOHeliosHWReq *iobase = NULL;
    HeliosEventMsg myListener;
    BOOL run, is_bm=FALSE;

    taskport = (APTR) GetTagData(HA_MsgPort, 0, tags);
    hw_ptr = (APTR) GetTagData(HA_UserData, 0, tags);
    name = (APTR) GetTagData(HA_Device, 0, tags);
    unit = GetTagData(HA_Unit, -1, tags);

    if ((NULL == taskport) || (NULL == hw_ptr) || (NULL == name) || (unit < 0))
    {
        _ERR("<%s,%ld>: bad call\n", name, unit);
        return;
    }

    *hw_ptr = NULL;

    hwport = CreateMsgPort();
    evtport = CreateMsgPort();
    if ((NULL == hwport) || (NULL == evtport))
    {
        _ERR("<%s,%ld>: ports creation failed\n", name, unit);
        goto error;
    }

    iobase = (APTR)CreateExtIO(hwport, sizeof(IOHeliosHWReq));
    if (NULL == iobase)
    {
        _ERR("<%s,%ld>: IOReq creation failed\n", name, unit);
        goto error;
    }

    if (OpenDevice(name, unit, (struct IORequest *)iobase, 0))
    {
        _ERR("<%s,%ld>: OpenDevice failed\n", name, unit);
        goto error;
    }

    hw = (APTR)iobase->iohh_Req.io_Unit;
    hw->hu_HWTask = self;
    HW_INCREF(hw);

    LOCK_REGION(HeliosBase);
    ADDTAIL(&HeliosBase->hb_Hardwares, &hw->hu_HWNode);
    UNLOCK_REGION(HeliosBase);

    *hw_ptr = hw;

    _INFO("New hardware %s,%ld added (%p)\n", name, unit, hw);
    Helios_TaskReady(self, TRUE);

    bzero(&myListener, sizeof(myListener));
    myListener.hm_Msg.mn_ReplyPort = evtport;
    myListener.hm_Msg.mn_Length = sizeof(myListener);
    myListener.hm_Type = HELIOS_MSGTYPE_EVENT;
    myListener.hm_EventMask = HEVTF_HARDWARE_TOPOLOGY;
    Helios_AddEventListener(&hw->hu_Listeners, &myListener);

    all_sigs  = 1ul << taskport->mp_SigBit;
    all_sigs |= 1ul << evtport->mp_SigBit;
    all_sigs |= SIGBREAKF_CTRL_C;

    helios_process_bm(hw, NULL, 0, FALSE, 0, &bm_retry);

    run = TRUE;
    do
    {
        HeliosMsg *msg;
        ULONG sigs;

        sigs = Wait(all_sigs);
        if (sigs & SIGBREAKF_CTRL_C)
        {
            run = FALSE;
        }

        if (sigs & (1ul << taskport->mp_SigBit))
        {
            while (NULL != (msg = (APTR)GetMsg(taskport)))
            {
                if (HELIOS_MSGTYPE_TASKKILL == msg->hm_Type)
                {
                    run = FALSE;
                }

                ReplyMsg((struct Message *)msg);
            }
        }

        if (sigs & (1ul << evtport->mp_SigBit))
        {
            HeliosEventMsg *next=NULL, *evt=NULL;

            /* Kept only the last topology event */
            do
            {
                /* Only one type of event possible on this port.
                 * So we don't check more.
                 */
                next = (APTR)GetMsg(evtport);
                if (NULL != next)
                {
                    if (NULL != evt)
                    {
                        FreeMem(evt, evt->hm_Msg.mn_Length);
                    }

                    evt = next;
                }
            }
            while (NULL != next);

            if (NULL != evt)
            {
                if ((bm_gen+1) != evt->hm_Result)
                {
                    is_bm = FALSE;
                }

                helios_process_bm(hw, &evt->hm_Time, evt->hm_Result, is_bm, bm_gen, &bm_retry);

                FreeMem(evt, evt->hm_Msg.mn_Length);
            }
        }
    }
    while (run);

    Helios_RemoveEventListener(&hw->hu_Listeners, &myListener);
    {
        struct Message *msg;

        while (NULL != (msg = GetMsg(evtport)))
        {
            FreeMem(msg, msg->mn_Length);
        }
    }

    LOCK_REGION(HeliosBase);
    REMOVE(&hw->hu_HWNode);
    UNLOCK_REGION(HeliosBase);

    HW_DECREF(hw);

    /* TODO: RefCnt protection here */

    CloseDevice((struct IORequest *)iobase);

error:
    if (NULL != iobase)
    {
        DeleteExtIO((struct IORequest *)iobase);
    }
    if (NULL != evtport)
    {
        DeleteMsgPort(evtport);
    }
    if (NULL != hwport)
    {
        DeleteMsgPort(hwport);
    }
}

static void helios_read_dev_name(HeliosDevice *dev, const QUADLET *dir)
{
    UBYTE buf[61];
    LONG len;

    len = Helios_ReadTextualDescriptor(dir, (STRPTR)buf, sizeof(buf)-1);
    if (len >= 0)
    {
        if (len)
        {
            buf[len] = '\0';
            Helios_ReportMsg(HRMB_INFO, "Device", "Node #%u: ROM says '%s'", dev->hd_NodeID & 0x3f, buf);
        }
    }
    else
    {
        _ERR("Helios_ReadTextualDescriptor() failed\n");
    }
}

static void helios_scanner_task(HeliosDevice *dev, STRPTR task_name, ULONG topogen)
{
    QUADLET *rom;
    UQUAD guid=0;
    ULONG len=CSR_CONFIG_ROM_SIZE;
    static const STRPTR default_task_name = "<helios rom scan (killed)>";

    rom = AllocPooled(HeliosBase->hb_MemPool, CSR_CONFIG_ROM_SIZE);
    if (NULL == rom)
    {
        _ERR("Rom alloc failed\n");
        goto bye;
    }

    /* IEEE1394 norm indicates a delay of 1s before scanning device's ROM */
    Helios_DelayMS(1000);

    LOCK_REGION_SHARED(dev);
    {
        if (dev->hd_Generation != topogen)
        {
            topogen = 0;
        }
    }
    UNLOCK_REGION_SHARED(dev);

    if (!topogen)
    {
        goto bye;
    }

    _INFO("Scanning dev %p @ gen #%lu\n", dev, topogen);

    if (!Helios_ReadROM(dev, rom, &len) && (len >= 5))
    {
        QUADLET *tmp;

        guid = ((UQUAD)rom[3] << 32) + rom[4];

        tmp = AllocPooled(HeliosBase->hb_MemPool, len);
        if (NULL != tmp)
        {
            CopyMem(rom, tmp, len);
            rom = tmp;
        }
        else
        {
            len = CSR_CONFIG_ROM_SIZE;
        }
    }
    else
    {
        len = CSR_CONFIG_ROM_SIZE;
    }

    LOCK_REGION(HeliosBase);
    {
        LOCK_REGION(dev);
        {
            /* Not changed of topology generation (or removed) ? */
            if (dev->hd_Generation == topogen)
            {
                Helios_ReportMsg(HRMB_DBG, "DEV", "[$%04x] (dev=%p) Gen #%lu, GUID=$%llX",
                                 dev->hd_NodeID, dev, dev->hd_Generation, guid);

                dev->hd_GUID.q = guid;
                if (0 != guid)
                {
                    ULONG i;
                    BOOL changed;

                    /* Check if the ROM has really changed.
                     * If not, don't change anything except the generation.
                     */

                    if (NULL != dev->hd_Rom)
                    {
                        _INFO("[$%04x] Old ROM=%p\n", dev->hd_Rom);
                        changed = FALSE;
                        for (i=0; i < MIN(MIN(len, 5ul), dev->hd_RomLength); i++)
                        {
                            BOOL diff = (rom[i] != dev->hd_Rom[i]);

                            changed |= diff;
                            if (diff)
                                _WARN("[$%04x] ROM[%u] differs: get $%08x, was $%08x\n",
                                      dev->hd_NodeID, i, rom[i], dev->hd_Rom[i]);
                        }
                    }
                    else
                    {
                        changed = TRUE;
                    }

                    if (changed)
                    {
                        HeliosUnit *unit, *next;
                        HeliosRomIterator ri;
                        QUADLET key, value;
                        LONG unitno = 0;

                        /* Remove previous units */
                        _WARN("[$%04x] ROM has changed, removing old units...\n", dev->hd_NodeID);

                        ForeachNodeSafe(&dev->hd_Units, unit, next)
                        {
                            helios_remove_unit(unit);
                        }

                        /* Free previous ROM
                         * DOC: ROM pointer shall be used in a dev locked region!
                         */
                        if (NULL != dev->hd_Rom)
                        {
                            FreePooled(HeliosBase->hb_MemPool, dev->hd_Rom, dev->hd_RomLength);
                        }

                        dev->hd_Rom = rom;
                        rom = NULL;

                        dev->hd_RomLength = len;
                        dev->hd_RomScanner = NULL;

                        helios_get_ids((APTR)dev->hd_Rom + ROM_ROOT_DIRECTORY, dev->hd_Ids.array);

                        /* Parse ROM for units */
                        Helios_InitRomIterator(&ri, (APTR)dev->hd_Rom + ROM_ROOT_DIRECTORY);
                        while (Helios_RomIterate(&ri, &key, &value))
                        {
                            switch (key)
                            {
                                case KEYTYPEV_LEAF | CSR_KEY_TEXTUAL_DESCRIPTOR:
                                    helios_read_dev_name(dev, &ri.actual[value - 1]);
                                    break;

                                case KEYTYPEV_DIRECTORY | CSR_KEY_UNIT_DIRECTORY:
                                    helios_create_unit(dev, &ri.actual[value-1], unitno++);
                                    break;
                            }
                        }
                    }
                    else
                    {
                        _INFO("[$%04x] ROM not changed\n", dev->hd_NodeID);
                    }

                    Helios_SendEvent(&dev->hd_Listeners, HEVTF_DEVICE_SCANNED, (ULONG)dev);
                }
                else
                {
                    _WARN("[$%04x] Zero GUID: set as dead...\n", dev->hd_NodeID);
                    helios_dev_set_dead(dev);
                }
            }
            else
            {
                _WARN("Abording ROM scan dev %p (topogen mismatch: %u)\n", dev, topogen);
            }
        }
        UNLOCK_REGION(dev);
    }
    UNLOCK_REGION(HeliosBase);

bye:
    if (NULL != rom)
    {
        FreePooled(HeliosBase->hb_MemPool, rom, len);
    }

    NewSetTaskAttrsA(NULL, (APTR)&default_task_name, sizeof(STRPTR), TASKINFOTYPE_NAME, NULL);
    FreePooled(HeliosBase->hb_MemPool, task_name, 64);

    Helios_ReleaseDevice(dev);
}

/* HeliosBase must be r-locked */
static HeliosUnit *helios_dev_next_unit(HeliosDevice *dev, HeliosUnit *unit)
{
    if (NULL == unit)
    {
        unit = (APTR)GetHead(&dev->hd_Units);
    }
    else
    {
        unit = (APTR)GetSucc(&unit->hso_SysNode);
    }

    if (NULL != unit)
    {
        UNIT_INCREF(unit);
    }

    return unit;
}

/* HeliosBase must be r-locked */
static HeliosDevice *helios_hw_next_device(HeliosHardware *hw, HeliosDevice *dev)
{
    if (NULL == dev)
    {
        dev = (APTR)GetHead(&hw->hu_Devices);
    }
    else
    {
        dev = (APTR)GetSucc(&dev->hso_SysNode);
    }

    if (NULL != dev)
    {
        DEV_INCREF(dev);
    }

    return dev;
}

/* HeliosBase must be r-locked */
static HeliosUnit *helios_hw_next_unit(HeliosHardware *hw, HeliosUnit *unit)
{
    HeliosDevice *dev;
    struct TagItem tags[] = {{HA_Hardware, (ULONG)hw}, {TAG_DONE, 0}};

    if (NULL != unit)
    {
        dev = unit->hu_Device;
        DEV_INCREF(dev);
    }
    else
    {
        dev = NULL;
    }

    /* Loop on all devices of fixed hardware */
    do
    {
        /* no more unit on current device? get next device */
        if (NULL == unit)
        {
            HeliosDevice *next_dev;

            next_dev = Helios_GetNextDeviceA(dev, tags); /* NR */
            if (NULL != dev)
            {
                Helios_ReleaseDevice(dev);
            }
            dev = next_dev;

            /* No more device on fixed hardware? */
            if (NULL == dev)
            {
                return NULL;
            }
        }

        unit = helios_dev_next_unit(dev, unit); /* NR */
    }
    while (NULL == unit);

    /* ref not used */
    Helios_ReleaseDevice(dev);

    return unit;
}


/*============================================================================*/
/*--- LIBRARY CODE SECTION ---------------------------------------------------*/

LONG Helios_GetAttrsA(ULONG type, APTR obj, struct TagItem *tags)
{
    LONG count=0;
    ULONG *packtab = NULL;
    struct TagItem *ti, *tmp_tags;

    if (type < HELIOSPTASIZE)
    {
        packtab = (ULONG *) gHeliosPTA[type];
    }

    switch (type)
    {
        case HGA_BASE:
        {
            struct HeliosBase *base = obj?obj:HeliosBase;

            LOCK_REGION_SHARED(base);
            {
                ti = FindTagItem(HA_EventListenerList, tags);
                if (NULL != ti)
                {
                    *(HeliosEventListenerList **)ti->ti_Data = &base->hb_Listeners;
                    count++;
                }
                count += (LONG) UnpackStructureTags(obj, packtab, tags);
            }
            UNLOCK_REGION_SHARED(base);
        }
        break;

        case HGA_HARDWARE:
        {
            HeliosHardware *hw = obj;

            LOCK_REGION_SHARED(hw);
            {
                count += (LONG) UnpackStructureTags(obj, packtab, tags);

                tmp_tags = tags;
                while (NULL != (ti = NextTagItem(&tmp_tags)))
                {
                    switch(ti->ti_Tag)
                    {
                        case HA_EventListenerList:
                            *(HeliosEventListenerList **)ti->ti_Data = &hw->hu_Listeners;
                            break;

                        case HA_NodeID:
                            *(UWORD *)ti->ti_Data = hw->hu_LocalNodeId;
                            break;

                        case HHA_Topology:
                        {
                            struct TagItem tags[] =
                            {
                                {HHA_Topology, (ULONG)ti->ti_Data},
                                {TAG_DONE, 0}
                            };
                            IOHeliosHWReq ioreq;

                            ioreq.iohh_Req.io_Message.mn_Length = sizeof(ioreq);
                            ioreq.iohh_Req.io_Command = HHIOCMD_QUERYDEVICE;
                            ioreq.iohh_Data = tags;
                            helios_do_simple_ioreq(hw, &ioreq);
                        }
                        break;

                        default: continue;
                    }

                    count++;
                }
            }
            UNLOCK_REGION_SHARED(hw);
        }
        break;

        case HGA_DEVICE:
        {
            HeliosDevice *dev = obj;

            LOCK_REGION_SHARED(dev);
            {
                count += (LONG) UnpackStructureTags(obj, packtab, tags);

                tmp_tags = tags;
                while (NULL != (ti = NextTagItem(&tmp_tags)))
                {
                    switch(ti->ti_Tag)
                    {
                        case HA_NodeInfo:
                            CopyMemQuick(&dev->hd_NodeInfo, (APTR)ti->ti_Data, sizeof(HeliosNode));
                            break;

                        case HA_EventListenerList:
                            *(HeliosEventListenerList **)ti->ti_Data = &dev->hd_Listeners;
                            break;

                        case HA_GUID:
                            *(UQUAD *)ti->ti_Data = dev->hd_GUID.q;
                            break;

                        case HA_Hardware:
                            *(HeliosHardware **)ti->ti_Data = dev->hd_Hardware;
                            HW_INCREF(dev->hd_Hardware);
                            break;

                        default: continue;
                    }

                    count++;
                }
            }
            UNLOCK_REGION_SHARED(dev);
        }
        break;

        case HGA_UNIT:
        {
            HeliosUnit *unit = obj;

            LOCK_REGION_SHARED(unit);
            {
                count += (LONG) UnpackStructureTags(obj, packtab, tags);

                tmp_tags = tags;
                while (NULL != (ti = NextTagItem(&tmp_tags)))
                {
                    switch(ti->ti_Tag)
                    {
                        case HA_Hardware:
                            *(HeliosHardware **)ti->ti_Data = unit->hu_Hardware;
                            HW_INCREF(unit->hu_Hardware);
                            break;

                        case HA_Device:
                            *(HeliosDevice **)ti->ti_Data = unit->hu_Device;
                            DEV_INCREF(unit->hu_Device);
                            break;

                        default: continue;
                    }

                    count++;
                }
            }
            UNLOCK_REGION_SHARED(unit);
        }
        break;

        case HGA_CLASS:
        {
            HeliosClass *hc = obj;

            count += HeliosClass_DoMethodA(HCM_GetAttrs, (ULONG *)tags);
        }
        break;
    }

    return count;
}

LONG Helios_SetAttrsA(ULONG type, APTR obj, struct TagItem *tags)
{
    LONG count=0;
    ULONG *packtab = NULL;

    if (type < HELIOSPTASIZE)
    {
        packtab = (ULONG *) gHeliosPTA[type];
    }

    switch (type)
    {
        case HGA_DEVICE:
        {
            HeliosDevice *dev = obj;

            LOCK_REGION(dev);
            count = (LONG) PackStructureTags(obj, packtab, tags);
            UNLOCK_REGION(dev);
        }
        break;

        case HGA_CLASS:
        {
            HeliosClass *hc = obj;

            count = HeliosClass_DoMethodA(HCM_SetAttrs, (ULONG *)tags);
        }
        break;
    }

    return count;
}

LONG Helios_DoIO(ULONG type, APTR obj, IOHeliosHWReq *ioreq)
{
    HeliosHardware *hw=NULL;

    switch (type)
    {
        case HGA_HARDWARE: hw = obj; break;
        case HGA_DEVICE: hw = ((HeliosDevice *)obj)->hd_Hardware; break;
    }

    return helios_do_simple_ioreq(hw, ioreq);
}

void Helios_InitIO(ULONG type, APTR obj, IOHeliosHWReq *ioreq)
{
    HeliosHardware *hw;

    switch (type)
    {
        case HGA_HARDWARE: hw = obj; break;
        case HGA_DEVICE: hw = ((HeliosDevice *)obj)->hd_Hardware; break;
        default: return;
    }

    ioreq->iohh_Req.io_Device = hw->hu_Device;
    ioreq->iohh_Req.io_Unit = &hw->hu_Unit;
}


/*----------------------------------------------------------------------------*/
/*--- Hardwares --------------------------------------------------------------*/

/* WARNING: Caller shall lock HeliosBase before calling this function */
HeliosHardware *Helios_GetNextHardware(HeliosHardware *hw)
{
    APTR addr;

    if (NULL == hw)
    {
        addr = GetHead(&HeliosBase->hb_Hardwares);
    }
    else
    {
        addr = GetSucc(&hw->hu_HWNode);
    }

    if (NULL == addr)
    {
        return NULL;
    }

    hw = (HeliosHardware *)(addr - offsetof(HeliosHardware, hu_HWNode));
    if (NULL != hw)
    {
        HW_INCREF(hw);
    }

    _INFO("hw=%p\n", hw);
    return hw;
}

void Helios_ReleaseHardware(HeliosHardware *hw)
{
    HW_DECREF(hw);
}

LONG Helios_DisableHardware(HeliosHardware *hw, BOOL state)
{
    IOHeliosHWReq ioreq;

    ioreq.iohh_Req.io_Message.mn_Length = sizeof(ioreq);
    ioreq.iohh_Req.io_Command = state?HHIOCMD_DISABLE:HHIOCMD_ENABLE;
    return helios_do_simple_ioreq(hw, &ioreq);
}

LONG Helios_BusReset(HeliosHardware *hw, BOOL short_br)
{
    IOHeliosHWReq ioreq;

    ioreq.iohh_Req.io_Message.mn_Length = sizeof(ioreq);
    ioreq.iohh_Req.io_Command = HHIOCMD_BUSRESET;
    ioreq.iohh_Data = (APTR)(short_br&1);
    return helios_do_simple_ioreq(hw, &ioreq);
}

HeliosHardware *Helios_AddHardware(STRPTR name, LONG unit)
{
    HeliosHardware *hw = NULL;
    char buf[64];
    struct TagItem tags[] =
    {
        {HA_Pool, (ULONG)HeliosBase->hb_MemPool},
        {HA_UserData, (ULONG)&hw},
        {HA_Device, (ULONG)name},
        {HA_Unit, (ULONG)unit},
        {TASKTAG_PRI, 5},
    };
    HeliosSubTask *task;

    utils_SafeSPrintF(buf, sizeof(buf), "helioshw <%s/%ld>", FilePart(name), unit);

    task = Helios_CreateSubTaskA(buf, helios_hardware_task, tags);
    if (NULL != task)
    {
        /* Wait that task fill hw variable */
        if (0 != Helios_WaitTaskReady(task, SIGBREAKF_CTRL_C))
        {
            _ERR("Task '%s' not ready\n", buf);
            Helios_KillSubTask(task);
            hw = NULL;
        }
    }
    else
    {
        _ERR("Failed to create hw task '%s'\n", buf);
    }

    return hw;
}

LONG Helios_RemoveHardware(HeliosHardware *hw)
{
    LONG refcnt;

    refcnt = ATOMIC_FETCH(&hw->hso_RefCnt);

    /* 1 for the HW task */
    if (refcnt > 1)
    {
        HeliosDevice *dev;
        APTR next;

        Helios_DisableHardware(hw, TRUE);

        ForeachNodeSafe(&hw->hu_Devices, dev, next)
        {
            Helios_RemoveDevice(dev);
        }

        if (ATOMIC_FETCH(&hw->hso_RefCnt) > 1)
        {
            return FALSE;
        }
    }

    Helios_KillSubTask(hw->hu_HWTask);
    return TRUE;
}


/*----------------------------------------------------------------------------*/
/*--- Devices ----------------------------------------------------------------*/

/* WARNING: Caller shall w-lock HeliosBase before calling this function */
HeliosDevice *Helios_GetNextDeviceA(HeliosDevice *dev, struct TagItem *tags)
{
    HeliosHardware *search_in_hw=NULL;
    struct TagItem *tag;

    while (NULL != (tag = NextTagItem(&tags)))
    {
        switch (tag->ti_Tag)
        {
            case HA_Hardware: search_in_hw = (APTR)tag->ti_Data; break;
        }
    }

    /* Sanity checks of given device */
    if (NULL != dev)
    {
        if ((NULL != search_in_hw) && (dev->hd_Hardware != search_in_hw))
        {
            _ERR("Inconsitent hardware given: given %p, dev_hw=%p\n",
                 search_in_hw, dev->hd_Hardware);
            return NULL;
        }
    }

    if (NULL != search_in_hw)
    {
        dev = helios_hw_next_device(search_in_hw, dev);
    }
    else
    {
        HeliosHardware *hw;

        if (NULL != dev)
        {
            hw = dev->hd_Hardware;
            HW_INCREF(hw);
        }
        else
        {
            hw = NULL;
        }

        /* Loop on all hardwares */
        do
        {
            if (NULL == hw)
            {
                hw = Helios_GetNextHardware(hw); /* NR */

                /* No more hardware? */
                if (NULL == hw)
                {
                    return NULL;
                }
            }

            /* Operate like if hardware was fixed now */
            dev = helios_hw_next_device(hw, dev);

            /* ref not used */
            Helios_ReleaseHardware(hw);
        }
        while (NULL == dev);
    }

    _INFO("dev=$%p\n", dev);
    return dev;
}

/* Note: this function is not safe!
** If the caller get a dev pointer and if this device has not been incref,
** it may be invalid if refcnt reaches 0 before the base lock.
** MUST DO: check that all functions giving dev pointer incref it before!
*/
LONG Helios_ObtainDevice(HeliosDevice *dev)
{
    return DEV_INCREF(dev) > 0;
}

void Helios_ReleaseDevice(HeliosDevice *dev)
{
    LONG old_refcnt;

    old_refcnt = DEV_DECREF(dev);
    if (old_refcnt <= 1)
    {
        /* Bad design? */
        if (old_refcnt <= 0)
        {
            _WARN("Too many releases of device %p called (old refcnt: %d)\n", dev, old_refcnt);
        }

        helios_free_dev(dev);
    }
}

void Helios_RemoveDevice(HeliosDevice *dev)
{
    HeliosHardware *hw;

    LOCK_REGION(HeliosBase);
    {
        hw = dev->hd_Hardware;

        /* Mark as dead, remove all connected units and inform listeners */
        LOCK_REGION(dev);
        helios_dev_set_dead(dev);
        UNLOCK_REGION(dev);

        REMOVE(dev);

        Helios_SendEvent(&dev->hd_Listeners, HEVTF_DEVICE_REMOVED, (ULONG)dev);
    }
    UNLOCK_REGION(HeliosBase);

    Helios_ReleaseDevice(dev);
    Helios_ReleaseHardware(hw);
}

/* HW must be w-locked */
HeliosDevice *Helios_AddDevice(HeliosHardware *hw,
                               HeliosNode *node,
                               ULONG topogen)
{
    HeliosDevice *dev;

    /* As devices may be free after hw removal, they are allocated on
     * the helios mem pool and not on the hw one.
     */
    dev = AllocPooled(HeliosBase->hb_MemPool, sizeof(*dev));
    if (NULL != dev)
    {
        LOCK_INIT(dev);
        NEWLIST(&dev->hd_Units);
        NEWLIST(&dev->hd_Listeners);
        LOCK_INIT(&dev->hd_Listeners);

        dev->hd_Hardware = hw;
        dev->hd_Generation = topogen;
        dev->hd_NodeID = HELIOS_LOCAL_BUS | node->n_PhyID;
        dev->hso_RefCnt = 1; /* for the ADDTAIL */

        HW_INCREF(hw);

        /* Copy node info in device */
        CopyMemQuick(node, &dev->hd_NodeInfo, sizeof(dev->hd_NodeInfo));

        ADDTAIL(&hw->hu_Devices, dev);
        _INFO("New dev %p (#%u), tgen=%lu\n",
              dev, dev->hd_NodeInfo.n_PhyID,
              dev->hd_Generation);
    }
    else
    {
        _ERR("Device allocation failed\n");
    }

    return dev;
}

void Helios_UpdateDevice(HeliosDevice *dev, HeliosNode *node, ULONG topogen)
{
    LOCK_REGION(HeliosBase); /* because we may remove units */
    {
        LOCK_REGION(dev);
        {
            _INFO("Update dev %p (%u->%u) @ tgen=%lu, Rom-up-flags: %s%s\n",
                  dev, dev->hd_NodeInfo.n_PhyID, node->n_PhyID,
                  dev->hd_Generation,
                  node->n_Flags.ResetInitiator?"I":"",
                  dev->hd_GUID.q == 0?"Q":"");

            dev->hd_Generation = topogen;
            dev->hd_RomScanner = NULL;

            /* need to update the rom ? */
            if (node->n_Flags.ResetInitiator)
            {
                dev->hd_GUID.q = 0;
            }

            CopyMemQuick(node, &dev->hd_NodeInfo, sizeof(dev->hd_NodeInfo));
            dev->hd_NodeID = HELIOS_LOCAL_BUS | node->n_PhyID;

            Helios_SendEvent(&dev->hd_Listeners, HEVTF_DEVICE_UPDATED, (ULONG)dev);
        }
        UNLOCK_REGION(dev);
    }
    UNLOCK_REGION(HeliosBase);
}

/* WARNING: dev shall be incref'ed before calling */
void Helios_ScanDevice(HeliosDevice *dev)
{
    ULONG gen;
    UWORD nid;

    LOCK_REGION_SHARED(dev);
    {
        gen = dev->hd_Generation;
        nid = dev->hd_NodeID;
    }
    UNLOCK_REGION_SHARED(dev);

    if (gen > 0)
    {
        STRPTR name = AllocPooled(HeliosBase->hb_MemPool, 64);

        if (NULL != name)
        {
            utils_SafeSPrintF(name, 64, "heliosdevice-scanner <%u,$%x>", gen, nid);

            DEV_INCREF(dev);
            dev->hd_RomScanner = NewCreateTask(TASKTAG_CODETYPE,    CODETYPE_PPC,
                                               TASKTAG_NAME,        (ULONG)name,
                                               TASKTAG_PRI,         0,
                                               TASKTAG_STACKSIZE,   32768,
                                               TASKTAG_PC,          (ULONG)helios_scanner_task,
                                               TASKTAG_PPC_ARG1,    (ULONG)dev,
                                               TASKTAG_PPC_ARG2,    (ULONG)name,
                                               TASKTAG_PPC_ARG3,    gen,
                                               TAG_DONE);
            if (NULL == dev->hd_RomScanner)
            {
                _ERR("Scanner creation failed for dev $%x-%p\n", nid, dev);

                /* Not possible to obtain ROM, mark it as dead */
                LOCK_REGION(HeliosBase);
                {
                    LOCK_REGION(dev);
                    helios_dev_set_dead(dev);
                    UNLOCK_REGION(dev);
                }
                UNLOCK_REGION(HeliosBase);


                Helios_ReleaseDevice(dev);
                FreePooled(HeliosBase->hb_MemPool, name, 64);
            }
        }
        else
        {
            _ERR("Scanner name alloc failed\n");
        }
    }
    else
    {
        _WARN("already dead device, not scanned\n");
    }
}

void Helios_ReadLockDevice(HeliosDevice *dev)
{
    LOCK_REGION_SHARED(dev);
}

void Helios_WriteLockDevice(HeliosDevice *dev)
{
    LOCK_REGION(dev);
}

void Helios_UnlockDevice(HeliosDevice *dev)
{
    UNLOCK_REGION(dev);
}


/*----------------------------------------------------------------------------*/
/*--- Unit -------------------------------------------------------------------*/


/* Requirements:
 * a) unit != NULL
 * b) unit not removed.
 * c) unit's device not dead or removed.
 *
 * DOC: caller shall be sure that unit is valid until the return.
 */
LONG Helios_ObtainUnit(HeliosUnit *unit)
{
    LONG res = FALSE;
    HeliosDevice *dev;

    if (UNIT_INCREF(unit) > 0)
    {
        dev = unit->hu_Device;

        /* Unit can't be obtained if removed or dead device */
        if (NULL != dev)
        {
            LOCK_REGION_SHARED(dev);
            {
                if (dev->hd_Generation > 0)
                {
                    res = TRUE;
                }
            }
            UNLOCK_REGION_SHARED(dev);
        }

        if (!res)
        {
            Helios_ReleaseUnit(unit);
        }
    }

    return res;
}

/* Requirements:
 * a) caller shall own a ref on the unit.
 */
void Helios_ReleaseUnit(HeliosUnit *unit)
{
    LONG old_refcnt;

    old_refcnt = UNIT_DECREF(unit);
    if (old_refcnt <= 1)
    {
        /* Bad design? */
        if (old_refcnt <= 0)
        {
            _WARN("Too many releases of unit %p called (old refcnt: %d)\n", unit, old_refcnt);
        }

        helios_free_unit(unit);
    }
}

/* WARNING:Caller shall r-lock HeliosBase before calling this function */
HeliosUnit *Helios_GetNextUnitA(HeliosUnit *unit, struct TagItem *tags)
{
    HeliosHardware *search_in_hw=NULL;
    HeliosDevice *search_in_dev=NULL;
    struct TagItem *tag;

    while (NULL != (tag = NextTagItem(&tags)))
    {
        switch (tag->ti_Tag)
        {
            case HA_Hardware: search_in_hw = (APTR)tag->ti_Data; break;
            case HA_Device: search_in_dev = (APTR)tag->ti_Data; break;
        }
    }

    /* Sanity checks of given unit */
    if (NULL != unit)
    {
        if ((NULL == unit->hu_Device) || (0 == unit->hu_Device->hd_Generation))
        {
            _ERR("Removed unit given!\n");
            return NULL;
        }

        if ((NULL != search_in_hw) && (unit->hu_Hardware != search_in_hw))
        {
            _ERR("Inconsitent hardware given: given %p, unit_hw=%p\n",
                 search_in_hw, unit->hu_Hardware);
            return NULL;
        }

        if ((NULL != search_in_dev) && (unit->hu_Device != search_in_dev))
        {
            _ERR("Inconsitent device given: given %p, unit_dev=%p\n",
                 search_in_dev, unit->hu_Device);
            return NULL;
        }
    }

    if (NULL != search_in_hw)
    {
        unit = helios_hw_next_unit(search_in_hw, unit);    /* NR */
    }
    else if (NULL != search_in_dev)
    {
        unit = helios_dev_next_unit(search_in_dev, unit);    /* NR */
    }
    else
    {
        HeliosHardware *hw;

        if (NULL != unit)
        {
            hw = unit->hu_Hardware;
            HW_INCREF(hw);
        }
        else
        {
            hw = NULL;
        }

        /* Loop on all hardwares */
        do
        {
            if (NULL == unit)
            {
                HeliosHardware *next_hw;

                /* search for an hardware with devices */
                do
                {
                    next_hw = Helios_GetNextHardware(hw); /* NR */
                    if (NULL != hw)
                    {
                        Helios_ReleaseHardware(hw);
                    }
                    hw = next_hw;

                    /* No more hardware? */
                    if (NULL == hw)
                    {
                        return NULL;
                    }
                }
                while (IsListEmpty((struct List *)&hw->hu_Devices));
            }

            /* Operate like if hardware was fixed now */
            unit = helios_hw_next_unit(hw, unit); /* NR */
        }
        while (NULL == unit);

        /* Not used */
        Helios_ReleaseHardware(hw);
    }

    _INFO("unit=$%p\n", unit);
    return unit;
}

LONG Helios_BindUnit(HeliosUnit *unit, HeliosClass *hc, APTR udata)
{
    HeliosDevice *dev = unit->hu_Device;
    LONG res = FALSE;

    LOCK_REGION_SHARED(dev);
    {
        /* Make sure to not bind on removed device */
        if (dev->hd_Generation > 0)
        {
            LOCK_REGION(unit);
            {
                /* Only one driver per unit */
                if (NULL == unit->hu_BindClass)
                {
                    res = Helios_ObtainClass(hc);
                    if (res)
                    {
                        unit->hu_BindClass = hc;
                        unit->hu_BindUData = udata;

                        _INFO("Unit #%lu (@%p, dev@%p $%04x) is bind on class @%p '%s'\n",
                              unit->hu_UnitNo, unit, dev, dev->hd_NodeID, hc, hc->hso_SysNode.ln_Name);
                    }
                }
                else
                    _WARN("Unit #%lu ($%p) is already bound on class '%s'\n",
                          unit->hu_UnitNo, unit, unit->hu_BindClass->hso_SysNode.ln_Name);
            }
            UNLOCK_REGION(unit);
        }
        else
        {
            _ERR("bind failed: dev %p is removed\n", unit->hu_Device);
        }
    }
    UNLOCK_REGION_SHARED(dev);

    return res;
}

LONG Helios_UnbindUnit(HeliosUnit *unit)
{
    HeliosDevice *dev = unit->hu_Device;
    LONG res;

    LOCK_REGION(unit);
    {
        if (NULL != unit->hu_BindClass)
        {
            _INFO("Unbind unit #%lu class@%p of dev $%llx\n",
                  unit->hu_UnitNo, unit->hu_BindClass, dev->hd_GUID);

            Helios_ReleaseClass(unit->hu_BindClass);

            unit->hu_BindClass = NULL;
            unit->hu_BindUData = NULL;
            res = TRUE;
        }
        else
        {
            res = FALSE;
        }
    }
    UNLOCK_REGION(unit);

    return res;
}


/*----------------------------------------------------------------------------*/
/*--- Debug ------------------------------------------------------------------*/

void Helios_DBG_DumpDevices(HeliosHardware *hw)
{
    HeliosDevice *dev = NULL;
    static const char c = ' ';
    ULONG count;
    struct TagItem tags[] = {{HA_Hardware, (ULONG)hw}, {TAG_DONE, 0}};

    kprintf("HeliosBase:          $%p\n", (ULONG)HeliosBase);
    kprintf("Using base hardware: $%p\n\n", (ULONG)hw);


    LOCK_REGION_SHARED(HeliosBase);
    {
        while (NULL != (dev = Helios_GetNextDeviceA(dev, tags)))
        {
            LOCK_REGION_SHARED(dev);
            {
                kprintf("Device @ $%p:\n", (ULONG)dev);
                kprintf("\tRefCnt:     %lu\n", dev->hso_RefCnt);
                kprintf("\t\tHardware:   $%p\n", (ULONG)dev->hd_Hardware);
                kprintf("\t\tGeneration: %lu\n", dev->hd_Generation);
                kprintf("\t\tNodeID:     $%08x\n", dev->hd_NodeID);
                kprintf("\t\tNodeInfo:\n");
                kprintf("\t\t\tPhyID:      %u\n", dev->hd_NodeInfo.n_PhyID);
                kprintf("\t\t\tFlags:      $%02x %s%s\n", *(UBYTE*)&dev->hd_NodeInfo.n_Flags,
                        dev->hd_NodeInfo.n_Flags.ResetInitiator?'I':c,
                        dev->hd_NodeInfo.n_Flags.LinkOn?'L':c);
                kprintf("\t\t\tPortCount:  %u\n", dev->hd_NodeInfo.n_PortCount);
                kprintf("\t\t\tPhySpeed:   %u\n", dev->hd_NodeInfo.n_PhySpeed);
                kprintf("\t\t\tMaxSpeed:   %u\n", dev->hd_NodeInfo.n_MaxSpeed);
                kprintf("\t\t\tMaxHops:    %u\n", dev->hd_NodeInfo.n_MaxHops);
                kprintf("\t\t\tMaxDepth:   %u\n", dev->hd_NodeInfo.n_MaxDepth);
                kprintf("\t\t\tParentPort: %d\n", dev->hd_NodeInfo.n_ParentPort);
                kprintf("\t\t\tDevice:     $%p\n", (ULONG)dev->hd_NodeInfo.n_Device);
                kprintf("\t\tRomScanner: $%p\n", (ULONG)dev->hd_RomScanner);
                kprintf("\t\tRom:        $%p\n", (ULONG)dev->hd_Rom);
                kprintf("\t\tRomLength:  %lu\n", dev->hd_RomLength);
                kprintf("\t\tGUID:       $%016llx\n", dev->hd_GUID.q);
                kprintf("\t\tUnits:      $%p -> $%p\n", (ULONG)GetHead(&dev->hd_Units), (ULONG)GetTail(&dev->hd_Units));
                ListLength(&dev->hd_Units, count);
                kprintf("\t\tUnits:      %lu item(s)\n", count);
            }
            UNLOCK_REGION_SHARED(dev);

            Helios_ReleaseDevice(dev);
            kprintf("\n");
        }
    }
    UNLOCK_REGION_SHARED(HeliosBase);

    kprintf("=== DUMP END ===\n");
}

