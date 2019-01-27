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
**
** SBP2 class IO commands API.
**
*/

//#define NDEBUG
#define DEBUG_SCSI
#define DEBUG_MEM

#include "sbp2.class.h"

#include <scsi/commands.h>
#include <scsi/values.h>
#include <hardware/byteswap.h>

#include <proto/dos.h>
#include <clib/macros.h>

#include <string.h>

#ifndef NDEBUG
static const STRPTR scsi_cmd_names[256] =
{
    [0x00]="TEST_UNIT_READY",
    [0x01]="REZERO_UNIT",
    [0x02]="$02",
    [0x03]="REQUEST_SENSE",
    [0x04]="$04",
    [0x05]="$05",
    [0x06]="$06",
    [0x07]="$07",
    [0x08]="READ_6",
    [0x09]="$09",
    [0x0a]="WRITE_6",
    [0x0b]="$0b",
    [0x0c]="$0c",
    [0x0d]="$0d",
    [0x0e]="$0e",
    [0x0f]="$0f",
    [0x10]="$10",
    [0x11]="$11",
    [0x12]="INQUIRY",
    [0x13]="$13",
    [0x14]="$14",
    [0x15]="MODE_SELECT_6",
    [0x16]="$16",
    [0x17]="$17",
    [0x18]="COPY",
    [0x19]="$19",
    [0x1a]="MODE_SENSE_6",
    [0x1b]="START_STOP_UNIT",
    [0x1c]="RECEIVE_DIAGNOSTIC_RESULTS",
    [0x1d]="SEND_DIAGNOSTIC",
    [0x1e]="PREVENT_ALLOW_MEDIUM_REMOVAL",
    [0x1f]="$1f",
    [0x20]="$20",
    [0x21]="$21",
    [0x22]="$22",
    [0x23]="$23",
    [0x24]="$24",
    [0x25]="READ_CAPACITY",
    [0x26]="$26",
    [0x27]="$27",
    [0x28]="READ_10",
    [0x29]="$29",
    [0x2a]="WRITE_10",
    [0x2b]="$2b",
    [0x2c]="$2c",
    [0x2d]="$2d",
    [0x2e]="WRITE_AND_VERIFY_10",
    [0x2f]="$2f",
    [0x30]="$30",
    [0x31]="$31",
    [0x32]="$32",
    [0x33]="$33",
    [0x34]="$34",
    [0x35]="SYNCHRONIZE_CACHE",
    [0x36]="$36",
    [0x37]="$37",
    [0x38]="$38",
    [0x39]="COMPARE",
    [0x3a]="COPY_AND_VERIFY",
    [0x3b]="WRITE_BUFFER",
    [0x3c]="READ_BUFFER",
    [0x3d]="$3d",
    [0x3e]="$3e",
    [0x3f]="$3f",
    [0x40]="CHANGE_DEFINITION",
    [0x41]="$41",
    [0x42]="$42",
    [0x43]="READ_TOC",
    [0x44]="$44",
    [0x45]="$45",
    [0x46]="$46",
    [0x47]="$47",
    [0x48]="$48",
    [0x49]="$49",
    [0x4a]="$4a",
    [0x4b]="$4b",
    [0x4c]="LOG_SELECT",
    [0x4d]="LOG_SENSE",
    [0x4e]="$4e",
    [0x4f]="$4f",
    [0x50]="$50",
    [0x51]="READ_DISC_INFRMATION",
    [0x52]="READ_TRACK_INFRMATION",
    [0x53]="$53",
    [0x54]="$54",
    [0x55]="MODE_SELECT_10",
    [0x56]="$56",
    [0x57]="$57",
    [0x58]="$58",
    [0x59]="$59",
    [0x5a]="MODE_SENSE_10",
    [0x5b]="CLOSE_TRACK/SESSION",
    [0x5c]="$5c",
    [0x5d]="$5d",
    [0x5e]="$5e",
    [0x5f]="$5f",
    [0x60]="$60",
    [0x61]="$61",
    [0x62]="$62",
    [0x63]="$63",
    [0x64]="$64",
    [0x65]="$65",
    [0x66]="$66",
    [0x67]="$67",
    [0x68]="$68",
    [0x69]="$69",
    [0x6a]="$6a",
    [0x6b]="$6b",
    [0x6c]="$6c",
    [0x6d]="$6d",
    [0x6e]="$6e",
    [0x6f]="$6f",
    [0x70]="$70",
    [0x71]="$71",
    [0x72]="$72",
    [0x73]="$73",
    [0x74]="$74",
    [0x75]="$75",
    [0x76]="$76",
    [0x77]="$77",
    [0x78]="$78",
    [0x79]="$79",
    [0x7a]="$7a",
    [0x7b]="$7b",
    [0x7c]="$7c",
    [0x7d]="$7d",
    [0x7e]="$7e",
    [0x7f]="$7f",
    [0x80]="$80",
    [0x81]="$81",
    [0x82]="$82",
    [0x83]="$83",
    [0x84]="$84",
    [0x85]="$85",
    [0x86]="$86",
    [0x87]="$87",
    [0x88]="READ_16",
    [0x89]="$89",
    [0x8a]="WRITE_16",
    [0x8b]="$8b",
    [0x8c]="$8c",
    [0x8d]="$8d",
    [0x8e]="WRITE_AND_VERIFY_16",
    [0x8f]="$8f",
    [0x90]="$90",
    [0x91]="$91",
    [0x92]="$92",
    [0x93]="$93",
    [0x94]="$94",
    [0x95]="$95",
    [0x96]="$96",
    [0x97]="$97",
    [0x98]="$98",
    [0x99]="$99",
    [0x9a]="$9a",
    [0x9b]="$9b",
    [0x9c]="$9c",
    [0x9d]="$9d",
    [0x9e]="$9e",
    [0x9f]="$9f",
    [0xa0]="$a0",
    [0xa1]="BLANK",
    [0xa2]="$a2",
    [0xa3]="$a3",
    [0xa4]="$a4",
    [0xa5]="$a5",
    [0xa6]="$a6",
    [0xa7]="$a7",
    [0xa8]="READ_12",
    [0xa9]="$a9",
    [0xaa]="WRITE_12",
    [0xab]="$ab",
    [0xac]="$ac",
    [0xad]="$ad",
    [0xae]="$ae",
    [0xaf]="$af",
    [0xb0]="$b0",
    [0xb1]="$b1",
    [0xb2]="$b2",
    [0xb3]="$b3",
    [0xb4]="$b4",
    [0xb5]="$b5",
    [0xb6]="$b6",
    [0xb7]="$b7",
    [0xb8]="$b8",
    [0xb9]="$b9",
    [0xba]="$ba",
    [0xbb]="SET_CD_SPEED",
    [0xbc]="$bc",
    [0xbd]="$bd",
    [0xbe]="READ_CD",
    [0xbf]="$bf",
    [0xc0]="$c0",
    [0xc1]="$c1",
    [0xc2]="$c2",
    [0xc3]="$c3",
    [0xc4]="$c4",
    [0xc5]="$c5",
    [0xc6]="$c6",
    [0xc7]="$c7",
    [0xc8]="$c8",
    [0xc9]="$c9",
    [0xca]="$ca",
    [0xcb]="$cb",
    [0xcc]="$cc",
    [0xcd]="$cd",
    [0xce]="$ce",
    [0xcf]="$cf",
    [0xd0]="$d0",
    [0xd1]="$d1",
    [0xd2]="$d2",
    [0xd3]="$d3",
    [0xd4]="$d4",
    [0xd5]="$d5",
    [0xd6]="$d6",
    [0xd7]="$d7",
    [0xd8]="$d8",
    [0xd9]="$d9",
    [0xda]="$da",
    [0xdb]="$db",
    [0xdc]="$dc",
    [0xdd]="$dd",
    [0xde]="$de",
    [0xdf]="$df",
    [0xe0]="$e0",
    [0xe1]="$e1",
    [0xe2]="$e2",
    [0xe3]="$e3",
    [0xe4]="$e4",
    [0xe5]="$e5",
    [0xe6]="$e6",
    [0xe7]="$e7",
    [0xe8]="$e8",
    [0xe9]="$e9",
    [0xea]="$ea",
    [0xeb]="$eb",
    [0xec]="$ec",
    [0xed]="$ed",
    [0xee]="$ee",
    [0xef]="$ef",
    [0xf0]="$f0",
    [0xf1]="$f1",
    [0xf2]="$f2",
    [0xf3]="$f3",
    [0xf4]="$f4",
    [0xf5]="$f5",
    [0xf6]="$f6",
    [0xf7]="$f7",
    [0xf8]="$f8",
    [0xf9]="$f9",
    [0xfa]="$fa",
    [0xfb]="$fb",
    [0xfc]="$fc",
    [0xfd]="$fd",
    [0xfe]="$fe",
    [0xff]="$ff"
};
#endif

/*============================================================================*/
/*--- LOCAL CODE -------------------------------------------------------------*/
/*============================================================================*/
static LONG
sbp2_scsi_getmodepage(SBP2Unit *unit, UBYTE page)
{
    UBYTE cmd10[10];
    struct SCSICmd scsicmd;
    UBYTE sensedata[18];
    LONG ioerr;
    UBYTE *res;

    bzero(unit->u_ModePageBuf, 18);
    bzero(sensedata, sizeof(sensedata));

    scsicmd.scsi_Data = (UWORD *) unit->u_ModePageBuf;
    scsicmd.scsi_Length = 256;
    scsicmd.scsi_Command = cmd10;
    scsicmd.scsi_CmdLength = 10;
    scsicmd.scsi_Flags = SCSIF_READ|SCSIF_AUTOSENSE;
    scsicmd.scsi_SenseData = sensedata;
    scsicmd.scsi_SenseLength = 18;
    cmd10[0] = SCSI_MODE_SENSE_10;
    cmd10[1] = 0x08; /* DisableBlockDescriptor=1 */
    cmd10[2] = page; /* PageControl=current */
    cmd10[3] = 0;    /* Reserved */
    cmd10[4] = 0;    /* Reserved */
    cmd10[5] = 0;    /* Reserved */
    cmd10[6] = 0;    /* Reserved */
    cmd10[7] = 1;    /* Alloc length hi */
    cmd10[8] = 0;    /* Alloc length lo */
    cmd10[9] = 0;    /* Control */

    _INF("do SCSI_MODE_SENSE_10...\n");
    ioerr = sbp2_do_scsi_cmd(unit, &scsicmd, ORB_TIMEOUT);
    if (ioerr)
    {
        _ERR("MODE_SENSE_10: page $%02x failed, ioerr=%ld\n", page, ioerr);
        return -1;
    }

    res = unit->u_ModePageBuf;
    /* check total amount of data */
    if ((scsicmd.scsi_Actual < 10) || (scsicmd.scsi_Actual < (((UWORD *)res)[0]+2)))
    {
        _ERR("SCSI_MODE_SENSE($%02x) failed: not enought data (get %lu, need %lu)\n",
             page, scsicmd.scsi_Actual, ((UWORD *)res)[0]+2);
        return -1;
    }

    /* skip mode header, jump to page data */
    res += 7;
    scsicmd.scsi_Actual -= 7;

    /* check page code */
    if ((res[0] & 0x3f) != (page & 0x3f))
    {
        _ERR("SCSI_MODE_SENSE($%02x) failed: wrong page returned ($%02x)\n", page, res[0]);
        return -1;
    }

    /* check page length */
    if (scsicmd.scsi_Actual < res[1]+2)
    {
        _ERR("SCSI_MODE_SENSE($%02x) failed: incomplete page\n", page);
        return -1;
    }

    return 0;
}
static void
sbp2_fakegeometry(SBP2Unit *unit)
{
    _INF("Faking geometry...\n");
    unit->u_Geometry.dg_Heads = 1;
    unit->u_Geometry.dg_TrackSectors = 1;
    unit->u_Geometry.dg_CylSectors = 1;
    unit->u_Geometry.dg_Cylinders = unit->u_Geometry.dg_TotalSectors;
}


/*============================================================================*/
/*--- PUBLIC CODE ------------------------------------------------------------*/
/*============================================================================*/
LONG
sbp2_iocmd_scsi(SBP2Unit *unit, struct IOStdReq *ioreq)
{
    UBYTE cmd10[10];
    struct SCSICmd *cmd, scsicmd10;
    LONG ioerr=0;
    BOOL try_again=FALSE;
    UBYTE *sensedata=NULL;
    UBYTE *buf;

    if (ioreq->io_Length < sizeof(struct SCSICmd))
    {
        _ERR("IO: ioreq data length too small for a SCSI cmd\n");
        ioreq->io_Actual = 0;
        ioerr = IOERR_BADLENGTH;
        goto out;
    }

    cmd = (APTR)ioreq->io_Data;
    if ((NULL == cmd) || (NULL == cmd->scsi_Command) || (12 < cmd->scsi_CmdLength))
    {
        _ERR("IO: no SCSI cmd or unsupported SCSI command length: cmd=%p, len=%u\n",
             cmd->scsi_Command, cmd->scsi_CmdLength);
        ioreq->io_Actual = 0;
        ioerr = IOERR_NOCMD;
        goto out;
    }

    _INF("SCSI[$%02x-%s] Sending, SCSI=[CmdLen=%u, Data=%p, Len=%lu]\n",
               cmd->scsi_Command[0], scsi_cmd_names[cmd->scsi_Command[0]],
               cmd->scsi_CmdLength, cmd->scsi_Data, cmd->scsi_Length);

    /* Force IMMED bit to not fall into sbp2 status timeout */
    switch (cmd->scsi_Command[0])
    {
        case SCSI_CD_BLANK:
            cmd->scsi_Command[1] |= 0x10;
            break;

        case SCSI_CD_CLOSE_TRACK:
        case SCSI_CD_LOAD_UNLOAD_MEDIUM:
            cmd->scsi_Command[1] |= 0x01;
            break;

        case SCSI_CD_SYNCHRONIZE_CACHE:
            cmd->scsi_Command[1] |= 0x02;
            break;
    }

send_cmd:
    ioerr = sbp2_do_scsi_cmd(unit, cmd, ORB_TIMEOUT);
    if (!ioerr)
    {
        ioreq->io_Actual = ioreq->io_Length;
        _INF("SCSI[$%02x-%s] OK, IO=[actual=%lu], SCSI=[Actual=%lu]\n",
                   cmd->scsi_Command[0], scsi_cmd_names[cmd->scsi_Command[0]],
                   ioreq->io_Actual, cmd->scsi_Actual);
    }
    else
    {
        _ERR("SCSI[$%02x-%s] Failed, IO=[err=%ld, actual=%lu], SCSI=[Status=$%02x, Actual=%lu]\n",
                  cmd->scsi_Command[0], scsi_cmd_names[cmd->scsi_Command[0]],
                  ioerr, ioreq->io_Actual, cmd->scsi_Status, cmd->scsi_Actual);
        if (cmd->scsi_SenseActual > 13)
            _ERR("SCSI[$%02x-%s] Failed, SenseLen=%u, SenseData=[key=$%x, asc/ascq=$%02x/$%02x]\n",
                      cmd->scsi_Command[0], scsi_cmd_names[cmd->scsi_Command[0]],
                      cmd->scsi_SenseActual, cmd->scsi_SenseData[2] & SK_MASK,
                      cmd->scsi_SenseData[12], cmd->scsi_SenseData[13]);
        switch (cmd->scsi_CmdLength)
        {
            case 6:
                _ERR("SCSI cmd6 [%02x %02x %02x %02x %02x %02x]\n",
                          cmd->scsi_Command[0], cmd->scsi_Command[1], cmd->scsi_Command[2],
                          cmd->scsi_Command[3], cmd->scsi_Command[4], cmd->scsi_Command[5]);
                break;

            case 10:
                _ERR("SCSI cmd10 [%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x]\n",
                          cmd->scsi_Command[0], cmd->scsi_Command[1], cmd->scsi_Command[2],
                          cmd->scsi_Command[3], cmd->scsi_Command[4], cmd->scsi_Command[5],
                          cmd->scsi_Command[6], cmd->scsi_Command[7], cmd->scsi_Command[8],
                          cmd->scsi_Command[9]);
                break;

            case 12:
                _ERR("SCSI cmd12 [%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x]\n",
                          cmd->scsi_Command[0], cmd->scsi_Command[1], cmd->scsi_Command[2],
                          cmd->scsi_Command[3], cmd->scsi_Command[4], cmd->scsi_Command[5],
                          cmd->scsi_Command[6], cmd->scsi_Command[7], cmd->scsi_Command[8],
                          cmd->scsi_Command[9], cmd->scsi_Command[10], cmd->scsi_Command[11]);
                break;
        }

        if (!try_again)
        {
            if ((HFERR_BadStatus == ioerr) && (6 == cmd->scsi_CmdLength))
            {
                try_again = TRUE;

                CopyMem(cmd, &scsicmd10, sizeof(scsicmd10));

                switch (cmd->scsi_Command[0])
                {
                    case SCSI_MODE_SENSE_6:
                        cmd10[0] = SCSI_MODE_SENSE_10;
                        cmd10[1] = cmd->scsi_Command[1] & 0xf7;
                        cmd10[2] = cmd->scsi_Command[2];
                        cmd10[3] = 0;
                        cmd10[4] = 0;
                        cmd10[5] = 0;
                        cmd10[6] = 0;
                        if ((cmd->scsi_Command[4] > 251) &&
                            (cmd->scsi_Length == cmd->scsi_Command[4]))
                        {
                            cmd->scsi_Command[4] -= 4;
                            cmd->scsi_Length -= 4;
                        }
                        cmd10[7] = (cmd->scsi_Command[4]+4)>>8;
                        cmd10[8] = cmd->scsi_Command[4]+4;
                        cmd10[9] = cmd->scsi_Command[5];

                        sensedata = AllocVec(cmd->scsi_Length+4, MEMF_PUBLIC|MEMF_CLEAR);
                        if (NULL != sensedata)
                        {
                            scsicmd10.scsi_Length = cmd->scsi_Length+4;
                            scsicmd10.scsi_Data = (UWORD *) sensedata;
                        }
                        break;

                    case SCSI_MODE_SELECT_6:
                        cmd10[0] = SCSI_MODE_SELECT_10;
                        cmd10[1] = cmd->scsi_Command[1];
                        cmd10[2] = cmd->scsi_Command[2];
                        cmd10[3] = 0;
                        cmd10[4] = 0;
                        cmd10[5] = 0;
                        cmd10[6] = 0;
                        cmd10[7] = (cmd->scsi_Command[4]+4)>>8;
                        cmd10[8] = cmd->scsi_Command[4]+4;
                        cmd10[9] = cmd->scsi_Command[5];

                        sensedata = AllocVec(cmd->scsi_Length+4, MEMF_PUBLIC|MEMF_CLEAR);
                        if((NULL != sensedata) && (cmd->scsi_Length >= 4))
                        {
                            buf = (UBYTE *) cmd->scsi_Data;

                            sensedata[1] = *buf++;
                            sensedata[2] = *buf++;
                            sensedata[3] = *buf++;
                            sensedata[7] = *buf++;

                            scsicmd10.scsi_Length = cmd->scsi_Length+4;
                            scsicmd10.scsi_Data = (UWORD *) sensedata;

                            CopyMem(buf, &sensedata[8], cmd->scsi_Length-4);
                        }
                        break;

                    default:
                        try_again = FALSE;
                }

                if (try_again)
                {
                    _WRN("Xlate 6->10 cmd %s\n",
                          scsi_cmd_names[((struct SCSICmd *)ioreq->io_Data)->scsi_Command[0]]);

                    cmd = &scsicmd10;
                    cmd->scsi_Command = cmd10;
                    cmd->scsi_CmdLength = 10;
                    cmd->scsi_Status = 0;
                    cmd->scsi_Actual = 0;
                    cmd->scsi_SenseActual = 0;
                    goto send_cmd;
                }
            }
        }
    }

    if (try_again)
    {
        cmd = (APTR)ioreq->io_Data;

        if (NULL != sensedata)
        {
            cmd->scsi_Actual = 0;
            if(scsicmd10.scsi_Command[0] == SCSI_MODE_SENSE_10)
            {
                if(scsicmd10.scsi_Actual >= 8)
                {
                    cmd->scsi_Actual = scsicmd10.scsi_Actual - 4;
                    buf = (UBYTE *) cmd->scsi_Data;
                    *buf++ = sensedata[1];
                    *buf++ = sensedata[2];
                    *buf++ = sensedata[3];
                    *buf++ = sensedata[7];
                    CopyMem(&sensedata[8], buf, (ULONG) scsicmd10.scsi_Actual - 8);
                }
            }
            FreeVec(sensedata);
        }
        else
            cmd->scsi_Actual = scsicmd10.scsi_Actual;

        cmd->scsi_CmdActual = scsicmd10.scsi_CmdActual;
        cmd->scsi_Status = scsicmd10.scsi_Status;
        cmd->scsi_SenseActual = scsicmd10.scsi_SenseActual;
    }

out:
    ioreq->io_Error = ioerr;
    return ioerr;
}
LONG
sbp2_iocmd_start_stop(SBP2Unit *unit, struct IOStdReq *ioreq)
{
    UBYTE cmd6[6];
    struct SCSICmd scsicmd;
    UBYTE sensedata[18];

    _INF("IO: CMD_START/CMD_STOP\n");

    bzero(sensedata, sizeof(sensedata));
    bzero(cmd6, 6);
    ioreq->io_Actual = 0;

    scsicmd.scsi_Data = NULL;
    scsicmd.scsi_Length = 0;
    scsicmd.scsi_Command = cmd6;
    scsicmd.scsi_CmdLength = 6;
    scsicmd.scsi_Flags = SCSIF_AUTOSENSE;
    scsicmd.scsi_SenseData = sensedata;
    scsicmd.scsi_SenseLength = 18;

    cmd6[0] = SCSI_DA_START_STOP_UNIT;

    switch(ioreq->io_Command)
    {
        case CMD_START:
            cmd6[4] = 0x01;
            break;

        case CMD_STOP:
            cmd6[4] = 0x00;
            break;

        case TD_EJECT:
            cmd6[4] = ioreq->io_Length ? 0x03 : 0x02;
            break;
    }
    
    _INF("do SCSI_DA_START_STOP_UNIT...\n");
    ioreq->io_Error = sbp2_do_scsi_cmd(unit, &scsicmd, ORB_TIMEOUT);
    return ioreq->io_Error;
}
LONG
sbp2_iocmd_read64(SBP2Unit *unit, struct IOStdReq *ioreq)
{
    UBYTE cmd10[10];
    struct SCSICmd scsicmd;
    UBYTE sensedata[18];
    LONG err;

    ULONG datalen, dataremain = ioreq->io_Length;
    ULONG maxtrans = 1ul<<21; /* TODO: how to set the max transfer ? */
    ULONG insideblockoffset, startblock, dataoffset=0;
    UQUAD offset64;

    offset64 = ((UQUAD)ioreq->io_HighOffset<<32) + ioreq->io_LowOffset;

    if (0xff == unit->u_Read10Cmd)
    {
        ioreq->io_Actual = 0;
        ioreq->io_Error = IOERR_NOCMD;
        return ioreq->io_Error;
    }

    _INF("Flags=$%02x, Data=%p, Length=%lu, Offset=$%llx\n",
          ioreq->io_Flags, ioreq->io_Data, ioreq->io_Length, offset64);

    if(dataremain & 511)
    {
        _ERR("Attempt to read partial block (%lu %% %lu != 0)!",
             dataremain, unit->u_BlockSize);
        ioreq->io_Actual = 0;
        ioreq->io_Error = IOERR_BADLENGTH;
        return ioreq->io_Error;
    }

    if(ioreq->io_LowOffset & 511)
    {
        _ERR("Attempt to read unaligned block (%lu %% %lu != 0)!",
             ioreq->io_LowOffset, unit->u_BlockSize);
        ioreq->io_Actual = 0;
        ioreq->io_Error = IOERR_BADADDRESS;
        return ioreq->io_Error;
    }

    if (unit->u_OneBlockSize < unit->u_BlockSize)
    {
        if (NULL != unit->u_OneBlock)
            FreePooled(unit->u_Class->hc_MemPool, unit->u_OneBlock, unit->u_OneBlockSize);
        unit->u_OneBlock = AllocPooled(unit->u_Class->hc_MemPool, unit->u_BlockSize);
        if (NULL == unit->u_OneBlock)
        {
            _ERR("Alloc block failed\n");
            ioreq->io_Actual = 0;
            ioreq->io_Error = IOERR_NOMEMORY;
            return ioreq->io_Error;
        }

        unit->u_OneBlockSize = unit->u_BlockSize;
    }

    /* This is the idea of operation:
     * the READ_10 scsi command permits to transfer zero or more blocks.
     * So we need to give the address of the start block and a block count.
     * Now the IO requests a buffer not aligned on block and of any size.
     * So we're going to split this buffer into block transfer and handles
     * non read used border data by doing mem copy from a temporary buffer.
     *
     * As the transfer length is a number of block, given as a 16-bits value,
     * we can't read more than 65,535 blocks per READ_10 command.
     * Most of Direct-Access Block Devices support a block length of 512 bytes.
     * That gives 33,553,920 bytes! Unfortunatly most of these devices doesn't
     * support this number of bytes per command.
     * That's why we also limit the total number of transfered bytes.
     *
     * Other thing: even if offset is a 64bit value, as the max logical blocks
     * is a 32bits value, so startblock is also a 32bit value.
     */

    startblock = offset64 >> unit->u_BlockShift;
    insideblockoffset = (ioreq->io_LowOffset & ((1ul << unit->u_BlockShift)-1));
    while (dataremain)
    {
        /* Limit transfer size */
        datalen = MIN(dataremain, maxtrans);

        _INF("Reading %lu bytes from block %ld, %ld bytes left...\n", datalen, startblock, dataremain);

        /* if offset is not block aligned or xfer size is less than a block size */
        if (insideblockoffset || (datalen < unit->u_BlockSize))
        {
            /* now limit xfert size to the block size */
            if (datalen > unit->u_BlockSize - insideblockoffset)
                datalen = unit->u_BlockSize - insideblockoffset;

            scsicmd.scsi_Data = (UWORD *) unit->u_OneBlock;
            scsicmd.scsi_Length = unit->u_BlockSize;
            scsicmd.scsi_Command = cmd10;
            scsicmd.scsi_CmdLength = 10;
            scsicmd.scsi_Flags = SCSIF_READ|SCSIF_AUTOSENSE;
            scsicmd.scsi_SenseData = sensedata;
            scsicmd.scsi_SenseLength = 18;
            cmd10[0] = SCSI_DA_READ_10;
            cmd10[1] = 0;
            *((ULONG *) (&cmd10[2])) = LE_SWAPLONG(startblock);
            cmd10[6] = 0;
            cmd10[7] = 0;
            cmd10[8] = 1;
            cmd10[9] = 0;

            err = sbp2_do_scsi_cmd(unit, &scsicmd, ORB_TIMEOUT);
            if (err)
            {
                ioreq->io_Error = err;
                goto out;
            }

            if (((ULONG)&unit->u_OneBlock[insideblockoffset] & 3) ||
                ((ULONG)&(((UBYTE *) ioreq->io_Data)[dataoffset]) & 3) ||
                (datalen & 3))
                _WRN("CopyMemQuick() will fails\n");
            CopyMemQuick(&unit->u_OneBlock[insideblockoffset],
                         &(((UBYTE *) ioreq->io_Data)[dataoffset]),
                         datalen);
            insideblockoffset = 0;
            startblock++;
        }
        else
        {
            scsicmd.scsi_Data = (UWORD *) &(((UBYTE *) ioreq->io_Data)[dataoffset]);
            scsicmd.scsi_Length = datalen;
            scsicmd.scsi_Command = cmd10;
            scsicmd.scsi_CmdLength = 10;
            scsicmd.scsi_Flags = SCSIF_READ|SCSIF_AUTOSENSE;
            scsicmd.scsi_SenseData = sensedata;
            scsicmd.scsi_SenseLength = 18;
            cmd10[0] = SCSI_DA_READ_10;
            cmd10[1] = 0;
            *((ULONG *) (&cmd10[2])) = LE_SWAPLONG(startblock);
            cmd10[6] = 0;
            cmd10[7] = datalen >> (unit->u_BlockShift+8);
            cmd10[8] = datalen >> unit->u_BlockShift;
            cmd10[9] = 0;

            err = sbp2_do_scsi_cmd(unit, &scsicmd, ORB_TIMEOUT);
            if (err)
            {
                ioreq->io_Error = err;
                goto out;
            }

            startblock += (insideblockoffset+datalen) >> unit->u_BlockShift;
        }

        dataoffset += datalen;
        dataremain -= datalen;
    }

out:
    ioreq->io_Actual = dataoffset;
    return ioreq->io_Error;
}
LONG
sbp2_iocmd_write64(SBP2Unit *unit, struct IOStdReq *ioreq)
{
    UBYTE cmd10[10];
    struct SCSICmd scsicmd;
    UBYTE sensedata[18];
    LONG err;

    ULONG datalen, dataremain = ioreq->io_Length;
    ULONG maxtrans = 1ul<<21; /* TODO: how to set the max transfer ? */
    ULONG insideblockoffset, startblock, dataoffset=0;
    UQUAD offset64;

    offset64 = ((UQUAD)ioreq->io_HighOffset<<32) + ioreq->io_LowOffset;

    if (unit->u_Flags.WriteProtected)
    {
        ioreq->io_Actual = 0;
        ioreq->io_Error = IOERR_NOCMD;
        return ioreq->io_Error;
    }

    _INF("Flags=$%02x, Data=%p, Length=%lu, Offset=$%llx\n",
          ioreq->io_Flags, ioreq->io_Data, ioreq->io_Length, offset64);

    if(dataremain & 511)
    {
        _ERR("Attempt to write partial block (%lu %% %lu != 0)!",
             dataremain, unit->u_BlockSize);
        ioreq->io_Actual = 0;
        ioreq->io_Error = IOERR_BADLENGTH;
        return ioreq->io_Error;
    }

    if(ioreq->io_LowOffset & 511)
    {
        _ERR("Attempt to write unaligned block (%lu %% %lu != 0)!",
             ioreq->io_LowOffset, unit->u_BlockSize);
        ioreq->io_Actual = 0;
        ioreq->io_Error = IOERR_BADADDRESS;
        return ioreq->io_Error;
    }

    if (unit->u_OneBlockSize < unit->u_BlockSize)
    {
        if (NULL != unit->u_OneBlock)
            FreePooled(unit->u_Class->hc_MemPool, unit->u_OneBlock, unit->u_OneBlockSize);
        unit->u_OneBlock = AllocPooled(unit->u_Class->hc_MemPool, unit->u_BlockSize);
        if (NULL == unit->u_OneBlock)
        {
            _ERR("Alloc block failed\n");
            ioreq->io_Actual = 0;
            ioreq->io_Error = IOERR_NOMEMORY;
            return ioreq->io_Error;
        }

        unit->u_OneBlockSize = unit->u_BlockSize;
    }

    /* Same remarks as READ64 */

    startblock = offset64 >> unit->u_BlockShift;
    insideblockoffset = (ioreq->io_LowOffset & ((1ul << unit->u_BlockShift)-1));
    while (dataremain)
    {
        /* Limit transfer size */
        datalen = MIN(dataremain, maxtrans);

        _INF("Writing %lu bytes from block %ld, %ld bytes left...\n", datalen, startblock, dataremain);

        /* if offset is not block aligned or xfer size is less than a block size */
        if (insideblockoffset || (datalen < unit->u_BlockSize))
        {
            /* now limit xfert size to the block size */
            if (datalen > unit->u_BlockSize - insideblockoffset)
                datalen = unit->u_BlockSize - insideblockoffset;

            scsicmd.scsi_Data = (UWORD *) unit->u_OneBlock;
            scsicmd.scsi_Length = unit->u_BlockSize;
            scsicmd.scsi_Command = cmd10;
            scsicmd.scsi_CmdLength = 10;
            scsicmd.scsi_Flags = SCSIF_READ|SCSIF_AUTOSENSE;
            scsicmd.scsi_SenseData = sensedata;
            scsicmd.scsi_SenseLength = 18;
            cmd10[0] = SCSI_DA_READ_10;
            cmd10[1] = 0;
            *((ULONG *) (&cmd10[2])) = LE_SWAPLONG(startblock);
            cmd10[6] = 0;
            cmd10[7] = 0;
            cmd10[8] = 1;
            cmd10[9] = 0;

            err = sbp2_do_scsi_cmd(unit, &scsicmd, ORB_TIMEOUT);
            if (err)
            {
                ioreq->io_Error = err;
                goto out;
            }

            CopyMemQuick(&(((UBYTE *) ioreq->io_Data)[dataoffset]),
                         &unit->u_OneBlock[insideblockoffset],
                         datalen);

            //scsicmd.scsi_Data = (UWORD *) unit->u_OneBlock;
            //scsicmd.scsi_Length = unit->u_BlockSize;
            //scsicmd.scsi_Command = cmd10;
            //scsicmd.scsi_CmdLength = 10;
            scsicmd.scsi_Flags = SCSIF_WRITE|SCSIF_AUTOSENSE;
            //scsicmd.scsi_SenseData = sensedata;
            //scsicmd.scsi_SenseLength = 18;
            cmd10[0] = SCSI_DA_WRITE_10;
            //cmd10[1] = 0;
            //*((ULONG *) (&cmd10[2])) = LE_SWAPLONG(startblock);
            //cmd10[6] = 0;
            //cmd10[7] = 0;
            //cmd10[8] = 1;
            //cmd10[9] = 0;

            err = sbp2_do_scsi_cmd(unit, &scsicmd, ORB_TIMEOUT);
            if (err)
            {
                ioreq->io_Error = err;
                goto out;
            }

            insideblockoffset = 0;
            startblock++;
        }
        else
        {
            scsicmd.scsi_Data = (UWORD *) &(((UBYTE *) ioreq->io_Data)[dataoffset]);
            scsicmd.scsi_Length = datalen;
            scsicmd.scsi_Command = cmd10;
            scsicmd.scsi_CmdLength = 10;
            scsicmd.scsi_Flags = SCSIF_WRITE|SCSIF_AUTOSENSE;
            scsicmd.scsi_SenseData = sensedata;
            scsicmd.scsi_SenseLength = 18;
            cmd10[0] = SCSI_DA_WRITE_10;
            cmd10[1] = 0;
            *((ULONG *) (&cmd10[2])) = LE_SWAPLONG(startblock);
            cmd10[6] = 0;
            cmd10[7] = datalen >> (unit->u_BlockShift+8);
            cmd10[8] = datalen >> unit->u_BlockShift;
            cmd10[9] = 0;

            err = sbp2_do_scsi_cmd(unit, &scsicmd, ORB_TIMEOUT);
            if (err)
            {
                ioreq->io_Error = err;
                goto out;
            }

            startblock += (insideblockoffset+datalen) >> unit->u_BlockShift;
        }

        dataoffset += datalen;
        dataremain -= datalen;
    }

out:
    ioreq->io_Actual = dataoffset;
    return ioreq->io_Error;
}
LONG
sbp2_iocmd_get_geometry(SBP2Unit *unit, struct IOStdReq *ioreq)
{
    struct DriveGeometry *dg;
    ULONG length;
    //ULONG tmpval;
    BOOL gotblks = FALSE;
    BOOL gotcyl = FALSE;
    BOOL gotheads = FALSE;
    BOOL gotsect = FALSE;
    BOOL gotcylsect = FALSE;

    _INF("IO: TD_GETGEOMETRY\n");

    ioreq->io_Actual = 0;
    ioreq->io_Error = 0;

    dg = (struct DriveGeometry *) ioreq->io_Data;
    if(NULL == dg)
    {
        _ERR("NULL io_Data\n");
        ioreq->io_Error = TDERR_NotSpecified;
        return ioreq->io_Error;
    }

    length = MIN(ioreq->io_Length, sizeof(struct DriveGeometry));

    if (unit->u_Geometry.dg_Cylinders > 0)
        goto geo_ok;

#if 0
    if (sbp2_scsi_read_capacity(unit, dev))
    {
        _ERR("ReadCapacity failed\n");
        ioreq->io_Error = TDERR_NotSpecified;
        return ioreq->io_Error;
    }
#endif

    if (0 == unit->u_BlockSize)
    {
        _ERR("Zero BlockSize!\n");
        ioreq->io_Error = TDERR_NotSpecified;
        return ioreq->io_Error;
    }

    gotblks = TRUE;

#if 0
    if (!sbp2_scsi_getmodepage(unit, 0x03))
    {
        tmpval = (unit->u_ModePageBuf[10] << 8) + unit->u_ModePageBuf[11];
        if (tmpval)
        {
            unit->u_Geometry.dg_TrackSectors = tmpval;
            gotsect = TRUE;

            if (!unit->u_Geometry.dg_Cylinders)
                unit->u_Geometry.dg_Cylinders = unit->u_Geometry.dg_TotalSectors;
        }
    }

    if (!sbp2_scsi_getmodepage(unit, 0x04))
    {
        tmpval = (unit->u_ModePageBuf[2] << 16) + (unit->u_ModePageBuf[3] << 8) + unit->u_ModePageBuf[4];
        if (tmpval)
        {
            unit->u_Geometry.dg_Cylinders = tmpval;
            unit->u_Geometry.dg_Heads = unit->u_ModePageBuf[5];
            gotcyl = gotheads = TRUE;
        }
    }

    if ((PDT_CDROM != unit->u_DeviceType) && (PDT_WORM != unit->u_DeviceType))
    {
        /* CD parameter page */
        if (!sbp2_scsi_getmodepage(unit, 0x05))
        {
            tmpval = (unit->u_ModePageBuf[4] << 8) + unit->u_ModePageBuf[5];
            if (tmpval)
            {
                unit->u_Geometry.dg_Cylinders = tmpval;
                unit->u_Geometry.dg_Heads = unit->u_ModePageBuf[4];
                unit->u_Geometry.dg_TrackSectors = unit->u_ModePageBuf[5];

                gotcyl = gotheads = gotsect = TRUE;

                if (!gotblks)
                {
                    unit->u_BlockSize = unit->u_Geometry.dg_SectorSize = (unit->u_ModePageBuf[6] << 8) + unit->u_ModePageBuf[7];
                    unit->u_BlockShift = 0;
                    while ((1ul << unit->u_BlockShift) < unit->u_BlockSize)
                        unit->u_BlockShift++;

                    _ERR("Capacity: %lu blocks of %lu bytes\n", unit->u_Geometry.dg_TotalSectors, unit->u_BlockSize);
                }
                else if (unit->u_BlockSize != (unit->u_ModePageBuf[6] <<8 ) + unit->u_ModePageBuf[7])
                    _ERR("Inconsistent block size information %lu != %lu!\n",
                         unit->u_BlockSize, (unit->u_ModePageBuf[6] << 8) + unit->u_ModePageBuf[7]);
            }
        }
    }
#endif

    _INF("Capacity (temp): Cylinders=%lu, Heads=%lu, TrackSectors=%u\n",
           unit->u_Geometry.dg_Cylinders, unit->u_Geometry.dg_Heads,
           unit->u_Geometry.dg_TrackSectors);

    /* missing total sectors? */
    if ((!gotblks) && gotcyl && gotheads && gotsect)
    {
        unit->u_Geometry.dg_TotalSectors = unit->u_Geometry.dg_Cylinders * unit->u_Geometry.dg_Heads * unit->u_Geometry.dg_TrackSectors;
        gotblks = TRUE;
    }

    /* missing cylinders? */
    if (gotblks && (!gotcyl) && gotheads && gotsect)
    {
        unit->u_Geometry.dg_Cylinders = unit->u_Geometry.dg_TotalSectors / (unit->u_Geometry.dg_Heads * unit->u_Geometry.dg_TrackSectors);
        gotcyl = TRUE;
    }

    /* missing heads? */
    if (gotblks && gotcyl && (!gotheads) && gotsect)
    {
        unit->u_Geometry.dg_Heads = unit->u_Geometry.dg_TotalSectors / (unit->u_Geometry.dg_Cylinders * unit->u_Geometry.dg_TrackSectors);
        gotheads = TRUE;
    }

    /* missing tracks per sector */
    if (gotblks && gotcyl && gotheads && (!gotsect))
    {
        unit->u_Geometry.dg_TrackSectors = unit->u_Geometry.dg_TotalSectors / (unit->u_Geometry.dg_Cylinders * unit->u_Geometry.dg_Heads);
        gotsect = TRUE;
    }

    /* some devices report these bogus values regardless of actual device capacity,
     * though the total number of blocks is correct.
     */
    if (((unit->u_Geometry.dg_Cylinders == 500) && (unit->u_Geometry.dg_TrackSectors == 32) && (unit->u_Geometry.dg_Heads == 8)) ||
       ((unit->u_Geometry.dg_Cylinders == 16383) && (unit->u_Geometry.dg_TrackSectors == 63) && (unit->u_Geometry.dg_Heads == 16)))
    {
        _ERR("Firmware returns known bogus geometry, will fall back to faked geometry!\n");
        gotheads = gotcyl = gotsect = FALSE;
    }

    /* missing more than one? */
    if (gotblks && !(gotheads && gotcyl && gotsect))
        sbp2_fakegeometry(unit);

    if (!gotcylsect)
        unit->u_Geometry.dg_CylSectors = unit->u_Geometry.dg_TrackSectors * unit->u_Geometry.dg_Heads;

    if (unit->u_DeviceType == PDT_SIMPLE_DIRECT_ACCESS)
        unit->u_Geometry.dg_DeviceType = DG_DIRECT_ACCESS;
    else if (unit->u_DeviceType < 10)
        unit->u_Geometry.dg_DeviceType = unit->u_DeviceType;
    else
        unit->u_Geometry.dg_DeviceType = DG_UNKNOWN;

    unit->u_Geometry.dg_Flags = unit->u_Flags.Removable ? DGF_REMOVABLE : 0;
    unit->u_Geometry.dg_BufMemType = MEMF_PUBLIC;
    unit->u_Geometry.dg_Reserved = 0;

geo_ok:
    if (unit->u_Geometry.dg_Cylinders * unit->u_Geometry.dg_CylSectors != unit->u_Geometry.dg_TotalSectors)
        _INF("Estimated Geometry yields %lu less total blocks %lu: Cylinders=%lu, CylSectors=%lu, Heads=%lu, TrackSectors=%lu, Blocks=%lu\n",
              unit->u_Geometry.dg_TotalSectors - unit->u_Geometry.dg_Cylinders * unit->u_Geometry.dg_CylSectors,
              unit->u_Geometry.dg_Cylinders * unit->u_Geometry.dg_CylSectors,
              unit->u_Geometry.dg_Cylinders, unit->u_Geometry.dg_CylSectors,
              unit->u_Geometry.dg_Heads, unit->u_Geometry.dg_TrackSectors,
              unit->u_Geometry.dg_TotalSectors);
    else
        _INF("Capacity (final): Cylinders=%lu, CylSectors=%lu, Heads=%lu, TrackSectors=%lu, Blocks=%lu, SectorSize=%lu\n",
              unit->u_Geometry.dg_Cylinders, unit->u_Geometry.dg_CylSectors,
              unit->u_Geometry.dg_Heads, unit->u_Geometry.dg_TrackSectors,
              unit->u_Geometry.dg_TotalSectors, unit->u_Geometry.dg_SectorSize);

    CopyMem(&unit->u_Geometry, dg, length);
    ioreq->io_Actual = length;

    return ioreq->io_Error;
}
/*=== EOF ====================================================================*/