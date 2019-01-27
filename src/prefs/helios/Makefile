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
## Makefile for building the Helios MUI Preference library.
##
##

PRJROOT := $(shell (cd ../../..; pwd))

LIBNAME  	:= helios.mprefs
VERSION  	:= 50
REVISION 	:= 0

MPREFS_SRCS := helios.c
MPREFS_OBJS := $(MPREFS_SRCS:.c=.o)

LIBRARY_SRCS := Helios_library.c
LIBRARY_OBJS := $(patsubst %.s, %.o, $(patsubst %.c, %.o, $(LIBRARY_SRCS)))

ALL_SRCS	:= $(LIBRARY_SRCS) $(MPREFS_SRCS)

DEFINES := LIBNAME=\"$(LIBNAME)\" DBNAME=\"$(LIBNAME)\" SYSTEM_PRIVATE

include $(PRJROOT)/common.mk

ifeq ("$(OS)", "MorphOS")
INCDIRS += /MOSPRVINC/../../prefs/common
else
INCDIRS += /pub2/morphoswb/prefs/common
endif

.PHONY: Helios_rev.h

all: $(LIBS_DIR)/$(LIBNAME)

local-clean:
	rm -vf $(LIBS_DIR)/$(LIBNAME)*

local-release: $(LIBS_DIR)/$(LIBNAME)
	mkdir -p $(RELARC_DIR)/Libs
	$(CP) $(LIBS_DIR)/$(LIBNAME) $(RELARC_DIR)/Libs/$(LIBNAME)

$(LIBS_DIR)/$(LIBNAME): $(LIBS_DIR)/$(LIBNAME).sym

$(LIBS_DIR)/$(LIBNAME).db: $(LIBRARY_OBJS) $(MPREFS_OBJS)
	@$(ECHO) $(COLOR_BOLD)">> " $(COLOR_HIGHLIGHT1)"$@"$(COLOR_BOLD)" : "$(COLOR_HIGHLIGHT2)"$^"$(COLOR_NORMAL)
	$(LD) $(LDFLAGS) $^ -o $@ $(LDLIBS)
