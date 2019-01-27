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
## Makefile for building FWInspect tool for Helios.
##
##

PRJROOT := $(shell (cd ../../..; pwd))

ALL_SRCS := main.c

include $(PRJROOT)/common.mk

TARGET = FWInspect
CPPFLAGS += -UUSE_INLINE_STDARG
LIBS += -lhelios

all: $(TARGET)

local-clean:
	rm -vf ./$(TARGET)*

local-release: $(TARGET)
	cp $^ $(RELARC_DIR)/
	cp helios_logo.png $(RELARC_DIR)/$(TARGET).info

$(TARGET): $(TARGET).sym
	@$(ECHO) $(COLOR_BOLD)">>"$(COLOR_HIGHLIGHT1)" $@ "$(COLOR_BOLD)": "$(COLOR_HIGHLIGHT2)"$^"$(COLOR_NORMAL)
	$(STRIP) -R.comment -o $@ $@.db; chmod +x $@

$(TARGET).db: $(ALL_SRCS:.c=.o)
	@$(ECHO) $(COLOR_BOLD)">>"$(COLOR_HIGHLIGHT1)" $@ "$(COLOR_BOLD)": "$(COLOR_HIGHLIGHT2)"$^"$(COLOR_NORMAL)
	$(CC) $(CFLAGS) $(CCLDFLAGS) $^ $(LIBS) -o $@
