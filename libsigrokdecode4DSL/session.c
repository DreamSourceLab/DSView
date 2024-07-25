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
#include "log.h"

SRD_PRIV int srd_call_sub_decoder_end(struct srd_decoder_inst *di, char **error);

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

/**
 * Create a decoding session.
 *
 * A session holds all decoder instances, their stack relationships and
 * output callbacks.
 *
 * @param sess A pointer which will hold a pointer to a newly
 *             initialized session on return. Must not be NULL.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.3.0
 */
SRD_API int srd_session_new(struct srd_session **sess)
{
	struct srd_session *se = NULL;

	if (!sess)
		return SRD_ERR_ARG;

	se = g_try_malloc0(sizeof(struct srd_session));
	if (se == NULL){
		srd_err("%s,ERROR:failed to alloc memory.", __func__);
		return SRD_ERR;
	}
	memset(se, 0, sizeof(struct srd_session));

	se->session_id = ++max_session_id;

	/* Keep a list of all sessions, so we can clean up as needed. */
	sessions = g_slist_append(sessions, se);

	*sess = se;

	//srd_info("Creating session %d.", (*sess)->session_id);

	return SRD_OK;
}

/**
 * Start a decoding session.
 *
 * Decoders, instances and stack must have been prepared beforehand,
 * and all SRD_CONF parameters set.
 *
 * @param sess The session to start. Must not be NULL.
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

	if (!sess)
		return SRD_ERR_ARG;

	srd_info("Calling start() of all instances in session %d.", sess->session_id);

	/* Run the start() method of all decoders receiving frontend data. */
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
	GSList *l;
	struct srd_decoder_inst *next_di;
	int ret;
	PyGILState_STATE gstate;

	if (key != SRD_CONF_SAMPLERATE)
		/* This is the only key we pass on to the decoder for now. */
		return SRD_OK;

	gstate = PyGILState_Ensure();

	if (PyObject_HasAttrString(di->py_inst, "metadata")) {
		py_ret = PyObject_CallMethod(di->py_inst, "metadata", "lK",
				(long)SRD_CONF_SAMPLERATE,
				(unsigned long long)g_variant_get_uint64(data));
		Py_XDECREF(py_ret);
	}

	PyGILState_Release(gstate);

	/* Push metadata to all the PDs stacked on top of this one. */
	for (l = di->next_di; l; l = l->next) {
		next_di = l->data;
		if ((ret = srd_inst_send_meta(next_di, key, data)) != SRD_OK)
			return ret;
	}

	return SRD_OK;
}

/**
 * Set a metadata configuration key in a session.
 *
 * @param sess The session to configure. Must not be NULL.
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

	if (!sess)
		return SRD_ERR_ARG;

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

	srd_dbg("Setting session %d samplerate to %"G_GUINT64_FORMAT".",
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
 * The calls to this function must provide the samples that shall be
 * used by the protocol decoder
 *  - in the correct order ([...]5, 6, 4, 7, 8[...] is a bug),
 *  - starting from sample zero (2, 3, 4, 5, 6[...] is a bug),
 *  - consecutively, with no gaps (0, 1, 2, 4, 5[...] is a bug).
 *
 * The start- and end-sample numbers are absolute sample numbers (relative
 * to the start of the whole capture/file/stream), i.e. they are not relative
 * sample numbers within the chunk specified by 'inbuf' and 'inbuflen'.
 *
 * Correct example (4096 samples total, 4 chunks @ 1024 samples each):
 *   srd_session_send(s, 0,    1023, inbuf, 1024, 1);
 *   srd_session_send(s, 1024, 2047, inbuf, 1024, 1);
 *   srd_session_send(s, 2048, 3071, inbuf, 1024, 1);
 *   srd_session_send(s, 3072, 4095, inbuf, 1024, 1);
 *
 * The chunk size ('inbuflen') can be arbitrary and can differ between calls.
 *
 * Correct example (4096 samples total, 7 chunks @ various samples each):
 *   srd_session_send(s, 0,    1023, inbuf, 1024, 1);
 *   srd_session_send(s, 1024, 1123, inbuf,  100, 1);
 *   srd_session_send(s, 1124, 1423, inbuf,  300, 1);
 *   srd_session_send(s, 1424, 1642, inbuf,  219, 1);
 *   srd_session_send(s, 1643, 2047, inbuf,  405, 1);
 *   srd_session_send(s, 2048, 3071, inbuf, 1024, 1);
 *   srd_session_send(s, 3072, 4095, inbuf, 1024, 1);
 *
 * INCORRECT example (4096 samples total, 4 chunks @ 1024 samples each, but
 * the start- and end-samplenumbers are not absolute):
 *   srd_session_send(s, 0,    1023, inbuf, 1024, 1);
 *   srd_session_send(s, 0,    1023, inbuf, 1024, 1);
 *   srd_session_send(s, 0,    1023, inbuf, 1024, 1);
 *   srd_session_send(s, 0,    1023, inbuf, 1024, 1);
 *
 * @param sess The session to use. Must not be NULL.
 * @param abs_start_samplenum The absolute starting sample number for the
 *              buffer's sample set, relative to the start of capture.
 * @param abs_end_samplenum The absolute ending sample number for the
 *              buffer's sample set, relative to the start of capture.
 * @param inbuf Pointer to sample data. Must not be NULL.
 * @param inbuflen Length in bytes of the buffer. Must be > 0.
 * @param unitsize The number of bytes per sample. Must be > 0.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.4.0
 */
SRD_API int srd_session_send(struct srd_session *sess,
		uint64_t abs_start_samplenum, uint64_t abs_end_samplenum,
        const uint8_t **inbuf, const uint8_t *inbuf_const, uint64_t inbuflen, char **error)
{
	GSList *d;
	int ret;

	if (!sess)
		return SRD_ERR_ARG;

	//foreach srd_decoder_inst* stack
	for (d = sess->di_list; d; d = d->next) {
		if ((ret = srd_inst_decode(d->data, abs_start_samplenum,
                abs_end_samplenum, inbuf, inbuf_const, inbuflen, error)) != SRD_OK)
			return ret;
	}

	return SRD_OK;
}

/**
 * Terminate currently executing decoders in a session, reset internal state.
 *
 * All decoder instances have their .wait() method terminated, which
 * shall terminate .decode() as well. Afterwards the decoders' optional
 * .reset() method gets executed.
 *
 * This routine allows callers to abort pending expensive operations,
 * when they are no longer interested in the decoders' results. Note
 * that the decoder state is lost and aborted work cannot resume.
 *
 * This routine also allows callers to re-use previously created decoder
 * stacks to process new input data which is not related to previously
 * processed input data. This avoids the necessity to re-construct the
 * decoder stack.
 *
 * @param sess The session in which to terminate decoders. Must not be NULL.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.5.1
 */
SRD_API int srd_session_terminate_reset(struct srd_session *sess)
{
	GSList *d;
	int ret;

	if (!sess)
		return SRD_ERR_ARG;

	for (d = sess->di_list; d; d = d->next) {
		ret = srd_inst_terminate_reset(d->data);
		if (ret != SRD_OK)
			return ret;
	}

	return SRD_OK;
}

/**
 * Destroy a decoding session.
 *
 * All decoder instances and output callbacks are properly released.
 *
 * @param sess The session to be destroyed. Must not be NULL.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.3.0
 */
SRD_API int srd_session_destroy(struct srd_session *sess)
{
	int session_id;

	if (!sess)
		return SRD_ERR_ARG;

	session_id = sess->session_id;
	if (sess->di_list)
		srd_inst_free_all(sess);
	if (sess->callbacks)
		g_slist_free_full(sess->callbacks, g_free);
	sessions = g_slist_remove(sessions, sess);
	g_free(sess);

	srd_info("Destroyed session %d.", session_id);

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
 *             Must not be NULL.
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

	if (!sess)
		return SRD_ERR_ARG;

	srd_dbg("Registering new callback for output type %s.",
		output_type_name(output_type));

	pd_cb = g_try_malloc0(sizeof(struct srd_pd_callback));
	if (pd_cb == NULL){
		srd_err("%s,ERROR:failed to alloc memory.", __func__);
		return SRD_ERR;
	}
	memset(pd_cb, 0, sizeof(struct srd_pd_callback));
	
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

	if (!sess)
		return NULL;

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

SRD_API int srd_session_end(struct srd_session *sess, char **error)
{
	GSList *d;
	struct srd_decoder_inst *di;
	PyGILState_STATE gstate;
	PyObject *py_res;
	int ret;

	if (!sess || !sess->di_list){
		return SRD_ERR;
	}

	gstate = PyGILState_Ensure();

	for (d = sess->di_list; d; d = d->next)
	{
		di = d->data;

		if (PyObject_HasAttrString(di->py_inst, "end"))
		{
			//set the last sample index
			PyObject *py_cur_samplenum = PyLong_FromUnsignedLongLong(di->abs_cur_samplenum);
            PyObject_SetAttrString(di->py_inst, "last_samplenum", py_cur_samplenum);
            Py_DECREF(py_cur_samplenum);

			py_res = PyObject_CallMethod(di->py_inst, "end", NULL);

			if (!py_res)
			{ 
				srd_exception_catch(error, "Protocol decoder instance %s",
									di->inst_id);
				PyGILState_Release(gstate);
				return SRD_ERR_PYTHON;
			}
		}

		if (di->next_di != NULL){
			ret = srd_call_sub_decoder_end(di, error);
			if (ret != SRD_OK){
				PyGILState_Release(gstate);
				return ret;
			}
		}
	}

	PyGILState_Release(gstate);
	return SRD_OK;
}


SRD_PRIV int srd_call_sub_decoder_end(struct srd_decoder_inst *di, char **error)
{
	assert(di && di->next_di);

	GSList *l;
	struct srd_decoder_inst *sub_dec;
	PyObject *py_res; 

	for (l = di->next_di; l; l = l->next){
		sub_dec = l->data;

		if (PyObject_HasAttrString(sub_dec->py_inst, "end"))
		{
			py_res = PyObject_CallMethod(sub_dec->py_inst, "end", NULL);

			if (!py_res)
			{ 
				srd_exception_catch(error, "Protocol decoder instance %s",
									sub_dec->inst_id);
				return SRD_ERR_PYTHON;
			}
		}

		//next level decoder
		if (sub_dec->next_di != NULL){
			if (srd_call_sub_decoder_end(sub_dec, error) != SRD_OK)
				return SRD_ERR_PYTHON;
		}
	}

	return SRD_OK;
}

/** @} */
