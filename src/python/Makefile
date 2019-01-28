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
## Makefile for building the Python Helios binding module.
##
##

PRJROOT  := ../..

SRCS := helios.c
OBJS := $(SRCS:.c=.o)

ALL_SRCS := $(SRCS)

include $(PRJROOT)/common.mk

.PHONY: helios

all: helios

local-clean:
	-rm -rvf build

local-release: helios
	python setup.py -q install --root $(RELARC_DIR)

helios: setup.py
	@echo $(COLOR_BOLD)">>"$(COLOR_HIGHLIGHT1)" $@ "$(COLOR_BOLD)": "$(COLOR_HIGHLIGHT2)"$^"$(COLOR_NORMAL)
	python setup.py -q build_ext

local: $(SRCS)
	python setup.py build_ext -fg install
