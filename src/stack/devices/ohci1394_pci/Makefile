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

##
## This file is copyrights 2008-2012 by Guillaume ROGUEZ.
##
## Makefile for building the ohci1394 Helios device.
##

PRJROOT := $(shell (cd ../../../..; pwd))

DEVNAME  	:= ohci1394_pci.device
VERSION  	:= 50
REVISION 	:= 2

OHCI1394_SRCS := $(PRJROOT)/src/common/utils.c
OHCI1394_SRCS := ohci1394core.c ohci1394cmd.c ohci1394trans.c
OHCI1394_OBJS := $(OHCI1394_SRCS:.c=.o)

DEVICE_SRCS := ohci1394.device.c
DEVICE_OBJS := $(patsubst %.s, %.o, $(patsubst %.c, %.o, $(DEVICE_SRCS)))

ALL_SRCS	:= $(DEVICE_SRCS) $(OHCI1394_SRCS)

SDK_INC_FILES := \
	$(PRJROOT)/include/devices/helios/ohci1394.h

include $(PRJROOT)/common.mk

DEFINES += -DDEVNAME='"$(DEVNAME)"' -DDBNAME='"OHCI"'

all: $(DEVS_DIR)/Helios $(DEVS_DIR)/Helios/$(DEVNAME)

local-clean:
	rm -vf $(DEVS_DIR)/Helios/$(DEVNAME)*

local-release: $(DEVS_DIR)/Helios/$(DEVNAME)
	mkdir -p $(RELARC_DIR)/Devs/Helios
	for dir in $(subst $(PRJROOT)/,,$(dir $(SDK_INC_FILES))); do mkdir -p $(RELARC_DIR)/SDK/$$dir; done
	$(CP) $(DEVS_DIR)/Helios/$(DEVNAME) $(RELARC_DIR)/Devs/Helios/$(DEVNAME)
	for file in $(subst $(PRJROOT)/,,$(SDK_INC_FILES)); do \
		$(CP) $(PRJROOT)/$$file $(RELARC_DIR)/SDK/$$file; done

$(DEVS_DIR)/Helios:
	mkdir -p $@

$(DEVS_DIR)/Helios/$(DEVNAME): $(DEVS_DIR)/Helios/$(DEVNAME).sym

$(DEVS_DIR)/Helios/$(DEVNAME).db: $(DEVICE_OBJS) $(OHCI1394_OBJS)
	@$(ECHO) $(COLOR_BOLD)">> " $(COLOR_HIGHLIGHT1)"$@"$(COLOR_NORMAL)
	$(LD) $(LDFLAGS) $^ -o $@ $(LDLIBS)

