/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "libsigrokdecode-internal.h" /* First, so we avoid a _POSIX_C_SOURCE warning. */
#include "libsigrokdecode.h"
#include <stdarg.h>
#include <glib.h>
#include <frameobject.h> /* Python header not pulled in by default. */

/** @private */
SRD_PRIV void srd_exception_catch(const char *format, char **error, ...)
{
	PyObject *etype, *evalue, *etb, *py_str;
	PyTracebackObject *py_tb;
	GString *msg;
	va_list args;
	char *ename, *str, *tracestr;

	if (!PyErr_Occurred())
		/* Nothing is wrong. */
		return;

	PyErr_Fetch(&etype, &evalue, &etb);
	PyErr_NormalizeException(&etype, &evalue, &etb);

	if (!(py_str = PyObject_Str(evalue))) {
		/* Shouldn't happen. */
		srd_dbg("Failed to convert exception value to string.");
		return;
	}

	/* Send the exception error message(s) to srd_err(). */
	if (evalue)
		ename = (char *)Py_TYPE(evalue)->tp_name;
	else
		/* Can be NULL. */
		ename = "(unknown exception)";

	msg = g_string_sized_new(128);
	g_string_append(msg, ename);
	g_string_append(msg, ": ");
    va_start(args, error);
	g_string_append_vprintf(msg, format, args);
	va_end(args);
	py_str_as_str(py_str, &str);
	g_string_append(msg, str);
	Py_DecRef(py_str);
	srd_err("%s", msg->str);

	/* Send a more precise error location to srd_dbg(), if we have it. */
	if (etb && etb != Py_None) {
		tracestr = NULL;
		py_tb = (PyTracebackObject *)etb;
		py_str = PyUnicode_FromFormat("%U:%d in %U",
					py_tb->tb_frame->f_code->co_filename,
					py_tb->tb_frame->f_lineno,
					py_tb->tb_frame->f_code->co_name);
		py_str_as_str(py_str, &tracestr);
		Py_DecRef(py_str);
		g_string_printf(msg, "%s in %s: %s", ename, tracestr, str);
		srd_dbg("%s", msg->str);
		g_free(tracestr);
	}
    if (error)
        *error = g_strdup(str);
	g_free(str);
	g_string_free(msg, TRUE);

	Py_XDECREF(etype);
	Py_XDECREF(evalue);
	Py_XDECREF(etb);

	/* Just in case. */
	PyErr_Clear();
}
