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
#include <inttypes.h>
#include <glib.h>

/**
 * @file
 *
 * Session handling.
 */

/**
 * @defgroup grp_session Session handling
 *
 * Starting and handling decoding sessions.
 *
 * @{
 */

/** @cond PRIVATE */

SRD_PRIV GSList *sessions = NULL;
SRD_PRIV int max_session_id = -1;

/** @endcond */

/** @private */
SRD_PRIV int session_is_valid(struct srd_session *sess)
{

	if (!sess || sess->session_id < 1)
		return SRD_ERR;

	return SRD_OK;
}

/**
 * Create a decoding session.
 *
 * A session holds all decoder instances, their stack relationships and
 * output callbacks.
 *
 * @param sess A pointer which will hold a pointer to a newly
 *             initialized session on return.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.3.0
 */
SRD_API int srd_session_new(struct srd_session **sess)
{

	if (!sess) {
		srd_err("Invalid session pointer.");
		return SRD_ERR_ARG;
	}

	*sess = g_malloc(sizeof(struct srd_session));
	(*sess)->session_id = ++max_session_id;
	(*sess)->di_list = (*sess)->callbacks = NULL;

	/* Keep a list of all sessions, so we can clean up as needed. */
	sessions = g_slist_append(sessions, *sess);

	srd_dbg("Created session %d.", (*sess)->session_id);

	return SRD_OK;
}

/**
 * Start a decoding session.
 *
 * Decoders, instances and stack must have been prepared beforehand,
 * and all SRD_CONF parameters set.
 *
 * @param sess The session to start.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.3.0
 */
SRD_API int srd_session_start(struct srd_session *sess, char **error)
{
	GSList *d;
	struct srd_decoder_inst *di;
	int ret;

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session pointer.");
		return SRD_ERR;
	}

	srd_dbg("Calling start() on all instances in session %d.", sess->session_id);

	/* Run the start() method on all decoders receiving frontend data. */
	ret = SRD_OK;
	for (d = sess->di_list; d; d = d->next) {
		di = d->data;
        if ((ret = srd_inst_start(di, error)) != SRD_OK)
			break;
	}

	return ret;
}

static int srd_inst_send_meta(struct srd_decoder_inst *di, int key,
		GVariant *data)
{
	PyObject *py_ret;

	if (key != SRD_CONF_SAMPLERATE)
		/* This is the only key we pass on to the decoder for now. */
		return SRD_OK;

	if (!PyObject_HasAttrString(di->py_inst, "metadata"))
		/* This decoder doesn't want metadata, that's fine. */
		return SRD_OK;

	py_ret = PyObject_CallMethod(di->py_inst, "metadata", "lK",
			(long)SRD_CONF_SAMPLERATE,
			(unsigned long long)g_variant_get_uint64(data));
	Py_XDECREF(py_ret);

	return SRD_OK;
}

/**
 * Set a metadata configuration key in a session.
 *
 * @param sess The session to configure.
 * @param key The configuration key (SRD_CONF_*).
 * @param data The new value for the key, as a GVariant with GVariantType
 *             appropriate to that key. A floating reference can be passed
 *             in; its refcount will be sunk and unreferenced after use.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.3.0
 */
SRD_API int srd_session_metadata_set(struct srd_session *sess, int key,
		GVariant *data)
{
	GSList *l;
	int ret;

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return SRD_ERR_ARG;
	}

	if (!key) {
		srd_err("Invalid key.");
		return SRD_ERR_ARG;
	}

	if (!data) {
		srd_err("Invalid value.");
		return SRD_ERR_ARG;
	}

	/* Hardcoded to samplerate/uint64 for now. */

	if (key != SRD_CONF_SAMPLERATE) {
		srd_err("Unknown config key %d.", key);
		return SRD_ERR_ARG;
	}
	if (!g_variant_is_of_type(data, G_VARIANT_TYPE_UINT64)) {
		srd_err("Invalid value type: expected uint64, got %s",
				g_variant_get_type_string(data));
		return SRD_ERR_ARG;
	}

	srd_dbg("Setting session %d samplerate to %"PRIu64".",
			sess->session_id, g_variant_get_uint64(data));

	ret = SRD_OK;
	for (l = sess->di_list; l; l = l->next) {
		if ((ret = srd_inst_send_meta(l->data, key, data)) != SRD_OK)
			break;
	}

	g_variant_unref(data);

	return ret;
}

/**
 * Send a chunk of logic sample data to a running decoder session.
 *
 * If no channel map has been set up, the logic samples must be arranged
 * in channel order, in the least amount of space possible. The default
 * channel set consists of all required channels + all optional channels.
 *
 * The size of a sample in inbuf is 'unitsize' bytes. If no channel map
 * has been configured, it is the minimum number of bytes needed to store
 * the default channels.
 *
 * @param sess The session to use.
 * @param start_samplenum The sample number of the first sample in this chunk.
 * @param end_samplenum The sample number of the last sample in this chunk.
 * @param inbuf Pointer to sample data.
 * @param inbuflen Length in bytes of the buffer.
 * @param unitsize The number of bytes per sample.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.4.0
 */
SRD_API int srd_session_send(struct srd_session *sess, uint8_t chunk_type,
		uint64_t start_samplenum, uint64_t end_samplenum,
        const uint8_t *inbuf, uint64_t inbuflen, uint64_t unitsize, char **error)
{
	GSList *d;
	int ret;

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return SRD_ERR_ARG;
	}

	for (d = sess->di_list; d; d = d->next) {
        if ((ret = srd_inst_decode(d->data, chunk_type, start_samplenum,
                end_samplenum, inbuf, inbuflen, unitsize, error)) != SRD_OK)
			return ret;
	}

	return SRD_OK;
}

/**
 * Destroy a decoding session.
 *
 * All decoder instances and output callbacks are properly released.
 *
 * @param sess The session to be destroyed.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.3.0
 */
SRD_API int srd_session_destroy(struct srd_session *sess)
{
	int session_id;

	if (!sess) {
		srd_err("Invalid session.");
		return SRD_ERR_ARG;
	}

	session_id = sess->session_id;
	if (sess->di_list)
		srd_inst_free_all(sess, NULL);
	if (sess->callbacks)
		g_slist_free_full(sess->callbacks, g_free);
	sessions = g_slist_remove(sessions, sess);
	g_free(sess);

	srd_dbg("Destroyed session %d.", session_id);

	return SRD_OK;
}

/**
 * Register/add a decoder output callback function.
 *
 * The function will be called when a protocol decoder sends output back
 * to the PD controller (except for Python objects, which only go up the
 * stack).
 *
 * @param sess The output session in which to register the callback.
 * @param output_type The output type this callback will receive. Only one
 *                    callback per output type can be registered.
 * @param cb The function to call. Must not be NULL.
 * @param cb_data Private data for the callback function. Can be NULL.
 *
 * @since 0.3.0
 */
SRD_API int srd_pd_output_callback_add(struct srd_session *sess,
		int output_type, srd_pd_output_callback cb, void *cb_data)
{
	struct srd_pd_callback *pd_cb;

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return SRD_ERR_ARG;
	}

	srd_dbg("Registering new callback for output type %d.", output_type);

	pd_cb = g_malloc(sizeof(struct srd_pd_callback));
	pd_cb->output_type = output_type;
	pd_cb->cb = cb;
	pd_cb->cb_data = cb_data;
	sess->callbacks = g_slist_append(sess->callbacks, pd_cb);

	return SRD_OK;
}

/** @private */
SRD_PRIV struct srd_pd_callback *srd_pd_output_callback_find(
		struct srd_session *sess, int output_type)
{
	GSList *l;
	struct srd_pd_callback *tmp, *pd_cb;

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return NULL;
	}

	pd_cb = NULL;
	for (l = sess->callbacks; l; l = l->next) {
		tmp = l->data;
		if (tmp->output_type == output_type) {
			pd_cb = tmp;
			break;
		}
	}

	return pd_cb;
}

/** @} */
