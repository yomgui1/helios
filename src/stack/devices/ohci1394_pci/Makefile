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
## Makefile for building the ohci1394 Helios device.
##

PRJROOT := ../../../..
LIBNAME := ohci1394_pci.device

LIBRARY_SRCS := \
	ohci1394.device.c \
	ohci1394cmd.c \
	ohci1394core.c \
	ohci1394topo.c \
	ohci1394trans.c \
	ohci1394dev.c \
	$(PRJROOT)/src/common/utils.c

LIBRARY_OBJS := $(patsubst %.s, %.o, $(patsubst %.c, %.o, $(LIBRARY_SRCS)))

ALL_SRCS := $(LIBRARY_SRCS)

SDK_INC_FILES := \
	$(PRJROOT)/include/devices/helios/ohci1394.h

include $(PRJROOT)/common.mk

DEFINES += -DDEVNAME='"$(LIBNAME)"' -DDBNAME='"OHCI"'

all: $(DEVS_DIR)/Helios/$(LIBNAME)

local-clean:
	rm -vf $(DEVS_DIR)/Helios/$(LIBNAME)*

local-release: $(DEVS_DIR)/Helios/$(LIBNAME)
	mkdir -p $(RELARC_DIR)/Devs/Helios
	for dir in $(subst $(PRJROOT)/,,$(dir $(SDK_INC_FILES))); do mkdir -p $(RELARC_DIR)/SDK/$$dir; done
	$(CP) $(DEVS_DIR)/Helios/$(LIBNAME) $(RELARC_DIR)/Devs/Helios/$(LIBNAME)
	for file in $(subst $(PRJROOT)/,,$(SDK_INC_FILES)); do \
		$(CP) $(PRJROOT)/$$file $(RELARC_DIR)/SDK/$$file; done

