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
## Makefile for building the SBP2 driver.
##
##

PRJROOT  	:= ../../..
LIBNAME  	:= sbp2.class

SBP2_SRCS := $(PRJROOT)/src/common/utils.c \
	sbp2.core.c sbp2.device.c sbp2.iocmd.c
SBP2_OBJS := $(SBP2_SRCS:.c=.o)

LIBRARY_SRCS := sbp2.class.c sbp2_functable.library.c
LIBRARY_OBJS := $(patsubst %.s, %.o, $(patsubst %.c, %.o, $(LIBRARY_SRCS)))

ALL_SRCS	:= $(LIBRARY_SRCS) $(SBP2_SRCS)

include $(PRJROOT)/common.mk

DEFINES += -DLIBNAME='"$(LIBNAME)"' -DDBNAME='"HC_SBP2"'

all: $(CLS_DIR)/$(LIBNAME)

local-clean:
	rm -vf $(CLS_DIR)/$(LIBNAME)*

local-release: $(CLS_DIR)/$(LIBNAME)
	mkdir -p $(RELARC_CLS_DIR)
	$(CP) $(CLS_DIR)/$(LIBNAME) $(RELARC_CLS_DIR)/$(LIBNAME)

$(CLS_DIR)/$(LIBNAME): $(CLS_DIR)/$(LIBNAME).sym

$(CLS_DIR)/$(LIBNAME).db: $(LIBRARY_OBJS) $(SBP2_OBJS)
	@$(ECHO) $(COLOR_BOLD)">> " $(COLOR_HIGHLIGHT1)"$@"$(COLOR_NORMAL)
	$(LD) $(LDFLAGS) $^ -o $@ $(LDLIBS)
