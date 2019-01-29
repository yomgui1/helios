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
** Helios AV/C 1394 Library core file.
**
*/

#include <avc1394_version.h>

#include "avc1394_libdata.h"
#include "libraries/avc1394.h"
#include "avc1394_log.h"

#include <dos/dostags.h>
#include <proto/dos.h>

#define DEBUG_INIT(x)       DB x;
#define DEBUG_OPEN(x)       DB x;
#define DEBUG_CLOSE(x)      DB x;
#define DEBUG_EXPUNGE(x)    DB x;
#define DEBUG_NULL(x)       DB x;

extern ULONG LibFuncTable[];
extern void avc1394_Server(void);

struct AVC1394Library * AVC1394Base = NULL;
struct ExecBase *   SysBase = NULL;
struct DosLibrary * DOSBase = NULL;
struct Library *    HeliosBase = NULL;

struct Library* LIB_Init(struct AVC1394Library *    MyLibBase,
                         BPTR                       SegList,
                         struct ExecBase *          SBase);

struct LibInitStruct
{
    ULONG   LibSize;
    APTR    FuncTable;
    APTR    DataTable;
    void    (*InitFunc)(void);
};

struct LibInitStruct LibInitStruct=
{
    sizeof(struct AVC1394Library),
    LibFuncTable,
    NULL,
    (void (*)(void)) &LIB_Init
};


struct Resident LibResident=
{
    RTC_MATCHWORD,
    &LibResident,
    &LibResident + 1,
    RTF_PPC | RTF_EXTENDED | RTF_AUTOINIT,
    VERSION,
    NT_LIBRARY,
    0,
    "avc1394.library",
    VSTRING,
    &LibInitStruct,
    /* New Fields */
    REVISION,
    NULL            /* No More Tags for now*/
};

/*
 * To tell the loader that this is a new abox elf and not
 * one for the ppc.library.
 * ** IMPORTANT **
 */
ULONG __abox__ = 1;

/*------------------ PRIVATE CODE SECTION -------------------------*/

static void SysError_NeedLibrary(STRPTR libname, ULONG version)
{
    // TODO
}

static APTR avc1394_RunServer(void)
{
    struct Process *proc;
    struct MsgPort *port;

    proc = CreateNewProcTags(NP_CodeType,    CODETYPE_PPC,
                             NP_Entry,       (ULONG) avc1394_Server,
                             NP_TaskMsgPort, (ULONG) &port,
                             NP_Name,        (ULONG) "AVC1394 [FCP Serveur]",
                             TAG_DONE);
    if (NULL != proc)
        return proc;
    else
        log_Error("Failed to create the FCP server process");

    return NULL;
}

static void avc1394_StopServer(void)
{
    AVC1394_ServerMsg msg;
    struct MsgPort *port, *sv_port;

    port = CreateMsgPort();
    if (NULL != port) {
        msg.SysMsg.mn_Node.ln_Type = NT_MESSAGE;
        msg.SysMsg.mn_ReplyPort = port;
        msg.SysMsg.mn_Length = sizeof(msg);

        msg.Error  = AVC1394_ERR_BYE;

        ObtainSemaphore(&AVC1394Base->Sema);
        sv_port = AVC1394Base->ServerPort;
        if (NULL != sv_port) {
            PutMsg(sv_port, (APTR) &msg);
            AVC1394Base->ServerPort = NULL;
        }
        ReleaseSemaphore(&AVC1394Base->Sema);

        if (NULL != sv_port) {
            WaitPort(port);
            GetMsg(port);
        } else
            log_Error("Server already dead");

        DeleteMsgPort(port);
    } else
        log_Error("Can't allocate a msg port for the dead message");

    return;
}

static ULONG LibExpunge(struct AVC1394Library *AVC1394Library)
{
    BPTR MySegment;

    DEBUG_EXPUNGE(("LIB_Expunge: LibBase 0x%p <%s> OpenCount %ld\n",
                   AVC1394Library,
                   AVC1394Library->Lib.lib_Node.ln_Name,
                   AVC1394Library->Lib.lib_OpenCnt));

    MySegment = AVC1394Library->SegList;

    if (AVC1394Library->Lib.lib_OpenCnt) {
        DEBUG_EXPUNGE(("LIB_Expunge: set LIBF_DELEXP\n"));
        AVC1394Library->Lib.lib_Flags |= LIBF_DELEXP;
        return NULL;
    }

    /* We don't need a forbid() because Expunge and Close
     * are called with a pending forbid.
     * But let's do it for safety if somebody does it by hand.
     */
    Forbid();
    DEBUG_EXPUNGE(("LIB_Expunge: remove the library\n"));
    Remove(&AVC1394Library->Lib.lib_Node);
    Permit();

    avc1394_StopServer();
    CloseLibrary(HeliosBase);
    CloseLibrary((struct Library *) DOSBase);

    DEBUG_EXPUNGE(("LIB_Expunge: free the library\n"));
    FreeMem((APTR)((ULONG)(AVC1394Library) - (ULONG)(AVC1394Library->Lib.lib_NegSize)),
            AVC1394Library->Lib.lib_NegSize + AVC1394Library->Lib.lib_PosSize);

    DEBUG_EXPUNGE(("LIB_Expunge: return Segment 0x%lx to ramlib\n",
                   MySegment));
    return (ULONG) MySegment;
}


/*------------------ LIBRARY CODE SECTION -------------------------*/

// The lib execute init protection which simply returns
LONG NoExecute(void)
{
    return -1;
}

struct Library* LIB_Init(struct AVC1394Library * MyLibBase,
                         BPTR                SegList,
                         struct ExecBase *   sBase)
{
    DEBUG_INIT(("LibInit: LibBase 0x%p SegList 0x%lx SysBase 0x%p\n",
                MyLibBase, SegList, sBase));

    AVC1394Base = MyLibBase;
    MyLibBase->SegList   = SegList;
    MyLibBase->MySysBase = SysBase = sBase;

    /* Open needed resources */
    DOSBase = (struct DosLibrary *) OpenLibrary("dos.library", 0);
    if (NULL != DOSBase) {
        HeliosBase = OpenLibrary("helios.library", 0);
        if (NULL != HeliosBase) {
            InitSemaphore(&MyLibBase->Sema);
                
            MyLibBase->ServerPort = NULL;
            MyLibBase->HeliosBase = HeliosBase;

            if (NULL != avc1394_RunServer())
                return &MyLibBase->Lib;
        }

        CloseLibrary((struct Library *)DOSBase);
    } else
        SysError_NeedLibrary("dos.library", 0);

    FreeMem((APTR)((ULONG)(MyLibBase) - (ULONG)(MyLibBase->Lib.lib_NegSize)),
            MyLibBase->Lib.lib_NegSize + MyLibBase->Lib.lib_PosSize);
    return NULL;
}

ULONG LIB_Expunge(void)
{
    struct AVC1394Library *AVC1394Library = (struct AVC1394Library *) REG_A6;

    DEBUG_EXPUNGE(("LIB_Expunge:\n"));
    return LibExpunge(AVC1394Library);
}

struct Library* LIB_Open(void)
{
    struct AVC1394Library *AVC1394Library = (struct AVC1394Library *) REG_A6;

    DEBUG_OPEN(("LIB_Open: 0x%p <%s> OpenCount %ld\n",
                AVC1394Library,
                AVC1394Library->Lib.lib_Node.ln_Name,
                AVC1394Library->Lib.lib_OpenCnt));

    AVC1394Library->Lib.lib_Flags &= ~LIBF_DELEXP;
    AVC1394Library->Lib.lib_OpenCnt++;
    return &AVC1394Library->Lib;
}

ULONG LIB_Close(void)
{
    struct AVC1394Library *AVC1394Library = (struct AVC1394Library *) REG_A6;

    DEBUG_CLOSE(("LIB_Close: 0x%p <%s> OpenCount %ld\n",
                 AVC1394Library,
                 AVC1394Library->Lib.lib_Node.ln_Name,
                 AVC1394Library->Lib.lib_OpenCnt));

    if ((--AVC1394Library->Lib.lib_OpenCnt) > 0) {
        DEBUG_CLOSE(("LIB_Close: done\n"));
    } else {
        if (AVC1394Library->Lib.lib_Flags & LIBF_DELEXP) {
            DEBUG_CLOSE(("LIB_Close: LIBF_DELEXP set\n"));
            return LibExpunge(AVC1394Library);
        }
    }
    return 0;
}

ULONG LIB_Reserved(void)
{
    DEBUG_NULL(("LIB_Reserved:\n"));

    return 0;
}


