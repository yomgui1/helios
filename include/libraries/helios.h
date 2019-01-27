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
*/

#ifndef LIBRARIES_HELIOS_H
#define LIBRARIES_HELIOS_H

#include <sys/types.h>
#include <exec/ports.h>
#include <devices/timer.h>

#define HELIOS_LIBNAME		"helios.library"
#define HELIOS_LIBVERSION	53

typedef u_int32_t QUADLET;
typedef UQUAD HeliosOffset; /* The 48-bit Node address space */

typedef enum {
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

/* Existing 1394 standards */
#define HSTD_1394_1995  (1<<0) /* Speeds: s100, s200, s400 */
#define HSTD_1394A_2000 (1<<1)
#define HSTD_1394B_2002 (1<<2) /* Speeds: s800, s1600, s3200 */
#define HSTD_1394C_2006 (1<<3)
#define HSTD_1394_2008  (1<<4)

/* RequestHandler flags */
#define HELIOS_RHF_ALLOCATE (1<<0) /* let Helios find the start address in given region */

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
#define HERR_NOTASK   -7

/* Types for Helios_GetAttrs, Helios_SetAttrs */
#define HGA_INVALID		0
#define HGA_HARDWARE	1
#define HGA_DEVICE		2
#define HGA_UNIT		3
#define HGA_CLASS		4

/* Helios Tags (Lib: 0x00..0xff, HW device: 0x100..0x1ff, classes: 0x200..0x2ff) */
#define HELIOS_TAGBASE			(0x8E710000)		/* Reserved area: range len = 0x10000 */
#define HA_Dummy				(HELIOS_TAGBASE)
#define HA_UserData				(HA_Dummy+1)
#define HA_NodeID				(HA_Dummy+2)
#define HA_GUID					(HA_Dummy+3)
#define HA_GUID_Hi				(HA_Dummy+4)
#define HA_GUID_Lo				(HA_Dummy+5)
#define HA_BusOptions			(HA_Dummy+6)
#define HA_MsgPort				(HA_Dummy+7)
#define HA_MemPool				(HA_Dummy+8)
#define HA_Hardware				(HA_Dummy+9)
#define HA_Priority				(HA_Dummy+10)
#define HA_UnitNo				(HA_Dummy+11)
#define HA_Callback				(HA_Dummy+12)
#define HA_Speed				(HA_Dummy+13)
#define HA_Device				(HA_Dummy+14)
#define HA_Unit					(HA_Dummy+15)
#define HA_EventMask			(HA_Dummy+16)
#define HA_Rom					(HA_Dummy+17)
#define HA_RomLength			(HA_Dummy+18)
#define HA_NodeInfo				(HA_Dummy+19)
#define HA_Generation			(HA_Dummy+20)
#define HA_EventListenerList	(HA_Dummy+21)
#define HA_CSR_VendorID			(HA_Dummy+22)
//#define HA_CSR_ModelID			(HA_Dummy+23)
//#define HA_CSR_UnitSpecID		(HA_Dummy+24)
//#define HA_CSR_UnitSWVersion	(HA_Dummy+25)
#define HA_UnitRomDirectory		(HA_Dummy+26)
#define HA_IsoType				(HA_Dummy+27)
#define HA_IsoContextID			(HA_Dummy+28)
#define HA_IsoBufferSize		(HA_Dummy+29)
#define HA_IsoBufferCount		(HA_Dummy+30)
#define HA_IsoHeaderLenght		(HA_Dummy+31)
#define HA_IsoPayloadAlign		(HA_Dummy+32)
#define HA_IsoCallback			(HA_Dummy+33)
#define HA_IsoContext			(HA_Dummy+34)
#define HA_IsoChannel			(HA_Dummy+35)
#define HA_IsoTag				(HA_Dummy+36)
#define HA_IsoRxDropEmpty		(HA_Dummy+37)
#define HA_ClassLoadName		(HA_Dummy+38)
#define HA_ObjectName			(HA_Dummy+39)
#define HA_ObjectFreeFunc		(HA_Dummy+40)
#define HA_ObjectGetAttrFunc	(HA_Dummy+41)
#define HA_ObjectSetAttrFunc	(HA_Dummy+42)

/*--- Class methods (HeliosClass_DoMethodA) ----*/
#define HCM_Dummy                (HELIOS_TAGBASE+0x200)
#define HCM_GetAttrs             (HCM_Dummy+0)
#define HCM_SetAttrs             (HCM_Dummy+1)
//#define HCM_ReleaseAllBindings   (HCM_Dummy+2)
#define HCM_AttemptDeviceBinding (HCM_Dummy+3)
#define HCM_ForceDeviceBinding   (HCM_Dummy+4)
#define HCM_ReleaseDeviceBinding (HCM_Dummy+5)
#define HCM_AttemptUnitBinding   (HCM_Dummy+6)
#define HCM_ForceUnitBinding     (HCM_Dummy+7)
#define HCM_ReleaseUnitBinding   (HCM_Dummy+8)
#define HCM_Initialize           (HCM_Dummy+9)
#define HCM_Terminate            (HCM_Dummy+10)

/* Transaction codes */
/* -- Request -- */
#define TCODE_WRITE_QUADLET_REQUEST 0x0 /* hl: 16, header data	*/
#define TCODE_WRITE_BLOCK_REQUEST   0x1 /* hl: 16, payload		*/
#define TCODE_READ_QUADLET_REQUEST  0x4 /* hl: 12				*/
#define TCODE_READ_BLOCK_REQUEST    0x5 /* hl: 16, header data	*/
#define TCODE_LOCK_REQUEST          0x9 /* hl: 16, payload		*/
#define TCODE_WRITE_STREAM          0xa /* hl:  8, payload		*/
#define TCODE_WRITE_PHY             0xe /* hl: 12				*/

/* -- Response -- */
#define TCODE_WRITE_RESPONSE        0x2 /* hl: 12				*/
#define TCODE_READ_QUADLET_RESPONSE 0x6 /* hl: 16, header data	*/
#define TCODE_READ_BLOCK_RESPONSE   0x7 /* hl: 16, payload		*/
#define TCODE_LOCK_RESPONSE         0xb /* hl: 16, payload		*/

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

#define HELIOS_1394_BUS_ID         (0x31333934) /* "1394" chars */

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
#define HELIOS_ACK_NOTSET        0  /* No ack code set */
#define HELIOS_ACK_GENERATION   -1  /* Packet not transmitted, bus-reset occured */
#define HELIOS_ACK_MISSING      -2  /* Packet transmitted, but not ack'ed */
#define HELIOS_ACK_TIMEOUT      -3  /* Packet transmitted, but packet timeout expired (for response packet only) */
#define HELIOS_ACK_OTHER        -4  /* other events during ack phase */

/*--- Helios Transaction Response Code ---*/
/* IEEE1394 */
#define HELIOS_RCODE_COMPLETE       0
#define HELIOS_RCODE_CONFLICT_ERROR 4
#define HELIOS_RCODE_DATA_ERROR     5
#define HELIOS_RCODE_TYPE_ERROR     6
#define HELIOS_RCODE_ADDRESS_ERROR  7

/* Helios RCode (out of IEEE1394 range) */
#define HELIOS_RCODE_INVALID       -1   /* No RCode set */
#define HELIOS_RCODE_CANCELLED     -2   /* Packet ack'ed, cancelled by user */
#define HELIOS_RCODE_GENERATION    -3   /* Packet ack'ed, bus-reset occured before response received */
#define HELIOS_RCODE_TIMEOUT       -4   /* Packet transmitted, ack received, but split-timeout expired */

/*--- Events types ---*/
#define HEVTF_HARDWARE_BUSRESET		(1<<0)
#define	HEVTF_HARDWARE_SELFID		(1<<1)
#define	HEVTF_HARDWARE_TOPOLOGY		(1<<2)
#define HEVTF_DEVICE_UPDATED		(1<<3)
#define HEVTF_DEVICE_SCANNED		(1<<4)
#define HEVTF_DEVICE_DEAD			(1<<5)
#define HEVTF_DEVICE_REMOVED		(1<<6)
#define HEVTF_DEVICE_NEW_UNIT		(1<<7)
#define HEVTF_CLASS_ADDED			(1<<8)
#define HEVTF_CLASS_REMOVED			(1<<9)

#define HEVTF_DEVICE_ALL (HEVTF_DEVICE_UPDATED|HEVTF_DEVICE_SCANNED|HEVTF_DEVICE_DEAD|HEVTF_DEVICE_REMOVED)

/*--- SelfID related constants ---*/
#define HELIOS_SELFID_LENGHT 		504 /* 63 nodes, 8 bytes per node */
#define HELIOS_PORTS_PER_NODE		16

/*--- Some CSR constants ---*/
/* CSR Address Space (48-bits) */
#define CSR_BASE_LO						(0xFFFFF0000000ULL)
#define CSR_BASE_HI						(0xFFFFFFFFFFFFULL)
#define CSR_BASE_MASK					(0x00000FFFFFFFULL)

#define CSR_CONFIG_ROM_OFFSET			256 /* Quadlet unit */
#define CSR_CONFIG_ROM_SIZE				256 /* Quadlet unit */
#define CSR_CONFIG_ROM_END				(CSR_CONFIG_ROM_OFFSET + CSR_CONFIG_ROM_SIZE - 1)

/* Some CSR offsets */
#define CSR_SPLIT_TIMEOUT_HI			(0x018)
#define CSR_SPLIT_TIMEOUT_LO			(0x01C)
#define CSR_CYCLE_TIME					(0x200)
#define CSR_BUS_TIME					(0x204)
#define CSR_BUS_MANAGER_ID				(0x21C)
#define CSR_BANDWIDTH_AVAILABLE			(0x220)
#define CSR_CHANNELS_AVAILABLE_HI		(0x224)
#define CSR_CHANNELS_AVAILABLE_LO		(0x228)

#define KEYTYPE_IMMEDIATE		0
#define KEYTYPE_OFFSET			1
#define KEYTYPE_LEAF			2
#define KEYTYPE_DIRECTORY		3

#define KEYTYPEV_IMMEDIATE		(KEYTYPE_IMMEDIATE<<6)
#define KEYTYPEV_OFFSET			(KEYTYPE_OFFSET<<6)
#define KEYTYPEV_LEAF			(KEYTYPE_LEAF<<6)
#define KEYTYPEV_DIRECTORY		(KEYTYPE_DIRECTORY<<6)

/* Other address spaces */
#define HELIOS_HIGHMEM_START			(0x000100000000ULL)
#define HELIOS_HIGHMEM_STOP				(0xFFFF00000000ULL)

/* Rom CSR */
#define ROM_ROOT_DIR_OFFSET 			5 /* in QUADLETs, not bytes */

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
/*=== Helios internals objects ===============================================*/
struct HeliosEventListenerList;
struct HeliosHardware;
struct HeliosDevice;
struct HeliosUnit;
struct HeliosClass;

typedef struct HeliosEventListenerList HeliosEventListenerList;
typedef struct HeliosHardware HeliosHardware;
typedef struct HeliosDevice HeliosDevice;
typedef struct HeliosUnit HeliosUnit;
typedef struct HeliosClass HeliosClass;

/*============================================================================*/
/*=== Helios messages ========================================================*/

#define HELIOS_MSG_HEADER \
	struct Message	hm_Msg; \
	ULONG			hm_Type; \
	ULONG			hm_Result;

typedef struct HeliosMsg {
	HELIOS_MSG_HEADER;
} HeliosMsg;

typedef struct HeliosEventMsg {
	HELIOS_MSG_HEADER;
	struct timeval	hm_Time;
	ULONG			hm_EventMask;
	APTR			hm_UserData;
} HeliosEventMsg;

/* Helios messages types */
enum {
	HELIOS_MSGTYPE_INIT=0,
	HELIOS_MSGTYPE_DIE,
	HELIOS_MSGTYPE_EVENT,
	HELIOS_MSGTYPE_FAST_EVENT,
} HeliosMsgType;

typedef union HeliosBusOptions {
	QUADLET quadlet;

	struct
	{
		/* bits from 31 to 0 */
		QUADLET IsoRsrcMgr		: 1;
		QUADLET CycleMaster		: 1;
		QUADLET Isochronous		: 1;
		QUADLET BusMgr			: 1;
		QUADLET PowerMgt		: 1;
		QUADLET Reserved2		: 3;
		QUADLET CycleClockAcc	: 8;
		QUADLET MaxRec			: 4;
		QUADLET Reserved1		: 4;
		QUADLET CfgROMChg		: 2;
		QUADLET Reserved0		: 3;
		QUADLET MaxLinkSpeed	: 3;
	};
} HeliosBusOptions;

typedef struct HeliosRomIterator
{
	const QUADLET *actual;
	const QUADLET *end;
} HeliosRomIterator;

/*============== IO structures ==============*/

/* Following structure is used to create a decoded form
 * of raw 1394 asynchronous packets.
 * It's a generic way to transmit packet data to a 1394 software device.
 * This last will build its own suitable packet using data here.
 * Not all fields are filled or used, depends on the TCode value.
 * Note: fields are ordered in way to keep alignment requirements.
 */
typedef struct HeliosPacket
{
	QUADLET			Header[4];		/* temporary memory used internally, DO NOT STORE ANYTHING HERE */
	UQUAD			Offset;
	UWORD			HeaderLength;
	UWORD			PayloadLength;
	UWORD			DestID;
	UWORD			SourceID;
	struct
	{
		ULONG		Retry:2;
		ULONG		Speed:3;
		ULONG		TCode:4;
		ULONG		TLabel:6;
		ULONG		Generation:8;
	};
	WORD			Ack;
	WORD			RCode;
	UWORD			ExtTCode;
	UWORD			TimeStamp;
	union {
		QUADLET		QuadletData;
		QUADLET *	Payload;
	};
} HeliosPacket;

typedef struct HeliosResponse
{
	struct Message	hr_SysMsg;
	HeliosPacket	hr_Packet;
} HeliosResponse;

typedef HeliosResponse* (*HeliosReqHCallback)(HeliosPacket *request, APTR userdata);

/* Used to register asynchronous request handlers */
typedef struct HeliosRequestHandler
{
	UQUAD				hrh_RegionStart;
	UQUAD				hrh_RegionStop;
	UQUAD				hrh_Start;
	ULONG				hrh_Length;
	ULONG				hrh_Flags;
	HeliosReqHCallback	hrh_Callback;
	APTR				hrh_UserData;
} HeliosRequestHandler;

typedef struct HeliosIRBuffer
{
	APTR	Header;
	ULONG	HeaderLength;
	APTR	Payload;
	ULONG	PayloadLength;
} HeliosIRBuffer;

typedef void (*HeliosIRCallback)(HeliosIRBuffer *irbuf, ULONG status, APTR userdata);

/*============== Topology structures ==============*/

typedef struct HeliosSelfIDStream
{
	UWORD		hss_PacketCount;
	UBYTE		hss_Generation;
	UBYTE		hss_LocalNodeID;
	QUADLET 	hss_Data[HELIOS_SELFID_LENGHT];
} HeliosSelfIDStream;

typedef struct HeliosNode
{
	HeliosDevice *	hn_Device;		/* PRIVATE: DON'T TOUCH */
	UBYTE			hn_PhyID;
	UBYTE			hn_PortCount;
	UBYTE			hn_PhySpeed;
	UBYTE			hn_MaxSpeed;
	UBYTE			hn_MaxHops;
	UBYTE			hn_MaxDepth;
	BYTE			hn_ParentPort;	/* Index in n_Ports array, -1 if not parent (root node) */
	struct
	{
		UBYTE	IsResetInitiator:1;
		UBYTE	IsLinkOn:1;
	}				hn_Flags;
	BYTE			hn_Ports[HELIOS_PORTS_PER_NODE]; /* Values = PhyID of connected node, -1 if not used */
} HeliosNode;

typedef struct HeliosTopology
{
	UBYTE		ht_Generation;
	UBYTE		ht_NodeCount;
	UBYTE		ht_GapCount;
	UBYTE		ht_LocalNodeID;
	UBYTE		ht_IRMNodeID;
	UBYTE		ht_RootNodeID;
	UBYTE		ht_Reserved[2];
	HeliosNode	ht_Nodes[64];
} HeliosTopology;

/*============================================================================*/
/*=== Helios SubTask =========================================================*/

struct HeliosSubTask; /* Private */
typedef struct HeliosSubTask HeliosSubTask;
typedef LONG (*HeliosSubTaskEntry)(HeliosSubTask *self, struct TagItem *tags);

/*============================================================================*/
/*=== Macros =================================================================*/

#define Helios_FillReadQuadletPacket(p, speed, offset) ({ \
		HeliosPacket *_p = (p); \
		_p->Speed = speed;						\
		_p->TCode = TCODE_READ_QUADLET_REQUEST;	\
		_p->Offset = offset;					\
		_p->Payload = NULL; })

#define Helios_FillReadBlockPacket(p, speed, offset, length) ({ \
		HeliosPacket *_p = (p); \
		_p->Speed = speed;						\
		_p->TCode = TCODE_READ_BLOCK_REQUEST;	\
		_p->Offset = offset;					\
		_p->PayloadLength = length; })

#define Helios_FillWriteBlockPacket(p, speed, offset, payload, length) ({ \
		HeliosPacket *_p = (p); \
		_p->Speed = speed;						\
		_p->TCode = TCODE_WRITE_BLOCK_REQUEST;	\
		_p->Offset = offset;					\
		_p->Payload = payload;					\
		_p->PayloadLength = length; })

#define Helios_FillWriteQuadletPacket(p, speed, offset, data) ({ \
		HeliosPacket *_p = (p); \
		_p->Speed = speed;							\
		_p->TCode = TCODE_WRITE_QUADLET_REQUEST;	\
		_p->Offset = offset;						\
		_p->QuadletData = data; })

#define Helios_FillLockPacket(p, speed, offset, ltc, payload, length) ({ \
		HeliosPacket *_p = (p); \
		_p->Speed = speed;				\
		_p->TCode = TCODE_LOCK_REQUEST;	\
		_p->ExtTCode = ltc;				\
		_p->Offset = offset;			\
		_p->Payload = payload;			\
		_p->PayloadLength = length; })

#endif /* LIBRARIES_HELIOS_H */
