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
** Provide API to handle external helios classes.
*/

#include "private.h"

/*============================================================================*/
/*--- LIBRARY CODE SECTION ---------------------------------------------------*/

HeliosClass *Helios_AddClass(STRPTR name, ULONG version)
{
    struct Library *cls_base;

    _INFO("Loading class <%s> v%lu...\n", name, version);
    cls_base = OpenLibrary(name, version);
    if (NULL != cls_base)
    {
        HeliosClass *hc;

        /* If not already exists, create or respawn a HeliosClass shared object:
         * This object is used when Helios or app references the class.
         */

        LOCK_REGION(HeliosBase);
        {
            /* Do nothing if the class already queued */
            hc = (HeliosClass *)FindName((struct List *)&HeliosBase->hb_Classes,
                                         cls_base->lib_Node.ln_Name);
            if (NULL == hc)
            {
                /* nothing exists, create it */
                hc = AllocPooled(HeliosBase->hb_MemPool, sizeof(HeliosClass));
                if (NULL != hc)
                {
                    ULONG prio;

                    LOCK_INIT(hc);
                    LOCK_INIT(&hc->hc_Listeners);
                    NEWLIST(&hc->hc_Listeners.ell_SysList);

                    hc->hso_SysNode.ln_Name = cls_base->lib_Node.ln_Name;
                    hc->hc_LoadedName = utils_DupStr(HeliosBase->hb_MemPool, name);
                    hc->hc_Base = cls_base;
                    cls_base = NULL; /* and don't close it ;-) */

                    HeliosClass_DoMethod(HCM_Initialize, (ULONG)hc);

                    /* Obtain class priority, default is 0 */
                    prio=0;
                    Helios_GetAttrs(HGA_CLASS, hc,
                                    HA_Priority, (ULONG)&prio,
                                    TAG_END);
                    hc->hso_SysNode.ln_Pri = prio;

                    hc->hso_RefCnt = 1;
                    Enqueue((struct List *)&HeliosBase->hb_Classes, &hc->hso_SysNode);

                    Helios_ReportMsg(HRMB_INFO, "CLASS", "Welcome to class <%s>.", hc->hso_SysNode.ln_Name);
                    Helios_SendEvent(&HeliosBase->hb_Listeners, HEVTF_NEW_CLASS, (ULONG)hc);
                }
                else
                {
                    Helios_ReportMsg(HRMB_ERROR, "CLASS", "Failed to allocate class <%s>.", hc->hso_SysNode.ln_Name);
                }
            }
            else
            {
                Helios_ReportMsg(HRMB_WARN, "CLASS", "Class <%s> already exist.", hc->hso_SysNode.ln_Name);
            }
        }
        UNLOCK_REGION(HeliosBase);

        if (NULL != cls_base)
        {
            CloseLibrary(cls_base);
        }
    }
    else
    {
        Helios_ReportMsg(HRMB_ERROR, "CLASS", "Class <%s> v%lu not found\n", name, version);
    }

    return NULL;
}

void Helios_RemoveClass(HeliosClass *hc)
{
    _INFO("Removing class %p\n", hc);

    /* Not seenable from Helios */
    LOCK_REGION(HeliosBase);
    REMOVE(&hc->hso_SysNode);
    UNLOCK_REGION(HeliosBase);

    /* Force the class to release all bound Helios units
     * and kill all unit tasks.
     */
    HeliosClass_DoMethod(HCM_ReleaseAllBindings);

    /* Let applications release the class */
    Helios_ReportMsg(HRMB_INFO, "CLASS", "Class <%s> removed", hc->hso_SysNode.ln_Name);
    Helios_SendEvent(&hc->hc_Listeners, HEVTF_CLASS_REMOVED, 0);

    Helios_ReleaseClass(hc);
}

LONG Helios_ObtainClass(HeliosClass *hc)
{
    return (CLS_INCREF(hc) > 0);
}

void Helios_ReleaseClass(HeliosClass *hc)
{
    LONG old_refcnt;

    old_refcnt = CLS_DECREF(hc);
    if (old_refcnt <= 1)
    {
        /* Bad design? */
        if (old_refcnt <= 0)
        {
            _WARN("Too many releases of class %p called (old refcnt: %d)\n", hc, old_refcnt);
        }

        Helios_ReportMsg(HRMB_DBG, "CLASS", "Closing class %p <%s>", hc, hc->hc_Base->lib_Node.ln_Name);

        /* As Helios get a token until RemoveClass called,
         * we can suppose that no binding remains when no token also.
         */

        /* As hc pointer will not be longer valid ask the class to forget it */
        HeliosClass_DoMethod(HCM_Terminate);

        CloseLibrary(hc->hc_Base);
        FreeVecPooled(HeliosBase->hb_MemPool, hc->hc_LoadedName);
        FreePooled(HeliosBase->hb_MemPool, hc, sizeof(*hc));
    }
}

HeliosClass *Helios_GetNextClass(HeliosClass *hc)
{
    if (NULL == hc)
    {
        hc = (HeliosClass *)GetHead(&HeliosBase->hb_Classes);
    }
    else
    {
        hc = (HeliosClass *)GetSucc(&hc->hso_SysNode);
    }

    if (NULL != hc)
    {
        CLS_INCREF(hc);
    }

    return hc;
}


/* EOF */
