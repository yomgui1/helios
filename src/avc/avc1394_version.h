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

/* 
**
** AV/C 1394 Library version data header file.
**
*/

#ifndef AVC1394_VERSION_H
#define AVC1394_VERSION_H

#if !defined(LIBNAME) || !defined(VERSION) || !defined(REVISION) || !defined(VR_ST) || !defined(DATE)
#error "Why don't you use the geniune Makefile ?"
#endif

#define VERS        LIBNAME" "VR_ST
#define VSTRING     LIBNAME" "VR_ST" ("DATE") \x40 2009 by Guillaume Roguez, written by Guillaume Roguez\r\n"
#define VERSTAG     "\0$VER: "LIBNAME" "VR_ST" ("DATE") \x40 2009 by Guillaume Roguez, written by Guillaume Roguez"

#endif /* AVC1394_VERSION_H */
