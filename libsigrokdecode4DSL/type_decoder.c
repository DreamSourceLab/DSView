/*
 * This file is part of the libsigrokdecode project.
 *
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
#include <inttypes.h>
#include <string.h>
#include "log.h"

/** @cond PRIVATE */
extern SRD_PRIV GSList *sessions;
/** @endcond */

typedef struct {
        PyObject_HEAD
} srd_Decoder;

/* This is only used for nicer srd_dbg() output. */
SRD_PRIV const char *output_type_name(unsigned int idx)
{
	static const char names[][16] = {
		"OUTPUT_ANN",
		"OUTPUT_PYTHON",
		"OUTPUT_BINARY",
		"OUTPUT_META",
		"(invalid)"
	};

	return names[MIN(idx, G_N_ELEMENTS(names) - 1)];
}

static void release_annotation(struct srd_proto_data_annotation *pda)
{
	if (!pda)
		return;
	if (pda->ann_text)
		g_strfreev(pda->ann_text);
}

static int py_parse_ann_data(PyObject *list_obj, char ***out_strv, int list_size, char *hex_str_buf, long long *numberic_value)
{
	PyObject *py_bytes;
	char **strv, *str; 
	PyGILState_STATE gstate;
	int ret = SRD_ERR_PYTHON;
	int text_num = 0;
	PyObject* text_items[10];
	PyObject *py_tmp;
	PyObject *py_numobj = NULL;	
	int i; 
	long long lv; 
	int nstr;
	char *up_ptr;
	char c;
	char *str_tmp;
	 
	gstate = PyGILState_Ensure();

    str = NULL;
    strv = NULL;  

	//get annotation text count
	for (i = 0; i < list_size; i++){
		py_tmp = PyList_GetItem(list_obj, i);

		//is a string
		if (PyUnicode_Check(py_tmp)){
			text_items[text_num] = py_tmp;
			text_num++;
		}
		else if (PyLong_Check(py_tmp)){
			py_numobj = py_tmp;	
		}	
	}

	if (py_numobj == NULL && text_num == 0){
		srd_err("list element type must be string or numberical");
		goto err;
	}

	//get numberic value
	if (py_numobj != NULL){
		lv = PyLong_AsLongLong(py_numobj);
		sprintf(hex_str_buf, "%02llX", lv);
		*numberic_value = lv;
	}

	//have no text, only one numberical
	if (text_num == 0){
		PyGILState_Release(gstate);
		return SRD_OK;
	}
 
	//more annotation text
	strv = g_try_new0(char *, text_num + 1);
	if (!strv) {
		srd_err("Failed to allocate result string vector.");
		ret = SRD_ERR_MALLOC;
		goto err;
	}

	for (i = 0; i < text_num; i++) { 
		py_bytes = PyUnicode_AsUTF8String(text_items[i]);
		if (!py_bytes)
			goto err;

		str = g_strdup(PyBytes_AsString(py_bytes));
		Py_DECREF(py_bytes);
		if (!str)
			goto err;

		//check numberic field value
		if (str[0]== '@'){
			nstr = strlen(str) - 1;

			if (nstr > 0 && nstr < DECODE_NUM_HEX_MAX_LEN){
				strcpy(hex_str_buf, str + 1);
				
				str[0] = '\n';  //set ignore flag
				str[1] = 0;

				//convert to upper
				up_ptr = hex_str_buf;

				while (*up_ptr)
				{
					c = *up_ptr;

					if (c >= 'a' && c <= 'f'){
						*up_ptr = c - 32;
					}
					
					up_ptr++;
				}
			}
			else if (nstr > 0){
				// Remove the first letter.
				str_tmp = g_strdup(str+1);
				free(str);
				str = str_tmp;
			}						
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

/*
 @obj is the fourth param from python calls put()
*/
static int convert_annotation(struct srd_decoder_inst *di, PyObject *obj,
		struct srd_proto_data *pdata)
{
	PyObject *py_tmp;
	struct srd_proto_data_annotation *pda;
	int ann_class;
    char **ann_text;
	gpointer ann_type_ptr;
	PyGILState_STATE gstate;
	int ann_size; 

	pda = pdata->data;

	gstate = PyGILState_Ensure();

	/* Should be a list of [annotation class, [string, ...]]. */
	if (!PyList_Check(obj)) {
		srd_err("Protocol decoder %s submitted an annotation that"
			" is not a list", di->decoder->name);
		goto err;
	}

	/* Should have 2 elements. */
	if (PyList_Size(obj) != 2) {
		srd_err("Protocol decoder %s submitted annotation list with "
			"%zd elements instead of 2", di->decoder->name,
			PyList_Size(obj));
		goto err;
	}

	/*
	 * The first element should be an integer matching a previously
	 * registered annotation class.
	 */
	py_tmp = PyList_GetItem(obj, 0);
	if (!PyLong_Check(py_tmp)) {
		srd_err("Protocol decoder %s submitted annotation list, but "
			"first element was not an integer.", di->decoder->name);
		goto err;
	}
	ann_class = PyLong_AsLong(py_tmp);

	if ((ann_class >= (int)g_slist_length(di->decoder->ann_types)) || ann_class < 0) {
		srd_err("Protocol decoder %s submitted data to unregistered "
			"annotation class %d.", di->decoder->name, ann_class);
		goto err;
	}
	ann_type_ptr = g_slist_nth_data(di->decoder->ann_types, ann_class);

	/* 
		Second element must be a list.
	 */
	py_tmp = PyList_GetItem(obj, 1);
	if (!PyList_Check(py_tmp)) {
		srd_err("Protocol decoder %s submitted annotation list, but "
			"second element was not a list.", di->decoder->name);
		goto err;
	}

	ann_size = PyList_Size(py_tmp);
	if (ann_size == 0){
			srd_err("Protocol decoder %s, put() param, the annotation list is empty.", di->decoder->name);
		goto err;
	}
	 
	pda->str_number_hex[0] = 0;
	ann_text = NULL;
	pda->numberic_value = 0; 

    if (py_parse_ann_data(py_tmp, &ann_text, ann_size, pda->str_number_hex, &pda->numberic_value) != SRD_OK) {
        srd_err("Protocol decoder %s submitted annotation list, but "
            "second element was malformed.", di->decoder->name);
        goto err;
    }
 
	pda->ann_class = ann_class;
	pda->ann_type = GPOINTER_TO_INT(ann_type_ptr);
    pda->ann_text = ann_text;

	PyGILState_Release(gstate);

	return SRD_OK;

err:
	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
}

static void release_binary(struct srd_proto_data_binary *pdb)
{
	if (!pdb)
		return;
	g_free((void *)pdb->data);
}

static int convert_binary(struct srd_decoder_inst *di, PyObject *obj,
		struct srd_proto_data *pdata)
{
	struct srd_proto_data_binary *pdb;
	PyObject *py_tmp;
	Py_ssize_t size;
	int bin_class;
	char *class_name, *buf;
	PyGILState_STATE gstate;

	gstate = PyGILState_Ensure();

	/* Should be a list of [binary class, bytes]. */
	if (!PyList_Check(obj)) {
		srd_err("Protocol decoder %s submitted non-list for SRD_OUTPUT_BINARY.",
			di->decoder->name);
		goto err;
	}

	/* Should have 2 elements. */
	if (PyList_Size(obj) != 2) {
		srd_err("Protocol decoder %s submitted SRD_OUTPUT_BINARY list "
				"with %zd elements instead of 2", di->decoder->name,
				PyList_Size(obj));
		goto err;
	}

	/* The first element should be an integer. */
	py_tmp = PyList_GetItem(obj, 0);
	if (!PyLong_Check(py_tmp)) {
		srd_err("Protocol decoder %s submitted SRD_OUTPUT_BINARY list, "
			"but first element was not an integer.", di->decoder->name);
		goto err;
	}
	bin_class = PyLong_AsLong(py_tmp);
	if (!(class_name = g_slist_nth_data(di->decoder->binary, bin_class))) {
		srd_err("Protocol decoder %s submitted SRD_OUTPUT_BINARY with "
			"unregistered binary class %d.", di->decoder->name, bin_class);
		goto err;
	}

	/* Second element should be bytes. */
	py_tmp = PyList_GetItem(obj, 1);
	if (!PyBytes_Check(py_tmp)) {
		srd_err("Protocol decoder %s submitted SRD_OUTPUT_BINARY list, "
			"but second element was not bytes.", di->decoder->name);
		goto err;
	}

	/* Consider an empty set of bytes a bug. */
	if (PyBytes_Size(py_tmp) == 0) {
		srd_err("Protocol decoder %s submitted SRD_OUTPUT_BINARY "
				"with empty data set.", di->decoder->name);
		goto err;
	}

	if (PyBytes_AsStringAndSize(py_tmp, &buf, &size) == -1)
		goto err;

	PyGILState_Release(gstate);

	pdb = pdata->data;
	pdb->bin_class = bin_class;
	pdb->size = size;
	if (!(pdb->data = g_try_malloc0(pdb->size))){
		srd_err("%s,ERROR:failed to alloc memory.", __func__);
		return SRD_ERR_MALLOC;
	}

	memcpy((void *)pdb->data, (const void *)buf, pdb->size);

	return SRD_OK;

err:
	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
}

static inline struct srd_decoder_inst *srd_sess_inst_find_by_obj(
	struct srd_session *sess, const GSList *stack, const PyObject *obj)
{
	const GSList *l;
	struct srd_decoder_inst *tmp, *di;

	if (!sess)
		return NULL;

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
 * @since 0.1.0
 */
static inline struct srd_decoder_inst *srd_inst_find_by_obj(
		const GSList *stack, const PyObject *obj)
{
	struct srd_decoder_inst *di;
	struct srd_session *sess;
	GSList *l;

	/* Performance shortcut: Handle the most common case first. */
	sess = sessions->data;
	di = sess->di_list->data;
	if (di->py_inst == obj)
		return di;

	di = NULL;
	for (l = sessions; di == NULL && l != NULL; l = l->next) {
		sess = l->data;
		di = srd_sess_inst_find_by_obj(sess, stack, obj);
	}

	return di;
}

static int convert_meta(struct srd_proto_data *pdata, PyObject *obj)
{
	long long intvalue;
	double dvalue;
	PyGILState_STATE gstate;

	gstate = PyGILState_Ensure();

	if (g_variant_type_equal(pdata->pdo->meta_type, G_VARIANT_TYPE_INT64)) {
		if (!PyLong_Check(obj)) {
			PyErr_Format(PyExc_TypeError, "This output was registered "
					"as 'int', but something else was passed.");
			goto err;
		}
		intvalue = PyLong_AsLongLong(obj);
		if (PyErr_Occurred())
			goto err;
		pdata->data = g_variant_new_int64(intvalue);
	} else if (g_variant_type_equal(pdata->pdo->meta_type, G_VARIANT_TYPE_DOUBLE)) {
		if (!PyFloat_Check(obj)) {
			PyErr_Format(PyExc_TypeError, "This output was registered "
					"as 'float', but something else was passed.");
			goto err;
		}
		dvalue = PyFloat_AsDouble(obj);
		if (PyErr_Occurred())
			goto err;
		pdata->data = g_variant_new_double(dvalue);
	}

	PyGILState_Release(gstate);

	return SRD_OK;

err:
	PyGILState_Release(gstate);

	return SRD_ERR_PYTHON;
}

static void release_meta(GVariant *gvar)
{
	if (!gvar)
		return;
	g_variant_unref(gvar);
}

static PyObject *Decoder_put(PyObject *self, PyObject *args)
{
	GSList *l;
	PyObject *py_data, *py_res;
	struct srd_decoder_inst *di, *next_di;
	struct srd_pd_output *pdo;
	struct srd_proto_data pdata;
	struct srd_proto_data_annotation pda;
	struct srd_proto_data_binary pdb;
	uint64_t start_sample, end_sample;
	int output_id;
	struct srd_pd_callback *cb;
	PyGILState_STATE gstate; 

	py_data = NULL; //the fourth param from python

	gstate = PyGILState_Ensure();

	if (!(di = srd_inst_find_by_obj(NULL, self))) {
		/* Shouldn't happen. */
		srd_dbg("put(): self instance not found.");
		goto err;
	}

	if (!PyArg_ParseTuple(args, "KKiO", &start_sample, &end_sample,
		&output_id, &py_data)) {
		/*
		 * This throws an exception, but by returning NULL here we let
		 * Python raise it. This results in a much better trace in
		 * controller.c on the decode() method call.
		 */
		goto err;
	}

	if (!(l = g_slist_nth(di->pd_output, output_id))) {
		srd_err("Protocol decoder %s submitted invalid output ID %d.",
			di->decoder->name, output_id);
		goto err;
	}
	pdo = l->data;

	/* Upon SRD_OUTPUT_PYTHON for stacked PDs, we have a nicer log message later. */
	if (pdo->output_type != SRD_OUTPUT_PYTHON && di->next_di != NULL) {
        srd_detail("Instance %s put %"PRIu64 "-%" PRIu64 " %s on "
			 "oid %d (%s).", di->inst_id, start_sample, end_sample,
			 output_type_name(pdo->output_type), output_id,
			 pdo->proto_id);
	}

	pdata.start_sample = start_sample;
	pdata.end_sample = end_sample;
	pdata.pdo = pdo;
	pdata.data = NULL;

	switch (pdo->output_type) {
	case SRD_OUTPUT_ANN:
		/* Annotations are only fed to callbacks. */
		if ((cb = srd_pd_output_callback_find(di->sess, pdo->output_type))) {
			pdata.data = &pda;
			/* Convert from PyDict to srd_proto_data_annotation. */
			if (convert_annotation(di, py_data, &pdata) != SRD_OK) {
				/* An error was already logged. */
				break;
			}
			Py_BEGIN_ALLOW_THREADS
			cb->cb(&pdata, cb->cb_data);
			Py_END_ALLOW_THREADS
			release_annotation(pdata.data);
		}
		break;

    case SRD_OUTPUT_PYTHON:
        for (l = di->next_di; l; l = l->next) {
            next_di = l->data;

            srd_detail("Instance %s put %" PRIu64 "-%" PRIu64 " %s "
                 "on oid %d (%s) to instance %s.", di->inst_id,
                 start_sample,
                 end_sample, output_type_name(pdo->output_type),
                 output_id, pdo->proto_id, next_di->inst_id);

            if (!(py_res = PyObject_CallMethod(
                next_di->py_inst, "decode", "KKO", start_sample,
                end_sample, py_data))) {
                srd_exception_catch(NULL, "Calling %s decode() failed",
                            next_di->inst_id);
            }

            Py_XDECREF(py_res);
        }
        if ((cb = srd_pd_output_callback_find(di->sess, pdo->output_type))) {
            /*
             * Frontends aren't really supposed to get Python
             * callbacks, but it's useful for testing.
             */
            pdata.data = py_data;
            cb->cb(&pdata, cb->cb_data);
        }
        break;
    case SRD_OUTPUT_BINARY:
        if ((cb = srd_pd_output_callback_find(di->sess, pdo->output_type))) {
            pdata.data = &pdb;
            /* Convert from PyDict to srd_proto_data_binary. */
            if (convert_binary(di, py_data, &pdata) != SRD_OK) {
                /* An error was already logged. */
                break;
            }
            Py_BEGIN_ALLOW_THREADS
            cb->cb(&pdata, cb->cb_data);
            Py_END_ALLOW_THREADS
            release_binary(pdata.data);
        }
        break;
    case SRD_OUTPUT_META:
        if ((cb = srd_pd_output_callback_find(di->sess, pdo->output_type))) {
            /* Annotations need converting from PyObject. */
            if (convert_meta(&pdata, py_data) != SRD_OK) {
                /* An exception was already set up. */
                break;
            }
            Py_BEGIN_ALLOW_THREADS
            cb->cb(&pdata, cb->cb_data);
            Py_END_ALLOW_THREADS
            release_meta(pdata.data);
        }
        break;
    default:
        srd_err("Protocol decoder %s submitted invalid output type %d.",
            di->decoder->name, pdo->output_type);
        break;
    }

	PyGILState_Release(gstate);

	Py_RETURN_NONE;

err:
	PyGILState_Release(gstate);

	return NULL;
}

/*
 return output info index
*/
static PyObject *Decoder_register(PyObject *self, PyObject *args,
		PyObject *kwargs)
{
	struct srd_decoder_inst *di;
	struct srd_pd_output *pdo;
	PyObject *py_new_output_id;
	PyTypeObject *meta_type_py;
	const GVariantType *meta_type_gv;
	int output_type;
	char *proto_id, *meta_name, *meta_descr;
	char *keywords[] = { "output_type", "proto_id", "meta", NULL };
	PyGILState_STATE gstate;
	gboolean is_meta;
	GSList *l;
	struct srd_pd_output *cmp;

	gstate = PyGILState_Ensure();

	meta_type_py = NULL;
	meta_type_gv = NULL;
	meta_name = meta_descr = NULL;

	if (!(di = srd_inst_find_by_obj(NULL, self))) {
		PyErr_SetString(PyExc_Exception, "decoder instance not found");
		goto err;
	}

	/* Default to instance ID, which defaults to class ID. */
	proto_id = di->inst_id;
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i|s(Oss)", keywords,
			&output_type, &proto_id,
			&meta_type_py, &meta_name, &meta_descr)) {
		/* Let Python raise this exception. */
		goto err;
	}

	/* Check if the meta value's type is supported. */
	is_meta = output_type == SRD_OUTPUT_META;
	if (is_meta) {
		if (meta_type_py == &PyLong_Type)
			meta_type_gv = G_VARIANT_TYPE_INT64;
		else if (meta_type_py == &PyFloat_Type)
			meta_type_gv = G_VARIANT_TYPE_DOUBLE;
		else {
			PyErr_Format(PyExc_TypeError, "Unsupported type.");
			goto err;
		}
	}

	pdo = NULL;
	for (l = di->pd_output; l; l = l->next) {
		cmp = l->data;
		if (cmp->output_type != output_type)
			continue;
		if (strcmp(cmp->proto_id, proto_id) != 0)
			continue;
		if (is_meta && cmp->meta_type != meta_type_gv)
			continue;
		if (is_meta && strcmp(cmp->meta_name, meta_name) != 0)
			continue;
		if (is_meta && strcmp(cmp->meta_descr, meta_descr) != 0)
			continue;
		pdo = cmp;
		break;
	}

	if (pdo) {
		py_new_output_id = Py_BuildValue("i", pdo->pdo_id);
		PyGILState_Release(gstate);
		return py_new_output_id;
	}

	pdo = g_try_malloc0(sizeof(struct srd_pd_output));
	if (pdo == NULL){
		PyGILState_Release(gstate);
		srd_err("%s,ERROR:failed to alloc memory.", __func__);
		return NULL;
	}
    memset(pdo, 0, sizeof(struct srd_pd_output));

	/* pdo_id is just a simple index, nothing is deleted from this list anyway. */
	pdo->pdo_id = g_slist_length(di->pd_output);
	pdo->output_type = output_type;
	pdo->di = di;
	pdo->proto_id = g_strdup(proto_id);
    pdo->meta_name = NULL;
    pdo->meta_descr = NULL;

	if (output_type == SRD_OUTPUT_META) {
		pdo->meta_type = meta_type_gv;
		pdo->meta_name = g_strdup(meta_name);
		pdo->meta_descr = g_strdup(meta_descr);
	}

	di->pd_output = g_slist_append(di->pd_output, pdo);
	py_new_output_id = Py_BuildValue("i", pdo->pdo_id);

	PyGILState_Release(gstate);

	srd_dbg("Instance %s creating new output type %s as oid %d (%s).",
		di->inst_id, output_type_name(output_type), pdo->pdo_id,
		proto_id);

	return py_new_output_id;

err:
	PyGILState_Release(gstate);

	return NULL;
}

static int get_term_type(const char *v)
{
	switch (v[0]) {
	case 'h':
		return SRD_TERM_HIGH;
	case 'l':
		return SRD_TERM_LOW;
	case 'r':
		return SRD_TERM_RISING_EDGE;
	case 'f':
		return SRD_TERM_FALLING_EDGE;
	case 'e':
		return SRD_TERM_EITHER_EDGE;
	case 'n':
		return SRD_TERM_NO_EDGE;
	default:
		return -1;
	}

	return -1;
}

/**
 * Get the pin values at the current sample number.
 *
 * @param di The decoder instance to use. Must not be NULL.
 *           The number of channels must be >= 1.
 *
 * @return A newly allocated PyTuple containing the pin values at the
 *         current sample number.
 */
static int get_current_pinvalues(const struct srd_decoder_inst *di)
{
	int i;
	uint8_t sample;
	const uint8_t *sample_pos;
    int bit_offset;
	PyGILState_STATE gstate;

	if (!di) {
		srd_err("Invalid decoder instance.");
        return SRD_ERR_ARG;
	}

	gstate = PyGILState_Ensure();

	for (i = 0; i < di->dec_num_channels; i++) {
		/* A channelmap value of -1 means "unused optional channel". */
		if (di->dec_channelmap[i] == -1) {
			/* Value of unused channel is 0xff, instead of 0 or 1. 
			   Done set -1 by srd_inst_channel_set_all()
			*/
            PyTuple_SetItem(di->py_pinvalues, i, PyLong_FromLong(0xff));
		} else {
            if (*(di->inbuf + i) == NULL) {
                sample = *(di->inbuf_const + i) ? 1 : 0;
                PyTuple_SetItem(di->py_pinvalues, i, PyLong_FromLong(sample));
            } else {
                sample_pos = *(di->inbuf + i) + ((di->abs_cur_samplenum - di->abs_start_samplenum) / 8);
                bit_offset = (di->abs_cur_samplenum - di->abs_start_samplenum) % 8;
                sample = *sample_pos & (1 << bit_offset) ? 1 : 0;
                PyTuple_SetItem(di->py_pinvalues, i, PyLong_FromLong(sample));
            }
		}
	}

	PyGILState_Release(gstate);

    return SRD_OK;
}

/**
 * Create a list of terms in the specified condition.
 *
 * If there are no terms in the condition, 'term_list' will be NULL.
 *
 * @param py_dict A Python dict containing terms. Must not be NULL.
 * @param term_list Pointer to a GSList which will be set to the newly
 *                  created list of terms. Must not be NULL.
 *
 * @return SRD_OK upon success, a negative error code otherwise.
 */
static int create_term_list(PyObject *py_dict, GSList **term_list, gboolean cur_matched)
{
	Py_ssize_t pos = 0;
	PyObject *py_key, *py_value;
	struct srd_term *term;
	uint64_t num_samples_to_skip;
	char *term_str;
	PyGILState_STATE gstate;

	if (!py_dict || !term_list)
		return SRD_ERR_ARG;

	/* "Create" an empty GSList of terms. */
	*term_list = NULL;

	gstate = PyGILState_Ensure();

	/* Iterate over all items in the current dict. */
	while (PyDict_Next(py_dict, &pos, &py_key, &py_value)) {
		/* Check whether the current key is a string or a number. */
		if (PyLong_Check(py_key)) {
			/* The key is a number. */
			/* TODO: Check if the number is a valid channel. */
			/* Get the value string. */
			/* key defined channel id, value defined term type*/
			if ((py_object_to_str_alloc(py_value, &term_str)) != SRD_OK) {
				srd_err("Failed to get the value.");
				goto err;
			} 

			term = g_try_malloc0(sizeof(struct srd_term));
			if (term != NULL){
                memset(term, 0, sizeof(struct srd_term));
				term->type = get_term_type(term_str);
				term->channel = PyLong_AsLong(py_key);
			}
			else{
				srd_err("%s,ERROR:failed to alloc memory.", __func__);
			}
			
			g_free(term_str);

		} else if (PyUnicode_Check(py_key)) {
			/* The key is a string. */
			/* TODO: Check if it's "skip". */
			if ((py_object_to_uint(py_value, &num_samples_to_skip)) != SRD_OK) {
				srd_err("Failed to get number of samples to skip.");
				goto err;
			}
			term = g_try_malloc0(sizeof(struct srd_term));
			if (term != NULL){
                memset(term, 0, sizeof(struct srd_term));
				term->type = SRD_TERM_SKIP;
				term->num_samples_to_skip = num_samples_to_skip;
				term->num_samples_already_skipped = cur_matched ? (term->num_samples_to_skip != 0) : 0;
			}
			else{
				srd_err("%s,ERROR:failed to alloc memory.", __func__);
			}	
			
		} else {
			srd_err("Term key is neither a string nor a number.");
			goto err;
		}

		/* Add the term to the list of terms. */
		*term_list = g_slist_append(*term_list, term);
	}

	PyGILState_Release(gstate);

	return SRD_OK;

err:
	PyGILState_Release(gstate);

	return SRD_ERR;
}

/**
 * Replace the current condition list with the new one.
 *
 * @param self TODO. Must not be NULL.
 * @param args TODO. Must not be NULL.
 *
 * @retval SRD_OK The new condition list was set successfully.
 * @retval SRD_ERR There was an error setting the new condition list.
 *                 The contents of di->condition_list are undefined.
 * @retval 9999 TODO.
 */
static int set_new_condition_list(struct srd_decoder_inst *di, PyObject *args)
{
	GSList *term_list;
	PyObject *py_conditionlist, *py_conds, *py_dict;
	int i, num_conditions, ret;
	PyGILState_STATE gstate;

    if (!args)
		return SRD_ERR_ARG;

	gstate = PyGILState_Ensure();

	/*
	 * Return an error condition from .wait() when termination is
	 * requested, such that decode() will terminate.
	 */
	if (di->want_wait_terminate) {
		srd_dbg("%s: %s: Skip (want_term).", di->inst_id, __func__);
		goto err;
	}

	/*
	 * Parse the argument of self.wait() into 'py_conds', and check
	 * the data type. The argument is optional, None is assumed in
	 * its absence. None or an empty dict or an empty list mean that
	 * there is no condition, and the next available sample shall
	 * get returned to the caller.
	 */
    py_conds = Py_None;
	if (!PyArg_ParseTuple(args, "|O", &py_conds)) {
		/* Let Python raise this exception. */
		goto err;
	}

	if (py_conds == Py_None) {
		/* 'py_conds' is None. */
		goto ret_9999;

	} else if (PyList_Check(py_conds)) {
		/* 'py_conds' is a list. */
		py_conditionlist = py_conds;
		num_conditions = PyList_Size(py_conditionlist);
		if (num_conditions == 0)
			goto ret_9999; /* The PD invoked self.wait([]). */
        Py_IncRef(py_conditionlist);

	} else if (PyDict_Check(py_conds)) {
		/* 'py_conds' is a dict. */
		if (PyDict_Size(py_conds) == 0)
			goto ret_9999; /* The PD invoked self.wait({}). */
		/* Make a list and put the dict in there for convenience. */
		py_conditionlist = PyList_New(1);
		Py_IncRef(py_conds);
		PyList_SetItem(py_conditionlist, 0, py_conds);
		num_conditions = 1;

	} else {
		srd_err("Condition list is neither a list nor a dict.");
		goto err;
	}

	/* Free the old condition list. */
	condition_list_free(di);

	ret = SRD_OK;

	/* py_conditionlist is a list */
	/* Iterate over the conditions, set di->condition_list accordingly. */
	for (i = 0; i < num_conditions; i++) {
		/* Get a condition (dict) from the condition list. */
		py_dict = PyList_GetItem(py_conditionlist, i);
		if (!PyDict_Check(py_dict)) {
			srd_err("Condition is not a dict.");
			ret = SRD_ERR;
			break;
		}

		/* Create the list of terms in this condition. */
        if ((ret = create_term_list(py_dict, &term_list, di->abs_cur_matched)) < 0)
			break;

		/* Add the new condition to the PD instance's condition list. */
		di->condition_list = g_slist_append(di->condition_list, term_list);
	}

	Py_DecRef(py_conditionlist);

	PyGILState_Release(gstate);

	return ret;

err:
	PyGILState_Release(gstate);

	return SRD_ERR;

ret_9999:
	PyGILState_Release(gstate);

	return 9999;
}

/**
 * Create a SKIP condition list for condition-less .wait() calls.
 *
 * @param di Decoder instance.
 * @param count Number of samples to skip.
 *
 * @retval SRD_OK The new condition list was set successfully.
 * @retval SRD_ERR There was an error setting the new condition list.
 *                 The contents of di->condition_list are undefined.
 *
 * This routine is a reduced and specialized version of the @ref
 * set_new_condition_list() and @ref create_term_list() routines which
 * gets invoked when .wait() was called without specifications for
 * conditions. This minor duplication of the SKIP term list creation
 * simplifies the logic and avoids the creation of expensive Python
 * objects with "constant" values which the caller did not pass in the
 * first place. It results in maximum sharing of match handling code
 * paths.
 */
static int set_skip_condition(struct srd_decoder_inst *di, uint64_t count)
{
	assert(di);

	struct srd_term *term;
	GSList *term_list = NULL;

	condition_list_free(di);

	term = g_try_malloc0(sizeof(struct srd_term));
	if (term != NULL){
		memset(term, 0, sizeof(struct srd_term));
		term->type = SRD_TERM_SKIP;
		term->num_samples_to_skip = count;
		term->num_samples_already_skipped = di->abs_cur_matched ? (term->num_samples_to_skip != 0) : 0;
		term_list = g_slist_append(NULL, term);
	}
	else{
		srd_err("%s,ERROR:failed to alloc memory.", __func__);
	}

	if (term_list != NULL){
		di->condition_list = g_slist_append(di->condition_list, term_list);
	}

	return SRD_OK;
}

static PyObject *Decoder_wait(PyObject *self, PyObject *args)
{
	int ret;
	uint64_t skip_count;
	gboolean found_match;
	struct srd_decoder_inst *di;
    PyGILState_STATE gstate; 

	if (!self || !args)
		return NULL;

    gstate = PyGILState_Ensure();

	if (!(di = srd_inst_find_by_obj(NULL, self))) {
		PyErr_SetString(PyExc_Exception, "decoder instance not found");
        PyGILState_Release(gstate);
		Py_RETURN_NONE;
	}

    ret = set_new_condition_list(di, args);
    if (ret < 0) {
        srd_dbg("%s: %s: Aborting wait().", di->inst_id, __func__);
        goto err;
    }

    if (ret == 9999) {
        /*
         * Empty condition list, automatic match. Arrange for the
         * execution of regular match handling code paths such that
         * the next available sample is returned to the caller.
         * Make sure to skip one sample when "anywhere within the
         * stream", yet make sure to not skip sample number 0.
         */
        if (!di->first_pos && di->abs_cur_samplenum)
            skip_count = 1;
        else if (!di->condition_list)
            skip_count = 0;
        else
            skip_count = 1;
        ret = set_skip_condition(di, skip_count);
        if (ret < 0) {
            srd_dbg("%s: %s: Cannot setup condition-less wait().",
                di->inst_id, __func__);
            goto err;
        }
    }


    while (1) {

        Py_BEGIN_ALLOW_THREADS

        /* Wait for new samples to process, or termination request. */
        g_mutex_lock(&di->data_mutex);
        while (!di->got_new_samples && !di->want_wait_terminate)
            g_cond_wait(&di->got_new_samples_cond, &di->data_mutex);

 
        /*
         * Check whether any of the current condition(s) match.
         * Arrange for termination requests to take a code path which
         * won't find new samples to process, pretends to have processed
         * previously stored samples, and returns to the main thread,
         * while the termination request still gets signalled.
         */
        found_match = FALSE;

        /* Ignore return value for now, should never be negative. */
        process_samples_until_condition_match(di, &found_match);

        Py_END_ALLOW_THREADS

        /* If there's a match, set self.samplenum etc. and return. */
        if (found_match) {
            /* Set self.samplenum to the (absolute) sample number that matched. */
            PyObject *py_cur_samplenum = PyLong_FromUnsignedLongLong(di->abs_cur_samplenum);
            PyObject_SetAttrString(di->py_inst, "samplenum", py_cur_samplenum);
            Py_DECREF(py_cur_samplenum);

            /* Set self.matched to math_array. */
            PyObject *py_matched = PyLong_FromUnsignedLongLong(di->match_array);
            PyObject_SetAttrString(di->py_inst, "matched", py_matched);
            Py_DECREF(py_matched);

            get_current_pinvalues(di);

            g_mutex_unlock(&di->data_mutex);

            PyGILState_Release(gstate);

            Py_INCREF(di->py_pinvalues);
            return (PyObject *)di->py_pinvalues;
        } 
 
		/* No match, reset state for the next chunk. */
		di->got_new_samples = FALSE;
		di->handled_all_samples = TRUE;
		di->abs_start_samplenum = 0;
		di->abs_end_samplenum = 0;
		di->inbuf = NULL;
		di->inbuflen = 0;

		/* Signal the main thread that we handled all samples. */
		g_cond_signal(&di->handled_all_samples_cond);

		/*
		 * When termination of wait() and decode() was requested,
		 * then exit the loop after releasing the mutex.
		 */
		if (di->want_wait_terminate) {
			srd_dbg("%s: %s: Will return from wait().",
				di->inst_id, __func__);
			g_mutex_unlock(&di->data_mutex);
			goto err;
		}

		g_mutex_unlock(&di->data_mutex);
	}

    PyGILState_Release(gstate);

	Py_RETURN_NONE;

err:
    PyGILState_Release(gstate);

	return NULL;
}

/**
 * Return whether the specified channel was supplied to the decoder.
 *
 * @param self TODO. Must not be NULL.
 * @param args TODO. Must not be NULL.
 *
 * @retval Py_True The channel has been supplied by the frontend.
 * @retval Py_False The channel has been supplied by the frontend.
 * @retval NULL An error occurred.
 */
static PyObject *Decoder_has_channel(PyObject *self, PyObject *args)
{
	int idx, count;
	struct srd_decoder_inst *di;
	PyGILState_STATE gstate;

	if (!self || !args)
		return NULL;

	gstate = PyGILState_Ensure();

	if (!(di = srd_inst_find_by_obj(NULL, self))) {
		PyErr_SetString(PyExc_Exception, "decoder instance not found");
		goto err;
	}

	/*
	 * Get the integer argument of self.has_channel(). Check for
	 * the range of supported PD input channel numbers.
	 */
	if (!PyArg_ParseTuple(args, "i", &idx)) {
		/* Let Python raise this exception. */
		goto err;
	}

	count = g_slist_length(di->decoder->channels) +
	        g_slist_length(di->decoder->opt_channels);
	if (idx < 0 || idx >= count) {
		srd_err("Invalid index %d, PD channel count %d.", idx, count);
		PyErr_SetString(PyExc_IndexError, "invalid channel index");
		goto err;
	}

	PyGILState_Release(gstate);

	if (di->dec_channelmap[idx] == -1){
		Py_INCREF(Py_False);
		return Py_False;
	}
	else{
		Py_INCREF(Py_True);
		return Py_True;
	}

err:
	PyGILState_Release(gstate);

	return NULL;
}

/*
 Receive python debug log, and print it to console
*/
static PyObject *Decoder_printlog(PyObject *self, PyObject *args)
{    
	if (self == NULL || args == NULL){
		//PyErr_SetString(PyExc_Exception, "---------------xx");
		return NULL;
	} 

	PyGILState_STATE gstate; 	
	PyObject *py_bytes = NULL;
	PyObject *py_data = NULL;
	char *str = NULL;

	gstate = PyGILState_Ensure();
 
	if (!PyArg_ParseTuple(args, "U", &py_data)) {
		srd_err("printlog() read param error!");
		goto err;
	}

	if (py_data == NULL){
		srd_err("printlog() param is null!");
		goto err;
	}

	if (PyUnicode_Check(py_data) == FALSE){
		srd_err("printlog() param type must be string!");	
		goto err;
	}

	py_bytes = PyUnicode_AsUTF8String(py_data);
	if (!py_bytes){
		goto err;
	}	

	str = PyBytes_AsString(py_bytes);
    srd_err("%s", str); //print string from python to console
	Py_DECREF(py_bytes);

    PyGILState_Release(gstate);
    Py_RETURN_NONE;

err:
	PyGILState_Release(gstate);
	return NULL;
}

//------------------------------------------------------- construct

static PyMethodDef Decoder_methods[] = {
	{ "put", Decoder_put, METH_VARARGS,
	  		"Accepts a dictionary with the following keys: startsample, endsample, data" },

	{ "register", (PyCFunction)((void*)&Decoder_register), METH_VARARGS|METH_KEYWORDS,
			"Register a new output stream" },

	{ "wait", Decoder_wait, METH_VARARGS,
			"Wait for one or more conditions to occur" },

	{ "has_channel", Decoder_has_channel, METH_VARARGS,
			"Report whether a channel was supplied" },

	{ "printlog", Decoder_printlog, METH_VARARGS,
			"Print string from python" },

	{NULL, NULL, 0, NULL}
};

/**
 * Create the sigrokdecode.Decoder type.
 *
 * @return The new type object.
 *
 * @private
 */
SRD_PRIV PyObject *srd_Decoder_type_new(void)
{
	PyType_Spec spec;
	PyType_Slot slots[] = {
		{ Py_tp_doc, "sigrok Decoder base class" },
		{ Py_tp_methods, Decoder_methods },
		{ Py_tp_new, (void *)&PyType_GenericNew },
		{ 0, NULL }
	};
	PyObject *py_obj;
	PyGILState_STATE gstate;

	gstate = PyGILState_Ensure();

	spec.name = "sigrokdecode.Decoder";
	spec.basicsize = sizeof(srd_Decoder);
	spec.itemsize = 0;
	spec.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
	spec.slots = slots;

	py_obj = PyType_FromSpec(&spec);

	PyGILState_Release(gstate);

	return py_obj;
}


