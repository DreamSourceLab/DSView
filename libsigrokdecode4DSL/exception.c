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
#include "log.h"

static char *py_stringify(PyObject *py_obj)
{
	PyObject *py_str, *py_bytes;
	char *str = NULL;

	/* Note: Caller already ran PyGILState_Ensure(). */

	if (!py_obj)
		return NULL;

	py_str = PyObject_Str(py_obj);
	if (!py_str || !PyUnicode_Check(py_str))
		goto cleanup;

	py_bytes = PyUnicode_AsUTF8String(py_str);
	if (!py_bytes)
		goto cleanup;

	str = g_strdup(PyBytes_AsString(py_bytes));
	Py_DECREF(py_bytes);

cleanup:
	Py_XDECREF(py_str);
	if (!str) {
		PyErr_Clear();
		srd_err("Failed to stringify object.");
	}

	return str;
}

static char *py_get_string_attr(PyObject *py_obj, const char *attr)
{
	PyObject *py_str, *py_bytes;
	char *str = NULL;

	/* Note: Caller already ran PyGILState_Ensure(). */

	if (!py_obj)
		return NULL;

	py_str = PyObject_GetAttrString(py_obj, attr);
	if (!py_str || !PyUnicode_Check(py_str))
		goto cleanup;

	py_bytes = PyUnicode_AsUTF8String(py_str);
	if (!py_bytes)
		goto cleanup;

	str = g_strdup(PyBytes_AsString(py_bytes));
	Py_DECREF(py_bytes);

cleanup:
	Py_XDECREF(py_str);
	if (!str) {
		PyErr_Clear();
		srd_err("Failed to get object attribute %s.", attr);
	}

	return str;
}

/** @private */
SRD_PRIV void srd_exception_catch(char **error, const char *format, ...)
{
	int i, ret;
	va_list args;
	PyObject *py_etype, *py_evalue, *py_etraceback;
	PyObject *py_mod, *py_func, *py_tracefmt;
	char *msg, *etype_name, *evalue_str, *outstr;
	const char *etype_name_fallback;
	PyGILState_STATE gstate;
	GString *s;
	char *final_msg;

	py_etype = py_evalue = py_etraceback = py_mod = py_func = NULL;

	va_start(args, format);
	msg = g_strdup_vprintf(format, args);
	va_end(args);

	gstate = PyGILState_Ensure();

	PyErr_Fetch(&py_etype, &py_evalue, &py_etraceback);
	if (!py_etype) {
		/* No current exception, so just print the message. */
		final_msg = g_strjoin(":", msg, "unknown error", NULL);
		srd_err("%s.", final_msg);
		goto cleanup;
	}
	PyErr_NormalizeException(&py_etype, &py_evalue, &py_etraceback);

	etype_name = py_get_string_attr(py_etype, "__name__");
	evalue_str = py_stringify(py_evalue);
	etype_name_fallback = (etype_name) ? etype_name : "(unknown exception)";

	if (evalue_str)
		final_msg = g_strjoin(":", msg, etype_name_fallback, evalue_str, NULL);
	else
		final_msg = g_strjoin(":", msg, etype_name_fallback, NULL);

	g_free(evalue_str);
	g_free(etype_name);

	srd_err("%s.", final_msg);

	/* If there is no traceback object, we are done. */
	if (!py_etraceback)
		goto cleanup;

	py_mod = py_import_by_name("traceback");
	if (!py_mod)
		goto cleanup;

	py_func = PyObject_GetAttrString(py_mod, "format_exception");
	if (!py_func || !PyCallable_Check(py_func))
		goto cleanup;

	/* Call into Python to format the stack trace. */
	py_tracefmt = PyObject_CallFunctionObjArgs(py_func,
			py_etype, py_evalue, py_etraceback, NULL);
	if (!py_tracefmt || !PyList_Check(py_tracefmt))
		goto cleanup;

	s = g_string_sized_new(128);
	for (i = 0; i < PyList_Size(py_tracefmt); i++) {
		ret = py_listitem_as_str(py_tracefmt, i, &outstr);
		if (ret == 0) {
			s = g_string_append(s, outstr);
			g_free(outstr);
		}
	}
	srd_err("%s", s->str);
	g_string_free(s, TRUE);

	Py_DECREF(py_tracefmt);

cleanup:
	if (error)
		*error = g_strdup(final_msg);
	Py_XDECREF(py_func);
	Py_XDECREF(py_mod);
	Py_XDECREF(py_etraceback);
	Py_XDECREF(py_evalue);
	Py_XDECREF(py_etype);

	/* Just in case. */
	PyErr_Clear();

	PyGILState_Release(gstate);

	g_free(msg);
	g_free(final_msg);
}
