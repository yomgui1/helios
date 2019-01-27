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
## Makefile for building all Helios stuff.
##
##

PRJROOT := $(shell (cd .; pwd))
SUBDIRS := src

include $(PRJROOT)/common.mk

EXAMPLES := $(shell ls src/examples/*.c)

all:
	$(MAKE) -C src all

local-release: docs/release/README docs/release/FAQ src/examples
	-rm -rvf $(RELARC_DIR)
	mkdir -p $(RELARC_DIR) $(RELARC_DIR)/SDK/Examples $(RELARC_C)
	sed -e "s/@\(RELVER\)@/$(RELVER)/g" docs/release/README | sed -e "s/yomgui1@gmail.com/yomgui1 gmail com/g" > $(RELARC_DIR)/README
	$(CP) docs/release/FAQ $(RELARC_DIR)
	sed -e "s/\.\.\/\.\./../g" src/examples/Makefile > $(RELARC_DIR)/SDK/Examples/Makefile
	$(CP) $(EXAMPLES) $(RELARC_DIR)/SDK/Examples/
	$(MAKE) -iC src local-release
