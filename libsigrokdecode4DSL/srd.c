/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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
#include <glib.h>
#include "log.h"

/** @cond PRIVATE */

/* Python module search paths */
SRD_PRIV GSList *searchpaths = NULL;

/* session.c */
extern SRD_PRIV GSList *sessions;
extern SRD_PRIV int max_session_id;

/** @endcond */

/**
 * @mainpage libsigrokdecode API
 *
 * @section sec_intro Introduction
 *
 * The <a href="http://sigrok.org">sigrok</a> project aims at creating a
 * portable, cross-platform, Free/Libre/Open-Source signal analysis software
 * suite that supports various device types (such as logic analyzers,
 * oscilloscopes, multimeters, and more).
 *
 * <a href="http://sigrok.org/wiki/Libsigrokdecode">libsigrokdecode</a> is a
 * shared library written in C which provides the basic API for (streaming)
 * protocol decoding functionality.
 *
 * The <a href="http://sigrok.org/wiki/Protocol_decoders">protocol decoders</a>
 * are written in Python (>= 3.0).
 *
 * @section sec_api API reference
 *
 * See the "Modules" page for an introduction to various libsigrokdecode
 * related topics and the detailed API documentation of the respective
 * functions.
 *
 * You can also browse the API documentation by file, or review all
 * data structures.
 *
 * @section sec_mailinglists Mailing lists
 *
 * There is one mailing list for sigrok/libsigrokdecode: <a href="https://lists.sourceforge.net/lists/listinfo/sigrok-devel">sigrok-devel</a>.
 *
 * @section sec_irc IRC
 *
 * You can find the sigrok developers in the
 * <a href="irc://chat.freenode.net/sigrok">\#sigrok</a>
 * IRC channel on Freenode.
 *
 * @section sec_website Website
 *
 * <a href="http://sigrok.org/wiki/Libsigrokdecode">sigrok.org/wiki/Libsigrokdecode</a>
 */

/**
 * @file
 *
 * Initializing and shutting down libsigrokdecode.
 */

/**
 * @defgroup grp_init Initialization
 *
 * Initializing and shutting down libsigrokdecode.
 *
 * Before using any of the libsigrokdecode functionality, srd_init() must
 * be called to initialize the library.
 *
 * When libsigrokdecode functionality is no longer needed, srd_exit() should
 * be called.
 *
 * @{
 */

static int searchpath_add_xdg_dir(const char *datadir)
{
	char *decdir;
	int ret;

	decdir = g_build_filename(datadir, PACKAGE_TARNAME, "decoders", NULL);

	if (g_file_test(decdir, G_FILE_TEST_IS_DIR))
		ret = srd_decoder_searchpath_add(decdir);
	else
		ret = SRD_OK; /* Just ignore non-existing directory. */

	g_free(decdir);

	return ret;
}

static int print_searchpaths(void)
{
	PyObject *py_paths, *py_path, *py_bytes;
	PyGILState_STATE gstate;
	GString *s;
	GSList *l;
	int i;

	s = g_string_sized_new(500);
	g_string_append(s, "Protocol decoder search paths:\n");
	for (l = searchpaths; l; l = l->next)
		g_string_append_printf(s, " - %s\n", (const char *)l->data);
	s->str[s->len - 1] = '\0';
	srd_dbg("%s", s->str);
	g_string_free(s, TRUE);

	gstate = PyGILState_Ensure();

	py_paths = PySys_GetObject("path");
	if (!py_paths)
		goto err;

	s = g_string_sized_new(500);
	g_string_append(s, "Python system search paths:\n");
	for (i = 0; i < PyList_Size(py_paths); i++) {
		py_path = PyList_GetItem(py_paths, i);
		py_bytes = PyUnicode_AsUTF8String(py_path);
		g_string_append_printf(s, " - %s\n", PyBytes_AsString(py_bytes));
		Py_DECREF(py_bytes);
	}
	s->str[s->len - 1] = '\0';
	srd_dbg("%s", s->str);
	g_string_free(s, TRUE);

	PyGILState_Release(gstate);

	return SRD_OK;

err:
	srd_err("Unable to query Python system search paths.");
	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
}

/**
 * Initialize libsigrokdecode.
 *
 * This initializes the Python interpreter, and creates and initializes
 * a "sigrokdecode" Python module.
 *
 * Then, it searches for sigrok protocol decoders in the "decoders"
 * subdirectory of the the libsigrokdecode installation directory.
 * All decoders that are found are loaded into memory and added to an
 * internal list of decoders, which can be queried via srd_decoder_list().
 *
 * The caller is responsible for calling the clean-up function srd_exit(),
 * which will properly shut down libsigrokdecode and free its allocated memory.
 *
 * Multiple calls to srd_init(), without calling srd_exit() in between,
 * are not allowed.
 *
 * @param path Path to an extra directory containing protocol decoders
 *             which will be added to the Python sys.path. May be NULL.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *         Upon Python errors, SRD_ERR_PYTHON is returned. If the decoders
 *         directory cannot be accessed, SRD_ERR_DECODERS_DIR is returned.
 *         If not enough memory could be allocated, SRD_ERR_MALLOC is returned.
 *
 * @since 0.1.0
 */
SRD_API int srd_init(const char *path)
{
	const char *const *sys_datadirs;
	size_t i;
	int ret;
	const char *env_path;

	srd_log_init(); //init log
 	
	if (max_session_id != -1) {
		srd_err("libsigrokdecode is already initialized.");
		return SRD_ERR;
	}

	srd_dbg("Initializing libsigrokdecode.");

	/* Add our own module to the list of built-in modules. */
	PyImport_AppendInittab("sigrokdecode", PyInit_sigrokdecode);

	/* Initialize the Python interpreter. */
    Py_InitializeEx(0); 

#ifdef DECODERS_DIR
	/* Hardcoded decoders install location, if defined. */
	if ((ret = srd_decoder_searchpath_add(DECODERS_DIR)) != SRD_OK) {
		Py_Finalize();
		return ret;
	}
#endif 

	/* Path specified by the user. */
	if (path) {
		if ((ret = srd_decoder_searchpath_add(path)) != SRD_OK) {
			Py_Finalize();
			return ret;
		}
	}
	else{ 
		/* Locations relative to the XDG system data directories. */
		sys_datadirs = g_get_system_data_dirs();
		for (i = g_strv_length((char **)sys_datadirs); i > 0; i--)
		{
			ret = searchpath_add_xdg_dir(sys_datadirs[i - 1]);
			if (ret != SRD_OK)
			{
				Py_Finalize();
				return ret;
			}
		}

		/* Location relative to the XDG user data directory. */
		ret = searchpath_add_xdg_dir(g_get_user_data_dir());
		if (ret != SRD_OK)
		{
			Py_Finalize();
			return ret;
		}

		/* Environment variable overrides everything, for debugging. */
		if ((env_path = g_getenv("SIGROKDECODE_DIR")))
		{
			if ((ret = srd_decoder_searchpath_add(env_path)) != SRD_OK)
			{
				Py_Finalize();
				return ret;
			}
		}
	}

	/* Initialize the Python GIL (this also happens to acquire it). */
	PyEval_InitThreads();

	/* Release the GIL (ignore return value, we don't need it here). */
	PyEval_SaveThread();

	max_session_id = 0;

	print_searchpaths();

	return SRD_OK;
}

static void srd_session_destroy_cb(void *arg, void *ignored)
{
	(void)ignored; // Prevent unused warning
	srd_session_destroy((struct srd_session *)arg);
}

/**
 * Shutdown libsigrokdecode.
 *
 * This frees all the memory allocated for protocol decoders and shuts down
 * the Python interpreter.
 *
 * This function should only be called if there was a (successful!) invocation
 * of srd_init() before. Calling this function multiple times in a row, without
 * any successful srd_init() calls in between, is not allowed.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.1.0
 */
SRD_API int srd_exit(void)
{
	srd_dbg("Exiting libsigrokdecode.");

	g_slist_foreach(sessions, srd_session_destroy_cb, NULL);
	g_slist_free(sessions);
	sessions = NULL;

	srd_decoder_unload_all();
	g_slist_free_full(searchpaths, g_free);
	searchpaths = NULL;

	/*
	 * Acquire the GIL, otherwise Py_Finalize() might have issues.
	 * Ignore the return value, we don't need it here.
	 */
	if (Py_IsInitialized())
		(void)PyGILState_Ensure();

	/* Py_Finalize() returns void, any finalization errors are ignored. */
	Py_Finalize();

	/* Note: No need to release the GIL since Python is shut down now. */

	max_session_id = -1;

	srd_log_uninit(); //uninit log

	return SRD_OK;
}

/**
 * Add an additional search directory for the protocol decoders.
 *
 * The specified directory is prepended (not appended!) to Python's sys.path,
 * in order to search for sigrok protocol decoders in the specified
 * directories first, and in the generic Python module directories (and in
 * the current working directory) last. This avoids conflicts if there are
 * Python modules which have the same name as a sigrok protocol decoder in
 * sys.path or in the current working directory.
 *
 * @param path Path to the directory containing protocol decoders which shall
 *             be added to the Python sys.path, or NULL.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @private
 *
 * @since 0.1.0
 */
SRD_PRIV int srd_decoder_searchpath_add(const char *path)
{
	PyGILState_STATE gstate;

	srd_dbg("Adding '%s' to module path.", path);

	gstate = PyGILState_Ensure();
	
	PyObject *py_cur_path, *py_item;
	py_cur_path = PySys_GetObject("path");
	if (!py_cur_path)
		goto err;

	py_item = PyUnicode_FromString(path);
	if (!py_item) {
        srd_exception_catch(NULL, "Failed to create Unicode object");
		goto err;
	}
	if (PyList_Insert(py_cur_path, 0, py_item) < 0) {
        srd_exception_catch(NULL, "Failed to insert path element");
		Py_DECREF(py_item);
		goto err;
	}
	Py_DECREF(py_item);

	PyGILState_Release(gstate);

	//append the directory to search list
	searchpaths = g_slist_prepend(searchpaths, g_strdup(path));

	return SRD_OK;

err:
	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
}

/**
 * Return the list of protocol decoder search paths.
 *
 * @return The list of search paths used when loading protocol decoders.
 *
 * @since 0.5.1
 */
SRD_API GSList *srd_searchpaths_get(void)
{
	GSList *paths = NULL;

	for (GSList *l = searchpaths; l; l = l->next)
		paths = g_slist_append(paths, g_strdup(l->data));

	return paths;
}

//set python home directory
SRD_API void srd_set_python_home(const wchar_t *path)
{
	Py_SetPythonHome((wchar_t*)path);
}

/** @} */
