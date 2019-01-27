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
** Header file for topology API
*/

#ifndef HELIOS_TOPOPOLGY_H
#define HELIOS_TOPOPOLGY_H

#include "libraries/helios.h"

extern BOOL _Helios_UpdateTopologyMapping(HeliosHardware *hh);
extern void _Helios_FreeTopology(HeliosHardware *hh);

#endif /* HELIOS_TOPOPOLGY_H */
