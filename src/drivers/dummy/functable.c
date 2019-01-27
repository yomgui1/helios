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
** class functions table definition file.
*/

#include "private.h"
#include "proto/heliosclsbase.h"

ULONG libFuncTable[]=
{
	FUNCARRAY_BEGIN,

	FUNCARRAY_32BIT_NATIVE,
	(ULONG) LIB_OPEN_FNAME,
	(ULONG) LIB_CLOSE_FNAME,
	(ULONG) LIB_EXPUNGE_FNAME,
	(ULONG) RESERVED_FNAME,
	-1,

	FUNCARRAY_32BIT_SYSTEMV,
	(ULONG) &HeliosClass_DoMethodA,
	-1,

	FUNCARRAY_END
};
