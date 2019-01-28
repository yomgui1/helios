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
*/

#include <dos/dos.h>
#include <exec/system.h>
#include <exec/errors.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/alib.h>

#include <devices/scsidisk.h>
#include <devices/trackdisk.h>
#include <scsi/commands.h>
#include <scsi/values.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void __chkabort(void) {};

int main(int argc, char **argv)
{
    struct MsgPort *port;
    int res = RETURN_ERROR;

    if (argc >= 8)
    {
        UBYTE cmd[12];
        ULONG len, i, flags = 0;

        argc--;
        argv++;

        bzero(cmd, 12);

        len = strtoul(argv[0], NULL, 0);
        if ((len != 6) && (len != 10) && (len != 12))
        {
            printf("command length shall be 6, 10 or 12, not %lu\n", len);
            return RETURN_ERROR;
        }

        argc--;
        argv++;
        if (argc < len)
        {
            printf("not enough parameters\n");
            return RETURN_ERROR;
        }

        if (argc > len)
            flags = strtoul(argv[len], NULL, 0)  & (~SCSIF_AUTOSENSE);

        printf("Sending SCSI cmd%lu, flags=$%lx:\n", len, flags);
        for (i=0; i<len; i++)
        {
            cmd[i] = strtoul(argv[i], NULL, 16);
            printf(" %02x", cmd[i]);
        }
        putchar('\n');

        port = CreateMsgPort();
        if (NULL != port)
        {
            struct IOStdReq *ioreq;

            ioreq = (APTR) CreateExtIO(port, sizeof(*ioreq));
            if (NULL != ioreq)
            {
                if (!OpenDevice("sbp2.device", 0, (struct IORequest *)ioreq, 0))
                {
                    ULONG sigs;
                    struct SCSICmd scsicmd;
                    ULONG response_buf[256/4] __attribute__((__aligned__(32)));
                    UBYTE sense_buf[18];

                    bzero(response_buf, sizeof(response_buf));
                    bzero(sense_buf, sizeof(sense_buf));

                    ioreq->io_Command = HD_SCSICMD;
                    ioreq->io_Data = &scsicmd;
                    ioreq->io_Length = sizeof(scsicmd);

                    switch (cmd[0])
                    {
                        case SCSI_DA_START_STOP_UNIT:
                        case SCSI_DA_TEST_UNIT_READY:
                            scsicmd.scsi_Data = NULL;
                            scsicmd.scsi_Length = 0;
                            flags = 0;
                            break;

                        case SCSI_DA_READ_CAPACITY:
                            scsicmd.scsi_Data = (UWORD *)response_buf;
                            scsicmd.scsi_Length = 8;
                            flags = SCSIF_READ;
                            break;

                        default:
                            if (flags & SCSIF_READ)
                            {
                                printf("with read data\n");
                                scsicmd.scsi_Data = (UWORD *)response_buf;
                                scsicmd.scsi_Length = 256;
                            }
                            else
                            {
                                scsicmd.scsi_Data = NULL;
                                scsicmd.scsi_Length = 0;
                            }
                    }

                    scsicmd.scsi_Command = cmd;
                    scsicmd.scsi_CmdLength = len;
                    scsicmd.scsi_Flags = flags | SCSIF_AUTOSENSE;
                    scsicmd.scsi_SenseData = sense_buf;
                    scsicmd.scsi_SenseLength = sizeof(sense_buf);

                    printf("Sending...\n");
                    SendIO((struct IORequest *)ioreq);
                    sigs = Wait((1ul << port->mp_SigBit) | SIGBREAKF_CTRL_C);
                    if (0 == (sigs & (1ul << port->mp_SigBit)))
                    {
                        if (!CheckIO((struct IORequest *)ioreq))
                        {
                            printf("** CTRL-C **\nAborting the IO...\n");
                            AbortIO((struct IORequest *)ioreq);
                            WaitIO((struct IORequest *)ioreq);
                        }

                        printf("Command Aborted\n");
                        res = RETURN_FAIL;
                    }
                    else if (0 == ioreq->io_Error)
                    {
                        if (NULL !=scsicmd.scsi_Data)
                        {
                            printf("Dumping results, Status=%u, Actual=%lu:\n",
                                   scsicmd.scsi_Status, scsicmd.scsi_Actual);
                            for (i=0; i<(256/4);)
                            {
                                LONG k;

                                for (k=0; k<4; k++, i++)
                                    printf("%08lx%s", response_buf[i], k<3?" ":"\n");
                            }
                            printf("\nSense Data result: %u\n", scsicmd.scsi_SenseActual);
                        }
                        else
                            printf("done\n");

                        res = RETURN_OK;
                    }
                    else
                    {
                        printf("Command resulted with error %d\n", ioreq->io_Error);
                        res = RETURN_FAIL;
                    }

                    CloseDevice((struct IORequest *)ioreq);
                }
                else
                    printf("OpenDevice(\"sbp2.device\") failed\n");

                DeleteExtIO((struct IORequest *)ioreq);
            }
        
            DeleteMsgPort(port);
        }
        else
            printf("Failed to create a new MsgPort structure\n");
    }

    return res;
}
