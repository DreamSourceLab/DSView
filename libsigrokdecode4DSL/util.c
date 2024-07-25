/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
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
#include "log.h"

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
	PyGILState_STATE gstate;

	gstate = PyGILState_Ensure();
  
	py_modname = PyUnicode_FromString(name);
	if (!py_modname) {
		PyGILState_Release(gstate);
		return NULL;
	} 
 
	py_mod = PyImport_Import(py_modname);
	Py_DECREF(py_modname);

	PyGILState_Release(gstate);

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
 *         The 'outstr' argument points to a g_try_malloc0()ed string upon success.
 *
 * @private
 */
SRD_PRIV int py_attr_as_str(PyObject *py_obj, const char *attr, char **outstr)
{
	PyObject *py_str;
	int ret;
	PyGILState_STATE gstate;

	gstate = PyGILState_Ensure();

	if (!PyObject_HasAttrString(py_obj, attr)) {
		srd_dbg("Object has no attribute '%s'.", attr);
		goto err;
	}

	if (!(py_str = PyObject_GetAttrString(py_obj, attr))) {
        srd_exception_catch(NULL, "Failed to get attribute '%s'", attr);
		goto err;
	}

	ret = py_str_as_str(py_str, outstr);
	Py_DECREF(py_str);

	PyGILState_Release(gstate);

	return ret;

err:
	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
}

/**
 * Get the value of a Python object's attribute, returned as a newly
 * allocated GSList of char *.
 *
 * @param[in] py_obj The object to probe.
 * @param[in] attr Name of the attribute to retrieve.
 * @param[out] outstrlist ptr to GSList of char * storage to be filled in.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *         The 'outstrlist' argument points to a GSList of g_try_malloc0()ed strings
 *         upon success.
 *
 * @private
 */
SRD_PRIV int py_attr_as_strlist(PyObject *py_obj, const char *attr, GSList **outstrlist)
{
	PyObject *py_list;
    int i;
	int ret;
	char *outstr;
	PyGILState_STATE gstate;

	gstate = PyGILState_Ensure();

	if (!PyObject_HasAttrString(py_obj, attr)) {
		srd_dbg("Object has no attribute '%s'.", attr);
		goto err;
	}

	if (!(py_list = PyObject_GetAttrString(py_obj, attr))) {
        srd_exception_catch(NULL, "Failed to get attribute '%s'", attr);
		goto err;
	}

	if (!PyList_Check(py_list)) {
		srd_dbg("Object is not a list.");
		goto err;
	}

	*outstrlist = NULL;

	for (i = 0; i < PyList_Size(py_list); i++) {
		ret = py_listitem_as_str(py_list, i, &outstr);
		if (ret < 0) {
            srd_dbg("Couldn't get item %d.", i);
			goto err;
		}
		*outstrlist = g_slist_append(*outstrlist, outstr);
	}

	Py_DECREF(py_list);

	PyGILState_Release(gstate);

	return SRD_OK;

err:
	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
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
 *         The 'outstr' argument points to a g_try_malloc0()ed string upon success.
 *
 * @private
 */
SRD_PRIV int py_dictitem_as_str(PyObject *py_obj, const char *key,
				char **outstr)
{
	PyObject *py_value;
	PyGILState_STATE gstate;

	gstate = PyGILState_Ensure();

	if (!PyDict_Check(py_obj)) {
		srd_dbg("Object is not a dictionary.");
		goto err;
	}

	if (!(py_value = PyDict_GetItemString(py_obj, key))) {
		goto err;
	}

	PyGILState_Release(gstate);

	return py_str_as_str(py_value, outstr);

err:
	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
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
    PyGILState_STATE gstate;

    gstate = PyGILState_Ensure();

    if (!PyDict_Check(py_obj)) {
        srd_dbg("Object is not a dictionary.");
        goto err;
    }

    if (!(py_value = PyDict_GetItemString(py_obj, key))) {
        goto err;
    }

    type = PyLong_Check(py_value) ? PyLong_AsLong(py_value) : SRD_ERR;
    return type;

err:
    PyGILState_Release(gstate);
    return SRD_ERR;
}

/**
 * Get the value of a Python list item, returned as a newly
 * allocated char *.
 *
 * @param[in] py_obj The list to probe.
 * @param[in] idx Index of the list item to retrieve.
 * @param[out] outstr Pointer to char * storage to be filled in.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *         The 'outstr' argument points to a g_try_malloc0()ed string upon success.
 *
 * @private
 */
SRD_PRIV int py_listitem_as_str(PyObject *py_obj, int idx,
				char **outstr)
{
	PyObject *py_value;
	PyGILState_STATE gstate;

	gstate = PyGILState_Ensure();

	if (!PyList_Check(py_obj)) {
		srd_dbg("Object is not a list.");
		goto err;
	}

	if (!(py_value = PyList_GetItem(py_obj, idx))) {
        srd_dbg("Couldn't get list item %d.", idx);
		goto err;
	}

	PyGILState_Release(gstate);

	return py_str_as_str(py_value, outstr);

err:
	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
}

/**
 * Get the value of a Python dictionary item, returned as a newly
 * allocated char *.
 *
 * @param py_obj The dictionary to probe.
 * @param py_key Key of the item to retrieve.
 * @param outstr Pointer to char * storage to be filled in.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *         The 'outstr' argument points to a g_try_malloc0()ed string upon success.
 *
 * @private
 */
SRD_PRIV int py_dict_value_to_str(PyObject *py_obj, PyObject *py_key,
				char **outstr)
{
	PyObject *py_value;
	PyGILState_STATE gstate;

	if (!py_obj || !py_key || !outstr)
		return SRD_ERR_ARG;

	gstate = PyGILState_Ensure();

	if (!PyDict_Check(py_obj)) {
		srd_dbg("Object is not a dictionary.");
		goto err;
	}

	if (!(py_value = PyDict_GetItem(py_obj, py_key))) {
		srd_dbg("Dictionary has no such key.");
		goto err;
	}

	if (!PyUnicode_Check(py_value)) {
		srd_dbg("Dictionary value should be a string.");
		goto err;
	}

	PyGILState_Release(gstate);

	return py_str_as_str(py_value, outstr);

err:
	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
}

/**
 * Get the value of a Python dictionary item, returned as a newly
 * allocated char *.
 *
 * @param py_obj The dictionary to probe.
 * @param py_key Key of the item to retrieve.
 * @param out TODO.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @private
 */
SRD_PRIV int py_pydictitem_as_long(PyObject *py_obj, PyObject *py_key, uint64_t *out)
{
	PyObject *py_value;
	PyGILState_STATE gstate;

	if (!py_obj || !py_key || !out)
		return SRD_ERR_ARG;

	gstate = PyGILState_Ensure();

	if (!PyDict_Check(py_obj)) {
		srd_dbg("Object is not a dictionary.");
		goto err;
	}

	if (!(py_value = PyDict_GetItem(py_obj, py_key))) {
		srd_dbg("Dictionary has no such key.");
		goto err;
	}

	if (!PyLong_Check(py_value)) {
		srd_dbg("Dictionary value should be a long.");
		goto err;
	}

	*out = PyLong_AsUnsignedLongLong(py_value);

	PyGILState_Release(gstate);

	return SRD_OK;

err:
	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
}
 

/**
 * Get the value of a Python unicode string object, returned as a newly
 * allocated char *.
 *
 * @param[in] py_str The unicode string object.
 * @param[out] outstr ptr to char * storage to be filled in.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *         The 'outstr' argument points to a g_try_malloc0()ed string upon success.
 *
 * @private
 */
SRD_PRIV int py_str_as_str(PyObject *py_str, char **outstr)
{
	PyObject *py_bytes;
	char *str;
	PyGILState_STATE gstate;

	gstate = PyGILState_Ensure();

	if (!PyUnicode_Check(py_str)) {
		srd_dbg("Object is not a string object.");
		PyGILState_Release(gstate);
		return SRD_ERR_PYTHON;
	}

	py_bytes = PyUnicode_AsUTF8String(py_str);
	if (py_bytes) {
		str = g_strdup(PyBytes_AsString(py_bytes));
		Py_DECREF(py_bytes);
		if (str) {
			*outstr = str;
			PyGILState_Release(gstate);
			return SRD_OK;
		}
	}
    srd_exception_catch(NULL, "Failed to extract string");

	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
}

/*
*/
SRD_PRIV int py_object_to_int(PyObject *py_obj, int64_t *out)
{
	PyGILState_STATE gstate;

	if (py_obj == NULL){
		return SRD_ERR_PYTHON;
	}

	gstate = PyGILState_Ensure();

	if (!PyLong_Check(py_obj))
	{
		srd_dbg("py_object_to_int param should be a long.");
		goto err;
	}

   *out = PyLong_AsLongLong(py_obj);

	PyGILState_Release(gstate);
	return SRD_OK;

err:
	PyGILState_Release(gstate);
	return SRD_ERR_PYTHON;
}

SRD_PRIV int py_object_to_uint(PyObject *py_obj, uint64_t *out)
{
	PyGILState_STATE gstate;

	if (py_obj == NULL){
		return SRD_ERR_PYTHON;
	}

	gstate = PyGILState_Ensure();

	if (!PyLong_Check(py_obj))
	{
		srd_dbg("py_object_to_int param should be a long.");
		goto err;
	}

   *out = PyLong_AsUnsignedLongLong(py_obj);

	PyGILState_Release(gstate);
	return SRD_OK;

err:
	PyGILState_Release(gstate);
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
	PyGILState_STATE gstate;
	int ret = SRD_ERR_PYTHON;
	int lv = 0;
	char dec_buf[15];

	gstate = PyGILState_Ensure();

    str = NULL;
    strv = NULL;
	if (!PySequence_Check(py_strseq)) {
		srd_err("Object does not provide sequence protocol.");
		goto err;
	}

	seq_len = PySequence_Size(py_strseq);
	if (seq_len < 0) {
        srd_exception_catch(NULL, "Failed to obtain sequence size");
		goto err;
	}

	strv = g_try_new0(char *, seq_len + 1);
	if (!strv) {
		srd_err("Failed to allocate result string vector.");
		ret = SRD_ERR_MALLOC;
		goto err;
	}

	for (i = 0; i < seq_len; i++) {
		py_item = PySequence_GetItem(py_strseq, i);
		if (!py_item)
			goto err;

		if (PyUnicode_Check(py_item))
		{
			py_bytes = PyUnicode_AsUTF8String(py_item);
			Py_DECREF(py_item);
			if (!py_bytes)
				goto err;

			str = g_strdup(PyBytes_AsString(py_bytes));
			Py_DECREF(py_bytes);
			if (!str)
				goto err;
		}
		else if (PyLong_Check(py_item))
		{
			lv = PyLong_AsLong(py_item);
			sprintf(dec_buf, "%d", lv);
			str = g_strdup(dec_buf);
		}
		else{
			Py_DECREF(py_item);
			goto err;
		}

		strv[i] = str;
	}
	*out_strv = strv;

	PyGILState_Release(gstate);

	return SRD_OK;

err:
	if (strv)
		g_strfreev(strv);
    srd_exception_catch(NULL, "Failed to obtain string item");
	PyGILState_Release(gstate);
	return ret;
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
	PyGILState_STATE gstate;

	gstate = PyGILState_Ensure();

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

	PyGILState_Release(gstate);

	return var;
}
