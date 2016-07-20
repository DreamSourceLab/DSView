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

/** @cond PRIVATE */

/* type_decoder.c */
extern SRD_PRIV PyTypeObject srd_Decoder_type;

/* type_logic.c */
extern SRD_PRIV PyTypeObject srd_logic_type;

/*
 * When initialized, a reference to this module inside the Python interpreter
 * lives here.
 */
SRD_PRIV PyObject *mod_sigrokdecode = NULL;

/** @endcond */

static struct PyModuleDef sigrokdecode_module = {
	PyModuleDef_HEAD_INIT,
	.m_name = "sigrokdecode",
	.m_doc = "sigrokdecode module",
	.m_size = -1,
};

/** @cond PRIVATE */
PyMODINIT_FUNC PyInit_sigrokdecode(void)
{
	PyObject *mod;

	/* tp_new needs to be assigned here for compiler portability. */
	srd_Decoder_type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&srd_Decoder_type) < 0)
		return NULL;

    srd_logic_type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&srd_logic_type) < 0)
		return NULL;

	mod = PyModule_Create(&sigrokdecode_module);
	Py_INCREF(&srd_Decoder_type);
	if (PyModule_AddObject(mod, "Decoder",
	    (PyObject *)&srd_Decoder_type) == -1)
		return NULL;
	Py_INCREF(&srd_logic_type);
	if (PyModule_AddObject(mod, "srd_logic",
	    (PyObject *)&srd_logic_type) == -1)
		return NULL;

	/* Expose output types as symbols in the sigrokdecode module */
	if (PyModule_AddIntConstant(mod, "OUTPUT_ANN", SRD_OUTPUT_ANN) == -1)
		return NULL;
	if (PyModule_AddIntConstant(mod, "OUTPUT_PYTHON", SRD_OUTPUT_PYTHON) == -1)
		return NULL;
	if (PyModule_AddIntConstant(mod, "OUTPUT_BINARY", SRD_OUTPUT_BINARY) == -1)
		return NULL;
	if (PyModule_AddIntConstant(mod, "OUTPUT_META", SRD_OUTPUT_META) == -1)
		return NULL;
	/* Expose meta input symbols. */
	if (PyModule_AddIntConstant(mod, "SRD_CONF_SAMPLERATE", SRD_CONF_SAMPLERATE) == -1)
		return NULL;

	mod_sigrokdecode = mod;

	return mod;
}
/** @endcond */
