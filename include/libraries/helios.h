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
** Public Helios Structures and Definitions.
**
*/

#ifndef LIBRARIES_HELIOS_H
#define LIBRARIES_HELIOS_H

#include <sys/types.h>
#include <exec/ports.h>
#include <devices/timer.h>

#define HELIOS_LIBNAME    "helios.library"
#define HELIOS_LIBVERSION 52

typedef u_int32_t QUADLET;
typedef UQUAD HeliosOffset; /* The 48-bit Node address space */

typedef enum
{
    S100=0,
    S200=1,
    S400=2,
    S800=3,
    S1600=4,
    S3200=5,

    /* Others values are reserved for future extensions */
} HeliosSpeed;

/* Physical speed reported by SelfID packets for beta nodes */
#define SBETA 3

/* HeliosReportMsg types */
#define HRMB_FATAL 0
#define HRMB_ERROR 1
#define HRMB_WARN  2
#define HRMB_INFO  3
#define HRMB_DBG   4
#define HRMF_NONE  0
#define HRMF_FATAL (1ul<<HRMB_FATAL)
#define HRMF_ERROR (1ul<<HRMB_ERROR)
#define HRMF_WARN  (1ul<<HRMB_WARN)
#define HRMF_INFO  (1ul<<HRMB_INFO)
#define HRMF_DBG   (1ul<<HRMB_DBG)

/* Transaction codes */
/* -- Request -- */
#define TCODE_READ_QUADLET_REQUEST  0x4 /* hl: 12 */
#define TCODE_WRITE_QUADLET_REQUEST 0x0 /* hl: 16 */
#define TCODE_READ_BLOCK_REQUEST    0x5 /* hl: 16 */
#define TCODE_WRITE_BLOCK_REQUEST   0x1 /* hl: 16, payload */
#define TCODE_LOCK_REQUEST          0x9 /* hl: 16, payload */
#define TCODE_WRITE_PHY             0xe /* hl: 12 */
#define TCODE_WRITE_STREAM          0xa /* hl: 8, payload */

/* -- Response -- */
#define TCODE_WRITE_RESPONSE        0x2 /* hl: 12 */
#define TCODE_READ_QUADLET_RESPONSE 0x6 /* hl: 16 */
#define TCODE_READ_BLOCK_RESPONSE   0x7 /* hl: 16, payload */
#define TCODE_LOCK_RESPONSE         0xb /* hl: 16, payload */

/* Extended transaction code */
#define EXTCODE_MASK_SWAP     1
#define EXTCODE_COMPARE_SWAP  2
#define EXTCODE_FETCH_ADD     3
#define EXTCODE_LITTLE_ADD    4
#define EXTCODE_BOUNDED_ADD   5
#define EXTCODE_WRAP_ADD      6

#define HELIOS_LOCAL_BUS      (0x3ff << 6)

#define HELIOS_BUSF_BUSMANAGER  (1<<0)
#define HELIOS_BUSF_CYCLEMASTER (1<<1)
#define HELIOS_BUSF_IRM         (1<<2)

#define HELIOS_1394_BUS_ID         (0x31333934)  // "1394" chars

/* Ischronous context types */
#define HELIOS_ISO_RX_CTX 0
#define HELIOS_ISO_TX_CTX 1

/* API return error codes */
#define HERR_NOERR     0
#define HERR_NOMEM    -1
#define HERR_SYSTEM   -2
#define HERR_BADCALL  -3
#define HERR_BUSRESET -4
#define HERR_IO       -5
#define HERR_TIMEOUT  -6

/* Types for Helios_GetAttrs, Helios_SetAttrs */
#define HGA_BASE      1
#define HGA_HARDWARE  2
#define HGA_DEVICE    3
#define HGA_UNIT      4
#define HGA_CLASS     5

/* Helios Tags (Lib: 0x00..0xff, HW device: 0x100..0x1ff), classes: 0x200..0x2ff) */
#define HELIOS_TAGBASE         (0x8E710000)         /* Reserved area: range len = 0x10000 */
#define HA_Dummy               (HELIOS_TAGBASE)
#define HA_UserData            (HA_Dummy+1)
#define HA_NodeID              (HA_Dummy+2)
#define HA_GUID                (HA_Dummy+3)
#define HA_GUID_Hi             (HA_Dummy+4)
#define HA_GUID_Lo             (HA_Dummy+5)
#define HA_BusOptions          (HA_Dummy+6)
#define HA_MsgPort             (HA_Dummy+7)
#define HA_Pool                (HA_Dummy+8)
#define HA_Hardware            (HA_Dummy+9)
#define HA_Priority            (HA_Dummy+10)
#define HA_UnitNo              (HA_Dummy+11)
#define HA_Callback            (HA_Dummy+12)
#define HA_Speed               (HA_Dummy+13)
#define HA_Device              (HA_Dummy+14)
#define HA_Unit                (HA_Dummy+15)
#define HA_EventMask           (HA_Dummy+16)
#define HA_Rom                 (HA_Dummy+17)
#define HA_RomLength           (HA_Dummy+18)
#define HA_NodeInfo            (HA_Dummy+19)
#define HA_Generation          (HA_Dummy+20)
#define HA_EventListenerList   (HA_Dummy+21)
#define HA_CSR_VendorID        (HA_Dummy+22)
#define HA_CSR_ModelID         (HA_Dummy+23)
#define HA_CSR_UnitSpecID      (HA_Dummy+24)
#define HA_CSR_UnitSWVersion   (HA_Dummy+25)
#define HA_UnitRomDirectory    (HA_Dummy+26)
#define HA_IsoType             (HA_Dummy+27)
#define HA_IsoContextID        (HA_Dummy+28)
#define HA_IsoBufferSize       (HA_Dummy+29)
#define HA_IsoBufferCount      (HA_Dummy+30)
#define HA_IsoHeaderLenght     (HA_Dummy+31)
#define HA_IsoPayloadAlign     (HA_Dummy+32)
#define HA_IsoCallback         (HA_Dummy+33)
#define HA_IsoContext          (HA_Dummy+34)
#define HA_IsoChannel          (HA_Dummy+35)
#define HA_IsoTag              (HA_Dummy+36)
#define HA_IsoRxDropEmpty      (HA_Dummy+37)

/*--- Class methods (HeliosClass_DoMethodA) ----*/
#define HCM_Dummy                (HELIOS_TAGBASE+0x200)
#define HCM_GetAttrs             (HCM_Dummy+0)
#define HCM_SetAttrs             (HCM_Dummy+1)
#define HCM_ReleaseAllBindings   (HCM_Dummy+2)
#define HCM_AttemptDeviceBinding (HCM_Dummy+3)
#define HCM_ForceDeviceBinding   (HCM_Dummy+4)
#define HCM_ReleaseDeviceBinding (HCM_Dummy+5)
#define HCM_AttemptUnitBinding   (HCM_Dummy+6)
#define HCM_ForceUnitBinding     (HCM_Dummy+7)
#define HCM_ReleaseUnitBinding   (HCM_Dummy+8)
#define HCM_Initialize           (HCM_Dummy+9)
#define HCM_Terminate            (HCM_Dummy+10)

/*--- Helios Packet Ack Code ---*/
/* IEEE1394 */
#define HELIOS_ACK_COMPLETE      1
#define HELIOS_ACK_PENDING       2
#define HELIOS_ACK_BUSY_X        4
#define HELIOS_ACK_BUSY_A        5
#define HELIOS_ACK_BUSY_B        6
#define HELIOS_ACK_TARDY        11
#define HELIOS_ACK_DATA_ERROR   13
#define HELIOS_ACK_TYPE_ERROR   14

/* Helios special Ack code */
#define HELIOS_ACK_NOTSET        0

/*--- Helios Transaction Response Code ---*/
/* IEEE1394 */
#define HELIOS_RCODE_COMPLETE       0
#define HELIOS_RCODE_CONFLICT_ERROR 4
#define HELIOS_RCODE_DATA_ERROR     5
#define HELIOS_RCODE_TYPE_ERROR     6
#define HELIOS_RCODE_ADDRESS_ERROR  7

/* Helios RCode (out of IEEE1394 range) */
#define HELIOS_RCODE_CANCELLED     -1
#define HELIOS_RCODE_GENERATION    -2
#define HELIOS_RCODE_MISSING_ACK   -3
#define HELIOS_RCODE_SEND_ERROR    -4
#define HELIOS_RCODE_BUSY          -5
#define HELIOS_RCODE_TIMEOUT       -6

/*--- Events types ---*/
#define HEVTF_HARDWARE_BUSRESET  (1<<0)
#define HEVTF_HARDWARE_TOPOLOGY  (1<<1)
#define HEVTF_DEVICE_UPDATED     (1<<2)
#define HEVTF_DEVICE_SCANNED     (1<<3)
#define HEVTF_DEVICE_DEAD        (1<<4)
#define HEVTF_DEVICE_REMOVED     (1<<5)
#define HEVTF_DEVICE_NEW_UNIT    (1<<6)
#define HEVTF_CLASS_REMOVED      (1<<9)
#define HEVTF_NEW_CLASS          (1<<10)
#define HEVTF_NEW_REPORTMSG      (1<<11)

#define HEVTF_DEVICE_ALL (HEVTF_DEVICE_UPDATED|HEVTF_DEVICE_SCANNED|HEVTF_DEVICE_DEAD|HEVTF_DEVICE_REMOVED)

/*--- Some CSR constants ---*/
/* CSR Address Space (48-bits) */
#define CSR_BASE_LO                  (0xFFFFF0000000ULL)
#define CSR_BASE_HI                  (0xFFFFFFFFFFFFULL)
#define CSR_BASE_MASK                (0x00000FFFFFFFULL)

#define CSR_CONFIG_ROM_OFFSET        (0x400)
#define CSR_CONFIG_ROM_SIZE          (0x400)
#define CSR_CONFIG_ROM_QUADLET_SIZED (0x400 / sizeof(QUADLET))
#define CSR_CONFIG_ROM_END           (CSR_CONFIG_ROM_OFFSET + CSR_CONFIG_ROM_SIZE - 1)

/* Some CSR offsets */
#define CSR_SPLIT_TIMEOUT_HI        (0x018)
#define CSR_SPLIT_TIMEOUT_LO        (0x01C)
#define CSR_CYCLE_TIME              (0x200)
#define CSR_BUS_TIME                (0x204)
#define CSR_BUS_MANAGER_ID          (0x21C)
#define CSR_BANDWIDTH_AVAILABLE     (0x220)
#define CSR_CHANNELS_AVAILABLE_HI   (0x224)
#define CSR_CHANNELS_AVAILABLE_LO   (0x228)

/* Other address spaces */
#define HELIOS_HIGHMEM_START         (0x000100000000ULL)
#define HELIOS_HIGHMEM_STOP          (0xFFFF00000000ULL)

/* Rom CSR */
#define ROM_ROOT_DIRECTORY 0x14

/* 1212-1994 */
#define CSR_KEY_TEXTUAL_DESCRIPTOR    0x01
#define CSR_KEY_BUS_DEPENDENT_INFO    0x02
#define CSR_KEY_MODULE_VENDOR_ID      0x03
#define CSR_KEY_MODULE_HW_VERSION     0x04
#define CSR_KEY_MODULE_SPEC_ID        0x05
#define CSR_KEY_MODULE_SW_VERSION     0x06
#define CSR_KEY_MODULE_DEPENDENT_INFO 0x07
#define CSR_KEY_NODE_VENDOR_ID        0x08
#define CSR_KEY_NODE_HW_VERSION       0x09
#define CSR_KEY_NODE_SPEC_ID          0x0a
#define CSR_KEY_NODE_SW_VERSION       0x0b
#define CSR_KEY_NODE_CAPABILITIES     0x0c
#define CSR_KEY_NODE_UNIQUE_ID        0x0d
#define CSR_KEY_NODE_UNITS_EXTENT     0x0e
#define CSR_KEY_NODE_MEMORY_EXTENT    0x0f
#define CSR_KEY_NODE_DEPENDENT_INFO   0x10
#define CSR_KEY_UNIT_DIRECTORY        0x11
#define CSR_KEY_UNIT_SPEC_ID          0x12
#define CSR_KEY_UNIT_SW_VERSION       0x13
#define CSR_KEY_UNIT_DEPENDENT_INFO   0x14
#define CSR_KEY_UNIT_LOCATION         0x15
#define CSR_KEY_UNIT_POLL_MASK        0x16

/* 1212-2001 */
#define CSR_KEY_MODEL_ID              0x17

/*============================================================================*/
/*=== Helios nodes ===========================================================*/

#ifndef HELIOS_INTERNAL
struct HeliosEventListenerList
{
    struct MinNode *Node;
};
struct HeliosHardware
{
    struct MinNode *Node;
};
struct HeliosDevice
{
    struct MinNode *Node;
};
struct HeliosUnit
{
    struct MinNode *Node;
};
struct HeliosClass
{
    struct MinNode *Node;
};
#else
struct HeliosEventListenerList;
struct HeliosHardware;
struct HeliosDevice;
struct HeliosUnit;
struct HeliosClass;
#endif
typedef struct HeliosEventListenerList HeliosEventListenerList;
typedef struct HeliosHardware HeliosHardware;
typedef struct HeliosDevice HeliosDevice;
typedef struct HeliosUnit HeliosUnit;
typedef struct HeliosClass HeliosClass;

/*============================================================================*/
/*=== Helios messages ========================================================*/

#define HELIOS_MSG_HEADER                       \
    struct Message  hm_Msg;                     \
    ULONG           hm_Type;                    \
    ULONG           hm_Result;

typedef struct HeliosMsg
{
    HELIOS_MSG_HEADER;
} HeliosMsg;

typedef struct HeliosEventMsg
{
    HELIOS_MSG_HEADER;
    struct timeval  hm_Time;
    ULONG           hm_EventMask;
    APTR            hm_UserData;
} HeliosEventMsg;

/* Helios messages types */
enum
{
    HELIOS_MSGTYPE_TASKKILL,
    HELIOS_MSGTYPE_EVENT,
    HELIOS_MSGTYPE_FAST_EVENT,
};

typedef struct HeliosReportMsg
{
    struct Message hrm_SysMsg;
    ULONG          hrm_TypeBit;
    CONST_STRPTR   hrm_Label;
    CONST_STRPTR   hrm_Msg;
} HeliosReportMsg;

typedef union HeliosBusOptions
{
    QUADLET value;

    struct
    {
        /* bits from 31 to 0 */
        QUADLET IsoRsrcMgr      : 1;
        QUADLET CycleMaster     : 1;
        QUADLET Isochronous     : 1;
        QUADLET BusMgr          : 1;
        QUADLET PowerMgt        : 1;
        QUADLET Reserved2       : 3;
        QUADLET CycleClockAcc   : 8;
        QUADLET MaxRec          : 4;
        QUADLET Reserved1       : 4;
        QUADLET CfgROMChg       : 2;
        QUADLET Reserved0       : 3;
        QUADLET MaxLinkSpeed    : 3;
    } r;
} HeliosBusOptions;

/* Following structure is used to create a decoded form
 * of raw 1394 asynchronous packets.
 * It's a generic way to transmit packet data to a 1394 software device.
 * This last will build its own suitable packet using data here.
 * Not all fields are filled or used, depends on the TCode value.
 */
typedef struct HeliosAPacket
{
    QUADLET         Header[4];
    UWORD           HeaderLength;
    UWORD           PayloadLength;
    UWORD           DestID;
    UWORD           SourceID;
    UBYTE           TCode;
    BYTE            TLabel;
    UBYTE           Speed;
    UBYTE           Retry;
    BYTE            RCode;
    BYTE            Ack;
    UBYTE           Reserved[2];
    UWORD           ExtTCode;
    UWORD           TimeStamp;
    HeliosOffset    Offset;
    QUADLET *       Payload;
    QUADLET         QuadletData;
} HeliosAPacket;

typedef void (*HeliosRespFreeFunc)(APTR ptr, ULONG size, APTR udata);

typedef struct HeliosResponse
{
    HeliosAPacket      hr_Packet;
    HeliosRespFreeFunc hr_FreeFunc; /* Callback to free the structure itself */
    ULONG              hr_AllocSize; /* Given to the free function */
    APTR               hr_FreeUData;
    APTR               hr_Reserved;
} HeliosResponse;

typedef struct HeliosIRBuffer
{
    APTR  Header;
    ULONG HeaderLength;
    APTR  Payload;
    ULONG PayloadLength;
} HeliosIRBuffer;

typedef void (*HeliosIRCallback)(HeliosIRBuffer *irbuf, ULONG status, APTR userdata);

typedef struct HeliosNode
{
    UBYTE          n_PhyID;
    struct
    {
        UBYTE ResetInitiator:1;
        UBYTE LinkOn:1;
    }              n_Flags;
    UBYTE          n_PortCount;
    UBYTE          n_PhySpeed;
    UBYTE          n_MaxSpeed;
    UBYTE          n_MaxHops;
    UBYTE          n_MaxDepth;
    BYTE           n_ParentPort;         /* Index in n_Ports array, -1 if not parent (root node) */
    HeliosDevice * n_Device;
    BYTE           n_Ports[16];          /* Values = PhyID of connected node, -1 if not used */
} HeliosNode __attribute__ ((aligned(4)));

typedef struct HeliosTopology
{
    ULONG      ht_Generation; /* Number of valid topologies created since the start of the stack */
    UBYTE      ht_NodeCount;
    UBYTE      ht_GapCount;
    UBYTE      ht_LocalNodeID;
    UBYTE      ht_IRMNodeID;
    UBYTE      ht_RootNodeID;
    UBYTE      ht_Reserved[3];
    HeliosNode ht_Nodes[64]; /* First node is always the local node */
} HeliosTopology;

struct HeliosTransaction;
typedef void (*HeliosTransCb)(struct HeliosTransaction *t,
                              BYTE status, QUADLET *payload, ULONG length);

typedef struct HeliosTransaction
{
    HeliosAPacket        htr_Packet;
    HeliosTransCb        htr_Callback;
    APTR                 htr_UserData;
    APTR                 htr_SplitTimerReq; /* Used for SPLIT-TIMEOUT detection */
    APTR                 htr_Private; /* For device usage only */
} HeliosTransaction;

typedef struct HeliosRomIterator
{
    const QUADLET *actual;
    const QUADLET *end;
} HeliosRomIterator;

/*============================================================================*/
/*=== Helios SubTask =========================================================*/

struct HeliosSubTask; /* Private */
typedef struct HeliosSubTask HeliosSubTask;
typedef void (*HeliosSubTaskEntry)(HeliosSubTask *self, struct TagItem *tags);


/*============================================================================*/
/*=== Macros =================================================================*/


#define Helios_FillReadQuadletPacket(p, speed, offset) ({    \
        p->Speed = speed;                                   \
        p->TCode = TCODE_READ_QUADLET_REQUEST;              \
        p->Offset = offset; })

#define Helios_FillReadBlockPacket(p, speed, offset, length) ({  \
        p->Speed = speed;                                       \
        p->TCode = TCODE_READ_BLOCK_REQUEST;                    \
        p->Offset = offset;                                     \
        p->PayloadLength = length; })

#define Helios_FillWriteBlockPacket(p, speed, offset, payload, length) ({ \
        p->Speed = speed;                                               \
        p->TCode = TCODE_WRITE_BLOCK_REQUEST;                           \
        p->Offset = offset;                                             \
        p->Payload = payload;                                           \
        p->PayloadLength = length; })

#define Helios_FillWriteQuadletPacket(p, speed, offset, data) ({ \
        p->Speed = speed;                                       \
        p->TCode = TCODE_WRITE_QUADLET_REQUEST;                 \
        p->Offset = offset;                                     \
        p->QuadletData = data; })

#define Helios_FillLockPacket(p, speed, offset, ltc, payload, length) ({ \
        p->Speed = speed;                               \
        p->TCode = TCODE_LOCK_REQUEST;                  \
        p->ExtTCode = ltc;                              \
        p->Offset = offset;                             \
        p->Payload = payload;                           \
        p->PayloadLength = length; })

#endif /* LIBRARIES_HELIOS_H */
