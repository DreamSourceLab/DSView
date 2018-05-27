/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2011 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef LIBSIGROKDECODE_LIBSIGROKDECODE_INTERNAL_H
#define LIBSIGROKDECODE_LIBSIGROKDECODE_INTERNAL_H

/* Use the stable ABI subset as per PEP 384. */
#define Py_LIMITED_API 0x03020000

#include <Python.h> /* First, so we avoid a _POSIX_C_SOURCE warning. */
#include "libsigrokdecode.h"
#include <structmember.h>

/* Custom Python types: */

typedef struct {
	PyObject_HEAD
	struct srd_decoder_inst *di;
	uint64_t start_samplenum;
	float itercnt;
	uint8_t **inbuf;
	const uint8_t *inbuf_const;
	uint64_t samplenum;
	PyObject *sample;

	uint64_t exp_logic;
	int edge_index;
	uint64_t logic_mask;
	uint64_t cur_pos;
} srd_logic;

/* srd.c */
SRD_PRIV int srd_decoder_searchpath_add(const char *path);

/* session.c */
SRD_PRIV int session_is_valid(struct srd_session *sess);
SRD_PRIV struct srd_pd_callback *srd_pd_output_callback_find(struct srd_session *sess,
		int output_type);

/* instance.c */
SRD_PRIV struct srd_decoder_inst *srd_inst_find_by_obj( const GSList *stack,
		const PyObject *obj);
SRD_PRIV int srd_inst_start(struct srd_decoder_inst *di, char **error);
SRD_PRIV int srd_inst_decode(const struct srd_decoder_inst *di, uint8_t chunk_type,
		uint64_t start_samplenum, uint64_t end_samplenum,
		const uint8_t **inbuf, const uint8_t *inbuf_const, char **error);
SRD_PRIV void srd_inst_free(struct srd_decoder_inst *di);
SRD_PRIV void srd_inst_free_all(struct srd_session *sess, GSList *stack);

/* log.c */
#if defined(G_OS_WIN32) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
/*
 * On MinGW, we need to specify the gnu_printf format flavor or GCC
 * will assume non-standard Microsoft printf syntax.
 */
SRD_PRIV int srd_log(int loglevel, const char *format, ...)
		__attribute__((__format__ (__gnu_printf__, 2, 3)));
#else
SRD_PRIV int srd_log(int loglevel, const char *format, ...) G_GNUC_PRINTF(2, 3);
#endif

#define srd_spew(...)	srd_log(SRD_LOG_SPEW, __VA_ARGS__)
#define srd_dbg(...)	srd_log(SRD_LOG_DBG,  __VA_ARGS__)
#define srd_info(...)	srd_log(SRD_LOG_INFO, __VA_ARGS__)
#define srd_warn(...)	srd_log(SRD_LOG_WARN, __VA_ARGS__)
#define srd_err(...)	srd_log(SRD_LOG_ERR,  __VA_ARGS__)

/* decoder.c */
SRD_PRIV long srd_decoder_apiver(const struct srd_decoder *d);

/* type_decoder.c */
SRD_PRIV PyObject *srd_Decoder_type_new(void);

/* type_logic.c */
SRD_PRIV PyObject *srd_logic_type_new(void);

/* module_sigrokdecode.c */
PyMODINIT_FUNC PyInit_sigrokdecode(void);

/* util.c */
SRD_PRIV PyObject *py_import_by_name(const char *name);
SRD_PRIV int py_attr_as_str(PyObject *py_obj, const char *attr, char **outstr);
SRD_PRIV int py_dictitem_as_str(PyObject *py_obj, const char *key, char **outstr);
SRD_PRIV int py_dictitem_to_int(PyObject *py_obj, const char *key);
SRD_PRIV int py_str_as_str(PyObject *py_str, char **outstr);
SRD_PRIV int py_strseq_to_char(PyObject *py_strseq, char ***out_strv);
SRD_PRIV GVariant *py_obj_to_variant(PyObject *py_obj);

/* exception.c */
#if defined(G_OS_WIN32) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
/*
 * On MinGW, we need to specify the gnu_printf format flavor or GCC
 * will assume non-standard Microsoft printf syntax.
 */
SRD_PRIV void srd_exception_catch(char **error, const char *format, ...)
		__attribute__((__format__ (__gnu_printf__, 2, 3)));
#else
SRD_PRIV void srd_exception_catch(char **error, const char *format, ...) G_GNUC_PRINTF(2, 3);
#endif

#endif
