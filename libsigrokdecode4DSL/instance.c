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
#include <glib.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdint.h>

/** @cond PRIVATE */

extern SRD_PRIV GSList *sessions;

/* type_logic.c */
extern SRD_PRIV PyTypeObject srd_logic_type;

/** @endcond */

/**
 * @file
 *
 * Decoder instance handling.
 */

/**
 * @defgroup grp_instances Decoder instances
 *
 * Decoder instance handling.
 *
 * @{
 */

/**
 * Set one or more options in a decoder instance.
 *
 * Handled options are removed from the hash.
 *
 * @param di Decoder instance.
 * @param options A GHashTable of options to set.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.1.0
 */
SRD_API int srd_inst_option_set(struct srd_decoder_inst *di,
		GHashTable *options)
{
	struct srd_decoder_option *sdo;
	PyObject *py_di_options, *py_optval;
	GVariant *value;
	GSList *l;
	double val_double;
	gint64 val_int;
	int ret;
	const char *val_str;

	if (!di) {
		srd_err("Invalid decoder instance.");
		return SRD_ERR_ARG;
	}

	if (!options) {
		srd_err("Invalid options GHashTable.");
		return SRD_ERR_ARG;
	}

	if (!PyObject_HasAttrString(di->decoder->py_dec, "options")) {
		/* Decoder has no options. */
		if (g_hash_table_size(options) == 0) {
			/* No options provided. */
			return SRD_OK;
		} else {
			srd_err("Protocol decoder has no options.");
			return SRD_ERR_ARG;
		}
		return SRD_OK;
	}

	ret = SRD_ERR_PYTHON;
	py_optval = NULL;

	/*
	 * The 'options' tuple is a class variable, but we need to
	 * change it. Changing it directly will affect the entire class,
	 * so we need to create a new object for it, and populate that
	 * instead.
	 */
	if (!(py_di_options = PyObject_GetAttrString(di->py_inst, "options")))
		goto err_out;
	Py_DECREF(py_di_options);
	py_di_options = PyDict_New();
	PyObject_SetAttrString(di->py_inst, "options", py_di_options);

	for (l = di->decoder->options; l; l = l->next) {
		sdo = l->data;
		if ((value = g_hash_table_lookup(options, sdo->id))) {
			/* A value was supplied for this option. */
			if (!g_variant_type_equal(g_variant_get_type(value),
				  g_variant_get_type(sdo->def))) {
				srd_err("Option '%s' should have the same type "
					"as the default value.", sdo->id);
				goto err_out;
			}
		} else {
			/* Use default for this option. */
			value = sdo->def;
		}
		if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
			val_str = g_variant_get_string(value, NULL);
			if (!(py_optval = PyUnicode_FromString(val_str))) {
				/* Some UTF-8 encoding error. */
				PyErr_Clear();
				srd_err("Option '%s' requires a UTF-8 string value.", sdo->id);
				goto err_out;
			}
		} else if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64)) {
			val_int = g_variant_get_int64(value);
			if (!(py_optval = PyLong_FromLong(val_int))) {
				/* ValueError Exception */
				PyErr_Clear();
				srd_err("Option '%s' has invalid integer value.", sdo->id);
				goto err_out;
			}
		} else if (g_variant_is_of_type(value, G_VARIANT_TYPE_DOUBLE)) {
			val_double = g_variant_get_double(value);
			if (!(py_optval = PyFloat_FromDouble(val_double))) {
				/* ValueError Exception */
				PyErr_Clear();
				srd_err("Option '%s' has invalid float value.",
					sdo->id);
				goto err_out;
			}
		}
		if (PyDict_SetItemString(py_di_options, sdo->id, py_optval) == -1)
			goto err_out;
		/* Not harmful even if we used the default. */
		g_hash_table_remove(options, sdo->id);
	}
	if (g_hash_table_size(options) != 0)
		srd_warn("Unknown options specified for '%s'", di->inst_id);

	ret = SRD_OK;

err_out:
	Py_XDECREF(py_optval);
	if (PyErr_Occurred()) {
        srd_exception_catch("Stray exception in srd_inst_option_set().", NULL);
		ret = SRD_ERR_PYTHON;
	}

	return ret;
}

/* Helper GComparefunc for g_slist_find_custom() in srd_inst_channel_set_all() */
static gint compare_channel_id(const struct srd_channel *pdch,
			const char *channel_id)
{
	return strcmp(pdch->id, channel_id);
}

/**
 * Set all channels in a decoder instance.
 *
 * This function sets _all_ channels for the specified decoder instance, i.e.,
 * it overwrites any channels that were already defined (if any).
 *
 * @param di Decoder instance.
 * @param new_channels A GHashTable of channels to set. Key is channel name,
 *                     value is the channel number. Samples passed to this
 *                     instance will be arranged in this order.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.4.0
 */
SRD_API int srd_inst_channel_set_all(struct srd_decoder_inst *di,
		GHashTable *new_channels)
{
	GVariant *channel_val;
	GList *l;
	GSList *sl;
	struct srd_channel *pdch;
	int *new_channelmap, new_channelnum, num_required_channels, i;
	char *channel_id;

	srd_dbg("Setting channels for instance %s with list of %d channels.",
		di->inst_id, g_hash_table_size(new_channels));

	if (g_hash_table_size(new_channels) == 0)
		/* No channels provided. */
		return SRD_OK;

	if (di->dec_num_channels == 0) {
		/* Decoder has no channels. */
		srd_err("Protocol decoder %s has no channels to define.",
			di->decoder->name);
		return SRD_ERR_ARG;
	}

	new_channelmap = g_malloc(sizeof(int) * di->dec_num_channels);

	/*
	 * For now, map all indexes to channel -1 (can be overridden later).
	 * This -1 is interpreted as an unspecified channel later.
	 */
	for (i = 0; i < di->dec_num_channels; i++)
		new_channelmap[i] = -1;

	for (l = g_hash_table_get_keys(new_channels); l; l = l->next) {
		channel_id = l->data;
		channel_val = g_hash_table_lookup(new_channels, channel_id);
		if (!g_variant_is_of_type(channel_val, G_VARIANT_TYPE_INT32)) {
			/* Channel name was specified without a value. */
			srd_err("No channel number was specified for %s.",
					channel_id);
			g_free(new_channelmap);
			return SRD_ERR_ARG;
		}
		new_channelnum = g_variant_get_int32(channel_val);
		if (!(sl = g_slist_find_custom(di->decoder->channels, channel_id,
				(GCompareFunc)compare_channel_id))) {
			/* Fall back on optional channels. */
			if (!(sl = g_slist_find_custom(di->decoder->opt_channels,
			     channel_id, (GCompareFunc)compare_channel_id))) {
				srd_err("Protocol decoder %s has no channel "
					"'%s'.", di->decoder->name, channel_id);
				g_free(new_channelmap);
				return SRD_ERR_ARG;
			}
		}
		pdch = sl->data;
		new_channelmap[pdch->order] = new_channelnum;
		srd_dbg("Setting channel mapping: %s (index %d) = channel %d.",
			pdch->id, pdch->order, new_channelnum);
	}

	srd_dbg("Final channel map:");
	num_required_channels = g_slist_length(di->decoder->channels);
	for (i = 0; i < di->dec_num_channels; i++) {
		srd_dbg(" - index %d = channel %d (%s)", i, new_channelmap[i],
			(i < num_required_channels) ? "required" : "optional");
	}

	/* Report an error if not all required channels were specified. */
	for (i = 0; i < num_required_channels; i++) {
		if (new_channelmap[i] != -1)
			continue;
		pdch = g_slist_nth(di->decoder->channels, i)->data;
		srd_err("Required channel '%s' (index %d) was not specified.",
			pdch->id, i);
		return SRD_ERR;
	}

	g_free(di->dec_channelmap);
	di->dec_channelmap = new_channelmap;

	return SRD_OK;
}

/**
 * Create a new protocol decoder instance.
 *
 * @param sess The session holding the protocol decoder instance.
 * @param decoder_id Decoder 'id' field.
 * @param options GHashtable of options which override the defaults set in
 *                the decoder class. May be NULL.
 *
 * @return Pointer to a newly allocated struct srd_decoder_inst, or
 *         NULL in case of failure.
 *
 * @since 0.3.0
 */
SRD_API struct srd_decoder_inst *srd_inst_new(struct srd_session *sess,
		const char *decoder_id, GHashTable *options)
{
	int i;
	struct srd_decoder *dec;
	struct srd_decoder_inst *di;
	char *inst_id;

	srd_dbg("Creating new %s instance.", decoder_id);

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return NULL;
	}

	if (!(dec = srd_decoder_get_by_id(decoder_id))) {
		srd_err("Protocol decoder %s not found.", decoder_id);
		return NULL;
	}

	di = g_malloc0(sizeof(struct srd_decoder_inst));

	di->decoder = dec;
	di->sess = sess;
	if (options) {
		inst_id = g_hash_table_lookup(options, "id");
		di->inst_id = g_strdup(inst_id ? inst_id : decoder_id);
		g_hash_table_remove(options, "id");
	} else
		di->inst_id = g_strdup(decoder_id);

	/*
	 * Prepare a default channel map, where samples come in the
	 * order in which the decoder class defined them.
	 */
	di->dec_num_channels = g_slist_length(di->decoder->channels) +
			g_slist_length(di->decoder->opt_channels);
	if (di->dec_num_channels) {
		di->dec_channelmap =
				g_malloc(sizeof(int) * di->dec_num_channels);
		for (i = 0; i < di->dec_num_channels; i++)
			di->dec_channelmap[i] = i;
		/*
		 * Will be used to prepare a sample at every iteration
		 * of the instance's decode() method.
		 */
		di->channel_samples = g_malloc(di->dec_num_channels);
	}

	/* Create a new instance of this decoder class. */
	if (!(di->py_inst = PyObject_CallObject(dec->py_dec, NULL))) {
		if (PyErr_Occurred())
            srd_exception_catch("failed to create %s instance: ", NULL,
					decoder_id);
		g_free(di->dec_channelmap);
		g_free(di);
		return NULL;
	}

	if (options && srd_inst_option_set(di, options) != SRD_OK) {
		g_free(di->dec_channelmap);
		g_free(di);
		return NULL;
	}

	/* Instance takes input from a frontend by default. */
	sess->di_list = g_slist_append(sess->di_list, di);

	return di;
}

/**
 * Stack a decoder instance on top of another.
 *
 * @param sess The session holding the protocol decoder instances.
 * @param di_bottom The instance on top of which di_top will be stacked.
 * @param di_top The instance to go on top.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.3.0
 */
SRD_API int srd_inst_stack(struct srd_session *sess,
		struct srd_decoder_inst *di_bottom,
		struct srd_decoder_inst *di_top)
{

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return SRD_ERR_ARG;
	}

	if (!di_bottom || !di_top) {
		srd_err("Invalid from/to instance pair.");
		return SRD_ERR_ARG;
	}

	if (g_slist_find(sess->di_list, di_top)) {
		/* Remove from the unstacked list. */
		sess->di_list = g_slist_remove(sess->di_list, di_top);
	}

	/* Stack on top of source di. */
	di_bottom->next_di = g_slist_append(di_bottom->next_di, di_top);

	srd_dbg("Stacked %s onto %s.", di_top->inst_id, di_bottom->inst_id);

	return SRD_OK;
}

/**
 * Find a decoder instance by its instance ID.
 *
 * Only the bottom level of instances are searched -- instances already stacked
 * on top of another one will not be found.
 *
 * @param sess The session holding the protocol decoder instance.
 * @param inst_id The instance ID to be found.
 *
 * @return Pointer to struct srd_decoder_inst, or NULL if not found.
 *
 * @since 0.3.0
 */
SRD_API struct srd_decoder_inst *srd_inst_find_by_id(struct srd_session *sess,
		const char *inst_id)
{
	GSList *l;
	struct srd_decoder_inst *tmp, *di;

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return NULL;
	}

	di = NULL;
	for (l = sess->di_list; l; l = l->next) {
		tmp = l->data;
		if (!strcmp(tmp->inst_id, inst_id)) {
			di = tmp;
			break;
		}
	}

	return di;
}

static struct srd_decoder_inst *srd_sess_inst_find_by_obj(
		struct srd_session *sess, const GSList *stack,
		const PyObject *obj)
{
	const GSList *l;
	struct srd_decoder_inst *tmp, *di;

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return NULL;
	}

	di = NULL;
	for (l = stack ? stack : sess->di_list; di == NULL && l != NULL; l = l->next) {
		tmp = l->data;
		if (tmp->py_inst == obj)
			di = tmp;
		else if (tmp->next_di)
			di = srd_sess_inst_find_by_obj(sess, tmp->next_di, obj);
	}

	return di;
}

/**
 * Find a decoder instance by its Python object.
 *
 * I.e. find that instance's instantiation of the sigrokdecode.Decoder class.
 * This will recurse to find the instance anywhere in the stack tree of all
 * sessions.
 *
 * @param stack Pointer to a GSList of struct srd_decoder_inst, indicating the
 *              stack to search. To start searching at the bottom level of
 *              decoder instances, pass NULL.
 * @param obj The Python class instantiation.
 *
 * @return Pointer to struct srd_decoder_inst, or NULL if not found.
 *
 * @private
 *
 * @since 0.1.0
 */
SRD_PRIV struct srd_decoder_inst *srd_inst_find_by_obj(const GSList *stack,
		const PyObject *obj)
{
	struct srd_decoder_inst *di;
	struct srd_session *sess;
	GSList *l;

	di = NULL;
	for (l = sessions; di == NULL && l != NULL; l = l->next) {
		sess = l->data;
		di = srd_sess_inst_find_by_obj(sess, stack, obj);
	}

	return di;
}

/** @private */
SRD_PRIV int srd_inst_start(struct srd_decoder_inst *di, char **error)
{
    srd_logic *logic;
	PyObject *py_res;
	GSList *l;
	struct srd_decoder_inst *next_di;
	int ret;

	srd_dbg("Calling start() method on protocol decoder instance %s.",
			di->inst_id);

	if (!(py_res = PyObject_CallMethod(di->py_inst, "start", NULL))) {
        srd_exception_catch("Protocol decoder instance %s: ", error,
				di->inst_id);
		return SRD_ERR_PYTHON;
	}
    Py_DecRef(py_res);

    if ((di->decoder->channels || di->decoder->opt_channels) != 0 ) {
        logic = PyObject_New(srd_logic, &srd_logic_type);
        //Py_INCREF(logic);
        logic->di = (struct srd_decoder_inst *)di;
        logic->sample = PyList_New(2);
        //Py_INCREF(logic->sample);
        di->py_logic = logic;
    }

	/* Start all the PDs stacked on top of this one. */
	for (l = di->next_di; l; l = l->next) {
		next_di = l->data;
        if ((ret = srd_inst_start(next_di, error)) != SRD_OK)
			return ret;
	}

	return SRD_OK;
}

/**
 * Run the specified decoder function.
 *
 * @param di The decoder instance to call. Must not be NULL.
 * @param start_samplenum The starting sample number for the buffer's sample
 * 			  set, relative to the start of capture.
 * @param end_samplenum The ending sample number for the buffer's sample
 * 			  set, relative to the start of capture.
 * @param inbuf The buffer to decode. Must not be NULL.
 * @param inbuflen Length of the buffer. Must be > 0.
 * @param unitsize The number of bytes per sample.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @private
 *
 * @since 0.4.0
 */
SRD_PRIV int srd_inst_decode(const struct srd_decoder_inst *di, uint8_t chunk_type,
		uint64_t start_samplenum, uint64_t end_samplenum,
        const uint8_t *inbuf, uint64_t inbuflen, uint64_t unitsize, char **error)
{
	PyObject *py_res;
	srd_logic *logic;

	/* Return an error upon unusable input. */
	if (!di) {
		srd_dbg("empty decoder instance");
		return SRD_ERR_ARG;
	}
	if (!inbuf) {
		srd_dbg("NULL buffer pointer");
		return SRD_ERR_ARG;
	}
	if (inbuflen == 0) {
		srd_dbg("empty buffer");
		return SRD_ERR_ARG;
	}

	((struct srd_decoder_inst *)di)->data_unitsize = unitsize;

	srd_dbg("Calling decode(), start sample %" PRIu64 ", end sample %"
		PRIu64 " (%" PRIu64 " samples, %" PRIu64 " bytes, unitsize = "
		"%d), instance %s.", start_samplenum, end_samplenum,
		end_samplenum - start_samplenum, inbuflen, di->data_unitsize,
		di->inst_id);

	/*
	 * Create new srd_logic object. Each iteration around the PD's loop
	 * will fill one sample into this object.
	 */
    logic = di->py_logic;
	logic->start_samplenum = start_samplenum;
    if (chunk_type == 0) {
        logic->itercnt = 0;
        logic->logic_mask = 0;
    }
    logic->inbuf = (uint8_t *)inbuf;
	logic->inbuflen = inbuflen;
    Py_INCREF(logic);

    //Py_IncRef(di->py_inst);
	if (!(py_res = PyObject_CallMethod(di->py_inst, "decode",
			"KKO", start_samplenum, end_samplenum, logic))) {
        srd_exception_catch("Protocol decoder instance %s: ", error,
				di->inst_id);
		return SRD_ERR_PYTHON;
	}
    Py_DecRef(py_res);

    if (logic->logic_mask == 0)
        logic->itercnt -= logic->inbuflen / logic->di->data_unitsize;

	return SRD_OK;
}

/** @private */
SRD_PRIV void srd_inst_free(struct srd_decoder_inst *di)
{
	GSList *l;
	struct srd_pd_output *pdo;
    srd_logic *logic = di->py_logic;

	srd_dbg("Freeing instance %s", di->inst_id);

    if ((di->decoder->channels || di->decoder->opt_channels) != 0 ) {
        if (logic && logic->sample)
            Py_XDECREF(logic->sample);
        if (logic)
            Py_XDECREF(logic);
    }

	Py_DecRef(di->py_inst);
	g_free(di->inst_id);
	g_free(di->dec_channelmap);
    g_free(di->channel_samples);
	g_slist_free(di->next_di);
	for (l = di->pd_output; l; l = l->next) {
		pdo = l->data;
		g_free(pdo->proto_id);
		g_free(pdo);
	}
	g_slist_free(di->pd_output);
	g_free(di);
}

/** @private */
SRD_PRIV void srd_inst_free_all(struct srd_session *sess, GSList *stack)
{
	GSList *l;
	struct srd_decoder_inst *di;

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return;
	}

	di = NULL;
	for (l = stack ? stack : sess->di_list; di == NULL && l != NULL; l = l->next) {
		di = l->data;
		if (di->next_di)
			srd_inst_free_all(sess, di->next_di);
		srd_inst_free(di);
	}
	if (!stack) {
		g_slist_free(sess->di_list);
		sess->di_list = NULL;
	}
}

/** @} */
