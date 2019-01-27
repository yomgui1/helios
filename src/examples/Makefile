## Copyright 2008-2013,2019 Guillaume Roguez
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

INCDIRS = . ../../include
LIBS_DIR = ../../lib

CC = ppc-morphos-gcc

OPT = -O2 -mmultiple -mstring -fno-strict-aliasing
DEFINES += $(CCDEFINES) \
	USE_INLINE_STDARG \
	AROS_ALMOST_COMPATIBLE
CCWARNS = -Wall

ifeq ("$(shell $(CC) -dumpversion | cut -d. -f1)", "4")
CCWARNS += -Wno-pointer-sign
endif

CFLAGS = -noixemul -g -Wl,--traditional-format
CPPFLAGS = $(CFLAGS) $(OPT) $(CCWARNS) $(INCDIRS:%=-I%) $(DEFINES:%=-D%)
LIBS = -L$(LIBS_DIR) -lhelios -lauto

PROGRAMS = FWDevLister FWBusReset FWRead FWAddClass FWRemoveAllClasses
_PROGRAMS = SCSICmd6 \
	FWReset FWSendPhy SimpleReportMsgClient

.SUFFIXES:
.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CPPFLAGS) $< -o $@ $(LIBS)

all: $(PROGRAMS)

%: %.c
	$(CC) $(CPPFLAGS) $@.c -o $@ $(LIBS)

clean:
	-rm -f *.o $(PROGRAMS)
