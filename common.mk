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
RELVER = $(RELVERSION).$(RELREVISION)-svn_r$(SVN_REV)
RELARC_NAME = Helios_$(RELVER)
RELARC_DIR  = $(TMPDIR)/Helios
RELARC_CLS_DIR = $(RELARC_DIR)/Classes/Helios
RELARC_C = $(RELARC_DIR)/C

# SVN stuff
SVN_REV  := $(shell svnversion $(PRJROOT))
SVN_ROOT := $(shell svn info $(PRJROOT) | grep "Repository Root:" | cut -d ":" -f "2-")

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
	-DDATE='"$(shell /bin/date +%d.%m.%y)"' \
	-DCOPYRIGHTS='"\xa9\x20Guillaume\x20ROGUEZ\x20[SVN:\x20r$(SVN_REV)]"'
CCWARNS = -Wall

ifeq ("$(shell $(CC) -dumpversion | cut -d. -f1)", "4")
CCWARNS += -Wno-pointer-sign
endif

ifneq ("$(VERSION)", "")
DEFINES += -DVERSION=$(VERSION) -DREVISION=$(REVISION)
DEFINES += -DVERSION_STR='"$(VERSION)"' -DREVISION_STR='"$(REVISION)"'
DEFINES += -DVR_ST='"$(VERSION).$(REVISION)"'
endif

CFLAGS = -noixemul -g
CPPFLAGS = $(CFLAGS) $(OPT) $(CCWARNS) $(INCDIRS:%=-I%) $(DEFINES)
LIBS = -L$(PRJROOT)/lib -lhelios -ldebug -lsyscall -lauto

CCLDFLAGS = -noixemul -g -Wl,--traditional-format -Wl,--cref -Wl,--stats -Wl,-Map=$@.map -L$(PRJROOT)/lib
LDFLAGS = --traditional-format --cref --stats -fl libnix -Map=mapfile.txt -L$(PRJROOT)/lib
LDLIBS = -labox -ldebug -lsyscall -lc

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
.PHONY: all clean distclean common-clean common-distclean mkdeps
.PHONY: local-clean local-distclean local-release

.SUFFIXES:
.SUFFIXES: .c .h .s .o .a .d .db .device .library .class .sym

all: mkdeps

debug:;

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

mkdeps: $(ALL_SRCS:.c=.d)

.c.o:
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"$@"$(COLOR_NORMAL)
	$(CC) -c $(CPPFLAGS) $< -o $@

.s.o:
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"$@"$(COLOR_NORMAL)
	$(CC) -c $(CPPFLAGS) $< -o $@

.c.d:
	@$(ECHO) $(COLOR_BOLD)">> "$(COLOR_HIGHLIGHT1)"$@"$(COLOR_BOLD)" : "$(COLOR_HIGHLIGHT2)"$^"$(COLOR_NORMAL)
	$(SHELL) -ec '$(CC) -MM $(CPPFLAGS) $< | sed -e '\''s%\($(notdir $*)\)\.o[ :]*%\1.o $@ : %g'\'' > $@;'

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

#--- SVN help targets ---

.PHONY: svn-release rel-lha rel-tbz release

svn-release:
	svn copy --parents -m "Making release $(RELVERSION).$(RELREVISION)" \
		$(PRJROOT) $(SVN_ROOT)/tags/release-$(RELVERSION).$(RELREVISION)

#--- Archive and Release help targets ---

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

#--- Dependencies section ---

# ALL_SRCS : user defined, all .c files
ifneq ("$(strip $(ALL_SRCS))", "")
ifeq ("$(NODEPS)", "")
-include $(patsubst %.c, %.d, $(filter %.c, $(ALL_SRCS)))
endif
endif
