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
## Makefile for building the AV/C 1394 library.
##
##

PRJROOT := $(shell (cd ../..; pwd))

LIBNAME  := avc1394.library
VERSION  := 0
REVISION := 1

LIBRARY_SRCS := avc1394_lib.c avc1394_libfunctable.c
LIBRARY_OBJS := $(LIBRARY_SRCS:.c=.o)

AVC1394_SRCS := avc1394_io.c avc1394_vcr.c
AVC1394_OBJS := $(AVC1394_SRCS:.c=.o)

ALL_SRCS := $(LIBRARY_SRCS) $(AVC1394_SRCS)

GEN_INCLUDES = \
	$(PRJROOT)/include/proto/avc1394.h \
	$(PRJROOT)/include/ppcinline/avc1394.h \
	$(PRJROOT)/include/inline/avc1394.h \

GLUELIB = $(PRJROOT)/lib/libavc1394.a

SDK_INC_FILES := \
	$(PRJROOT)/include/fd/avc1394_lib.fd \
	$(PRJROOT)/include/clib/avc1394_protos.h \
	$(GEN_INCLUDES)

DEFINES := LIBNAME=\"$(LIBNAME)\"

include $(PRJROOT)/common.mk

.PHONY: avc1394_version.h sdk

all: $(LIBS_DIR)/$(LIBNAME) sdk

local-clean:
	rm -f $(LIBS_DIR)/$(LIBNAME)* avc1394stubs.[so] $(GLUELIB) $(GEN_INCLUDES)

local-release: $(LIBS_DIR)/$(LIBNAME) sdk
	mkdir -p $(RELARC_DIR)/Libs $(RELARC_DIR)/SDK/lib
	for dir in $(subst $(PRJROOT)/,,$(dir $(SDK_INC_FILES))); do mkdir -p $(RELARC_DIR)/SDK/$$dir; done
	$(CP) $(LIBS_DIR)/$(LIBNAME) $(RELARC_DIR)/Libs/$(LIBNAME)
	$(CP) $(GLUELIB) $(RELARC_DIR)/SDK/lib/
	for dir in $(subst $(PRJROOT)/,,$(dir $(SDK_INC_FILES))); do \
		(cd $(PRJROOT)/$$dir && find -regex ".*\.\(h\|fd\)" -exec $(CP) {} $(RELARC_DIR)/SDK/$$dir ";"); \
		done

$(LIBS_DIR)/$(LIBNAME): $(LIBS_DIR)/$(LIBNAME).sym

$(LIBS_DIR)/$(LIBNAME).db: $(LIBRARY_OBJS) $(AVC1394_OBJS)
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"$@"$(COLOR_BOLD)" : "$(COLOR_HIGHLIGHT2)"$^"$(COLOR_NORMAL)
	$(LD) $(LDFLAGS) $^ -o $@ $(LDLIBS)

avc1394stubs.s: $(PRJROOT)/include/fd/avc1394_lib.fd
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"Making varargs stubs"$(COLOR_NORMAL)
	$(CVINCLUDE) -q --fd $(PRJROOT)/include/fd/avc1394_lib.fd \
				 --clib $(PRJROOT)/include/clib/avc1394_protos.h \
				 --sysvtagstubs $@

sdk: $(GEN_INCLUDES) $(GLUELIB)

$(GEN_INCLUDES): $(PRJROOT)/include/fd/avc1394_lib.fd $(PRJROOT)/include/clib/avc1394_protos.h
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"Making includes..."$(COLOR_NORMAL)
	$(CVINCLUDE) -q --fd $(PRJROOT)/include/fd/avc1394_lib.fd \
				 --clib $(PRJROOT)/include/clib/avc1394_protos.h \
				 --proto $(PRJROOT)/include/proto/avc1394.h \
				 --inlines $(PRJROOT)/include/ppcinline/avc1394.h \
				 --vbccinline $(PRJROOT)/include/inline/avc1394.h

$(GLUELIB): $(PRJROOT)/include/fd/avc1394_lib.fd $(PRJROOT)/include/clib/avc1394_protos.h
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"Making library stubs..."$(COLOR_NORMAL)
	$(CVINCLUDE) -q --fd $(PRJROOT)/include/fd/avc1394_lib.fd \
				 --clib $(PRJROOT)/include/clib/avc1394_protos.h \
				 --gluelib $(PRJROOT)/lib/libavc1394.a
