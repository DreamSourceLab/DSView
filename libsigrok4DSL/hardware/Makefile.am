##
## This file is part of the libsigrok project.
##
## Copyright (C) 2011 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright (C) 2012 Alexandru Gagniuc <mr.nuke.me@gmail.com>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

SUBDIRS = \
	demo \
	common \
	DSL

noinst_LTLIBRARIES = libsigrok4DSLhardware.la

libsigrok4DSLhardware_la_SOURCES =

libsigrok4DSLhardware_la_LIBADD = 

if HW_DEMO
libsigrok4DSLhardware_la_LIBADD += demo/libsigrok4DSL_hw_demo.la
endif

libsigrok4DSLhardware_la_LIBADD += common/libsigrok4DSL_hw_common.la
libsigrok4DSLhardware_la_LIBADD += DSL/libsigrok4DSL_hw_dsl.la
