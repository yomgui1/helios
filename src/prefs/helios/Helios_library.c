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

/******************************************************************************/
/*                                                                            */
/* includes                                                                   */
/*                                                                            */
/******************************************************************************/

#include <exec/types.h>
#include <exec/resident.h>
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <libraries/mui.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/locale.h>

#define bug kprintf
//#define D(x) x
#define D(x)

/******************************************************************************/
/*                                                                            */
/* MCC/MCP name and version                                                   */
/*                                                                            */
/* ATTENTION:  The FIRST LETTER of NAME MUST be UPPERCASE                     */
/*                                                                            */
/******************************************************************************/

//#include "../common/arrowstring_class.h"
//#include "../macros/vapor.h"
#include "Helios_private.h"
#include "Helios_rev.h"
//#include "Helios_Cat.h"

//#include "BoardList_class.h"

#define CLASS				MUIC_HeliosPrefs
#define SUPERCLASS			MUIC_Group

#define	Data				HeliosPrefs_Data
#define	_Dispatcher			HeliosPrefs_dispatch

#define UserLibID			"$VER: " MUIC_HeliosPrefs " " LIB_REV_STRING " (" LIB_DATE ") " LIB_COPYRIGHT "\0\0"
#define MASTERVERSION	    19

#define PreClassInit
#define PostClassExit

#define	LIBQUERYCLASS			QUERYCLASS_PREFSCLASS
#define	LIBQUERYID			    CLASS " " LIB_REV_STRING " (" LIB_DATE ") " LIB_COPYRIGHT
#define	LIBQUERYSUBCLASS		QUERYSUBCLASS_PREFSCLASS_DEVICES
#define	LIBQUERYSUBCLASSPRI		994
#define	LIBQUERYDESCRIPTION		"Helios System Prefs"

struct Library *LocaleBase;
struct Library *HeliosBase;

//+ CleanupClasses
void CleanupClasses(void)
{}
//-
//+ SetupClasses
BOOL SetupClasses(void)
{
    return TRUE;
}
//-
//+ PreClassInitFunc
BOOL PreClassInitFunc(void)
{
    HeliosBase = OpenLibrary("helios.library", 50);
    if (NULL != HeliosBase)
        return FALSE;

    if (!SetupClasses()) {
        CloseLibrary(HeliosBase);
        return FALSE;
    }

  	LocaleBase = OpenLibrary("locale.library", 40);
  	//OpenHeliosCatalog();

    return TRUE;
}
//-
//+ PostClassExitFunc
VOID PostClassExitFunc(void)
{
    CleanupClasses();

    if (HeliosBase)
        CloseLibrary(HeliosBase);

    //CloseHeliosCatalog();

    if (LocaleBase)
        CloseLibrary(LocaleBase);
}
//-

/******************************************************************************/
/*                                                                            */
/* include the lib startup code for the mcc/mcp  (and muimaster inlines)      */
/*                                                                            */
/******************************************************************************/

/* define that if you want the mcc class to be expunge at last close */
/* which can be very nice for tests (can avoid lot of avail flush) ! */
//#define EXPUNGE_AT_LAST_CLOSE

#include <mui/mccheader.c>

#define NO_PPCINLINE_STDARG
#include <proto/muimaster.h>
#undef NO_PPCINLINE_STDARG
