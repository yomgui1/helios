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
** SBP2 class IO commands API header file.
**
*/

#ifndef SBP2_IOCMD_H
#define SBP2_IOCMD_H

#include "sbp2.class.h"

extern LONG sbp2_iocmd_scsi(SBP2Unit *unit, struct IOStdReq *ioreq);
extern LONG sbp2_iocmd_start_stop(SBP2Unit *unit, struct IOStdReq *ioreq);
extern LONG sbp2_iocmd_read64(SBP2Unit *unit, struct IOStdReq *ioreq);
extern LONG sbp2_iocmd_write64(SBP2Unit *unit, struct IOStdReq *ioreq);
extern LONG sbp2_iocmd_get_geometry(SBP2Unit *unit, struct IOStdReq *ioreq);

#endif /* SBP2_IOCMD_H */
