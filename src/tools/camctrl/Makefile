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
## Makefile for building FWCamController for Helios.
##
##

PRJROOT := $(shell (cd ../../..; pwd))

ALL_SRCS := mcc_ringbuffer.c mcc_decoder.c mcc_videopreview.c mcc_app.c main.c

include $(PRJROOT)/common.mk

TARGET = FWCamController
CPPFLAGS += -Iinclude
LIBS += -lsyscall -lc -lm -lz -lhelios

ifeq ("$(OS)", "MorphOS")
#DEFINES += WITH_FFMPEG
#LIBS += -Llib -lavformat -lavcodec -lavutil
endif

all: $(TARGET)

local-clean:
	rm -f $(TARGET)

local-release: $(TARGET)
	cp $^ $(RELARC_DIR)/

$(TARGET): $(TARGET).sym
	@$(ECHO) $(COLOR_BOLD)">>"$(COLOR_HIGHLIGHT1)" $@ "$(COLOR_BOLD)": "$(COLOR_HIGHLIGHT2)"$^"$(COLOR_NORMAL)
	$(STRIP) -R.comment -o $@ $@.db; chmod +x $@

$(TARGET).db: $(ALL_SRCS:.c=.o)
	@$(ECHO) $(COLOR_BOLD)">>"$(COLOR_HIGHLIGHT1)" $@ "$(COLOR_BOLD)": "$(COLOR_HIGHLIGHT2)"$^"$(COLOR_NORMAL)
	$(CC) $(CCLDFLAGS) $^ -o $@ $(LIBS)
