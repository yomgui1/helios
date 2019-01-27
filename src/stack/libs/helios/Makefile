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
##
## Makefile for building the Helios library.
##
##

PRJROOT := $(shell (cd ../../../..; pwd))

LIBNAME  	:= helios.library
VERSION  	:= 53
REVISION 	:= 0

HELIOS_SRCS := $(PRJROOT)/src/common/utils.c \
	misc.c \
	rom.c \
	topology.c \
	objects.c \
	hardwares.c \
	devices.c \
	classes.c \
	units.c
HELIOS_OBJS := $(HELIOS_SRCS:.c=.o)

LIBRARY_SRCS := helios.library.c helios_functable.library.c
LIBRARY_OBJS := $(patsubst %.s, %.o, $(patsubst %.c, %.o, $(LIBRARY_SRCS)))

ALL_SRCS	:= $(LIBRARY_SRCS) $(HELIOS_SRCS)

SDK_NEEDED_FILES = \
	$(PRJROOT)/include/fd/helios_lib.fd \
	$(PRJROOT)/include/fd/heliosclsbase_lib.fd \
	$(PRJROOT)/include/clib/helios_protos.h \
	$(PRJROOT)/include/clib/heliosclsbase_protos.h

GEN_INCLUDES = \
	$(PRJROOT)/include/proto/helios.h \
	$(PRJROOT)/include/ppcinline/helios.h \
	$(PRJROOT)/include/inline/helios.h \
	$(PRJROOT)/include/proto/heliosclsbase.h \
	$(PRJROOT)/include/ppcinline/heliosclsbase.h \
	$(PRJROOT)/include/inline/heliosclsbase.h

GLUELIB = $(PRJROOT)/lib/libhelios.a

SDK_INC_FILES := \
	$(SDK_NEEDED_FILES) \
	$(PRJROOT)/include/libraries/helios.h \
	$(GEN_INCLUDES)

include $(PRJROOT)/common.mk

DEFINES += -DLIBNAME='"$(LIBNAME)"' -DDBNAME='"HELIOS"'

.PHONY: dirs sdk install

all: dirs $(LIBS_DIR)/$(LIBNAME)

dirs:
	[ -d "$(PRJROOT)/libs" ] || mkdir -p "$(PRJROOT)/libs"

local-clean:
	rm -vf $(LIBS_DIR)/$(LIBNAME)* $(GLUELIB) $(GEN_INCLUDES)

local-release: $(LIBS_DIR)/$(LIBNAME) sdk
	mkdir -p $(RELARC_DIR)/Libs $(RELARC_DIR)/SDK/lib
	for dir in $(subst $(PRJROOT)/,,$(dir $(SDK_INC_FILES))); do mkdir -p $(RELARC_DIR)/SDK/$$dir; done
	$(CP) $(LIBS_DIR)/$(LIBNAME) $(RELARC_DIR)/Libs/$(LIBNAME)
	$(CP) $(GLUELIB) $(RELARC_DIR)/SDK/lib/
	for file in $(subst $(PRJROOT)/,,$(SDK_INC_FILES)); do \
		$(CP) $(PRJROOT)/$$file $(RELARC_DIR)/SDK/$$file; done

install: $(LIBS_DIR)/$(LIBNAME)
	$(CP) $(LIBS_DIR)/$(LIBNAME) /LIBS/
	-FlushLib $(LIBNAME)
	
uninstall:
	@$(ECHO) $(COLOR_BOLD)">> " $(COLOR_HIGHLIGHT1)"Removing LIBS:$(LIBNAME)"$(COLOR_NORMAL)
	-Delete LIBS:$(LIBNAME)
	-FlushLib $(LIBNAME)

$(LIBS_DIR)/$(LIBNAME): $(LIBS_DIR)/$(LIBNAME).sym

$(LIBS_DIR)/$(LIBNAME).db: $(LIBRARY_OBJS) $(HELIOS_OBJS)
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"$@"$(COLOR_NORMAL)
	$(LD) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(GEN_INCLUDES:$(PRJROOT)/include/%=%): sdk

sdk: $(SDK_NEEDED_FILES)
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"Making includes..."$(COLOR_NORMAL)
	[ -d "$(PRJROOT)/include/proto" ] || mkdir -p "$(PRJROOT)/include/proto"
	[ -d "$(PRJROOT)/include/inline" ] || mkdir -p "$(PRJROOT)/include/inline"
	[ -d "$(PRJROOT)/include/ppcinline" ] || mkdir -p "$(PRJROOT)/include/ppcinline"
	$(CVINCLUDE) -q --fd $(PRJROOT)/include/fd/helios_lib.fd \
				 --clib $(PRJROOT)/include/clib/helios_protos.h \
				 --proto $(PRJROOT)/include/proto/helios.h \
				 --inlines $(PRJROOT)/include/ppcinline/helios.h \
				 --vbccinline $(PRJROOT)/include/inline/helios.h
	$(CVINCLUDE) -q --fd $(PRJROOT)/include/fd/heliosclsbase_lib.fd \
				 --clib $(PRJROOT)/include/clib/heliosclsbase_protos.h \
				 --proto $(PRJROOT)/include/proto/heliosclsbase.h \
				 --inlines $(PRJROOT)/include/ppcinline/heliosclsbase.h \
				 --vbccinline $(PRJROOT)/include/inline/heliosclsbase.h
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"Making library stubs..."$(COLOR_NORMAL)
	[ -d "$(PRJROOT)/lib" ] || mkdir -p "$(PRJROOT)/lib"
	$(CVINCLUDE) -q --fd $(PRJROOT)/include/fd/helios_lib.fd \
				 --clib $(PRJROOT)/include/clib/helios_protos.h \
				 --gluelib $(PRJROOT)/lib/libhelios.a
