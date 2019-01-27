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

#ifndef CLIB_HELIOSCLSBASE_PROTOS_H
#define CLIB_HELIOSCLSBASE_PROTOS_H

/* $Id$
** This file is copyrights 2008-2012 by Guillaume ROGUEZ.
**
** C prototypes for Helios Classes.
*/

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern LONG HeliosClass_DoMethodA(ULONG methodid, ULONG *data);

/* VarArgs APIs */
#if !defined(USE_INLINE_STDARG)
extern LONG HeliosClass_DoMethod(ULONG methodid, ...);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CLIB_HELIOSCLSBASE_PROTOS_H */
