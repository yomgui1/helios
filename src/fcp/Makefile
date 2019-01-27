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
## Makefile for building the FCP library.
##
##

PRJROOT := $(shell (cd ../..; pwd))

LIBNAME  	:= fcp.device
VERSION  	:= 0
REVISION 	:= 1

FCP_SRCS := fcp_func.c
FCP_OBJS := $(FCP_SRCS:.c=.o)

LIBRARY_SRCS := fcp_dev.c
LIBRARY_OBJS := $(patsubst %.s, %.o, $(patsubst %.c, %.o, $(LIBRARY_SRCS)))

ALL_SRCS	:= $(LIBRARY_SRCS) $(FCP_SRCS)

SDK_INC_FILES := \
	$(PRJROOT)/include/devices/helios/fcp.h

DEFINES := LIBNAME=\"$(LIBNAME)\"

include $(PRJROOT)/common.mk

.PHONY: fcp_version.h sdk

all: $(DEVS_DIR)/$(LIBNAME) sdk

local-clean:
	rm -vf $(DEVS_DIR)/$(LIBNAME)*

local-release: $(DEVS_DIR)/$(LIBNAME) sdk
	mkdir -p $(RELARC_DIR)/Devs/helios
	for dir in $(subst $(PRJROOT)/,,$(dir $(SDK_INC_FILES))); do mkdir -p $(RELARC_DIR)/SDK/$$dir; done
	$(CP) $(DEVS_DIR)/$(LIBNAME) $(RELARC_DIR)/Devs/helios/$(LIBNAME)
	for file in $(subst $(PRJROOT)/,,$(SDK_INC_FILES)); do \
		$(CP) $(PRJROOT)/$$file $(RELARC_DIR)/SDK/$$file; done

$(DEVS_DIR)/$(LIBNAME): $(DEVS_DIR)/$(LIBNAME).sym

$(DEVS_DIR)/$(LIBNAME).db: $(LIBRARY_OBJS) $(FCP_OBJS)
	@$(ECHO) $(COLOR_BOLD)">> " $(COLOR_HIGHLIGHT1)"$@"$(COLOR_BOLD)" : "$(COLOR_HIGHLIGHT2)"$^"$(COLOR_NORMAL)
	$(LD) $(LDFLAGS) $^ -o $@ $(LDLIBS)
