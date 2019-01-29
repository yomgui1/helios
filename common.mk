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
## Common part used by all project Makefile's.
##

export
unexport SUBDIRS

# Mercurial stuff
SCM_REV := $(shell git describe --always --long --dirty)

# Paths stuff
LIBS_DIR := $(PRJROOT)/libs
DEVS_DIR := $(PRJROOT)/devs
CLS_DIR  := $(PRJROOT)/classes/Helios

VPATH    := $(PRJROOT):$(PRJROOT)/include:$(PRJROOT)/lib:$(LIBS_DIR):$(DEVS_DIR):$(CLS_DIR):.

OS      := $(shell uname)

CP      := cp -av
ECHO    := echo -e

NODEPS := $(filter clean distclean,$(MAKECMDGOALS))

INCDIRS := . $(PRJROOT)/include $(PRJROOT)/src/common

ifeq ("$(OS)", "MorphOS")
TMPDIR := RAM:t
else
TMPDIR := /tmp
endif

RELVERSION  = 0
RELREVISION = 0
RELVER = $(RELVERSION).$(RELREVISION)-$(SCM_REV)
RELARC_NAME = Helios_$(RELVER)
RELARC_DIR  = $(TMPDIR)/Helios
RELARC_CLS_DIR = $(RELARC_DIR)/Classes/Helios
RELARC_C = $(RELARC_DIR)/C

#--- MorphOS dev tools framework ---

# GCC/Binutils
CC      := ppc-morphos-gcc
LD      := ppc-morphos-ld
AR      := ppc-morphos-ar
NM      := ppc-morphos-nm
STRIP   := ppc-morphos-strip

ARFLAGS = rcs

OPT = -O2 -mmultiple -mstring -fno-strict-aliasing
DEFINES := $(CCDEFINES) \
	-DUSE_INLINE_STDARG \
	-DAROS_ALMOST_COMPATIBLE \
	-DSCM_REV='"$(SCM_REV)"' \
	-DBUILD_DATE='"$(shell /bin/date +%d.%m.%y)"'
CCWARNS = -Wall

ifeq ("$(shell $(CC) -dumpversion | cut -d. -f1)", "4")
CCWARNS += -Wno-pointer-sign
endif

CFLAGS = -noixemul -g
CPPFLAGS = $(CFLAGS) $(OPT) $(CCWARNS) $(INCDIRS:%=-I%) $(DEFINES)
LIBS = -L$(PRJROOT)/lib -lhelios -ldebug -lauto

CCLDFLAGS = -noixemul -g -Wl,--traditional-format -Wl,--cref -Wl,--stats -Wl,-Map=$@.map -L$(PRJROOT)/lib -L/usr/lib -L/usr/local/lib
LDFLAGS = --traditional-format --cref --stats -fl libnix -Map=mapfile.txt -L$(PRJROOT)/lib -L/usr/lib -L/usr/local/lib
LDLIBS = -labox -ldebug -lc

ifneq ("$(OS)", "MorphOS")
LDLIBS += -lmath
endif

# Other build stuff
CVINCLUDE := cvinclude.pl

#--- Echo beautifull stuff ---

COLOR_EMPHASIZE  = "\033[37m"
COLOR_HIGHLIGHT1 = "\033[33m"
COLOR_HIGHLIGHT2 = "\033[32m"
COLOR_BOLD       = "\033[0m\033[1m"
COLOR_NORMAL     = "\033[0m"

ifeq ("$(OS)", "MorphOS")
FLUSH := FlushLib quiet
else
FLUSH := true
endif

#--- Build help targets ---

.DEFAULT: all
.PHONY: all clean distclean common-clean common-distclean
.PHONY: local-clean local-distclean local-release mkdeps

.SUFFIXES:
.SUFFIXES: .c .h .s .o .a .d .db .device .library .class .sym

all: mkdeps

debug:;

mkdeps: $(ALL_SRCS:.c=.d)

$(addsuffix -%,$(SUBDIRS)):
	$(MAKE) -C $(patsubst %-$*,%,$@) $*

common-clean:
	rm -vf *.[ao] mapfile.txt *.map *.sym *.db

common-distclean: $(common_clean)
	rm -vf *.d *~ *.tmp

clean: common-clean local-clean
	@for dir in $(SUBDIRS); do \
		$(ECHO) $(COLOR_BOLD)"++ "$(COLOR_HIGHLIGHT1)"Clean in"$(COLOR_BOLD)": "$(COLOR_HIGHLIGHT2)"$$dir"$(COLOR_NORMAL); \
		(cd $$dir && $(MAKE) $(MFLAGS) --no-print-directory clean); \
	done;

distclean: clean common-distclean local-distclean
	@for dir in $(SUBDIRS); do \
		$(ECHO) $(COLOR_BOLD)"++ "$(COLOR_HIGHLIGHT1)"Distclean in"$(COLOR_BOLD)": "$(COLOR_HIGHLIGHT2)"$$dir"$(COLOR_NORMAL); \
		(cd $$dir && $(MAKE) $(MFLAGS) --no-print-directory distclean); \
	done;

.c.o:
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"$@"$(COLOR_NORMAL)
	$(CC) -c $(CPPFLAGS) $< -o $@

.s.o:
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"$@"$(COLOR_NORMAL)
	$(CC) -c $(CPPFLAGS) $< -o $@

.c.d:
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"$@"$(COLOR_NORMAL)
	$(SHELL) -ec '$(CC) -MM $(CPPFLAGS) $< | sed -e '\''s%\($(notdir $*)\)\.o[ :]*%\1.o $@ : %g'\'' > $@; [ -s $@ ] || rm -fv $@'

%.library: %.library.db
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"$@"$(COLOR_NORMAL)
	$(STRIP) -R.comment -o $@ $<
	-$(FLUSH) $(notdir $@)

%.device: %.device.db
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"$@"$(COLOR_NORMAL)
	$(STRIP) -R.comment -o $@ $<
	-$(FLUSH) $(notdir $@)

%.class: %.class.db
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"$@"$(COLOR_NORMAL)
	$(STRIP) -R.comment -o $@ $<
	-$(FLUSH) $(notdir $@)

%.sym: %.db
	$(NM) -n $^ >$@

#--- Archive and Release help targets ---

.PHONY: rel-lha rel-tbz release

rel-lha:
	-delete force $(RELARC_DIR)//$(RELARC_NAME).lha
	(cd $(RELARC_DIR)/..; lha -eraxFY3 r $(RELARC_NAME).lha $(notdir $(RELARC_DIR)))

rel-tbz:
	rm -vf $(RELARC_DIR)/../$(RELARC_NAME).tar
	(cd $(RELARC_DIR)/..; tar --show-stored-names -vcf $(RELARC_NAME).tar $(notdir $(RELARC_DIR)) && bzip2 -vz9f $(RELARC_NAME).tar)

local-release:

release: local-release

ifeq ("$(OS)", "MorphOS")
release: rel-lha
else
release: rel-tbz
endif

astyle:
	astyle --options=$(PRJROOT)/.astylerc -RQ $(PRJROOT)/src/*.c $(PRJROOT)/src/*.h $(PRJROOT)/include/*.h

#--- Dependencies section ---

# ALL_SRCS : user defined, all .c files
ifneq ("$(strip $(ALL_SRCS))", "")
ifeq ("$(NODEPS)", "")
ifeq ("$(test -f $(PRJROOT)/include/ppcinline/heliosclsbase.h && echo yes)", "yes")
-include $(patsubst %.c, %.d, $(filter %.c, $(ALL_SRCS)))
endif
endif
endif
