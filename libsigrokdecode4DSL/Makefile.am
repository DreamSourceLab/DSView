##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

ACLOCAL_AMFLAGS = -I m4
AM_LIBTOOLFLAGS = --silent
GNUMAKEFLAGS = --no-print-directory

DECODERS_DIR = $(pkgdatadir)/decoders
# Do not hard-code the decoders location on Windows.
if WIN32
AM_CPPFLAGS =
else
AM_CPPFLAGS = -DDECODERS_DIR='"$(DECODERS_DIR)"'
endif

# The tests CFLAGS are a superset of the libsigrokdecode CFLAGS.
AM_CFLAGS = $(SRD_EXTRA_CFLAGS) $(SRD_WFLAGS) $(TESTS_CFLAGS)

lib_LTLIBRARIES = libsigrokdecode4DSL.la

libsigrokdecode4DSL_la_SOURCES = \
	srd.c \
	session.c \
	decoder.c \
	instance.c \
	log.c \
	util.c \
	exception.c \
	module_sigrokdecode.c \
	type_decoder.c \
	type_logic.c \
	error.c \
	version.c

libsigrokdecode4DSL_la_LIBADD = $(SRD_EXTRA_LIBS) $(LIBSIGROKDECODE_LIBS)
libsigrokdecode4DSL_la_LDFLAGS = -version-info $(SRD_LIB_VERSION) -no-undefined

pkginclude_HEADERS = libsigrokdecode.h
nodist_pkginclude_HEADERS = version.h
noinst_HEADERS = libsigrokdecode-internal.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libsigrokdecode4DSL.pc

EXTRA_DIST = Doxyfile HACKING contrib/sigrok-logo-notext.png

if HAVE_CHECK
TESTS = tests/main
check_PROGRAMS = ${TESTS}
endif

tests_main_SOURCES = \
	libsigrokdecode.h \
	tests/lib.h \
	tests/main.c \
	tests/core.c \
	tests/decoder.c \
	tests/inst.c \
	tests/session.c

tests_main_CPPFLAGS = -DDECODERS_TESTDIR='"$(abs_top_srcdir)/decoders"'
tests_main_LDADD = libsigrokdecode4DSL.la $(SRD_EXTRA_LIBS) $(TESTS_LIBS)

MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog install-decoders

ChangeLog:
	git --git-dir '$(top_srcdir)/.git' log >$@ || touch $@

dist-hook: ChangeLog
	$(MKDIR_P) $(distdir)/tools
	cp ${top_srcdir}/tools/install-decoders $(distdir)/tools
	$(MKDIR_P) $(distdir)/decoders
	${top_srcdir}/tools/install-decoders -i ${top_srcdir}/decoders \
		-o $(distdir)/decoders

install-decoders:
	$(MKDIR_P) $(DESTDIR)$(DECODERS_DIR)
	$(PYTHON3) ${top_srcdir}/tools/install-decoders \
		-i ${top_srcdir}/decoders -o $(DESTDIR)$(DECODERS_DIR)

install-data-hook: install-decoders

