## Copyright 2008-2013, 2018 Guillaume Roguez
##
## This file is part of Helios.
##
## Helios is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## Helios is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with Helios.  If not, see <https://www.gnu.org/licenses/>.
##

##
##
## C tools Makefile
##
##

PRJROOT := ../../..
PROGRAMS := helios_remove helios_rom_start
ALL_SRCS := $(PROGRAMS:%=%.c)

include $(PRJROOT)/common.mk

all: $(PROGRAMS)

local-clean:
	rm -vf $(PROGRAMS)

local-release: $(PROGRAMS)
	$(CP) $^ $(RELARC_C)

$(PROGRAMS): $(PROGRAMS:%=%.o)
	$(CC) $(CCLDFLAGS) $@.o -o $@ $(LIBS)
