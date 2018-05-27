/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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
/**
 * Import a Python module by name.
 *
 * This function is implemented in terms of PyImport_Import() rather than
 * PyImport_ImportModule(), so that the import hooks are not bypassed.
 *
 * @param[in] name The name of the module to load as UTF-8 string.
 * @return The Python module object, or NULL if an exception occurred. The
 *  caller is responsible for evaluating and clearing the Python error state.
 *
 * @private
 */
SRD_PRIV PyObject *py_import_by_name(const char *name)
{
	PyObject *py_mod, *py_modname;

	py_modname = PyUnicode_FromString(name);
	if (!py_modname)
		return NULL;

	py_mod = PyImport_Import(py_modname);
	Py_DECREF(py_modname);

	return py_mod;
}

/**
 * Get the value of a Python object's attribute, returned as a newly
 * allocated char *.
 *
 * @param[in] py_obj The object to probe.
 * @param[in] attr Name of the attribute to retrieve.
 * @param[out] outstr ptr to char * storage to be filled in.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *         The 'outstr' argument points to a g_malloc()ed string upon success.
 *
 * @private
 */
SRD_PRIV int py_attr_as_str(PyObject *py_obj, const char *attr, char **outstr)
{
	PyObject *py_str;
	int ret;

	if (!PyObject_HasAttrString(py_obj, attr)) {
		srd_dbg("Object has no attribute '%s'.", attr);
		return SRD_ERR_PYTHON;
	}

	if (!(py_str = PyObject_GetAttrString(py_obj, attr))) {
		srd_exception_catch(NULL, "Failed to get attribute '%s'", attr);
		return SRD_ERR_PYTHON;
	}

	ret = py_str_as_str(py_str, outstr);
	Py_DECREF(py_str);

	return ret;
}

/**
 * Get the value of a Python dictionary item, returned as a newly
 * allocated char *.
 *
 * @param[in] py_obj The dictionary to probe.
 * @param[in] key Key of the item to retrieve.
 * @param[out] outstr Pointer to char * storage to be filled in.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *         The 'outstr' argument points to a g_malloc()ed string upon success.
 *
 * @private
 */
SRD_PRIV int py_dictitem_as_str(PyObject *py_obj, const char *key,
				char **outstr)
{
	PyObject *py_value;

	if (!PyDict_Check(py_obj)) {
		srd_dbg("Object is not a dictionary.");
		return SRD_ERR_PYTHON;
	}

	if (!(py_value = PyDict_GetItemString(py_obj, key))) {
		srd_dbg("Dictionary has no attribute '%s'.", key);
		return SRD_ERR_PYTHON;
	}

	return py_str_as_str(py_value, outstr);
}

/**
 * Get the value of a Python dictionary item, returned as a int.
 *
 * @param[in] py_obj The dictionary to probe.
 * @param[in] key Key of the item to retrieve.
 *
 * @return -1 upon failed
 *
 * @private
 */
SRD_PRIV int py_dictitem_to_int(PyObject *py_obj, const char *key)
{
	PyObject *py_value;
	long type;

	if (!PyDict_Check(py_obj)) {
		srd_dbg("Object is not a dictionary.");
		return -1;
	}

	if (!(py_value = PyDict_GetItemString(py_obj, key))) {
		srd_dbg("Dictionary has no attribute '%s'.", key);
		return -1;
	}

	type = PyLong_Check(py_value) ? PyLong_AsLong(py_value) : -1;

	return type;
}

/**
 * Get the value of a Python unicode string object, returned as a newly
 * allocated char *.
 *
 * @param[in] py_str The unicode string object.
 * @param[out] outstr ptr to char * storage to be filled in.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *         The 'outstr' argument points to a g_malloc()ed string upon success.
 *
 * @private
 */
SRD_PRIV int py_str_as_str(PyObject *py_str, char **outstr)
{
	PyObject *py_bytes;
	char *str;

	if (!PyUnicode_Check(py_str)) {
		srd_dbg("Object is not a string object.");
		return SRD_ERR_PYTHON;
	}

	py_bytes = PyUnicode_AsUTF8String(py_str);
	if (py_bytes) {
		str = g_strdup(PyBytes_AsString(py_bytes));
		Py_DECREF(py_bytes);
		if (str) {
			*outstr = str;
			return SRD_OK;
		}
	}
	srd_exception_catch(NULL, "Failed to extract string");

	return SRD_ERR_PYTHON;
}

/**
 * Convert a Python list of unicode strings to a C string vector.
 * On success, a pointer to a newly allocated NULL-terminated array of
 * allocated C strings is written to @a out_strv. The caller must g_free()
 * each string and the array itself.
 *
 * @param[in] py_strseq The sequence object.
 * @param[out] out_strv Address of string vector to be filled in.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @private
 */
SRD_PRIV int py_strseq_to_char(PyObject *py_strseq, char ***out_strv)
{
	PyObject *py_item, *py_bytes;
	char **strv, *str;
	ssize_t seq_len, i;

	if (!PySequence_Check(py_strseq)) {
		srd_err("Object does not provide sequence protocol.");
		return SRD_ERR_PYTHON;
	}

	seq_len = PySequence_Size(py_strseq);
	if (seq_len < 0) {
		srd_exception_catch(NULL, "Failed to obtain sequence size");
		return SRD_ERR_PYTHON;
	}

	strv = g_try_new0(char *, seq_len + 1);
	if (!strv) {
		srd_err("Failed to allocate result string vector.");
		return SRD_ERR_MALLOC;
	}

	for (i = 0; i < seq_len; i++) {
		py_item = PySequence_GetItem(py_strseq, i);
		if (!py_item)
			goto err_out;

		if (!PyUnicode_Check(py_item)) {
			Py_DECREF(py_item);
			goto err_out;
		}
		py_bytes = PyUnicode_AsUTF8String(py_item);
		Py_DECREF(py_item);
		if (!py_bytes)
			goto err_out;

		str = g_strdup(PyBytes_AsString(py_bytes));
		Py_DECREF(py_bytes);
		if (!str)
			goto err_out;

		strv[i] = str;
	}
	*out_strv = strv;

	return SRD_OK;

err_out:
	g_strfreev(strv);
	srd_exception_catch(NULL, "Failed to obtain string item");

	return SRD_ERR_PYTHON;
}

/**
 * Convert a Python scalar object to a GLib variant.
 * Supported variant types are string, int64 and double.
 *
 * @param[in] py_obj The Python object. Must not be NULL.
 * @return A floating reference to a new variant, or NULL on failure.
 *
 * @private
 */
SRD_PRIV GVariant *py_obj_to_variant(PyObject *py_obj)
{
	GVariant *var = NULL;

	if (PyUnicode_Check(py_obj)) { /* string */
		PyObject *py_bytes;
		const char *str;

		py_bytes = PyUnicode_AsUTF8String(py_obj);
		if (py_bytes) {
			str = PyBytes_AsString(py_bytes);
			if (str)
				var = g_variant_new_string(str);
			Py_DECREF(py_bytes);
		}
		if (!var)
			srd_exception_catch(NULL, "Failed to extract string value");

	} else if (PyLong_Check(py_obj)) { /* integer */
		int64_t val;

		val = PyLong_AsLongLong(py_obj);
		if (!PyErr_Occurred())
			var = g_variant_new_int64(val);
		else
			srd_exception_catch(NULL, "Failed to extract integer value");

	} else if (PyFloat_Check(py_obj)) { /* float */
		double val;

		val = PyFloat_AsDouble(py_obj);
		if (!PyErr_Occurred())
			var = g_variant_new_double(val);
		else
			srd_exception_catch(NULL, "Failed to extract float value");

	} else {
		srd_err("Failed to extract value of unsupported type.");
	}

	return var;
}
