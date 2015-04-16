##
## This file is part of the libsigrok4DSLogic project.
##
## Copyright (C) 2010-2012 Bert Vermeulen <bert@biot.com>
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

ACLOCAL_AMFLAGS = -I autostuff

AM_CPPFLAGS = -I$(top_srcdir)

SUBDIRS = hardware input output tests

lib_LTLIBRARIES = libsigrok4DSLogic.la

libsigrok4DSLogic_la_SOURCES = \
	backend.c \
	device.c \
	session.c \
	session_file.c \
	session_driver.c \
	hwdriver.c \
	filter.c \
	strutil.c \
	log.c \
        trigger.c \
	version.c \
	error.c \
	std.c

libsigrok4DSLogic_la_LIBADD = \
	$(LIBOBJS) \
	hardware/libsigrok4DSLogichardware.la \
	input/libsigrok4DSLogicinput.la \
	output/libsigrok4DSLogicoutput.la

libsigrok4DSLogic_la_LDFLAGS = $(SR_LIB_LDFLAGS)

library_includedir = $(includedir)/libsigrok4DSLogic
library_include_HEADERS = libsigrok.h proto.h version.h
noinst_HEADERS = libsigrok-internal.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libsigrok4DSLogic.pc

EXTRA_DIST = Doxyfile README.devices

MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog
ChangeLog:
	git --git-dir $(top_srcdir)/.git log > ChangeLog || touch ChangeLog

dist-hook: ChangeLog

