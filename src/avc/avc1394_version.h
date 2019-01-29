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
** AV/C 1394 Library version data header file.
**
*/

#ifndef AVC1394_VERSION_H
#define AVC1394_VERSION_H

#if !defined(BUILD_DATE)
#error "BUILD_DATE are not defined but are mandatory"
#endif

#ifndef SCM_REV
#define SCM_REV "private"
#endif

#define COPYRIGHTS "\xa9\x20Guillaume\x20Roguez\x20[" SCM_REV "]"
#define VERSION 0
#define REVISION 2
#define VR_ST "0.2"
#define VERS    LIBNAME" "VR_ST
#define VSTRING LIBNAME" "VR_ST" ("BUILD_DATE") "COPYRIGHTS"\r\n"
#define VTAG "\0$VER: "LIBNAME" "VR_ST" ("BUILD_DATE") "COPYRIGHTS

#endif /* AVC1394_VERSION_H */
