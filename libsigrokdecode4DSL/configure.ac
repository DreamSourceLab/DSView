##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2010 Bert Vermeulen <bert@biot.com>
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

# We require at least autoconf 2.63 (AC_INIT format changed there).
AC_PREREQ([2.63])

# libsigrokdecode package version number (NOT the same as shared lib version!).
AC_INIT([libsigrokdecode4DSL], [0.4.0],
	[support@dreamsourcelab.com], [libsigrokdecode4DSL],
	[http://www.dreamsourcelab.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([autostuff])
AC_CONFIG_HEADERS([config.h version.h])

# We require at least automake 1.11 (needed for 'silent rules').
AM_INIT_AUTOMAKE([1.11 -Wall -Werror no-define subdir-objects check-news color-tests])
AM_SILENT_RULES([yes])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_CANONICAL_HOST

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

# Required for per-target flags or subdir-objects with C sources.
AM_PROG_CC_C_O

# Set the standard the C library headers should conform to.
AH_VERBATIM([_POSIX_C_SOURCE], [/* The targeted POSIX standard. */
#ifndef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE 200809L
#endif])

# Get compiler versions.
SR_PROG_VERSION([$CC], [srd_cc_version])

# Initialize libtool.
LT_INIT

# Set up the libsigrokdecode version defines.
SR_PKG_VERSION_SET([SRD_PACKAGE_VERSION], [AC_PACKAGE_VERSION])

# Library version for libsigrokdecode (NOT the same as the package version).
# Carefully read the libtool docs before updating these numbers!
# The algorithm for determining which number to change (and how) is nontrivial!
# http://www.gnu.org/software/libtool/manual/libtool.html#Updating-version-info
SR_LIB_VERSION_SET([SRD_LIB_VERSION], [2:0:0])

############################
##  Package dependencies  ##
############################

# Initialize pkg-config.
# We require at least 0.22, as "Requires.private" behaviour changed there.
PKG_PROG_PKG_CONFIG([0.22])

# Collect the pkg-config module names of all dependencies in SRD_PKGLIBS.
# These are used to derive the compiler flags and for the "Requires.private"
# field in the generated libsigrokdecode.pc file.
SRD_PKGLIBS=
SRD_PKGLIBS_TESTS=

# Keep track of all checked modules so we can list them at the end.
SR_PKG_CHECK_SUMMARY([srd_pkglibs_summary])

# Python 3 is always needed.
SR_PKG_CHECK([python3], [SRD_PKGLIBS],
	[python3 >= 3.2], [python-3.4 >= 3.4], [python-3.3 >= 3.3], [python-3.2 >= 3.2])
AS_IF([test "x$sr_have_python3" = xno],
	[AC_MSG_ERROR([Cannot find Python 3 development headers.])])

# We also need to find the name of the python3 executable (for 'make install').
# Some OSes call this python3, some call it python3.2, etc. etc.
AC_ARG_VAR([PYTHON3], [Python 3 interpreter])
AC_CHECK_PROGS([PYTHON3], [python3.4 python3.3 python3.2 python3])
AS_IF([test "x$PYTHON3" = x],
	[AC_MSG_ERROR([Cannot find Python 3 interpreter.])])

######################
##  Feature checks  ##
######################

# Keep track of all checked modules so we can list them at the end.
SR_PKG_CHECK_SUMMARY([srd_pkglibs_opt_summary])

# The Check unit testing framework is optional. Disable if not found.
SR_PKG_CHECK([check], [SRD_PKGLIBS_TESTS], [check >= 0.9.4])
AM_CONDITIONAL([HAVE_CHECK], [test "x$sr_have_check" = xyes])

# Enable the C99 standard if possible, and enforce the use
# of SRD_API to explicitly mark all public API functions.
SRD_EXTRA_CFLAGS=
SR_CHECK_COMPILE_FLAGS([SRD_EXTRA_CFLAGS], [C99], [-std=c99 -c99 -AC99 -qlanglvl=extc99])
SR_CHECK_COMPILE_FLAGS([SRD_EXTRA_CFLAGS], [visibility], [-fvisibility=hidden])

# Select suitable compiler warning flags.
SR_ARG_ENABLE_WARNINGS([SRD_WFLAGS], [-Wall], [-Wall -Wextra -Wmissing-prototypes])

# Link against libm, this is required (among other things) by Python.
SRD_EXTRA_LIBS=
SR_SEARCH_LIBS([SRD_EXTRA_LIBS], [pow], [m])

AC_SYS_LARGEFILE

##############################
##  Finalize configuration  ##
##############################

AC_SUBST([SRD_PKGLIBS])

# Retrieve the compile and link flags for all modules combined.
# Also, bail out at this point if any module dependency is not met.
PKG_CHECK_MODULES([LIBSIGROKDECODE], [glib-2.0 >= 2.24.0 $SRD_PKGLIBS])
PKG_CHECK_MODULES([TESTS], [$SRD_PKGLIBS_TESTS glib-2.0 $SRD_PKGLIBS])

srd_glib_version=`$PKG_CONFIG --modversion glib-2.0 2>&AS_MESSAGE_LOG_FD`

AC_CONFIG_FILES([Makefile libsigrokdecode4DSL.pc])

AC_OUTPUT

cat >&AS_MESSAGE_FD <<_EOF

libsigrokdecode configuration summary:
 - Package version................. $SRD_PACKAGE_VERSION
 - Library ABI version............. $SRD_LIB_VERSION
 - Prefix.......................... $prefix
 - Building on..................... $build
 - Building for.................... $host

Compile configuration:
 - C compiler...................... $CC
 - C compiler version.............. $srd_cc_version
 - C compiler flags................ $CFLAGS
 - Additional C compiler flags..... $SRD_EXTRA_CFLAGS
 - C compiler warnings............. $SRD_WFLAGS

Detected libraries (required):
 - glib-2.0 >= 2.24.0.............. $srd_glib_version
$srd_pkglibs_summary
Detected libraries (optional):
$srd_pkglibs_opt_summary
_EOF
