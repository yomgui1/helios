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
## Makefile for building the Helios library.
##

PRJROOT = ../../../..
LIBNAME = helios.library
LIBRARY_SRCS = $(wildcards *.c) $(PRJROOT)/src/common/utils.c
include $(PRJROOT)/common.mk

GLUELIB = $(PRJROOT)/lib/libhelios.a
GENERATED_INCLUDES = \
	$(PRJROOT)/include/proto/helios.h \
	$(PRJROOT)/include/ppcinline/helios.h \
	$(PRJROOT)/include/inline/helios.h \
	$(PRJROOT)/include/proto/heliosclsbase.h \
	$(PRJROOT)/include/ppcinline/heliosclsbase.h \
	$(PRJROOT)/include/inline/heliosclsbase.h
SDK_INC_FILES = \
	$(PRJROOT)/include/fd/helios_lib.fd \
	$(PRJROOT)/include/fd/heliosclsbase_lib.fd \
	$(PRJROOT)/include/clib/helios_protos.h \
	$(PRJROOT)/include/clib/heliosclsbase_protos.h \
	$(PRJROOT)/include/libraries/helios.h \
	$(GEN_INCLUDES)
DEFINES += -DLIBNAME='"$(LIBNAME)"' -DDBNAME='"HELIOS"'

all: $(LIBS_DIR)/$(LIBNAME)

local-clean:
	rm -vf $(LIBS_DIR)/$(LIBNAME)* $(GLUELIB) $(GENERATED_INCLUDES)

local-release: $(LIBS_DIR)/$(LIBNAME) sdk
	mkdir -p $(RELARC_DIR)/Libs $(RELARC_DIR)/SDK/lib
	for dir in $(subst $(PRJROOT)/,,$(dir $(SDK_INC_FILES))); do mkdir -p $(RELARC_DIR)/SDK/$$dir; done
	$(CP) $(LIBS_DIR)/$(LIBNAME) $(RELARC_DIR)/Libs/$(LIBNAME)
	$(CP) $(GLUELIB) $(RELARC_DIR)/SDK/lib/
	for file in $(subst $(PRJROOT)/,,$(SDK_INC_FILES)); do \
		$(CP) $(PRJROOT)/$$file $(RELARC_DIR)/SDK/$$file; done

sdk: $(GENERATED_INCLUDES) $(GLUELIB)

$(GENERATED_INCLUDES): \
		$(PRJROOT)/include/fd/helios_lib.fd $(PRJROOT)/include/clib/helios_protos.h \
		$(PRJROOT)/include/fd/heliosclsbase_lib.fd $(PRJROOT)/include/clib/heliosclsbase_protos.h
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"Making includes..."$(COLOR_NORMAL)
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

$(GLUELIB): $(PRJROOT)/include/fd/helios_lib.fd $(PRJROOT)/include/clib/helios_protos.h
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"Making library stubs..."$(COLOR_NORMAL)
	$(CVINCLUDE) -q --fd $(PRJROOT)/include/fd/helios_lib.fd \
				 --clib $(PRJROOT)/include/clib/helios_protos.h \
				 --gluelib $(PRJROOT)/lib/libhelios.a
