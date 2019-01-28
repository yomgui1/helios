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

#include "../common/mui_misc.h"
#include "../common/mui_help.h"
#include "../common/help.h"
#include "../common/iffparse.h"
#include "HeliosPrefs_mcc.h"
#include "../macros/vapor.h"
#include <proto/helios.h>
#include "Helios_private.h"
//#include "Helios_Cat.h"

#define	DEBUG(x)	;

//+ HeliosPrefs_New
static ULONG HeliosPrefs_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
    Object *actionobj;
    if (NULL != HeliosBase) {
        return(NULL);
    }

    obj = (Object *) DoSuperNew(cl, obj,
                                //Child, NULL,
                                TAG_MORE, (ULONG) msg->ops_AttrList);

    if (NULL != obj) {
        struct HeliosPrefs_Data *data = INST_DATA(cl,obj);
        return((ULONG)obj);
    }

    return 0;
}
//-
//+ HeliosPrefs_Dispose
static ULONG HeliosPrefs_Dispose(struct IClass *cl,Object *obj,Msg msg)
{
    return DoSuperMethodA(cl,obj,msg);
}
//-

BEGINMTABLE2(HeliosPrefs)
    case OM_NEW     : return HeliosPrefs_New     (cl,obj,(APTR)msg);
    case OM_DISPOSE : return HeliosPrefs_Dispose (cl,obj,(APTR)msg);
ENDMTABLE
