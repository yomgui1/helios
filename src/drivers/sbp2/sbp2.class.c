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
** SBP2 Class core file.
**
*/

//#define NDEBUG
#define DEBUG_LIB

#if !defined(VERSION) || !defined(REVISION) || !defined(VR_ST) || !defined(DATE)
#error "Why don't you use the geniune Makefile ?"
#endif

#define VERS    LIBNAME" "VR_ST
#define VERSTAG "\0$VER: "LIBNAME"  "VR_ST" ("DATE") "COPYRIGHTS

#include "sbp2.class.h"
#include "sbp2.device.h"

extern ULONG LibFuncTable[]; /* defined in sbp2_functable.library.c */

struct Library* LIB_Init(SBP2ClassLib *       MyLibBase,
                         BPTR              SegList,
                         struct ExecBase * SBase);

struct LibInitStruct
{
    ULONG   LibSize;
    APTR    FuncTable;
    APTR    DataTable;
    void    (*InitFunc)(void);
};

struct LibInitStruct LibInitStruct=
{
    sizeof(SBP2ClassLib),
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
 *
 * Merge with version also ;-)
 */
CONST STRPTR __abox__ __attribute__((section (".rodata"))) = VERSTAG;

#define SysBase (base->hc_SysBase)
#define DOSBase (base->hc_DOSBase)
#define UtilityBase (base->hc_UtilityBase)

/*------------------ PRIVATE CODE SECTION -------------------------*/

static void SysError_NeedLibrary(STRPTR libname, ULONG version)
{
    _ERR_LIB("OpenLibrary(\"%s\", %lu) failed\n", libname, version);
}

static ULONG LibExpunge(SBP2ClassLib *base)
{
    BPTR MySegment;

    _INFO_LIB("Base %p, Dev OpenCount %ld\n", base, base->hc_DevBase->dv_Library.lib_OpenCnt);

    /* Don't expunge if sbp2.device is used */
    if (base->hc_DevBase->dv_Library.lib_OpenCnt > 0)
    {
        base->hc_Lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    devCleanup(base->hc_DevBase);

    MySegment = base->hc_SegList;

    /* We don't need a forbid() because Expunge and Close
     * are called with a pending forbid.
     * But let's do it for safety if somebody does it by hand.
     */
    Forbid();
    _INFO_LIB("Remove from mem library %s\n", base->hc_Lib.lib_Node.ln_Name);
    REMOVE(&base->hc_Lib.lib_Node);
    Permit();

    CloseLibrary((struct Library *)base->hc_DOSBase);
    CloseLibrary(base->hc_UtilityBase);

    _INFO_LIB("Delete mem pool\n");
    DeletePool(base->hc_MemPool);

    _INFO_LIB("Free the library\n");
    FreeMem((char *)base - base->hc_Lib.lib_NegSize,
            base->hc_Lib.lib_NegSize + base->hc_Lib.lib_PosSize);

    _INFO_LIB("Return Segment %lx to ramlib\n", MySegment);
    return (ULONG)MySegment;
}


/*------------------ LIBRARY CODE SECTION -------------------------*/

// The lib execute init protection which simply returns
LONG NoExecute(void)
{
    return -1;
}

struct Library* LIB_Init(SBP2ClassLib *base,
                         BPTR SegList,
                         struct ExecBase *sBase)
{
    _INFO_LIB("+ Base %p, SegList %lx, SysBase %p\n", base, SegList, sBase);

    base->hc_SegList = SegList;
    base->hc_SysBase = sBase;

    /* Open needed resources */
    base->hc_MemPool = CreatePool(MEMF_PUBLIC|MEMF_CLEAR|MEMF_SEM_PROTECTED, 16384, 4096);
    if (NULL != base->hc_MemPool)
    {
        base->hc_DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 39);
        if (NULL != base->hc_DOSBase)
        {
            base->hc_UtilityBase = OpenLibrary("utility.library", 39);
            if (NULL != base->hc_UtilityBase)
            {
                LOCK_INIT(base);
                NEWLIST(&base->hc_Units);
                base->hc_MaxUnitNo = 0;

                _INFO_LIB("Loading device %s into memory\n", DEVNAME);
                base->hc_DevBase = (SBP2Device *)NewCreateLibraryTags(LIBTAG_FUNCTIONINIT, (ULONG)devFuncTable,
                                                                      LIBTAG_LIBRARYINIT, (ULONG)devInit,
                                                                      LIBTAG_BASESIZE, sizeof(SBP2Device),
                                                                      LIBTAG_MACHINE, MACHINE_PPC,
                                                                      LIBTAG_TYPE, NT_DEVICE,
                                                                      LIBTAG_FLAGS, LIBF_SUMUSED | LIBF_CHANGED | LIBF_QUERYINFO,
                                                                      LIBTAG_NAME, (ULONG)DEVNAME,
                                                                      LIBTAG_VERSION, DEV_VERSION,
                                                                      LIBTAG_REVISION, DEV_REVISION,
                                                                      LIBTAG_IDSTRING, (ULONG)DEV_VERSION_STR,
                                                                      LIBTAG_PUBLIC, TRUE,
                                                                      LIBTAG_QUERYINFO, TRUE,
                                                                      TAG_DONE);

                if (NULL != base->hc_DevBase)
                {
                    base->hc_DevBase->dv_SBP2ClassBase = base;
                    return &base->hc_Lib;
                }
                    
                CloseLibrary(UtilityBase);
            }
            else
                SysError_NeedLibrary("utility.library", 39);

            CloseLibrary((struct Library *)DOSBase);
        }
        else
            SysError_NeedLibrary("dos.library", 39);

        DeletePool(base->hc_MemPool);
    }
    else
        _ERR("CreatePool() failed\n");

    FreeMem((APTR)((ULONG)(base) - (ULONG)(base->hc_Lib.lib_NegSize)),
            base->hc_Lib.lib_NegSize + base->hc_Lib.lib_PosSize);

    _INFO_LIB("- Failed\n");
    return NULL;
}

ULONG LIB_Expunge(void)
{
    return LibExpunge((SBP2ClassLib *)REG_A6);
}

struct Library* LIB_Open(void)
{
    SBP2ClassLib *base = (SBP2ClassLib *)REG_A6;

    _INFO_LIB("+ Cnt=%d\n", base->hc_Lib.lib_OpenCnt);

    if (0 == base->hc_Lib.lib_OpenCnt++)
    {
        base->hc_Lib.lib_Flags &= ~LIBF_DELEXP;

        base->hc_HeliosBase = OpenLibrary(HELIOS_LIBNAME, HELIOS_LIBVERSION);
        if (NULL != base->hc_HeliosBase)
            return &base->hc_Lib;
        else
            SysError_NeedLibrary(HELIOS_LIBNAME, HELIOS_LIBVERSION);

        --base->hc_Lib.lib_OpenCnt;
        base->hc_Lib.lib_Flags |= LIBF_DELEXP;
    }
    else
        return &base->hc_Lib;

    return NULL;
}

ULONG LIB_Close(void)
{
    SBP2ClassLib *base = (SBP2ClassLib *) REG_A6;
    LONG cnt;

    LOCK_REGION(base);
    cnt = --base->hc_Lib.lib_OpenCnt;
    UNLOCK_REGION(base);

    if (cnt > 0)
        _INFO_LIB("Not yet, OpenCount=%ld\n", base->hc_Lib.lib_OpenCnt);
    else
    {
        CloseLibrary(base->hc_HeliosBase);
        base->hc_HeliosBase = NULL;

        if (base->hc_Lib.lib_Flags & LIBF_DELEXP)
            return LibExpunge(base);

        _INFO_LIB("Ready for expunge\n");
    }

    return 0;
}

ULONG LIB_Reserved(void)
{
    return 0;
}

LONG HeliosClass_DoMethodA(SBP2ClassLib *base, ULONG methodid, ULONG *data)
{
    _INFO("base=%p, methodid=%x, data[0]=%p\n", base, methodid, data[0]);
    switch (methodid)
    {
        /* Mandatory methods */
        case HCM_Initialize: return sbp2_InitClass(base, (HeliosClass *)data[0]);
        case HCM_Terminate: return sbp2_TermClass(base);
        case HCM_ReleaseAllBindings: return sbp2_ReleaseAllBindings(base);
        case HCM_ReleaseUnitBinding: return sbp2_ReleaseUnitBinding(base, (HeliosUnit *)data[0], (SBP2Unit *)data[1]);

        /* Optional methods */
        case HCM_AttemptUnitBinding: return sbp2_AttemptUnitBinding(base, (HeliosUnit *)data[0]);
    }

    return 0;
}


#undef SysBase

void *memcpy(void *dst, void *src, size_t size)
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    CopyMem(src, dst, size);
    return dst;
}

