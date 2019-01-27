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
** This file is copyrights 2008-2013 by Guillaume ROGUEZ.
**
** SBP2 class private API header file.
*/

#ifndef SBP2_PRIVATE_H
#define SBP2_PRIVATE_H

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include "proto/helios.h"

#include <devices/scsidisk.h>
#include <devices/trackdisk.h>
#include <libraries/mount.h>

#include "utils.h"
#include "debug.h"

#define LOGIN_RETRY 5

#define ORBPOINTER_NULL (1ull<<63)

#define ORBCONTROLTYPE_LOGIN            0
#define ORBCONTROLTYPE_QUERYLOGINS      1
#define ORBCONTROLTYPE_CREATETASKSET    2
#define ORBCONTROLTYPE_RECONNECT        3
#define ORBCONTROLTYPE_SETPASSWORD      4
#define ORBCONTROLTYPE_NODEHANDLE       5
#define ORBCONTROLTYPE_LOGOUT           7
#define ORBCONTROLTYPE_ABORTTASK        0xb
#define ORBCONTROLTYPE_ABORTTASKSET     0xc
#define ORBCONTROLTYPE_LOGICALUNITRESET 0xe
#define ORBCONTROLTYPE_TARGETRESET      0xf

#define ORBCONTROLF_NOTIFY          (1UL<<15)
#define ORBCONTROLF_READ            (1UL<<11)

#define ORBLOGINCONTROLF_EXCLUSIVE  (1UL<<12)
#define ORBLOGINRECONNECT(x)        (x<<4)

#define ORBCONTROLSPEED(x)      (x<<8)
#define ORBCONTROLPAYLOAD(x)    (x<<4)
#define ORBCONTROLF_PAGETABLE   (1UL<<3)
#define ORBCONTROLPAGESIZE(x)   (x<<0)

/* Offsets for command block agent registers */
#define SBP2_AGENT_STATE        0x00
#define SBP2_AGENT_RESET        0x04
#define SBP2_ORB_POINTER        0x08
#define SBP2_DOORBELL           0x10
#define SBP2_UNSOLICITED_STATUS_ENABLE  0x14

#define STATUS_GET_ORB_HIGH(v)	 ((v).status & 0xffff)
#define STATUS_GET_SBP_STATUS(v) (((v).status >> 16) & 0xff)
#define STATUS_GET_LEN(v)		 (((v).status >> 24) & 0x07)
#define STATUS_GET_DEAD(v)		 (((v).status >> 27) & 0x01)
#define STATUS_GET_RESPONSE(v)	 (((v).status >> 28) & 0x03)
#define STATUS_GET_SOURCE(v)	 (((v).status >> 30) & 0x03)
#define STATUS_GET_ORB_LOW(v)	 ((v).orb_low)
#define STATUS_GET_DATA(v)		 ((v).data)

#define ORB_PAGE_SIZE 0xfffc /* DataLen = 0xffff, but addresses must be 4-bytes aligned */
#define ORB_SG_PAGES 64 /* enough for maxXfer=2MB and page_size=0xfffc */
#define ORB_TIMEOUT 3500 /* important to let the time to launch motor for some devices like CD */

#define SBP2_VENDORID_LEN 8
#define SBP2_PRODUCTID_LEN 16
#define SBP2_PRODUCTVERSION_LEN 4

struct SBP2ORBRequest;
struct SBP2Unit;
struct SBP2ClassLib;
typedef struct SBP2ClassLib SBP2ClassLib;

typedef union ORBPointer
{
	struct {
		ULONG hi;
		ULONG lo;
	} addr __attribute__((packed));
	UQUAD q;
} ORBPointer __attribute__((__aligned__(8)));

typedef struct ORBStatus {
	QUADLET status;
	QUADLET orb_low;
	UBYTE data[24];
} ORBStatus;

/* Optimization : writting length + addr_hi in single 32bits write */
typedef struct SBP2SGPage {
	ULONG length_addr_hi;
	ULONG addr_lo;
} SBP2SGPage __attribute__((__aligned__(8)));

typedef void(*ORBDoneCallback)(struct SBP2Unit *unit, struct SBP2ORBRequest *orbreq, ORBStatus *status);

typedef struct SBP2ORBRequest
{
	struct IOStdReq     or_Base;        /* For the transport layer (Helios) */
	struct MinNode      or_Node;        /* To enqueue into the pending list */
	HeliosPacket        or_Packet;      /* IEEE1394 Packet */
	ORBPointer          or_ORBAddr;     /* The ORB 1394 address (written to the target) */
	ORBDoneCallback     or_ORBDone;     /* Called when ORB status is available (or error) */
	ORBStatus           or_ORBStatus;   /* SBP2 ORB status */
	struct Task *       or_ORBStTask;
	ULONG               or_ORBStSignal;
} SBP2ORBRequest;

typedef struct ORBLogin
{
	ORBPointer password;
	ORBPointer response;
	UWORD      control;
	UWORD      lun;
	UWORD      passwordlen;
	UWORD      responselen;
	UQUAD      statusfifo;
} ORBLogin __attribute__((__aligned__(4)));

typedef struct ORBLogout
{
	UQUAD      reserved;
	UQUAD      reserved1;
	UWORD      control;
	UWORD      loginid;
	ULONG      reserved2;
	UQUAD      statusfifo;
} ORBLogout __attribute__((__aligned__(4)));

typedef struct ORBReconnect
{
	UQUAD      reserved;
	UQUAD      reserved1;
	UWORD      control;
	UWORD      loginid;
	ULONG      reserved2;
	UQUAD      statusfifo;
} ORBReconnect __attribute__((__aligned__(4)));

typedef struct ORBDummy
{
	ORBPointer next;
	UQUAD      reserved1;
	UWORD      control;
	UWORD      reserved2;
} ORBDummy __attribute__((__aligned__(4)));

typedef struct ORBSCSICommand
{
	ORBPointer next;
	ULONG      desc_hi;
	ULONG      desc_lo;
	UWORD      control;
	UWORD      datalen;
	UBYTE      cdb[16];
} ORBSCSICommand __attribute__((__aligned__(4)));

typedef struct ORBLoginResponse
{
	UWORD      length;
	UWORD      login_id;
	UQUAD      command_agent __attribute__((packed));
	UWORD      reserved;
	UWORD      reconnect_hold;
} ORBLoginResponse;

typedef struct SBP2SCSICmdReq
{
	SBP2ORBRequest    sr_Base;
	ORBSCSICommand    sr_ORB;
	struct SCSICmd *  sr_Cmd;
	struct SBP2Unit * sr_Unit;

	/* Align SG pages on a cache line */
	SBP2SGPage        sr_SGPages[ORB_SG_PAGES];
} SBP2SCSICmdReq;

typedef struct {
	ULONG Logged:1;
	ULONG Ordered:1;
	ULONG AcceptIO:1;
	ULONG Removable:1;
	ULONG Ready:1;
	ULONG WriteProtected:1;
	ULONG AutoMountCD:1;
	ULONG ProcessIO;
	ULONG Public;
	ULONG NotUsed0:6;

	/* Workarounds */
	ULONG Inquiry36:1;    /* Force inquiry length to 36 bytes */
	ULONG PowerCond:1;    /* Set POWER_CONDITION bits */
	ULONG FixCapacity:1;  /* Try to fix capacity values if wrong */
	ULONG MaxXfert128k:1; /* Use for xfert 128k-bytes at max per SCSI command */
} SBP2Flags;

/* SBP2 logical unit: */
typedef struct SBP2Unit
{
	struct Unit				u_SysUnit;

	/* Management data */
	LOCK_PROTO;
	SBP2ClassLib *			u_Class;
	LONG					u_UnitNo;
	SBP2Flags				u_Flags;
	struct Task *			u_DriverTask;
	struct MsgPort *		u_TimerPort;
	struct timerequest *	u_IOTimeReq;
	ULONG					u_ChangeCounter;
	APTR					u_NotifyUnit;
	ULONG					u_AutoReconnect;
	
	/* Transport info */
	HeliosHardware *		u_HeliosHardware;
	HeliosDevice *			u_HeliosDevice;
	HeliosUnit *			u_HeliosUnit;
	ULONG					u_Generation;
	UQUAD					u_GUID;
	UWORD					u_NodeID;
	UBYTE					u_MaxSpeed;
	UBYTE					u_MaxPayload;

	/* ROM data */
	HeliosOffset			u_MgtAgentBase;
	LONG					u_MgtTimeout;
	UWORD					u_LUN;
	UWORD					u_MaxReconnectHold;
	UWORD					u_DeviceType;
	UBYTE					u_ORBSize;
	UBYTE					u_Reserved0;

	/* Login data */
	ORBLoginResponse		u_ORBLoginResponse;
	UWORD					u_LoginID;
	UBYTE					u_Reserved1[2];

	/* ORB management data */
	LOCKED_MINLIST_PROTO(u_PendingORBs);
	HeliosRequestHandler	u_FifoStatusRH;

	/* Unit specifics and SCSI cmds management data */
	STRPTR					u_UnitName;
	struct MsgPort *		u_OrbPort;
	struct DriveGeometry	u_Geometry;
	ULONG					u_BlockSize;
	UBYTE					u_BlockShift;
	UBYTE					u_Read10Cmd;
	BYTE					u_ORBStatusSigBit;
	UBYTE					u_Reserved2;
	UBYTE *					u_OneBlock;
	ULONG					u_OneBlockSize;
	UBYTE					u_ModePageBuf[256];

	UBYTE					u_VendorID[SBP2_VENDORID_LEN+1];
	UBYTE					u_ProductID[SBP2_PRODUCTID_LEN+1];
	UBYTE					u_ProductVersion[SBP2_PRODUCTVERSION_LEN+1];
} SBP2Unit;

extern void sbp2_cancel_orbs(SBP2Unit *unit);
extern LONG sbp2_do_scsi_cmd(SBP2Unit *unit, struct SCSICmd *scsicmd, ULONG timeout);
extern void sbp2_unmount_all(SBP2Unit *unit);

extern void sbp2_incref(SBP2Unit *unit);
extern void sbp2_decref(SBP2Unit *unit);

extern LONG sbp2_InitClass(SBP2ClassLib *base, HeliosClass *hc);
extern LONG sbp2_TermClass(SBP2ClassLib *base);
extern LONG sbp2_ReleaseAllBindings(SBP2ClassLib *base);
extern LONG sbp2_ForceUnitBinding(SBP2ClassLib *base, HeliosUnit *unit);
extern LONG sbp2_AttemptUnitBinding(SBP2ClassLib *base, HeliosUnit *unit);
extern LONG sbp2_ReleaseUnitBinding(SBP2ClassLib *base, HeliosUnit *unit);

#endif /* SBP2_PRIVATE_H */
