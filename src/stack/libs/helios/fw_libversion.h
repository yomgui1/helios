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
** Helios Library version data header file.
**
*/

#ifndef FW_LIBVERSION_H
#define FW_LIBVERSION_H

#if !defined(VERSION) || !defined(REVISION) || !defined(VR_ST) || !defined(DATE) || !defined(COPYRIGHTS)
#error "Why don't you use the geniune Makefile ?"
#endif

#define VERS    LIBNAME" "VR_ST
#define VSTRING LIBNAME" "VR_ST" ("DATE") "COPYRIGHTS"\r\n"
#define VERSTAG "\0$VER: "LIBNAME" "VR_ST" ("DATE") "COPYRIGHTS

#endif /* FW_LIBVERSION_H */
