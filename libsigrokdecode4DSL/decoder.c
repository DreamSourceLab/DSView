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

/**
 * @file
 *
 * Listing, loading, unloading, and handling protocol decoders.
 */

/**
 * @defgroup grp_decoder Protocol decoders
 *
 * Handling protocol decoders.
 *
 * @{
 */

/** @cond PRIVATE */

/* The list of protocol decoders. */
static GSList *pd_list = NULL;

/* srd.c */
extern SRD_PRIV GSList *searchpaths;

/* session.c */
extern SRD_PRIV GSList *sessions;
extern SRD_PRIV int max_session_id;

/* module_sigrokdecode.c */
extern SRD_PRIV PyObject *mod_sigrokdecode;

/** @endcond */

static gboolean srd_check_init(void)
{
	if (max_session_id < 0) {
		srd_err("Library is not initialized.");
		return FALSE;
	} else
		return TRUE;
}

/**
 * Returns the list of supported/loaded protocol decoders.
 *
 * This is a GSList of pointers to struct srd_decoder items.
 *
 * @return List of decoders, NULL if none are supported or loaded.
 *
 * @since 0.2.0
 */
SRD_API const GSList *srd_decoder_list(void)
{
	return pd_list;
}

/**
 * Get the decoder with the specified ID.
 *
 * @param id The ID string of the decoder to return.
 *
 * @return The decoder with the specified ID, or NULL if not found.
 *
 * @since 0.1.0
 */
SRD_API struct srd_decoder *srd_decoder_get_by_id(const char *id)
{
	GSList *l;
	struct srd_decoder *dec;

	for (l = pd_list; l; l = l->next) {
		dec = l->data;
		if (!strcmp(dec->id, id))
			return dec;
	}

	return NULL;
}

static int get_channels(const struct srd_decoder *d, const char *attr,
		GSList **pdchl)
{
	PyObject *py_channellist, *py_entry;
	struct srd_channel *pdch;
	int ret, num_channels, i;

	if (!PyObject_HasAttrString(d->py_dec, attr))
		/* No channels of this type specified. */
		return SRD_OK;

	py_channellist = PyObject_GetAttrString(d->py_dec, attr);
	if (!PyTuple_Check(py_channellist)) {
		srd_err("Protocol decoder %s %s attribute is not a tuple.",
				d->name, attr);
		return SRD_ERR_PYTHON;
	}

	if ((num_channels = PyTuple_Size(py_channellist)) == 0)
		/* Empty channellist. */
		return SRD_OK;

	ret = SRD_OK;
	for (i = 0; i < num_channels; i++) {
		py_entry = PyTuple_GetItem(py_channellist, i);
		if (!PyDict_Check(py_entry)) {
			srd_err("Protocol decoder %s %s attribute is not "
				"a list with dict elements.", d->name, attr);
			ret = SRD_ERR_PYTHON;
			break;
		}

		pdch = g_malloc(sizeof(struct srd_channel));

		if ((py_dictitem_as_str(py_entry, "id", &pdch->id)) != SRD_OK) {
			ret = SRD_ERR_PYTHON;
			break;
		}
		if ((py_dictitem_as_str(py_entry, "name", &pdch->name)) != SRD_OK) {
			ret = SRD_ERR_PYTHON;
			break;
		}
		if ((py_dictitem_as_str(py_entry, "desc", &pdch->desc)) != SRD_OK) {
			ret = SRD_ERR_PYTHON;
			break;
		}
		pdch->order = i;

		*pdchl = g_slist_append(*pdchl, pdch);
	}

	Py_DecRef(py_channellist);

	return ret;
}

static int get_options(struct srd_decoder *d)
{
	PyObject *py_opts, *py_opt, *py_val, *py_default, *py_item;
	Py_ssize_t opt, i;
	struct srd_decoder_option *o;
	GVariant *gvar;
	gint64 lval;
	double dval;
	int overflow;
	char *sval;

	if (!PyObject_HasAttrString(d->py_dec, "options"))
		/* No options, that's fine. */
		return SRD_OK;

	/* If present, options must be a tuple. */
	py_opts = PyObject_GetAttrString(d->py_dec, "options");
	if (!PyTuple_Check(py_opts)) {
		srd_err("Protocol decoder %s: options attribute is not "
				"a tuple.", d->id);
		return SRD_ERR_PYTHON;
	}

	for (opt = 0; opt < PyTuple_Size(py_opts); opt++) {
		py_opt = PyTuple_GetItem(py_opts, opt);
		if (!PyDict_Check(py_opt)) {
			srd_err("Protocol decoder %s options: each option "
					"must consist of a dictionary.", d->name);
			return SRD_ERR_PYTHON;
		}
		if (!(py_val = PyDict_GetItemString(py_opt, "id"))) {
			srd_err("Protocol decoder %s option %zd has no "
					"id.", d->name, opt);
			return SRD_ERR_PYTHON;
		}
		o = g_malloc0(sizeof(struct srd_decoder_option));
		py_str_as_str(py_val, &o->id);

		if ((py_val = PyDict_GetItemString(py_opt, "desc")))
			py_str_as_str(py_val, &o->desc);

		if ((py_default = PyDict_GetItemString(py_opt, "default"))) {
			if (PyUnicode_Check(py_default)) {
				/* UTF-8 string */
				py_str_as_str(py_default, &sval);
				o->def = g_variant_new_string(sval);
				g_free(sval);
			} else if (PyLong_Check(py_default)) {
				/* Long */
				lval = PyLong_AsLongAndOverflow(py_default, &overflow);
				if (overflow) {
					/* Value is < LONG_MIN or > LONG_MAX */
					PyErr_Clear();
					srd_err("Protocol decoder %s option 'default' has "
							"invalid default value.", d->name);
					return SRD_ERR_PYTHON;
				}
				o->def = g_variant_new_int64(lval);
			} else if (PyFloat_Check(py_default)) {
				/* Float */
				if ((dval = PyFloat_AsDouble(py_default)) == -1.0) {
					PyErr_Clear();
					srd_err("Protocol decoder %s option 'default' has "
							"invalid default value.", d->name);
					return SRD_ERR_PYTHON;
				}
				o->def = g_variant_new_double(dval);
			} else {
				srd_err("Protocol decoder %s option 'default' has "
						"value of unsupported type '%s'.", d->name,
						Py_TYPE(py_default)->tp_name);
				return SRD_ERR_PYTHON;
			}
			g_variant_ref_sink(o->def);
		}

		if ((py_val = PyDict_GetItemString(py_opt, "values"))) {
			/* A default is required if a list of values is
			 * given, since it's used to verify their type. */
			if (!o->def) {
				srd_err("No default for option '%s'", o->id);
				return SRD_ERR_PYTHON;
			}
			if (!PyTuple_Check(py_val)) {
				srd_err("Option '%s' values should be a tuple.", o->id);
				return SRD_ERR_PYTHON;
			}
			for (i = 0; i < PyTuple_Size(py_val); i++) {
				py_item = PyTuple_GetItem(py_val, i);
				if (Py_TYPE(py_default) != Py_TYPE(py_item)) {
					srd_err("All values for option '%s' must be "
							"of the same type as the default.",
							o->id);
					return SRD_ERR_PYTHON;
				}
				if (PyUnicode_Check(py_item)) {
					/* UTF-8 string */
					py_str_as_str(py_item, &sval);
					gvar = g_variant_new_string(sval);
					g_variant_ref_sink(gvar);
					g_free(sval);
					o->values = g_slist_append(o->values, gvar);
				} else if (PyLong_Check(py_item)) {
					/* Long */
					lval = PyLong_AsLongAndOverflow(py_item, &overflow);
					if (overflow) {
						/* Value is < LONG_MIN or > LONG_MAX */
						PyErr_Clear();
						srd_err("Protocol decoder %s option 'values' "
								"has invalid value.", d->name);
						return SRD_ERR_PYTHON;
					}
					gvar = g_variant_new_int64(lval);
					g_variant_ref_sink(gvar);
					o->values = g_slist_append(o->values, gvar);
				} else if (PyFloat_Check(py_item)) {
					/* Float */
					if ((dval = PyFloat_AsDouble(py_item)) == -1.0) {
						PyErr_Clear();
						srd_err("Protocol decoder %s option 'default' has "
								"invalid default value.", d->name);
						return SRD_ERR_PYTHON;
					}
					gvar = g_variant_new_double(dval);
					g_variant_ref_sink(gvar);
					o->values = g_slist_append(o->values, gvar);
				}
			}
		}
		d->options = g_slist_append(d->options, o);
	}

	return SRD_OK;
}

/**
 * Load a protocol decoder module into the embedded Python interpreter.
 *
 * @param module_name The module name to be loaded.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.1.0
 */
SRD_API int srd_decoder_load(const char *module_name)
{
	PyObject *py_basedec, *py_method, *py_attr, *py_annlist, *py_ann;
	PyObject *py_bin_classes, *py_bin_class, *py_ann_rows, *py_ann_row;
	PyObject *py_ann_classes, *py_long;
	struct srd_decoder *d;
	int ret, i, j;
	char **ann, **bin, *ann_row_id, *ann_row_desc;
	struct srd_channel *pdch;
	GSList *l, *ann_classes;
	struct srd_decoder_annotation_row *ann_row;
    int ann_type = 7;
    int ann_len;

	if (!srd_check_init())
		return SRD_ERR;

	if (!module_name)
		return SRD_ERR_ARG;

	if (PyDict_GetItemString(PyImport_GetModuleDict(), module_name)) {
		/* Module was already imported. */
		return SRD_OK;
	}

	srd_dbg("Loading protocol decoder '%s'.", module_name);

	py_basedec = py_method = py_attr = NULL;

	d = g_malloc0(sizeof(struct srd_decoder));

	ret = SRD_ERR_PYTHON;

	/* Import the Python module. */
	if (!(d->py_mod = PyImport_ImportModule(module_name))) {
        srd_exception_catch("Import of '%s' failed.", NULL, module_name);
		goto err_out;
	}

	/* Get the 'Decoder' class as Python object. */
	if (!(d->py_dec = PyObject_GetAttrString(d->py_mod, "Decoder"))) {
		/* This generated an AttributeError exception. */
		PyErr_Clear();
		srd_err("Decoder class not found in protocol decoder %s.",
			module_name);
		goto err_out;
	}

	if (!(py_basedec = PyObject_GetAttrString(mod_sigrokdecode, "Decoder"))) {
		srd_dbg("sigrokdecode module not loaded.");
		goto err_out;
	}

	if (!PyObject_IsSubclass(d->py_dec, py_basedec)) {
		srd_err("Decoder class in protocol decoder module %s is not "
			"a subclass of sigrokdecode.Decoder.", module_name);
		goto err_out;
	}
	Py_CLEAR(py_basedec);

	/*
	 * Check that this decoder has the correct PD API version.
	 * PDs of different API versions are incompatible and cannot work.
	 */
	py_long = PyObject_GetAttrString(d->py_dec, "api_version");
	if (PyLong_AsLong(py_long) != 2) {
		srd_err("Only PDs of API version 2 are supported.");
		goto err_out;
	}
	Py_CLEAR(py_long);

	/* Check for a proper start() method. */
	if (!PyObject_HasAttrString(d->py_dec, "start")) {
		srd_err("Protocol decoder %s has no start() method Decoder "
			"class.", module_name);
		goto err_out;
	}
	py_method = PyObject_GetAttrString(d->py_dec, "start");
	if (!PyFunction_Check(py_method)) {
		srd_err("Protocol decoder %s Decoder class attribute 'start' "
			"is not a method.", module_name);
		goto err_out;
	}
	Py_CLEAR(py_method);

	/* Check for a proper decode() method. */
	if (!PyObject_HasAttrString(d->py_dec, "decode")) {
		srd_err("Protocol decoder %s has no decode() method Decoder "
			"class.", module_name);
		goto err_out;
	}
	py_method = PyObject_GetAttrString(d->py_dec, "decode");
	if (!PyFunction_Check(py_method)) {
		srd_err("Protocol decoder %s Decoder class attribute 'decode' "
			"is not a method.", module_name);
		goto err_out;
	}
	Py_CLEAR(py_method);

	/* Store required fields in newly allocated strings. */
	if (py_attr_as_str(d->py_dec, "id", &(d->id)) != SRD_OK)
		goto err_out;

	if (py_attr_as_str(d->py_dec, "name", &(d->name)) != SRD_OK)
		goto err_out;

	if (py_attr_as_str(d->py_dec, "longname", &(d->longname)) != SRD_OK)
		goto err_out;

	if (py_attr_as_str(d->py_dec, "desc", &(d->desc)) != SRD_OK)
		goto err_out;

	if (py_attr_as_str(d->py_dec, "license", &(d->license)) != SRD_OK)
		goto err_out;

	/* All options and their default values. */
	if (get_options(d) != SRD_OK)
		goto err_out;

	/* Check and import required channels. */
	if (get_channels(d, "channels", &d->channels) != SRD_OK)
		goto err_out;

	/* Check and import optional channels. */
	if (get_channels(d, "optional_channels", &d->opt_channels) != SRD_OK)
		goto err_out;

	/*
	 * Fix order numbers for the optional channels.
	 *
	 * Example:
	 * Required channels: r1, r2, r3. Optional: o1, o2, o3, o4.
	 * 'order' fields in the d->channels list = 0, 1, 2.
	 * 'order' fields in the d->opt_channels list = 3, 4, 5, 6.
	 */
	for (l = d->opt_channels; l; l = l->next) {
		pdch = l->data;
		pdch->order += g_slist_length(d->channels);
	}

	/* Convert annotation class attribute to GSList of char **. */
	d->annotations = NULL;
	if (PyObject_HasAttrString(d->py_dec, "annotations")) {
		py_annlist = PyObject_GetAttrString(d->py_dec, "annotations");
		if (!PyTuple_Check(py_annlist)) {
			srd_err("Protocol decoder %s annotations should "
					"be a tuple.", module_name);
			goto err_out;
		}
		for (i = 0; i < PyTuple_Size(py_annlist); i++) {
			py_ann = PyTuple_GetItem(py_annlist, i);
            if (!PyTuple_Check(py_ann) || (PyTuple_Size(py_ann) != 3 && PyTuple_Size(py_ann) != 2)) {
				srd_err("Protocol decoder %s annotation %d should "
						"be a tuple with two elements.", module_name, i + 1);
				goto err_out;
			}
            if (py_strseq_to_char(py_ann, &ann) != SRD_OK) {
                goto err_out;
            }
            d->annotations = g_slist_append(d->annotations, ann);
            if (PyTuple_Size(py_ann) == 3) {
                ann_type = 0;
                ann_len = strlen(ann[0]);
                for (j = 0; j < ann_len; j++)
                    ann_type = ann_type * 10 + (ann[0][j] - '0');
                d->ann_types = g_slist_append(d->ann_types, GINT_TO_POINTER(ann_type));
            } else if (PyTuple_Size(py_ann) == 2) {
                d->ann_types = g_slist_append(d->ann_types, GINT_TO_POINTER(ann_type));
                ann_type++;
            }
		}
	}

	/* Convert annotation_rows to GSList of 'struct srd_decoder_annotation_row'. */
	d->annotation_rows = NULL;
	if (PyObject_HasAttrString(d->py_dec, "annotation_rows")) {
		py_ann_rows = PyObject_GetAttrString(d->py_dec, "annotation_rows");
		if (!PyTuple_Check(py_ann_rows)) {
			srd_err("Protocol decoder %s annotation row list "
				"must be a tuple.", module_name);
			goto err_out;
		}
		for (i = 0; i < PyTuple_Size(py_ann_rows); i++) {
			py_ann_row = PyTuple_GetItem(py_ann_rows, i);
			if (!PyTuple_Check(py_ann_row)) {
				srd_err("Protocol decoder %s annotation rows "
					"must be tuples.", module_name);
				goto err_out;
			}
			if (PyTuple_Size(py_ann_row) != 3
					|| !PyUnicode_Check(PyTuple_GetItem(py_ann_row, 0))
					|| !PyUnicode_Check(PyTuple_GetItem(py_ann_row, 1))
					|| !PyTuple_Check(PyTuple_GetItem(py_ann_row, 2))) {
				srd_err("Protocol decoder %s annotation rows "
					"must contain tuples containing two "
					"strings and a tuple.", module_name);
				goto err_out;
			}

			if (py_str_as_str(PyTuple_GetItem(py_ann_row, 0), &ann_row_id) != SRD_OK)
				goto err_out;

			if (py_str_as_str(PyTuple_GetItem(py_ann_row, 1), &ann_row_desc) != SRD_OK)
				goto err_out;

			py_ann_classes = PyTuple_GetItem(py_ann_row, 2);
			ann_classes = NULL;
			for (j = 0; j < PyTuple_Size(py_ann_classes); j++) {
				py_long = PyTuple_GetItem(py_ann_classes, j);
				if (!PyLong_Check(py_long)) {
					srd_err("Protocol decoder %s annotation row class "
						"list must only contain numbers.", module_name);
					goto err_out;
				}
				ann_classes = g_slist_append(ann_classes,
					GINT_TO_POINTER(PyLong_AsLong(py_long)));
			}

			ann_row = g_malloc0(sizeof(struct srd_decoder_annotation_row));
			ann_row->id = ann_row_id;
			ann_row->desc = ann_row_desc;
			ann_row->ann_classes = ann_classes;
			d->annotation_rows = g_slist_append(d->annotation_rows, ann_row);
		}
	}

	/* Convert binary class to GSList of char *. */
	d->binary = NULL;
	if (PyObject_HasAttrString(d->py_dec, "binary")) {
		py_bin_classes = PyObject_GetAttrString(d->py_dec, "binary");
		if (!PyTuple_Check(py_bin_classes)) {
			srd_err("Protocol decoder %s binary classes should "
					"be a tuple.", module_name);
			goto err_out;
		}
		for (i = 0; i < PyTuple_Size(py_bin_classes); i++) {
			py_bin_class = PyTuple_GetItem(py_bin_classes, i);
			if (!PyTuple_Check(py_bin_class)) {
				srd_err("Protocol decoder %s binary classes "
						"should consist of tuples.", module_name);
				goto err_out;
			}
			if (PyTuple_Size(py_bin_class) != 2
					|| !PyUnicode_Check(PyTuple_GetItem(py_bin_class, 0))
					|| !PyUnicode_Check(PyTuple_GetItem(py_bin_class, 1))) {
				srd_err("Protocol decoder %s binary classes should "
						"contain tuples with two strings.", module_name);
				goto err_out;
			}

			if (py_strseq_to_char(py_bin_class, &bin) != SRD_OK) {
				goto err_out;
			}
			d->binary = g_slist_append(d->binary, bin);
		}
	}

	/* Append it to the list of supported/loaded decoders. */
	pd_list = g_slist_append(pd_list, d);

	ret = SRD_OK;

err_out:
	if (ret != SRD_OK) {
		Py_XDECREF(py_method);
		Py_XDECREF(py_basedec);
		Py_XDECREF(d->py_dec);
		Py_XDECREF(d->py_mod);
		g_free(d);
	}

	return ret;
}

/**
 * Return a protocol decoder's docstring.
 *
 * @param dec The loaded protocol decoder.
 *
 * @return A newly allocated buffer containing the protocol decoder's
 *         documentation. The caller is responsible for free'ing the buffer.
 *
 * @since 0.1.0
 */
SRD_API char *srd_decoder_doc_get(const struct srd_decoder *dec)
{
	PyObject *py_str;
	char *doc;

	if (!srd_check_init())
		return NULL;

	if (!dec)
		return NULL;

	if (!PyObject_HasAttrString(dec->py_mod, "__doc__"))
		return NULL;

	if (!(py_str = PyObject_GetAttrString(dec->py_mod, "__doc__"))) {
        srd_exception_catch("", NULL);
		return NULL;
	}

	doc = NULL;
	if (py_str != Py_None)
		py_str_as_str(py_str, &doc);
	Py_DecRef(py_str);

	return doc;
}

static void free_channels(GSList *channellist)
{
	GSList *l;
	struct srd_channel *pdch;

	if (channellist == NULL)
		return;

	for (l = channellist; l; l = l->next) {
		pdch = l->data;
		g_free(pdch->id);
		g_free(pdch->name);
		g_free(pdch->desc);
		g_free(pdch);
	}
	g_slist_free(channellist);
}

/**
 * Unload the specified protocol decoder.
 *
 * @param dec The struct srd_decoder to be unloaded.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.1.0
 */
SRD_API int srd_decoder_unload(struct srd_decoder *dec)
{
	struct srd_decoder_option *o;
    struct srd_decoder_annotation_row *row;
	struct srd_session *sess;
	GSList *l;

	if (!srd_check_init())
		return SRD_ERR;

	if (!dec)
		return SRD_ERR_ARG;

	srd_dbg("Unloading protocol decoder '%s'.", dec->name);

	/*
	 * Since any instances of this decoder need to be released as well,
	 * but they could be anywhere in the stack, just free the entire
	 * stack. A frontend reloading a decoder thus has to restart all
	 * instances, and rebuild the stack.
	 */
	for (l = sessions; l; l = l->next) {
		sess = l->data;
		srd_inst_free_all(sess, NULL);
	}

	for (l = dec->options; l; l = l->next) {
		o = l->data;
		g_free(o->id);
		g_free(o->desc);
		g_variant_unref(o->def);
		g_free(o);
	}
	g_slist_free(dec->options);

    g_slist_foreach(dec->annotations, (GFunc)g_free, NULL);
    g_slist_free(dec->annotations);

    for (l = dec->annotation_rows; l; l = l->next) {
        row = l->data;
        g_free(row->id);
        g_free(row->desc);
        g_slist_free(row->ann_classes);
        g_free(row);
    }
    g_slist_free(dec->annotation_rows);

    g_slist_foreach(dec->binary, (GFunc)g_free, NULL);
    g_slist_free(dec->binary);

	free_channels(dec->channels);
	free_channels(dec->opt_channels);
	g_free(dec->id);
	g_free(dec->name);
	g_free(dec->longname);
	g_free(dec->desc);
	g_free(dec->license);

	/* The module's Decoder class. */
	Py_XDECREF(dec->py_dec);
	/* The module itself. */
	Py_XDECREF(dec->py_mod);

	g_free(dec);

	return SRD_OK;
}

static void srd_decoder_load_all_zip_path(char *path)
{
	PyObject *zipimport_mod, *zipimporter_class, *zipimporter;
	PyObject *prefix_obj, *files, *key, *value, *set, *modname;
	Py_ssize_t pos = 0;
	char *prefix;
	size_t prefix_len;

	set = files = prefix_obj = zipimporter = zipimporter_class = NULL;

	zipimport_mod = PyImport_ImportModule("zipimport");
	if (zipimport_mod == NULL)
		goto err_out;

	zipimporter_class = PyObject_GetAttrString(zipimport_mod, "zipimporter");
	if (zipimporter_class == NULL)
		goto err_out;

	zipimporter = PyObject_CallFunction(zipimporter_class, "s", path);
	if (zipimporter == NULL)
		goto err_out;

	prefix_obj = PyObject_GetAttrString(zipimporter, "prefix");
	if (prefix_obj == NULL)
		goto err_out;

	files = PyObject_GetAttrString(zipimporter, "_files");
	if (files == NULL)
		goto err_out;

	set = PySet_New(NULL);
	if (set == NULL)
		goto err_out;

	if (py_str_as_str(prefix_obj, &prefix) != SRD_OK)
		goto err_out;

	prefix_len = strlen(prefix);

	while (PyDict_Next(files, &pos, &key, &value)) {
		char *path, *slash;
		if (py_str_as_str(key, &path) == SRD_OK) {
			if (strlen(path) > prefix_len &&
			    !memcmp(path, prefix, prefix_len) &&
			    (slash = strchr(path+prefix_len, '/'))) {
				modname =
				  PyUnicode_FromStringAndSize(path+prefix_len,
							      slash-(path+prefix_len));
				if (modname == NULL) {
					PyErr_Clear();
				} else {
					PySet_Add(set, modname);
					Py_XDECREF(modname);
				}
			}
			free(path);
		}
	}

	free(prefix);

	while ((modname = PySet_Pop(set))) {
		char *modname_str;
		if (py_str_as_str(modname, &modname_str) == SRD_OK) {
			/* The directory name is the module name (e.g. "i2c"). */
			srd_decoder_load(modname_str);
			free(modname_str);
		}
		Py_XDECREF(modname);
	}

err_out:
	Py_XDECREF(set);
	Py_XDECREF(files);
	Py_XDECREF(prefix_obj);
	Py_XDECREF(zipimporter);
	Py_XDECREF(zipimporter_class);
	Py_XDECREF(zipimport_mod);
	PyErr_Clear();
}

static void srd_decoder_load_all_path(char *path)
{
	GDir *dir;
	const gchar *direntry;

	if (!(dir = g_dir_open(path, 0, NULL))) {
		/* Not really fatal */
		/* Try zipimport method too */
		srd_decoder_load_all_zip_path(path);
		return;
	}

	/* This ignores errors returned by srd_decoder_load(). That
	 * function will have logged the cause, but in any case we
	 * want to continue anyway. */
	while ((direntry = g_dir_read_name(dir)) != NULL) {
		/* The directory name is the module name (e.g. "i2c"). */
		srd_decoder_load(direntry);
	}
	g_dir_close(dir);

}

/**
 * Load all installed protocol decoders.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.1.0
 */
SRD_API int srd_decoder_load_all(void)
{
	GSList *l;

	if (!srd_check_init())
		return SRD_ERR;

	for (l = searchpaths; l; l = l->next)
		srd_decoder_load_all_path(l->data);

	return SRD_OK;
}

/**
 * Unload all loaded protocol decoders.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.1.0
 */
SRD_API int srd_decoder_unload_all(void)
{
	GSList *l;
	struct srd_decoder *dec;

	for (l = pd_list; l; l = l->next) {
		dec = l->data;
		srd_decoder_unload(dec);
	}
	g_slist_free(pd_list);
	pd_list = NULL;

	return SRD_OK;
}

/** @} */
