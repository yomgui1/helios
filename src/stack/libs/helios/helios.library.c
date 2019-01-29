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
** Helios Library core file.
**
*/

//#define NDEBUG

#include "fw_libversion.h"

#include "private.h"
#include "clib/helios_protos.h"
#include <proto/intuition.h>

extern ULONG LibFuncTable[]; /* defined in helios_functable.library.c */

struct HeliosBase * HeliosBase = NULL;
struct ExecBase *   SysBase = NULL;
struct DosLibrary * DOSBase = NULL;
struct Library *    UtilityBase = NULL;

struct Library* LIB_Init(struct HeliosBase *    MyLibBase,
                         BPTR                   SegList,
                         struct ExecBase *      SBase);

struct LibInitStruct
{
    ULONG   LibSize;
    APTR    FuncTable;
    APTR    DataTable;
    void    (*InitFunc)(void);
};

struct LibInitStruct LibInitStruct=
{
    sizeof(struct HeliosBase),
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
    LIBNAME,
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
const STRPTR version = VERSTAG;

/*------------------ PRIVATE CODE SECTION -------------------------*/

static void SysError_NeedLibrary(STRPTR libname, ULONG version)
{
    _ERR("OpenLibrary(\"%s\", %lu) failed\n", libname, version);
}

static ULONG LibExpunge(struct HeliosBase *base)
{
    BPTR MySegment;

    _INFO("Base %p, OpenCount %ld\n",base, base->hb_Lib.lib_OpenCnt);

    if (base->hb_Lib.lib_OpenCnt)
    {
        _INFO("Set LIBF_DELEXP\n");
        base->hb_Lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    MySegment = base->hb_SegList;

    /* We don't need a forbid() because Expunge and Close
     * are called with a pending forbid.
     * But let's do it for safety if somebody does it by hand.
     */
    Forbid();
    _INFO("Remove the library\n");
    Remove(&base->hb_Lib.lib_Node);
    Permit();

    CloseLibrary((struct Library *)DOSBase);
    CloseLibrary(UtilityBase);

    DeletePool(base->hb_MemPool);

    _INFO("Free the library\n");
    FreeMem((APTR)((ULONG)(HeliosBase) - (ULONG)(base->hb_Lib.lib_NegSize)),
            base->hb_Lib.lib_NegSize + base->hb_Lib.lib_PosSize);

    _INFO("Return Segment %lx to ramlib\n", MySegment);
    return (ULONG)MySegment;
}


/*------------------ LIBRARY CODE SECTION -------------------------*/

// The lib execute init protection which simply returns
LONG NoExecute(void)
{
    return -1;
}

struct Library* LIB_Init(struct HeliosBase * base,
                         BPTR                SegList,
                         struct ExecBase *   sBase)
{
    _INFO("+ Base %p, SegList %lx, SysBase %p\n", base, SegList, sBase);

    HeliosBase = base;
    base->hb_SegList = SegList;
    SysBase = sBase;

    /* Open needed resources */
    base->hb_MemPool = CreatePool(MEMF_PUBLIC|MEMF_CLEAR|MEMF_SEM_PROTECTED, 16384, 4096);
    if (NULL != base->hb_MemPool)
    {
        DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 39);
        if (NULL != DOSBase)
        {
            UtilityBase = OpenLibrary("utility.library", 39);
            if (NULL != UtilityBase)
            {
                LOCK_INIT(base);
                LOCK_INIT(&base->hb_Listeners);
                NEWLIST(&base->hb_Listeners.ell_SysList);
                NEWLIST(&base->hb_Hardwares);
                NEWLIST(&base->hb_Classes);
                NEWLIST(&base->hb_ReportList);

                _INFO("- OK\n");
                return &base->hb_Lib;

                CloseLibrary(UtilityBase);
            }
            else
                SysError_NeedLibrary("utility.library", 39);

            CloseLibrary((struct Library *)DOSBase);
        }
        else
            SysError_NeedLibrary("dos.library", 39);

        DeletePool(base->hb_MemPool);
    }
    else
        _ERR("CreatePool() failed\n");

    FreeMem((APTR)((ULONG)(base) - (ULONG)(base->hb_Lib.lib_NegSize)),
            base->hb_Lib.lib_NegSize + base->hb_Lib.lib_PosSize);

    _INFO("- Failed\n");
    return NULL;
}

ULONG LIB_Expunge(void)
{
    return LibExpunge((struct HeliosBase *)REG_A6);
}

struct Library* LIB_Open(void)
{
    struct HeliosBase *base = (struct HeliosBase *)REG_A6;
    struct Library *ret;

    _INFO("+ Base: %p, OpenCount %ld\n", base, base->hb_Lib.lib_OpenCnt);

    if (0 == base->hb_Lib.lib_OpenCnt)
    {
        struct Task *task;

        base->hb_Lib.lib_Flags &= ~LIBF_DELEXP;
        base->hb_Lib.lib_OpenCnt++;

        task = FindTask(NULL);

        /* If the executing task is not my helios_rom_start, abort! */
        if (task->tc_UserData == (APTR)0x837105)
        {
            task->tc_UserData = NULL;

            base->hb_TimeReq.tr_node.io_Message.mn_Node.ln_Type = NT_MESSAGE;
            base->hb_TimeReq.tr_node.io_Message.mn_ReplyPort    = NULL;
            base->hb_TimeReq.tr_node.io_Message.mn_Length       = sizeof(base->hb_TimeReq);

            /* This timer is used by Helios_DelayMS */
            if(!OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *) &base->hb_TimeReq, 0))
            {
                ret = (APTR) base;
                goto out;
            }
            else
                _ERR("Timer device open failed\n");
        }
        else
        {
            struct Library *IntuitionBase;
            static const STRPTR msg = "Helios stack not correctly installed!\nPlease run helios_rom_start first.\n";

            IntuitionBase = OpenLibrary("intuition.library", 0);
            if (NULL != IntuitionBase)
            {
                static struct EasyStruct panic = {
                    sizeof(struct EasyStruct),
                    0,
                    (UBYTE*)"helios.library",
                    0,
                    (UBYTE*)"Abort"
                };
                
                panic.es_TextFormat = (UBYTE*)msg;
                EasyRequestArgs(NULL, &panic, NULL, NULL);
                
                CloseLibrary((struct Library *) IntuitionBase);
            }
            else
                kprintf(msg);
        }

        base->hb_Lib.lib_Flags |= LIBF_DELEXP;
        base->hb_Lib.lib_OpenCnt--;
        ret = NULL;
    }
    else
    {
        base->hb_Lib.lib_Flags &= ~LIBF_DELEXP;
        base->hb_Lib.lib_OpenCnt++;
        ret = (APTR)base;
    }

out:
    _INFO("- ret=%p, OpenCnt=%ld\n", ret, base->hb_Lib.lib_OpenCnt);
    return ret;
}

ULONG LIB_Close(void)
{
    struct HeliosBase *base = (struct HeliosBase *) REG_A6;

    _INFO("+Base %p, OpenCount %ld\n", base, base->hb_Lib.lib_OpenCnt);

    if ((--base->hb_Lib.lib_OpenCnt) > 0)
        _INFO("done\n");
    else
    {
        HeliosReportMsg *msg;
        APTR *next;

        _INFO("Remove report msg...\n");
        ForeachNodeSafe(&base->hb_ReportList, msg, next)
            Helios_FreeReportMsg(msg);

        _INFO("Closing timer device...\n");
        CloseDevice((struct IORequest *) &base->hb_TimeReq);

        if (base->hb_Lib.lib_Flags & LIBF_DELEXP)
        {
            _INFO("LIBF_DELEXP set\n");
            return LibExpunge(base);
        }
    }

    _INFO("-Base %p, OpenCount %ld\n", base, base->hb_Lib.lib_OpenCnt);
    return 0;
}

ULONG LIB_Reserved(void)
{
    return 0;
}

