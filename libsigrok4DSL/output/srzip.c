/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Bert Vermeulen <bert@biot.com>
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <zip.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

#define LOG_PREFIX "output/srzip"

struct out_context {
	gboolean zip_created;
	uint64_t samplerate;
	char *filename;
};

static int init(struct sr_output *o, GHashTable *options)
{
	struct out_context *outc;

	outc = g_malloc0(sizeof(struct out_context));
	o->priv = outc;
	outc->filename = g_strdup(g_variant_get_string(g_hash_table_lookup(options, "filename"), NULL));
	if (strlen(outc->filename) == 0)
		return SR_ERR_ARG;

	return SR_OK;
}

static int zip_create(const struct sr_output *o)
{
	struct out_context *outc;
	struct sr_channel *ch;
	FILE *meta;
	struct zip *zipfile;
	struct zip_source *versrc, *metasrc;
	GVariant *gvar;
	GSList *l;
	int tmpfile, ret;
	char version[1], metafile[32], *s;

	outc = o->priv;
	if (outc->samplerate == 0) {
		if (sr_config_get(o->sdi->driver, o->sdi, NULL, NULL, SR_CONF_SAMPLERATE,
				&gvar) == SR_OK) {
			outc->samplerate = g_variant_get_uint64(gvar);
			g_variant_unref(gvar);
		}
	}

	/* Quietly delete it first, libzip wants replace ops otherwise. */
	unlink(outc->filename);
	if (!(zipfile = zip_open(outc->filename, ZIP_CREATE, &ret)))
		return SR_ERR;

	/* "version" */
	version[0] = '2';
	if (!(versrc = zip_source_buffer(zipfile, version, 1, 0)))
		return SR_ERR;
	if (zip_add(zipfile, "version", versrc) == -1) {
		sr_info("Error saving version into zipfile: %s.",
			zip_strerror(zipfile));
		return SR_ERR;
	}

	/* init "metadata" */
	strcpy(metafile, "sigrok-meta-XXXXXX");
	if ((tmpfile = g_mkstemp(metafile)) == -1)
		return SR_ERR;
	close(tmpfile);
	meta = g_fopen(metafile, "wb");
	fprintf(meta, "[global]\n");
	fprintf(meta, "sigrok version = %s\n", PACKAGE_VERSION);
	fprintf(meta, "[device 1]\ncapturefile = logic-1\n");
	fprintf(meta, "total probes = %d\n", g_slist_length(o->sdi->channels));
	s = sr_samplerate_string(outc->samplerate);
	fprintf(meta, "samplerate = %s\n", s);
	g_free(s);

	for (l = o->sdi->channels; l; l = l->next) {
		ch = l->data;
		if (ch->type != SR_CHANNEL_LOGIC)
			continue;
		if (!ch->enabled)
			continue;
		fprintf(meta, "probe%d = %s\n", ch->index + 1, ch->name);
	}
	fclose(meta);

	if (!(metasrc = zip_source_file(zipfile, metafile, 0, -1))) {
		unlink(metafile);
		return SR_ERR;
	}
	if (zip_add(zipfile, "metadata", metasrc) == -1) {
		unlink(metafile);
		return SR_ERR;
	}

	if ((ret = zip_close(zipfile)) == -1) {
		sr_info("Error saving zipfile: %s.", zip_strerror(zipfile));
		unlink(metafile);
		return SR_ERR;
	}

	unlink(metafile);

	return SR_OK;
}

static int zip_append(const struct sr_output *o, unsigned char *buf,
		int unitsize, int length)
{
	struct out_context *outc;
	struct zip *archive;
	struct zip_source *logicsrc;
	zip_int64_t num_files;
	struct zip_file *zf;
	struct zip_stat zs;
	struct zip_source *metasrc;
	GKeyFile *kf;
	GError *error;
	gsize len;
	int chunk_num, next_chunk_num, tmpfile, ret, i;
	const char *entry_name;
	char *metafile, tmpname[32], chunkname[16];

	outc = o->priv;
	if (!(archive = zip_open(outc->filename, 0, &ret)))
		return SR_ERR;

	if (zip_stat(archive, "metadata", 0, &zs) == -1)
		return SR_ERR;

	metafile = g_malloc(zs.size);
	zf = zip_fopen_index(archive, zs.index, 0);
	zip_fread(zf, metafile, zs.size);
	zip_fclose(zf);

	/*
	 * If the file was only initialized but doesn't yet have any
	 * data it in, it won't have a unitsize field in metadata yet.
	 */
	error = NULL;
	kf = g_key_file_new();
	if (!g_key_file_load_from_data(kf, metafile, zs.size, 0, &error)) {
		sr_err("Failed to parse metadata: %s.", error->message);
		return SR_ERR;
	}
	g_free(metafile);
	tmpname[0] = '\0';
	if (!g_key_file_has_key(kf, "device 1", "unitsize", &error)) {
		if (error && error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
			sr_err("Failed to check unitsize key: %s", error ? error->message : "?");
			return SR_ERR;
		}
		/* Add unitsize field. */
		g_key_file_set_integer(kf, "device 1", "unitsize", unitsize);
		metafile = g_key_file_to_data(kf, &len, &error);
		strcpy(tmpname, "sigrok-meta-XXXXXX");
		if ((tmpfile = g_mkstemp(tmpname)) == -1)
			return SR_ERR;
		if (write(tmpfile, metafile, len) < 0) {
			sr_dbg("Failed to create new metadata: %s", strerror(errno));
			g_free(metafile);
			unlink(tmpname);
			return SR_ERR;
		}
		close(tmpfile);
		if (!(metasrc = zip_source_file(archive, tmpname, 0, -1))) {
			sr_err("Failed to create zip source for metadata.");
			g_free(metafile);
			unlink(tmpname);
			return SR_ERR;
		}
		if (zip_replace(archive, zs.index, metasrc) == -1) {
			sr_err("Failed to replace metadata file.");
			g_free(metafile);
			unlink(tmpname);
			return SR_ERR;
		}
		g_free(metafile);
	}
	g_key_file_free(kf);

	next_chunk_num = 1;
	num_files = zip_get_num_entries(archive, 0);
	for (i = 0; i < num_files; i++) {
		entry_name = zip_get_name(archive, i, 0);
		if (strncmp(entry_name, "logic-1", 7))
			continue;
		if (strlen(entry_name) == 7) {
			/* This file has no extra chunks, just a single "logic-1".
			 * Rename it to "logic-1-1" * and continue with chunk 2. */
			if (zip_rename(archive, i, "logic-1-1") == -1) {
				sr_err("Failed to rename 'logic-1' to 'logic-1-1'.");
				unlink(tmpname);
				return SR_ERR;
			}
			next_chunk_num = 2;
			break;
		} else if (strlen(entry_name) > 8 && entry_name[7] == '-') {
			chunk_num = strtoull(entry_name + 8, NULL, 10);
			if (chunk_num >= next_chunk_num)
				next_chunk_num = chunk_num + 1;
		}
	}
	snprintf(chunkname, 15, "logic-1-%d", next_chunk_num);
	if (!(logicsrc = zip_source_buffer(archive, buf, length, FALSE))) {
		unlink(tmpname);
		return SR_ERR;
	}
	if (zip_add(archive, chunkname, logicsrc) == -1) {
		unlink(tmpname);
		return SR_ERR;
	}
	if ((ret = zip_close(archive)) == -1) {
		sr_info("error saving session file: %s", zip_strerror(archive));
		unlink(tmpname);
		return SR_ERR;
	}
	unlink(tmpname);

	return SR_OK;
}

static int receive(const struct sr_output *o, const struct sr_datafeed_packet *packet,
		GString **out)
{
	struct out_context *outc;
	const struct sr_datafeed_meta *meta;
	const struct sr_datafeed_logic *logic;
	const struct sr_config *src;
	GSList *l;

	int ret;

	*out = NULL;
	if (!o || !o->sdi || !(outc = o->priv))
		return SR_ERR_ARG;

	switch (packet->type) {
	case SR_DF_META:
		meta = packet->payload;
		for (l = meta->config; l; l = l->next) {
			src = l->data;
			if (src->key != SR_CONF_SAMPLERATE)
				continue;
			outc->samplerate = g_variant_get_uint64(src->data);
		}
		break;
	case SR_DF_LOGIC:
		if (!outc->zip_created) {
			if ((ret = zip_create(o)) != SR_OK)
				return ret;
			outc->zip_created = TRUE;
		}
		logic = packet->payload;
		ret = zip_append(o, logic->data, logic->unitsize, logic->length);
		break;
	}

	return SR_OK;
}

static int cleanup(struct sr_output *o)
{
	struct out_context *outc;

	outc = o->priv;
	g_free(outc->filename);
	g_free(outc);
	o->priv = NULL;

	return SR_OK;
}

static struct sr_option options[] = {
	{ "filename", "Filename", "File to write", NULL, NULL },
	{0}
};

static const struct sr_option *get_options(void)
{
	if (!options[0].def)
		options[0].def = g_variant_ref_sink(g_variant_new_string(""));

	return options;
}

SR_PRIV struct sr_output_module output_srzip = {
	.id = "srzip",
	.name = "srzip",
	.desc = "srzip session file",
	.exts = (const char*[]){"sr", NULL},
	.options = get_options,
	.init = init,
	.receive = receive,
	.cleanup = cleanup,
};

