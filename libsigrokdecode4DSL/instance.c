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
#include <assert.h>
#include "log.h"

/** @cond PRIVATE */

extern SRD_PRIV GSList *sessions;

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

static void oldpins_array_seed(struct srd_decoder_inst *di)
{
	size_t count;
	GArray *arr;

	if (!di)
		return;
	if (di->old_pins_array)
		return;

	count = di->dec_num_channels;
	arr = g_array_sized_new(FALSE, TRUE, sizeof(uint8_t), count);
	g_array_set_size(arr, count);
	memset(arr->data, SRD_INITIAL_PIN_SAME_AS_SAMPLE0, count);
	di->old_pins_array = arr;
}

static void oldpins_array_free(struct srd_decoder_inst *di)
{
	if (!di)
		return;
	if (!di->old_pins_array)
		return;

	srd_dbg("%s: Releasing initial pin state.", di->inst_id);

	g_array_free(di->old_pins_array, TRUE);
	di->old_pins_array = NULL;
}

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
	PyGILState_STATE gstate;

	if (!di) {
		srd_err("Invalid decoder instance.");
		return SRD_ERR_ARG;
	}

	if (!options) {
		srd_err("Invalid options GHashTable.");
		return SRD_ERR_ARG;
	}

	gstate = PyGILState_Ensure();

	if (!PyObject_HasAttrString(di->decoder->py_dec, "options")) {
		/* Decoder has no options. */
		PyGILState_Release(gstate);
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
        srd_exception_catch(NULL, "Stray exception in srd_inst_option_set()");
		ret = SRD_ERR_PYTHON;
	}
	PyGILState_Release(gstate);

	return ret;
}

/* Helper GComparefunc for g_slist_find_custom() in srd_inst_channel_set_all(). */
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

	new_channelmap = g_try_malloc0(sizeof(int) * di->dec_num_channels);
	if (new_channelmap == NULL){
		srd_err("%s,ERROR:failed to alloc memory.", __func__);
		return SRD_ERR;
	}		

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
		srd_dbg("Setting channel mapping: %s (PD ch idx %d) = input data ch idx %d.",
			pdch->id, pdch->order, new_channelnum);
	}

	srd_dbg("Final channel map:");
	num_required_channels = g_slist_length(di->decoder->channels);
	for (i = 0; i < di->dec_num_channels; i++) {
		GSList *ll = g_slist_nth(di->decoder->channels, i);
		if (!ll)
			ll = g_slist_nth(di->decoder->opt_channels,
				i - num_required_channels);
		pdch = ll->data;
		srd_dbg(" - PD ch idx %d (%s) = input data ch idx %d (%s)", i,
			pdch->id, new_channelmap[i],
			(i < num_required_channels) ? "required" : "optional");
	}

	/* Report an error if not all required channels were specified. */
	for (i = 0; i < num_required_channels; i++) {
		if (new_channelmap[i] != -1)
			continue;
		pdch = g_slist_nth(di->decoder->channels, i)->data;
		srd_err("Required channel '%s' (index %d) was not specified.",
			pdch->id, i);
		g_free(new_channelmap);
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
 *             Must not be NULL.
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
	PyGILState_STATE gstate;

	i = 1;

	if (!sess)
		return NULL;

	if (!(dec = srd_decoder_get_by_id(decoder_id))) {
		srd_err("Protocol decoder %s not found.", decoder_id);
		return NULL;
	}

	di = g_try_malloc0(sizeof(struct srd_decoder_inst));
	if (di == NULL){
		srd_err("%s,ERROR:failed to alloc memory.", __func__);
		return NULL;
	}
	memset(di, 0, sizeof(struct srd_decoder_inst));
	
	di->decoder = dec;
	di->sess = sess;

	if (options) {
		inst_id = g_hash_table_lookup(options, "id");
		if (inst_id)
			di->inst_id = g_strdup(inst_id);
		g_hash_table_remove(options, "id");
	}

	/* Create a unique instance ID (as none was provided). */
	if (!di->inst_id) {
		di->inst_id = g_strdup_printf("%s-%d", decoder_id, i++);
		while (srd_inst_find_by_id(sess, di->inst_id)) {
			g_free(di->inst_id);
			di->inst_id = g_strdup_printf("%s-%d", decoder_id, i++);
		}
	}

	gstate = PyGILState_Ensure();

	/*
	 * Prepare a default channel map, where samples come in the
	 * order in which the decoder class defined them.
	 */
    di->py_pinvalues = NULL;
	di->dec_num_channels = g_slist_length(di->decoder->channels) +
			g_slist_length(di->decoder->opt_channels);
			
	if (di->dec_num_channels > 0) {
		di->dec_channelmap = g_try_malloc0(sizeof(int) * di->dec_num_channels);

		if (di->dec_channelmap == NULL){
			PyGILState_Release(gstate);
			srd_err("%s,ERROR:failed to alloc memory.", __func__);
			return NULL;
		}

		for (i = 0; i < di->dec_num_channels; i++)
			di->dec_channelmap[i] = i;

        di->py_pinvalues = PyTuple_New(di->dec_num_channels);
	}

	/* Default to the initial pins being the same as in sample 0. */
	oldpins_array_seed(di);

	/* Create a new instance of this decoder class. */
	if (!(di->py_inst = PyObject_CallObject(dec->py_dec, NULL))) {
		if (PyErr_Occurred())
            srd_exception_catch(NULL, "Failed to create %s instance",
					decoder_id);
        goto err;
	}

    if (options && srd_inst_option_set(di, options) != SRD_OK) {
        goto err;
	}

    PyGILState_Release(gstate);

	di->condition_list = NULL;
    di->match_array = 0;
	di->abs_start_samplenum = 0;
	di->abs_end_samplenum = 0;
	di->inbuf = NULL;
	di->inbuflen = 0;
	di->abs_cur_samplenum = 0;
	di->thread_handle = NULL;
	di->got_new_samples = FALSE;
	di->handled_all_samples = FALSE;
	di->want_wait_terminate = FALSE;
	di->decoder_state = SRD_OK;
	di->python_proc_error = NULL;
	di->is_task_stop_signal = FALSE;

	/*
	 * Strictly speaking initialization of statically allocated
	 * condition and mutex variables (or variables allocated on the
	 * stack) is not required, but won't harm either. Explicitly
	 * running init() will better match subsequent clear() calls.
	 */
	g_cond_init(&di->got_new_samples_cond);
	g_cond_init(&di->handled_all_samples_cond);
	g_mutex_init(&di->data_mutex);

	/* Instance takes input from a frontend by default. */
	sess->di_list = g_slist_append(sess->di_list, di);
	srd_dbg("Creating new %s instance %s.", decoder_id, di->inst_id);

	return di;

err:
    PyGILState_Release(gstate);
    g_free(di->dec_channelmap);
    g_free(di);
    return NULL;
}

static void srd_inst_join_decode_thread(struct srd_decoder_inst *di)
{
	if (!di)
		return;
	if (!di->thread_handle)
		return;

	srd_dbg("%s: Joining decoder thread.", di->inst_id);

	/*
	 * Terminate potentially running threads which still
	 * execute the decoder instance's decode() method.
	 */
	srd_dbg("%s: Raising want_term, sending got_new.", di->inst_id);
	g_mutex_lock(&di->data_mutex);
	//srd_err("set flag =true");
	di->want_wait_terminate = TRUE;
	di->is_task_stop_signal = TRUE;
	g_cond_signal(&di->got_new_samples_cond);
	g_mutex_unlock(&di->data_mutex);

	srd_dbg("%s: Running join().", di->inst_id);
	(void)g_thread_join(di->thread_handle);
	srd_dbg("%s: Call to join() done.", di->inst_id);
	di->thread_handle = NULL;

	/*
	 * Reset condition and mutex variables, such that next
	 * operations on them will find them in a clean state.
	 */
	g_cond_clear(&di->got_new_samples_cond);
	g_cond_init(&di->got_new_samples_cond);
	g_cond_clear(&di->handled_all_samples_cond);
	g_cond_init(&di->handled_all_samples_cond);
	g_mutex_clear(&di->data_mutex);
	g_mutex_init(&di->data_mutex);
}

static void srd_inst_reset_state(struct srd_decoder_inst *di)
{
	if (!di)
		return;

	srd_dbg("%s: Resetting decoder state.", di->inst_id);

	/* Reset internal state of the decoder. */
	condition_list_free(di);
	di->abs_start_samplenum = 0;
	di->abs_end_samplenum = 0;
	di->inbuf = NULL;
	di->inbuflen = 0;
	di->abs_cur_samplenum = 0;
	oldpins_array_free(di);
	di->got_new_samples = FALSE;
	di->handled_all_samples = FALSE;
	di->want_wait_terminate = FALSE;
	di->decoder_state = SRD_OK;
	/* Conditions and mutex got reset after joining the thread. */
}

/**
 * Stack a decoder instance on top of another.
 *
 * @param sess The session holding the protocol decoder instances.
 *             Must not be NULL.
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
	if (!sess)
		return SRD_ERR_ARG;

	if (!di_bottom || !di_top) {
		srd_err("Invalid from/to instance pair.");
		return SRD_ERR_ARG;
	}

	if (g_slist_find(sess->di_list, di_top)) {
		/* Remove from the unstacked list. */
		sess->di_list = g_slist_remove(sess->di_list, di_top);
	}

	/*
	 * Check if there's at least one matching input/output pair
	 * for the stacked PDs. We warn if that's not the case, but it's
	 * not a hard error for the time being.
	 */
	gboolean at_least_one_match = FALSE;
	for (GSList *out = di_bottom->decoder->outputs; out; out = out->next) {
		const char *o = out->data;
		for (GSList *in = di_top->decoder->inputs; in; in = in->next) {
			const char *i = in->data;
			if (!strcmp(o, i)) {
				at_least_one_match = TRUE;
				break;
			}
		}
	}

	if (!at_least_one_match)
		srd_warn("No matching in-/output when stacking %s onto %s.",
			di_top->inst_id, di_bottom->inst_id);

	/* Stack on top of source di. */
	di_bottom->next_di = g_slist_append(di_bottom->next_di, di_top);

	srd_dbg("Stacking %s onto %s.", di_top->inst_id, di_bottom->inst_id);

	return SRD_OK;
}

/**
 * Search a decoder instance and its stack for instance ID.
 *
 * @param[in] inst_id ID to search for.
 * @param[in] stack A decoder instance, potentially with stacked instances.
 *
 * @return The matching instance, or NULL.
 */
static struct srd_decoder_inst *srd_inst_find_by_id_stack(const char *inst_id,
		struct srd_decoder_inst *stack)
{
	const GSList *l;
	struct srd_decoder_inst *tmp, *di;

	if (!strcmp(stack->inst_id, inst_id))
		return stack;

	/* Otherwise, look recursively in our stack. */
	di = NULL;
	if (stack->next_di) {
		for (l = stack->next_di; l; l = l->next) {
			tmp = l->data;
			if (!strcmp(tmp->inst_id, inst_id)) {
				di = tmp;
				break;
			}
		}
	}

	return di;
}

/**
 * Find a decoder instance by its instance ID.
 *
 * This will recurse to find the instance anywhere in the stack tree of the
 * given session.
 *
 * @param sess The session holding the protocol decoder instance.
 *             Must not be NULL.
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

	if (!sess)
		return NULL;

	di = NULL;
	for (l = sess->di_list; l; l = l->next) {
		tmp = l->data;
		if ((di = srd_inst_find_by_id_stack(inst_id, tmp)) != NULL)
			break;
	}

	return di;
}

/**
 * Set the list of initial (assumed) pin values.
 *
 * @param di Decoder instance to use. Must not be NULL.
 * @param initial_pins A GArray of uint8_t values. Must not be NULL.
 *
 * @since 0.5.0
 */
SRD_API int srd_inst_initial_pins_set_all(struct srd_decoder_inst *di, GArray *initial_pins)
{
	int i;
	GString *s;

	if (!di) {
		srd_err("Invalid decoder instance.");
		return SRD_ERR_ARG;
	}

	if (!initial_pins)
		return SRD_ERR_ARG;

	if (initial_pins->len != (guint)di->dec_num_channels) {
		srd_err("Incorrect number of channels (need %d, got %d).",
			di->dec_num_channels, initial_pins->len);
		return SRD_ERR_ARG;
	}

	/* Sanity-check initial pin state values. */
	for (i = 0; i < di->dec_num_channels; i++) {
		if (initial_pins->data[i] <= 2)
			continue;
		srd_err("Invalid initial channel %d pin state: %d.",
			i, initial_pins->data[i]);
		return SRD_ERR_ARG;
	}

	s = g_string_sized_new(100);
	oldpins_array_seed(di);
	for (i = 0; i < di->dec_num_channels; i++) {
		di->old_pins_array->data[i] = initial_pins->data[i];
		g_string_append_printf(s, "%d, ", di->old_pins_array->data[i]);
	}
	s = g_string_truncate(s, s->len - 2);
	srd_dbg("Initial pins: %s.", s->str);
	g_string_free(s, TRUE);

	return SRD_OK;
}

/** @private */
SRD_PRIV int srd_inst_start(struct srd_decoder_inst *di, char **error)
{
	PyObject *py_res;
	GSList *l;
	struct srd_decoder_inst *next_di;
	int ret;
	PyGILState_STATE gstate;

	srd_dbg("Calling start() of instance %s.", di->inst_id);

	gstate = PyGILState_Ensure();

	/* Run self.start(). */
	if (!(py_res = PyObject_CallMethod(di->py_inst, "start", NULL))) {
        srd_exception_catch(error, "Protocol decoder instance %s",
				di->inst_id);
		PyGILState_Release(gstate);
		return SRD_ERR_PYTHON;
	}
	Py_DecRef(py_res);

    /* first pos */
    di->first_pos = TRUE;

    /* none matched */
    di->abs_cur_matched = FALSE;

    /* skip zero flag */
    di->skip_zero = FALSE;

	/* Set self.samplenum to 0. */
	PyObject_SetAttrString(di->py_inst, "samplenum", PyLong_FromLong(0));

    /* Set self.matched to 0. */
    PyObject_SetAttrString(di->py_inst, "matched", PyLong_FromLong(0));

	PyGILState_Release(gstate);

	/* Start all the PDs stacked on top of this one. */
	for (l = di->next_di; l; l = l->next) {
		next_di = l->data;
        if ((ret = srd_inst_start(next_di, error)) != SRD_OK)
			return ret;
	}

	return SRD_OK;
}

/**
 * Check whether the specified sample matches the specified term.
 *
 * In the case of SRD_TERM_SKIP, this function can modify
 * term->num_samples_already_skipped.
 *
 * @param old_sample The value of the previous sample (0/1).
 * @param sample The value of the current sample (0/1).
 * @param term The term that should be checked for a match. Must not be NULL.
 *
 * @retval TRUE The current sample matches the specified term.
 * @retval FALSE The current sample doesn't match the specified term, or an
 *               invalid term was provided.
 *
 * @private
 */
__attribute__((always_inline))
static inline gboolean sample_matches(uint8_t old_sample, uint8_t sample, struct srd_term *term)
{
	/* Caller ensures term != NULL. */

	switch (term->type) {
	case SRD_TERM_HIGH:
		if (sample == 1)
			return TRUE;
		break;
	case SRD_TERM_LOW:
		if (sample == 0)
			return TRUE;
		break;
	case SRD_TERM_RISING_EDGE:
		if (old_sample == 0 && sample == 1)
			return TRUE;
		break;
	case SRD_TERM_FALLING_EDGE:
		if (old_sample == 1 && sample == 0)
			return TRUE;
		break;
	case SRD_TERM_EITHER_EDGE:
		if ((old_sample == 1 && sample == 0) || (old_sample == 0 && sample == 1))
			return TRUE;
		break;
	case SRD_TERM_NO_EDGE:
		if ((old_sample == 0 && sample == 0) || (old_sample == 1 && sample == 1))
			return TRUE;
		break;
	case SRD_TERM_SKIP:
		if (term->num_samples_already_skipped == term->num_samples_to_skip)
			return TRUE;
		term->num_samples_already_skipped++;
		break;
	default:
		srd_err("Unknown term type %d.", term->type);
		break;
	}

	return FALSE;
}

/** @private */
SRD_PRIV void condition_list_free(struct srd_decoder_inst *di)
{
	GSList *l, *ll;

	if (!di)
		return;

	for (l = di->condition_list; l; l = l->next) {
		ll = l->data;
		if (ll)
			g_slist_free_full(ll, g_free);
	}

    g_slist_free(di->condition_list);
	di->condition_list = NULL;
}

static gboolean have_non_null_conds(const struct srd_decoder_inst *di)
{
	GSList *l, *cond;

	if (!di)
		return FALSE;

	for (l = di->condition_list; l; l = l->next) {
		cond = l->data;
		if (cond)
			return TRUE;
	}

	return FALSE;
}

static void update_old_pins_array(struct srd_decoder_inst *di)
{
	uint8_t sample;
    int i, bit_offset;
    const uint8_t *sample_pos;

    if (!di || !di->dec_channelmap)
		return;

	oldpins_array_seed(di);
	for (i = 0; i < di->dec_num_channels; i++) {
        if (*(di->inbuf + i) == NULL) {
            sample = *(di->inbuf_const + i) ? 1 : 0;
            di->old_pins_array->data[i] = sample;
        } else {
            sample_pos = *(di->inbuf + i) + ((di->abs_cur_samplenum - di->abs_start_samplenum) / 8);
            bit_offset = (di->abs_cur_samplenum - di->abs_start_samplenum) % 8;
            sample = *sample_pos & (1 << bit_offset) ? 1 : 0;
            di->old_pins_array->data[i] = sample;
        }
	}
}

static void update_old_pins_array_initial_pins(struct srd_decoder_inst *di)
{
	uint8_t sample;
    int i, bit_offset;
	const uint8_t *sample_pos;

	if (!di || !di->dec_channelmap)
		return;

	oldpins_array_seed(di);
	for (i = 0; i < di->dec_num_channels; i++) {
		if (di->old_pins_array->data[i] != SRD_INITIAL_PIN_SAME_AS_SAMPLE0)
			continue;

        if (*(di->inbuf + i) == NULL) {
            sample = *(di->inbuf_const + i) ? 1 : 0;
            di->old_pins_array->data[i] = sample;
        } else {
            sample_pos = *(di->inbuf + i) + ((di->abs_cur_samplenum - di->abs_start_samplenum) / 8);
            bit_offset = (di->abs_cur_samplenum - di->abs_start_samplenum) % 8;
            sample = *sample_pos & (1 << bit_offset) ? 1 : 0;
            di->old_pins_array->data[i] = sample;
        }
	}
}

static gboolean term_matches(struct srd_decoder_inst *di,
        struct srd_term *term, gboolean *skip_allow)
{
	uint8_t old_sample, sample;
    int bit_offset, ch;
    const uint8_t *sample_pos;

	/* Caller ensures di, di->dec_channelmap, term, sample_pos != NULL. */

    *skip_allow = FALSE;
    if (term->type == SRD_TERM_SKIP) {
        if (di->abs_cur_matched && term->num_samples_to_skip == 0)
            di->skip_zero = TRUE;
		return sample_matches(0, 0, term);
    }

	ch = term->channel;
    if (*(di->inbuf + ch) == NULL) {
        sample = *(di->inbuf_const + ch) ? 1 : 0;
        *skip_allow = TRUE;
    } else {
        sample_pos = *(di->inbuf + ch) + ((di->abs_cur_samplenum - di->abs_start_samplenum) / 8);
        bit_offset = (di->abs_cur_samplenum - di->abs_start_samplenum) % 8;
        sample = *sample_pos & (1 << bit_offset) ? 1 : 0;
    }
	old_sample = di->old_pins_array->data[ch];

	return sample_matches(old_sample, sample, term);
}

static gboolean all_terms_match(struct srd_decoder_inst *di,
        const GSList *cond, gboolean *skip_allow)
{
	const GSList *l;
	struct srd_term *term;

	/* Caller ensures di, cond, sample_pos != NULL. */

	for (l = cond; l; l = l->next) {
		term = l->data;
        if (!term_matches(di, term, skip_allow))
			return FALSE;
	}

    if (di->skip_zero) {
        di->abs_cur_samplenum--;
        di->skip_zero = FALSE;
    }
	return TRUE;
}

static gboolean 
find_match(struct srd_decoder_inst *di)
{
    uint64_t j;
	GSList *l, *cond;
    gboolean skip_allow;
    gboolean all_skip_allow = TRUE;

	/* Caller ensures di != NULL. */

	/* Check whether the condition list is NULL/empty. */
    if (!di->condition_list) {
        srd_dbg("NULL/empty condition list, automatic match.");
        return TRUE;
    }

	/* Check whether we have any non-NULL conditions. */
    if (!have_non_null_conds(di)) {
        srd_dbg("Only NULL conditions in list, automatic match.");
        return TRUE;
    }

    /* di->match_array is 0 here. Create a new GArray. */
    di->match_array = 0;

	/* Sample 0: Set di->old_pins_array for SRD_INITIAL_PIN_SAME_AS_SAMPLE0 pins. */
    if (di->first_pos) {
        di->first_pos = FALSE;
		update_old_pins_array_initial_pins(di);
    }

    if (di->abs_cur_matched)
        di->abs_cur_samplenum++;

    while (di->abs_cur_samplenum < di->abs_end_samplenum) {

        /* Check whether the current sample matches at least one of the conditions (logical OR). */
        /* IMPORTANT: We need to check all conditions, even if there was a match already! */
        for (l = di->condition_list, j = 0; l; l = l->next, j++) {
            cond = l->data;
            if (!cond)
                continue;

            /* All terms in 'cond' must match (logical AND). */
            if (all_terms_match(di, cond, &skip_allow)) {
                all_skip_allow = FALSE;
                di->match_array |= (1 << j);
            } else {
                all_skip_allow &= skip_allow;
            }
        }

        update_old_pins_array(di);

        /* If at least one condition matched we're done. */
        di->abs_cur_matched = (di->match_array != 0);
        if (di->abs_cur_matched)
            return TRUE;

        if (all_skip_allow)
            di->abs_cur_samplenum = di->abs_end_samplenum;
        else
            di->abs_cur_samplenum++;
    }

	return FALSE;
}

/**
 * Process available samples and check if they match the defined conditions.
 *
 * This function returns if there is an error, or when a match is found, or
 * when all samples have been processed (whether a match was found or not).
 * This function immediately terminates when the decoder's wait() method
 * invocation shall get terminated.
 *
 * @param di The decoder instance to use. Must not be NULL.
 * @param found_match Will be set to TRUE if at least one condition matched,
 *                    FALSE otherwise. Must not be NULL.
 *
 * @retval SRD_OK No errors occured, see found_match for the result.
 * @retval SRD_ERR_ARG Invalid arguments.
 *
 * @private
 */
SRD_PRIV int process_samples_until_condition_match(struct srd_decoder_inst *di, gboolean *found_match)
{
	if (!di || !found_match)
		return SRD_ERR_ARG;

	*found_match = FALSE;
	if (di->want_wait_terminate)
		return SRD_OK;

	/* Check if any of the current condition(s) match. */
	while (TRUE) {
		/* Feed the (next chunk of the) buffer to find_match(). */
        *found_match = find_match(di);

		/* Did we handle all samples yet? */
        if (di->abs_cur_samplenum >= di->abs_end_samplenum) {
			srd_dbg("Done, handled all samples (abs cur %" PRIu64
				" / abs end %" PRIu64 ").",
				di->abs_cur_samplenum, di->abs_end_samplenum);
			return SRD_OK;
		}

		/* If we didn't find a match, continue looking. */
		if (!(*found_match))
			continue;

		/* At least one condition matched, return. */
		return SRD_OK;
	}

	return SRD_OK;
}

/**
 * Worker thread (per PD-stack).
 *
 * @param data Pointer to the lowest-level PD's device instance.
 *             Must not be NULL.
 *
 * @return NULL if there was an error.
 */
static gpointer di_thread(gpointer data)
{
	PyObject *py_res;
	struct srd_decoder_inst *di;
	int wanted_term = 0;
	PyGILState_STATE gstate;
	int is_task_stop_signal = FALSE;

	assert(data);

	di = data; 

	srd_dbg("%s: Starting thread routine for decoder.", di->inst_id);

	gstate = PyGILState_Ensure();

	/*
	 * Call self.decode(). Only returns if the PD throws an exception.
	 * "Regular" termination of the decode() method is not expected.
	 */
	srd_dbg("%s: Calling decode().", di->inst_id);
	py_res = PyObject_CallMethod(di->py_inst, "decode", NULL);
	srd_dbg("%s: decode() terminated.", di->inst_id);

	is_task_stop_signal = di->is_task_stop_signal;
	//srd_err("get flag:%d", is_task_stop_signal);

	/**
	 * decode() returns a error!
	 * before the send thread exits, write the error
	 * 
	*/
	if (py_res){
		
		di->decoder_state = SRD_ERR;

		if (PyUnicode_Check(py_res))
		{
			PyObject *py_bytes = PyUnicode_AsUTF8String(py_res);
			char *err_str = PyBytes_AsString(py_bytes);
			srd_err("python method decode() returns an error:\n %s", err_str);
			di->python_proc_error = g_strdup(err_str);
		}
		else{
			di->python_proc_error = g_strdup("python method decode() returns an unknown type error!");
		}

		Py_DecRef(py_res);
	}
	
	/**
	 * decode() throw an except!
	 * before the send thread exits, write the error
	 * 
	*/
	if (!py_res && !is_task_stop_signal)
	{
		srd_exception_catch(&di->python_proc_error, "Protocol decoder instance %s: ", di->inst_id);
	}

	/*
	 * Make sure to unblock potentially pending srd_inst_decode()
	 * calls in application threads after the decode() method might
	 * have terminated, while it neither has processed sample data
	 * nor has terminated upon request. This happens e.g. when "need
	 * a samplerate to decode" exception is thrown.
	 */
	g_mutex_lock(&di->data_mutex);
	wanted_term = di->want_wait_terminate;
	di->want_wait_terminate = TRUE;
	di->handled_all_samples = TRUE;
	g_cond_signal(&di->handled_all_samples_cond);
	g_mutex_unlock(&di->data_mutex);
 
	/*
	 * normal
	 * except
	 * returns a value
	 * task_stop_signal
	 */
	if (!is_task_stop_signal)
		srd_dbg("%s: decode() terminated (req %d).", di->inst_id, wanted_term);

	PyErr_Clear();	
	PyGILState_Release(gstate); 

	return NULL;
}

/**
 * Decode a chunk of samples.
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
 *   srd_inst_decode(di, 0,    1024, inbuf, 1024, 1);
 *   srd_inst_decode(di, 1024, 2048, inbuf, 1024, 1);
 *   srd_inst_decode(di, 2048, 3072, inbuf, 1024, 1);
 *   srd_inst_decode(di, 3072, 4096, inbuf, 1024, 1);
 *
 * The chunk size ('inbuflen') can be arbitrary and can differ between calls.
 *
 * Correct example (4096 samples total, 7 chunks @ various samples each):
 *   srd_inst_decode(di, 0,    1024, inbuf, 1024, 1);
 *   srd_inst_decode(di, 1024, 1124, inbuf,  100, 1);
 *   srd_inst_decode(di, 1124, 1424, inbuf,  300, 1);
 *   srd_inst_decode(di, 1424, 1643, inbuf,  219, 1);
 *   srd_inst_decode(di, 1643, 2048, inbuf,  405, 1);
 *   srd_inst_decode(di, 2048, 3072, inbuf, 1024, 1);
 *   srd_inst_decode(di, 3072, 4096, inbuf, 1024, 1);
 *
 * INCORRECT example (4096 samples total, 4 chunks @ 1024 samples each, but
 * the start- and end-samplenumbers are not absolute):
 *   srd_inst_decode(di, 0,    1024, inbuf, 1024, 1);
 *   srd_inst_decode(di, 0,    1024, inbuf, 1024, 1);
 *   srd_inst_decode(di, 0,    1024, inbuf, 1024, 1);
 *   srd_inst_decode(di, 0,    1024, inbuf, 1024, 1);
 *
 * @param di The decoder instance to call. Must not be NULL.
 * @param abs_start_samplenum The absolute starting sample number for the
 * 		buffer's sample set, relative to the start of capture.
 * @param abs_end_samplenum The absolute ending sample number for the
 * 		buffer's sample set, relative to the start of capture.
 * @param inbuf The buffer to decode. Must not be NULL.
 * @param inbuflen Length of the buffer. Must be > 0.
 * @param unitsize The number of bytes per sample. Must be > 0.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @private
 */
SRD_PRIV int srd_inst_decode(struct srd_decoder_inst *di,
		uint64_t abs_start_samplenum, uint64_t abs_end_samplenum,
        const uint8_t **inbuf, const uint8_t *inbuf_const, uint64_t inbuflen,
        char **error)
{
	/* Return an error upon unusable input. */
	if (!di) {
        *error = g_strdup("empty decoder instance");
		return SRD_ERR_ARG;
	}
	if (!inbuf) {
        *error = g_strdup("NULL buffer pointer");
		return SRD_ERR_ARG;
	}
	if (inbuflen == 0) {
        *error = g_strdup("empty buffer");
		return SRD_ERR_ARG;
	}

    if (di->first_pos) {
        di->abs_cur_samplenum = abs_start_samplenum;
    }

	if (abs_start_samplenum != di->abs_cur_samplenum ||
	    abs_end_samplenum < abs_start_samplenum) {
		srd_dbg("Incorrect sample numbers: start=%" PRIu64 ", cur=%"
			PRIu64 ", end=%" PRIu64 ".", abs_start_samplenum,
			di->abs_cur_samplenum, abs_end_samplenum);
		return SRD_ERR_ARG;
	}

	srd_dbg("Decoding: abs start sample %" PRIu64 ", abs end sample %"
        PRIu64 " (%" PRIu64 " samples, %" PRIu64 " bytes), instance %s.",
        abs_start_samplenum, abs_end_samplenum,
        abs_end_samplenum - abs_start_samplenum, inbuflen, di->inst_id);

	/* 
		If this is the first call, start the worker thread. 
		One session may be have more decoder,so more thread will be created
	*/
	if (!di->thread_handle) {
		srd_dbg("No worker thread for this decoder stack "
			"exists yet, creating one: %s.", di->inst_id);

		di->thread_handle = g_thread_new(di->inst_id,
						 		di_thread, di);
	}

	/* Push the new sample chunk to the worker thread. */
	g_mutex_lock(&di->data_mutex);
    di->abs_start_samplenum = abs_start_samplenum & ~7ULL;
	di->abs_end_samplenum = abs_end_samplenum;
	di->inbuf = inbuf;
    di->inbuf_const = inbuf_const;
	di->inbuflen = inbuflen;
	di->got_new_samples = TRUE;
	di->handled_all_samples = FALSE;

	/* Signal the thread that we have new data. */
	g_cond_signal(&di->got_new_samples_cond);
	g_mutex_unlock(&di->data_mutex);

	/* When all samples in this chunk were handled, return. */
	g_mutex_lock(&di->data_mutex);
	while (!di->handled_all_samples && !di->want_wait_terminate)
		g_cond_wait(&di->handled_all_samples_cond, &di->data_mutex);
	g_mutex_unlock(&di->data_mutex);


	//the python got error
	if (di->python_proc_error)
	{
		*error = di->python_proc_error;
		di->python_proc_error = NULL;
		return SRD_ERR_TERM_REQ;
	}				

	return SRD_OK;
}

/**
 * Terminate current decoder work, prepare for re-use on new input data.
 *
 * Terminates all decoder operations in the specified decoder instance
 * and the instances stacked on top of it. Resets internal state such
 * that the previously constructed stack can process new input data that
 * is not related to previously processed input data. This avoids the
 * expensive and complex re-construction of decoder stacks.
 *
 * Callers are expected to follow up with start, metadata, and decode
 * calls like they would for newly constructed decoder stacks.
 *
 * @param di The decoder instance to call. Must not be NULL.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @private
 */
SRD_PRIV int srd_inst_terminate_reset(struct srd_decoder_inst *di)
{
	PyGILState_STATE gstate;
	PyObject *py_ret;
	GSList *l;
	int ret;

	if (!di)
		return SRD_ERR_ARG;

	/*
	 * Request termination and wait for previously initiated
	 * background operation to finish. Reset internal state, but
	 * do not start releasing resources yet. This shall result in
	 * decoders' state just like after creation. This block handles
	 * the C language library side.
	 */
	srd_dbg("Terminating instance %s", di->inst_id);
	srd_inst_join_decode_thread(di);
	srd_inst_reset_state(di);

	/*
	 * Have the Python side's .reset() method executed (if the PD
	 * implements it). It's assumed that .reset() assigns variables
	 * very much like __init__() used to do in the past. Thus memory
	 * that was allocated in previous calls gets released by Python
	 * as it's not referenced any longer.
	 */
	gstate = PyGILState_Ensure();
	if (PyObject_HasAttrString(di->py_inst, "reset")) {
		srd_dbg("Calling reset() of instance %s", di->inst_id);
		py_ret = PyObject_CallMethod(di->py_inst, "reset", NULL);
		Py_XDECREF(py_ret);
	}
	PyGILState_Release(gstate);

	/* Pass the "restart" request to all stacked decoders. */
	for (l = di->next_di; l; l = l->next) {
		ret = srd_inst_terminate_reset(l->data);
		if (ret != SRD_OK)
			return ret;
	}

	return di->decoder_state;
}

/** @private */
SRD_PRIV void srd_inst_free(struct srd_decoder_inst *di)
{
	GSList *l;
	struct srd_pd_output *pdo;
	PyGILState_STATE gstate;

	srd_dbg("Freeing instance %s.", di->inst_id);

	srd_inst_join_decode_thread(di);

	srd_inst_reset_state(di);

	gstate = PyGILState_Ensure();
	Py_DecRef(di->py_inst);
    if (di->py_pinvalues) {
        Py_DecRef(di->py_pinvalues);
    }
	PyGILState_Release(gstate);

	g_free(di->inst_id);
	g_free(di->dec_channelmap);
	g_slist_free(di->next_di);
	for (l = di->pd_output; l; l = l->next) {
		pdo = l->data;
		g_free(pdo->proto_id);
        if (pdo->meta_name)
            g_free(pdo->meta_name);
        if (pdo->meta_descr)
            g_free(pdo->meta_descr);
		g_free(pdo);
	}
	g_slist_free(di->pd_output);
	g_free(di);
}

/** @private */
SRD_PRIV void srd_inst_free_all(struct srd_session *sess)
{
	if (!sess)
		return;

	g_slist_free_full(sess->di_list, (GDestroyNotify)srd_inst_free);
}

/** @} */
