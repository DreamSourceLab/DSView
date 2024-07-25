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
 
#include "libsigrok-internal.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <minizip/zip.h>
#include <time.h>
#include <assert.h>
#include "../log.h"
#include "../version.h"
#include <stdio.h>

#undef LOG_PREFIX
#define LOG_PREFIX "srzip: "

struct out_context { 
	uint64_t 	samplerate;
	char 		*filename;
	zipFile  	zipArchive;
	int 		bCreated;
    zip_fileinfo zi;
	GKeyFile 	*meta;
	int 		chunk_index;
};

static int init(struct sr_output *o, GHashTable *options)
{
	struct out_context *outc;

	outc = g_try_malloc0(sizeof(struct out_context));
	if (outc == NULL){
		sr_err("%s,ERROR:failed to alloc memory.", __func__);
		return SR_ERR;
	}
    memset(outc, 0, sizeof(struct out_context));

	o->priv = outc;
	outc->zipArchive = NULL;
	outc->bCreated = 0;
	outc->meta = NULL;
	outc->chunk_index = 0;

    outc->filename = g_strdup(g_variant_get_bytestring(g_hash_table_lookup(options, "filename")));

	if (strlen(outc->filename) == 0)
		return SR_ERR_ARG;

	return SR_OK;
}

/*
  add zip inner file
*/
static int add_file_from_buffer(zipFile zipArchive, zip_fileinfo *zi, const char *innerFile, 
			const char *buffer, unsigned int buferSize)
{
    assert(buffer);
    assert(innerFile);
    assert(zipArchive);
	assert(zi);

    int level = Z_BEST_SPEED;
    if (level < Z_DEFAULT_COMPRESSION  || level > Z_BEST_COMPRESSION){
        level = Z_DEFAULT_COMPRESSION;
    }

    zipOpenNewFileInZip(zipArchive,innerFile,zi,
                                NULL,0,NULL,0,NULL ,
                                Z_DEFLATED,
                                level);

    zipWriteInFileInZip(zipArchive, buffer, (unsigned int)buferSize);

    zipCloseFileInZip(zipArchive);

    return 0;
}

/*
 add meta file to archive
*/
static void save_meta_file(const struct sr_output *o)
{
	struct out_context *outc;
	char *metabuf;
	gsize metalen;

	outc = o->priv;

	if (outc->meta != NULL && outc->zipArchive != NULL){
		metabuf = g_key_file_to_data(outc->meta, &metalen, NULL);
		add_file_from_buffer(outc->zipArchive, &outc->zi, "metadata", metabuf, metalen);
		g_free(metabuf); 
		g_key_file_free(outc->meta);
		outc->meta = NULL;
	}
}

/*
 create zip archive
*/
static int create_archive(const struct sr_output *o)
{
	struct out_context *outc;
	struct sr_channel *ch;
	GKeyFile *meta;
	GVariant *gvar;
	GSList *l;
	const char *devgroup;
	size_t ch_nr;
    char version[1];
    char *s = NULL;
    int logic_channels = 0;
    int enabled_logic_channels = 0;

	outc = o->priv;
	outc->zipArchive = NULL;

	//zi
	time_t rawtime;
    time (&rawtime);
    struct tm *ti = localtime(&rawtime);
    zip_fileinfo *zi = &outc->zi;
    zi->tmz_date.tm_year = ti->tm_year;
    zi->tmz_date.tm_mon  = ti->tm_mon;
    zi->tmz_date.tm_mday = ti->tm_mday;
    zi->tmz_date.tm_hour = ti->tm_hour;
    zi->tmz_date.tm_min  = ti->tm_min;
    zi->tmz_date.tm_sec  = ti->tm_sec;
    zi->dosDate = 0;

	if (outc->samplerate == 0) {
		if (sr_config_get(o->sdi->driver, o->sdi, NULL, NULL, SR_CONF_SAMPLERATE,
				&gvar) == SR_OK) {
            if (gvar != NULL) {
                outc->samplerate = g_variant_get_uint64(gvar);
                g_variant_unref(gvar);
            }
		}
	}

	/* Quietly delete it first, libzip wants replace ops otherwise. */
	unlink(outc->filename);

	outc->zipArchive = zipOpen64(outc->filename, FALSE);
	if (outc->zipArchive  == NULL){
		sr_err("create srzip archive error.");
		return SR_ERR;
	}

	/* "version" */
	version[0] = '2';	
	add_file_from_buffer(outc->zipArchive, zi, "version", version, 1);

	/* init "metadata" */
	meta = g_key_file_new();

	g_key_file_set_string(meta, "global", "sigrok version",
            SR_PACKAGE_VERSION_STRING);

	devgroup = "device 1";
	logic_channels = 0;
	enabled_logic_channels = 0;

	for (l = o->sdi->channels; l; l = l->next) {
		ch = l->data;

		if (ch->type == SR_CHANNEL_LOGIC){
			if (ch->enabled)
				enabled_logic_channels++;
			logic_channels++;	
		}
	}

	/* Only set capturefile and probes if we will actually save logic data. */
	if (enabled_logic_channels > 0) {
		g_key_file_set_string(meta, devgroup, "capturefile", "logic-1");
		g_key_file_set_integer(meta, devgroup, "total probes", logic_channels);
	}

	s = sr_samplerate_string(outc->samplerate);
	g_key_file_set_string(meta, devgroup, "samplerate", s);
	g_free(s);

	for (l = o->sdi->channels; l; l = l->next) {
		ch = l->data;

		if (!ch->enabled || ch->type != SR_CHANNEL_LOGIC){
			continue;
		}

		s = NULL;
		ch_nr = ch->index + 1;
		s = g_strdup_printf("probe%zu", ch_nr);

		if (s) {
			g_key_file_set_string(meta, devgroup, s, ch->name);
			g_free(s);
		}
	}
	outc->meta = meta;
  
	return SR_OK;
}

/*
 append data to archive
*/
static int append_archive_data(const struct sr_output *o, const char *buf,
		int unitsize, int length)
{
	struct out_context *outc;
	char chunkname[20];

	outc = o->priv;
 
	/*
	 * If the file was only initialized but doesn't yet have any
	 * data it in, it won't have a unitsize field in metadata yet.
	 */
	if (outc->meta != NULL) { 
		/* Add unitsize field. */
		g_key_file_set_integer(outc->meta, "device 1", "unitsize", unitsize);
		save_meta_file(o);
		outc->meta = NULL;
	} 

	outc->chunk_index++;
	snprintf(chunkname, 19, "logic-1-%d", (short)outc->chunk_index);

	add_file_from_buffer(outc->zipArchive, &outc->zi, 
			(const char*)chunkname, buf, length);

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
		if (!outc->bCreated) {
			if ((ret = create_archive(o)) != SR_OK)
				return ret;
			outc->bCreated = TRUE;
		}
		logic = packet->payload;
		ret = append_archive_data(o, logic->data, logic->unitsize, logic->length);
		break;
	}

	return SR_OK;
}

static int cleanup(struct sr_output *o)
{
	struct out_context *outc;

	outc = o->priv;

	if (outc->zipArchive != NULL){
		save_meta_file(o);
		zipClose(outc->zipArchive, NULL);
	}

	g_free(outc->filename);
	g_free(outc);
	o->priv = NULL;

	return SR_OK;
}

static struct sr_option options[] = {
	{ "filename", "Filename", "File to write", NULL, NULL },
    {0, 0, 0, 0, 0}
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

