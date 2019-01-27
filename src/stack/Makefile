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
##

# Declare here all projects directories
PRJROOT := $(shell (cd ../..; pwd))
SUBDIRS := libs/helios devices/ohci1394_pci

include $(PRJROOT)/common.mk

# Just forward commands to project's top makefiles
all local-release local-clean local-distclean :
	@for dir in $(SUBDIRS); do \
		$(ECHO) $(COLOR_BOLD)"-> Enter in"$(COLOR_BOLD)": "$(COLOR_HIGHLIGHT2)"$$dir"$(COLOR_NORMAL); \
		$(MAKE) -C $$dir $(MFLAGS) $@; \
	done;
