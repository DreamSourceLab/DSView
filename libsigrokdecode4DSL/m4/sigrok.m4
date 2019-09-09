##
## This file is part of the sigrok project.
##
## Copyright (C) 2009 Openismus GmbH
## Copyright (C) 2015 Daniel Elstner <daniel.kitta@gmail.com>
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

#serial 20150910

## SR_APPEND(var-name, [list-sep,] element)
##
## Append the shell word <element> to the shell variable named <var-name>,
## prefixed by <list-sep> unless the list was empty before appending. If
## only two arguments are supplied, <list-sep> defaults to a single space
## character.
##
AC_DEFUN([SR_APPEND],
[dnl
m4_assert([$# >= 2])[]dnl
$1=[$]{$1[}]m4_if([$#], [2], [[$]{$1:+' '}$2], [[$]{$1:+$2}$3])[]dnl
])

## SR_PREPEND(var-name, [list-sep,] element)
##
## Prepend the shell word <element> to the shell variable named <var-name>,
## suffixed by <list-sep> unless the list was empty before prepending. If
## only two arguments are supplied, <list-sep> defaults to a single space
## character.
##
AC_DEFUN([SR_PREPEND],
[dnl
m4_assert([$# >= 2])[]dnl
$1=m4_if([$#], [2], [$2[$]{$1:+' '}], [$3[$]{$1:+$2}])[$]$1[]dnl
])

## _SR_PKG_VERSION_SET(var-prefix, pkg-name, tag-prefix, base-version, major, minor, [micro])
##
m4_define([_SR_PKG_VERSION_SET],
[dnl
m4_assert([$# >= 6])[]dnl
$1=$4
sr_git_deps=
# Check if we can get revision information from git.
sr_head=`git -C "$srcdir" rev-parse --verify --short HEAD 2>&AS_MESSAGE_LOG_FD`

AS_IF([test "$?" = 0 && test "x$sr_head" != x], [dnl
	test ! -f "$srcdir/.git/HEAD" \
		|| sr_git_deps="$sr_git_deps \$(top_srcdir)/.git/HEAD"

	sr_head_name=`git -C "$srcdir" rev-parse --symbolic-full-name HEAD 2>&AS_MESSAGE_LOG_FD`
	AS_IF([test "$?" = 0 && test -f "$srcdir/.git/$sr_head_name"],
		[sr_git_deps="$sr_git_deps \$(top_srcdir)/.git/$sr_head_name"])

	# Append the revision hash unless we are exactly on a tagged release.
	git -C "$srcdir" describe --match "$3$4" \
		--exact-match >&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD \
		|| $1="[$]$1-git-$sr_head"
])
# Use $(wildcard) so that things do not break if for whatever
# reason these files do not exist anymore at make time.
AS_IF([test -n "$sr_git_deps"],
	[SR_APPEND([CONFIG_STATUS_DEPENDENCIES], ["\$(wildcard$sr_git_deps)"])])
AC_SUBST([CONFIG_STATUS_DEPENDENCIES])[]dnl
AC_SUBST([$1])[]dnl
dnl
AC_DEFINE([$1_MAJOR], [$5], [Major version number of $2.])[]dnl
AC_DEFINE([$1_MINOR], [$6], [Minor version number of $2.])[]dnl
m4_ifval([$7], [AC_DEFINE([$1_MICRO], [$7], [Micro version number of $2.])])[]dnl
AC_DEFINE_UNQUOTED([$1_STRING], ["[$]$1"], [Version of $2.])[]dnl
])

## SR_PKG_VERSION_SET(var-prefix, version-triple)
##
## Set up substitution variables and macro definitions for the package
## version components. Derive the version suffix from the repository
## revision if possible.
##
## Substitutions: <var-prefix>
## Macro defines: <var-prefix>_{MAJOR,MINOR,MICRO,STRING}
##
AC_DEFUN([SR_PKG_VERSION_SET],
[dnl
m4_assert([$# >= 2])[]dnl
_SR_PKG_VERSION_SET([$1],
	m4_defn([AC_PACKAGE_NAME]),
	m4_defn([AC_PACKAGE_TARNAME])[-],
	m4_expand([$2]),
	m4_unquote(m4_split(m4_expand([$2]), [\.])))
])

## _SR_LIB_VERSION_SET(var-prefix, pkg-name, abi-triple, current, revision, age)
##
m4_define([_SR_LIB_VERSION_SET],
[dnl
m4_assert([$# >= 6])[]dnl
$1=$3
AC_SUBST([$1])[]dnl
AC_DEFINE([$1_CURRENT], [$4], [Binary version of $2.])[]dnl
AC_DEFINE([$1_REVISION], [$5], [Binary revision of $2.])[]dnl
AC_DEFINE([$1_AGE], [$6], [Binary age of $2.])[]dnl
AC_DEFINE([$1_STRING], ["$3"], [Binary version triple of $2.])[]dnl
])

## SR_LIB_VERSION_SET(var-prefix, abi-triple)
##
## Set up substitution variables and macro definitions for a library
## binary version.
##
## Substitutions: <var-prefix>
## Macro defines: <var-prefix>_{CURRENT,REVISION,AGE,STRING}
##
AC_DEFUN([SR_LIB_VERSION_SET],
[dnl
m4_assert([$# >= 1])[]dnl
_SR_LIB_VERSION_SET([$1],
	m4_defn([AC_PACKAGE_NAME]),
	[$2], m4_unquote(m4_split([$2], [:])))
])

## SR_SEARCH_LIBS(libs-var, function, search-libs,
##                [action-if-found], [action-if-not-found], [other-libs])
##
## Same as AC_SEARCH_LIBS, except that the result is prepended
## to <libs-var> instead of LIBS. Calls AC_SUBST on <libs-var>.
##
AC_DEFUN([SR_SEARCH_LIBS],
[dnl
m4_assert([$# >= 3])[]dnl
sr_sl_save_LIBS=$LIBS
AC_SEARCH_LIBS([$2], [$3],,, m4_join([$6], [[$]$1]))
LIBS=$sr_sl_save_LIBS
AS_CASE([$ac_cv_search_$2], [no*],,
	[SR_PREPEND([$1], [$ac_cv_search_$2])])
m4_ifvaln([$4$5], [AS_IF([test "x$ac_cv_search_$2" = xno], [$5], [$4])])[]dnl
AC_SUBST([$1])[]dnl
])

## _SR_VAR_SUMMARY(tag, var-name, line-leader, align-columns, align-char)
##
m4_define([_SR_VAR_SUMMARY], [dnl
$2=
$1_append() {
	sr_aligned=`printf '%.$4s' "[$][1]m4_for([i], [1], [$4],, [$5])"`
	$2="[$]{$2}$3$sr_aligned [$]2"'
'
}
])

## SR_VAR_SUMMARY(tag, [var-name = <tag>],
##                [line-leader = [ - ]], [align-columns = 32], [align-char = .])
##
## Define a shell function <tag>_append() to be used for aggregating
## a summary of test results in the shell variable <var-name>.
##
AC_DEFUN([SR_VAR_SUMMARY],
[dnl
m4_assert([$# >= 1])[]dnl
_SR_VAR_SUMMARY([$1],
	m4_default_quoted([$2], [$1]),
	m4_default_quoted([$3], [ - ]),
	m4_default_quoted([$4], [32]),
	m4_default_quoted([$5], [.]))[]dnl
])

## SR_PKG_CHECK_SUMMARY([var-name = sr_pkg_check_summary],
##                      [line-leader = [ - ]], [align-columns = 32], [align-char = .])
##
## Prepare for the aggregation of package check results
## in the shell variable <var-name>.
##
AC_DEFUN([SR_PKG_CHECK_SUMMARY],
	[SR_VAR_SUMMARY([sr_pkg_check_summary], $@)])

## SR_PKG_CHECK(tag, [collect-var], module...)
##
## Check for each pkg-config <module> in the argument list. <module> may
## include a version constraint.
##
## Output variables: sr_have_<tag>, sr_<tag>_version
##
AC_DEFUN([SR_PKG_CHECK],
[dnl
m4_assert([$# >= 3])[]dnl
AC_REQUIRE([PKG_PROG_PKG_CONFIG])[]dnl
AC_REQUIRE([SR_PKG_CHECK_SUMMARY])[]dnl
dnl
PKG_CHECK_EXISTS([$3], [dnl
	sr_have_$1=yes
	m4_ifval([$2], [SR_APPEND([$2], ["$3"])
	])sr_$1_version=`$PKG_CONFIG --modversion "$3" 2>&AS_MESSAGE_LOG_FD`
	sr_pkg_check_summary_append "$3" "$sr_$1_version"[]dnl
], [dnl
	sr_pkg_check_summary_append "$3" no
	m4_ifval([$4],
		[SR_PKG_CHECK([$1], [$2], m4_shift3($@))],
		[sr_have_$1=no sr_$1_version=])[]dnl
])
])

## SR_VAR_OPT_PKG([modules-var], [features-var])
##
## Enable the collection of SR_ARG_OPT_PKG results into the shell variables
## <modules-var> and <features-var>.
##
AC_DEFUN([SR_VAR_OPT_PKG],
[dnl
m4_define([_SR_VAR_OPT_PKG_MODULES], [$1])[]dnl
m4_define([_SR_VAR_OPT_PKG_FEATURES], [$2])[]dnl
m4_ifvaln([$1], [$1=])[]dnl
m4_ifvaln([$2], [$2=])[]dnl
])

## _SR_ARG_OPT_IMPL(sh-name, [features-var], opt-name,
##                  [cpp-name], [cond-name], check-commands)
##
m4_define([_SR_ARG_OPT_IMPL],
[dnl
AC_ARG_WITH([$3], [AS_HELP_STRING([--without-$3],
			[disable $3 support [default=detect]])])
AS_IF([test "x$with_$1" = xno], [sr_have_$1=no],
	[test "x$sr_have_$1" != xyes], [dnl
AC_MSG_CHECKING([for $3])
$6
AC_MSG_RESULT([$sr_have_$1])[]dnl
])
AS_IF([test "x$with_$1$sr_have_$1" = xyesno],
	[AC_MSG_ERROR([$3 support requested, but it was not found.])])
AS_IF([test "x$sr_have_$1" = xyes], [m4_ifval([$2], [
	SR_APPEND([$2], ["$3"])])[]m4_ifval([$4], [
	AC_DEFINE([HAVE_$4], [1], [Whether $3 is available.])])[]dnl
])
m4_ifvaln([$5], [AM_CONDITIONAL([$5], [test "x$sr_have_$1" = xyes])])[]dnl
])

## _SR_ARG_OPT_CHECK(sh-name, [features-var], opt-name, [cpp-name],
##                   [cond-name], check-commands, [summary-result])
##
m4_define([_SR_ARG_OPT_CHECK],
[dnl
_SR_ARG_OPT_IMPL($@)
sr_pkg_check_summary_append "$3" m4_default([$7], ["$sr_have_$1"])
])

## SR_ARG_OPT_CHECK(opt-name, [cpp-name], [cond-name], check-commands,
##                  [summary-result = $sr_have_<opt-name>])
##
## Provide a --without-<opt-name> configure option for explicit disabling
## of an optional dependency. If not disabled, the availability of the
## optional dependency is auto-detected by running <check-commands>.
##
## The <check-commands> should set the shell variable sr_have_<opt-name>
## to "yes" if the dependency is available, otherwise to "no". Optionally,
## the <summary-result> argument may be used to generate a line in the
## configuration summary. If supplied, it should be a shell word which
## expands to the result to be displayed for the <opt-name> dependency.
##
## Use SR_VAR_OPT_PKG to generate lists of available modules and features.
##
AC_DEFUN([SR_ARG_OPT_CHECK],
[dnl
m4_assert([$# >= 4])[]dnl
AC_REQUIRE([SR_PKG_CHECK_SUMMARY])[]dnl
AC_REQUIRE([SR_VAR_OPT_PKG])[]dnl
dnl
_SR_ARG_OPT_CHECK(m4_expand([AS_TR_SH([$1])]),
	m4_defn([_SR_VAR_OPT_PKG_FEATURES]),
	$@)[]dnl
])

## _SR_ARG_OPT_PKG([features-var], [cond-name], opt-name,
##                 [cpp-name], sh-name, [modules-var], module...)
##
m4_define([_SR_ARG_OPT_PKG],
[dnl
_SR_ARG_OPT_IMPL([$5], [$1], [$3], [$4], [$2],
	[SR_PKG_CHECK(m4_shiftn([4], $@))])
m4_ifvaln([$4], [AS_IF([test "x$sr_have_$5" = xyes],
	[AC_DEFINE_UNQUOTED([CONF_$4_VERSION], ["$sr_$5_version"],
		[Build-time version of $3.])])])[]dnl
])

## SR_ARG_OPT_PKG(opt-name, [cpp-name], [cond-name], module...)
##
## Provide a --without-<opt-name> configure option for explicit disabling
## of an optional dependency. If not disabled, the availability of the
## optional dependency is auto-detected.
##
## Each pkg-config <module> argument is checked in turn, and the first one
## found is selected. On success, the shell variable sr_have_<opt-name>
## is set to "yes", otherwise to "no". Optionally, a preprocessor macro
## HAVE_<cpp-name> and an Automake conditional <cond-name> are generated.
##
## Use SR_VAR_OPT_PKG to generate lists of available modules and features.
##
AC_DEFUN([SR_ARG_OPT_PKG],
[dnl
m4_assert([$# >= 4])[]dnl
AC_REQUIRE([PKG_PROG_PKG_CONFIG])[]dnl
AC_REQUIRE([SR_PKG_CHECK_SUMMARY])[]dnl
AC_REQUIRE([SR_VAR_OPT_PKG])[]dnl
dnl
_SR_ARG_OPT_PKG(m4_defn([_SR_VAR_OPT_PKG_FEATURES]),
	[$3], [$1], [$2],
	m4_expand([AS_TR_SH([$1])]),
	m4_defn([_SR_VAR_OPT_PKG_MODULES]),
	m4_shift3($@))[]dnl
])

## SR_PROG_VERSION(program, sh-var)
##
## Obtain the version of <program> and store it in <sh-var>.
##
AC_DEFUN([SR_PROG_VERSION],
[dnl
m4_assert([$# >= 2])[]dnl
sr_prog_ver=`$1 --version 2>&AS_MESSAGE_LOG_FD | sed 1q 2>&AS_MESSAGE_LOG_FD`
AS_CASE([[$]?:$sr_prog_ver],
	[[0:*[0-9].[0-9]*]], [$2=$sr_prog_ver],
	[$2=unknown])[]dnl
])

## SR_PROG_MAKE_ORDER_ONLY
##
## Check whether the make program supports order-only prerequisites.
## If so, set the substitution variable ORDER to '|', or to the empty
## string otherwise.
##
AC_DEFUN([SR_PROG_MAKE_ORDER_ONLY],
[dnl
AC_CACHE_CHECK([whether [$]{MAKE:-make} supports order-only prerequisites],
	[sr_cv_prog_make_order_only], [
cat >conftest.mk <<'_SREOF'
a: b | c
a b c: ; @:
.PHONY: a b c
_SREOF
AS_IF([[$]{MAKE:-make} -f conftest.mk >&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD],
	[sr_cv_prog_make_order_only=yes], [sr_cv_prog_make_order_only=no])
rm -f conftest.mk
])
AS_IF([test "x$sr_cv_prog_make_order_only" = xyes], [ORDER='|'], [ORDER=])
AC_SUBST([ORDER])
AM_SUBST_NOTMAKE([ORDER])[]dnl
])

## SR_CHECK_COMPILE_FLAGS(flags-var, description, flags)
##
## Find a compiler flag for <description>. For each flag in <flags>, check
## if the compiler for the current language accepts it. On success, stop the
## search and append the last tested flag to <flags-var>. Calls AC_SUBST
## on <flags-var>.
##
AC_DEFUN([SR_CHECK_COMPILE_FLAGS],
[dnl
m4_assert([$# >= 3])[]dnl
AC_MSG_CHECKING([compiler flag for $2])
sr_ccf_result=no
sr_ccf_save_CPPFLAGS=$CPPFLAGS
for sr_flag in $3
do
	CPPFLAGS="$sr_ccf_save_CPPFLAGS $sr_flag"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])], [sr_ccf_result=$sr_flag])
	test "x$sr_ccf_result" = xno || break
done
CPPFLAGS=$sr_ccf_save_CPPFLAGS
AS_IF([test "x$sr_ccf_result" != xno],
	[SR_APPEND([$1], [$sr_ccf_result])])
AC_MSG_RESULT([$sr_ccf_result])
AC_SUBST([$1])
])

## _SR_ARG_ENABLE_WARNINGS_ONCE
##
## Implementation helper macro of SR_ARG_ENABLE_WARNINGS. Pulled in
## through AC_REQUIRE so that it is only expanded once.
##
m4_define([_SR_ARG_ENABLE_WARNINGS_ONCE],
[dnl
AC_PROVIDE([$0])[]dnl
AC_ARG_ENABLE([warnings],
		[AS_HELP_STRING([[--enable-warnings[=min|max|fatal|no]]],
				[set compile pedantry level [default=max]])],
		[sr_enable_warnings=$enableval],
		[sr_enable_warnings=max])[]dnl
dnl
# Test whether the compiler accepts each flag.  Look at standard output,
# since GCC only shows a warning message if an option is not supported.
sr_check_compile_warning_flags() {
	for sr_flag
	do
		sr_cc_out=`$sr_cc $sr_warning_flags $sr_flag -c "$sr_conftest" 2>&1 || echo failed`
		AS_IF([test "$?$sr_cc_out" = 0],
			[SR_APPEND([sr_warning_flags], [$sr_flag])],
			[AS_ECHO(["$sr_cc: $sr_cc_out"]) >&AS_MESSAGE_LOG_FD])
		rm -f "conftest.[$]{OBJEXT:-o}"
	done
}
])

## SR_ARG_ENABLE_WARNINGS(variable, min-flags, max-flags)
##
## Provide the --enable-warnings configure argument, set to "min" by default.
## <min-flags> and <max-flags> should be space-separated lists of compiler
## warning flags to use with --enable-warnings=min or --enable-warnings=max,
## respectively. Warning level "fatal" is the same as "max" but in addition
## enables -Werror mode.
##
## In order to determine the warning options to use with the C++ compiler,
## call AC_LANG([C++]) first to change the current language. If different
## output variables are used, it is also fine to call SR_ARG_ENABLE_WARNINGS
## repeatedly, once for each language setting.
##
AC_DEFUN([SR_ARG_ENABLE_WARNINGS],
[dnl
m4_assert([$# >= 3])[]dnl
AC_REQUIRE([_SR_ARG_ENABLE_WARNINGS_ONCE])[]dnl
dnl
AS_CASE([$ac_compile],
	[[*'$CXXFLAGS '*]], [sr_lang='C++' sr_cc=$CXX sr_conftest="conftest.[$]{ac_ext:-cc}"],
	[[*'$CFLAGS '*]],   [sr_lang=C sr_cc=$CC sr_conftest="conftest.[$]{ac_ext:-c}"],
	[AC_MSG_ERROR([[current language is neither C nor C++]])])
dnl
AC_MSG_CHECKING([which $sr_lang compiler warning flags to use])
sr_warning_flags=
AC_LANG_CONFTEST([AC_LANG_SOURCE([[
int main(int argc, char** argv) { return (argv != 0) ? argc : 0; }
]])])
AS_CASE([$sr_enable_warnings],
	[no], [],
	[min], [sr_check_compile_warning_flags $2],
	[fatal], [sr_check_compile_warning_flags $3 -Werror],
	[sr_check_compile_warning_flags $3])
rm -f "$sr_conftest"
AC_SUBST([$1], [$sr_warning_flags])
AC_MSG_RESULT([[$]{sr_warning_flags:-none}])[]dnl
])
