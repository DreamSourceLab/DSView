/*
 * This file is part of the libsigrok project.
 *
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

#include "libsigrok.h"
#include "libsigrok-internal.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <zip.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "config.h" /* Needed for PACKAGE_VERSION and others. */

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "session-file: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

/**
 * @file
 *
 * Loading and saving libsigrok session files.
 */

/**
 * @addtogroup grp_session
 *
 * @{
 */

extern struct sr_session *session;
extern SR_PRIV struct sr_dev_driver session_driver;

/** @private */
SR_PRIV int sr_sessionfile_check(const char *filename)
{
    struct stat st;
    struct zip *archive;
    struct zip_file *zf;
    struct zip_stat zs;
    int version, ret;
    char s[11];

    if (!filename)
        return SR_ERR_ARG;

    if (stat(filename, &st) == -1) {
        sr_err("Couldn't stat %s: %s", filename, strerror(errno));
        return SR_ERR;
    }

    if (!(archive = zip_open(filename, 0, &ret)))
        /* No logging: this can be used just to check if it's
         * a sigrok session file or not. */
        return SR_ERR;

    /* check "version" */
    version = 0;
    if (!(zf = zip_fopen(archive, "version", 0))) {
        sr_dbg("Not a sigrok session file: no version found.");
        return SR_ERR;
    }
    if ((ret = zip_fread(zf, s, 10)) == -1)
        return SR_ERR;
    zip_fclose(zf);
    s[ret] = 0;
    version = strtoull(s, NULL, 10);
    if (version > 2) {
        sr_dbg("Cannot handle sigrok session file version %d.", version);
        return SR_ERR;
    }
    sr_spew("Detected sigrok session file version %d.", version);

    /* read "metadata" */
    if (zip_stat(archive, "metadata", 0, &zs) == -1) {
        sr_dbg("Not a valid sigrok session file.");
        return SR_ERR;
    }

    return SR_OK;
}

/**
 * Load the session from the specified filename.
 *
 * @param filename The name of the session file to load. Must not be NULL.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid arguments,
 *         SR_ERR_MALLOC upon memory allocation errors, or SR_ERR upon
 *         other errors.
 */
SR_API int sr_session_load(const char *filename)
{
	GKeyFile *kf;
	GPtrArray *capturefiles;
	struct zip *archive;
	struct zip_file *zf;
	struct zip_stat zs;
	struct sr_dev_inst *sdi;
	struct sr_channel *probe;
    int ret, devcnt, i, j;
    uint16_t probenum;
    uint64_t tmp_u64, total_probes, enabled_probes;
    uint16_t p;
    int64_t tmp_64;
    char **sections, **keys, *metafile, *val;
	char probename[SR_MAX_PROBENAME_LEN + 1];
    int mode = LOGIC;
    int channel_type = SR_CHANNEL_LOGIC;
    double tmp_double;
    int version = 1;

	if (!filename) {
		sr_err("%s: filename was NULL", __func__);
		return SR_ERR_ARG;
	}

	if (!(archive = zip_open(filename, 0, &ret))) {
		sr_dbg("Failed to open session file: zip error %d", ret);
		return SR_ERR;
	}

	/* read "metadata" */
    if (zip_stat(archive, "header", 0, &zs) == -1) {
        sr_dbg("Not a valid DSView data file.");
		return SR_ERR;
	}

	if (!(metafile = g_try_malloc(zs.size))) {
		sr_err("%s: metafile malloc failed", __func__);
		return SR_ERR_MALLOC;
	}

	zf = zip_fopen_index(archive, zs.index, 0);
	zip_fread(zf, metafile, zs.size);
	zip_fclose(zf);

	kf = g_key_file_new();
	if (!g_key_file_load_from_data(kf, metafile, zs.size, 0, NULL)) {
		sr_dbg("Failed to parse metadata.");
		return SR_ERR;
	}

	sr_session_new();

	devcnt = 0;
	capturefiles = g_ptr_array_new_with_free_func(g_free);
	sections = g_key_file_get_groups(kf, NULL);
	for (i = 0; sections[i]; i++) {
        if (!strcmp(sections[i], "version")) {
            keys = g_key_file_get_keys(kf, sections[i], NULL, NULL);
            for (j = 0; keys[j]; j++) {
                val = g_key_file_get_string(kf, sections[i], keys[j], NULL);
                if (!strcmp(keys[j], "version")) {
                    version = strtoull(val, NULL, 10);
                }
            }
        }
        if (!strncmp(sections[i], "header", 6)) {
			/* device section */
			sdi = NULL;
			enabled_probes = total_probes = 0;
			keys = g_key_file_get_keys(kf, sections[i], NULL, NULL);
			for (j = 0; keys[j]; j++) {
				val = g_key_file_get_string(kf, sections[i], keys[j], NULL);
                if (!strcmp(keys[j], "device mode")) {
                    mode = strtoull(val, NULL, 10);
                } else if (!strcmp(keys[j], "capturefile")) {
                    sdi = sr_dev_inst_new(mode, devcnt, SR_ST_ACTIVE, NULL, NULL, NULL);
					sdi->driver = &session_driver;
					if (devcnt == 0)
						/* first device, init the driver */
						sdi->driver->init(NULL);
					sr_dev_open(sdi);
					sr_session_dev_add(sdi);
					sdi->driver->config_set(SR_CONF_SESSIONFILE,
                            g_variant_new_bytestring(filename), sdi, NULL, NULL);
					sdi->driver->config_set(SR_CONF_CAPTUREFILE,
                            g_variant_new_bytestring(val), sdi, NULL, NULL);
					g_ptr_array_add(capturefiles, val);
                    sdi->driver->config_set(SR_CONF_FILE_VERSION,
                            g_variant_new_int16(version), sdi, NULL, NULL);
				} else if (!strcmp(keys[j], "samplerate")) {
					sr_parse_sizestring(val, &tmp_u64);
					sdi->driver->config_set(SR_CONF_SAMPLERATE,
                            g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                } else if (!strcmp(keys[j], "total samples")) {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_LIMIT_SAMPLES,
                            g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                } else if (!strcmp(keys[j], "hDiv")) {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_TIMEBASE,
                            g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                } else if (!strcmp(keys[j], "bits")) {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_UNIT_BITS,
                            g_variant_new_byte(tmp_u64), sdi, NULL, NULL);
                } else if (!strcmp(keys[j], "trigger time")) {
                    tmp_64 = strtoll(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_TRIGGER_TIME,
                            g_variant_new_int64(tmp_64), sdi, NULL, NULL);
                } else if (!strcmp(keys[j], "trigger pos")) {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_TRIGGER_POS,
                            g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                } else if (!strcmp(keys[j], "total blocks")) {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_NUM_BLOCKS,
                           g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                } else if (!strcmp(keys[j], "total probes")) {
					total_probes = strtoull(val, NULL, 10);
					sdi->driver->config_set(SR_CONF_CAPTURE_NUM_PROBES,
                            g_variant_new_uint64(total_probes), sdi, NULL, NULL);
                    if (version == 1) {
                        channel_type = (mode == DSO) ? SR_CHANNEL_DSO :
                                       (mode == ANALOG) ? SR_CHANNEL_ANALOG : SR_CHANNEL_LOGIC;
                        for (p = 0; p < total_probes; p++) {
                            snprintf(probename, SR_MAX_PROBENAME_LEN, "%u", p);
                            if (!(probe = sr_channel_new(p, channel_type, FALSE,
                                    probename)))
                                return SR_ERR;
                            sdi->channels = g_slist_append(sdi->channels, probe);
                        }
                    }
				} else if (!strncmp(keys[j], "probe", 5)) {
					if (!sdi)
						continue;
					enabled_probes++;
					tmp_u64 = strtoul(keys[j]+5, NULL, 10);
					/* sr_session_save() */
                    if (version == 1) {
                        sr_dev_probe_name_set(sdi, tmp_u64, val);
                        sr_dev_probe_enable(sdi, tmp_u64, TRUE);
                    } else if (version == 2) {
                        channel_type = (mode == DSO) ? SR_CHANNEL_DSO :
                                       (mode == ANALOG) ? SR_CHANNEL_ANALOG : SR_CHANNEL_LOGIC;
                        if (!(probe = sr_channel_new(tmp_u64, channel_type, TRUE,
                                val)))
                            return SR_ERR;
                        sdi->channels = g_slist_append(sdi->channels, probe);
                    }
				} else if (!strncmp(keys[j], "trigger", 7)) {
					probenum = strtoul(keys[j]+7, NULL, 10);
					sr_dev_trigger_set(sdi, probenum, val);
                } else if (!strncmp(keys[j], "enable", 6)) {
                    if (mode != LOGIC)
                        continue;
                    probenum = strtoul(keys[j]+6, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_EN,
                            g_variant_new_boolean(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "coupling", 8)) {
                    probenum = strtoul(keys[j]+8, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_COUPLING,
                            g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "vDiv", 4)) {
                    probenum = strtoul(keys[j]+4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_VDIV,
                            g_variant_new_uint64(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "vFactor", 7)) {
                    probenum = strtoul(keys[j]+7, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_FACTOR,
                            g_variant_new_uint64(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "vPos", 4)) {
                    probenum = strtoul(keys[j]+4, NULL, 10);
                    tmp_double = strtod(val, NULL);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_VPOS,
                            g_variant_new_double(tmp_double), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "vTrig", 5)) {
                    probenum = strtoul(keys[j]+5, NULL, 10);
                    tmp_u64 = strtod(val, NULL);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_TRIGGER_VALUE,
                            g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "period", 6)) {
                    probenum = strtoul(keys[j]+6, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_PERIOD,
                            g_variant_new_uint64(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "pcnt", 4)) {
                    probenum = strtoul(keys[j]+4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_PCNT,
                            g_variant_new_uint64(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "max", 3)) {
                    probenum = strtoul(keys[j]+3, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_MAX,
                            g_variant_new_uint64(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "min", 3)) {
                    probenum = strtoul(keys[j]+3, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_MIN,
                            g_variant_new_uint64(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "mapUnit", 7)) {
                    probenum = strtoul(keys[j]+7, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_MAP_UNIT,
                            g_variant_new_string(val), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "mapMax", 6)) {
                    probenum = strtoul(keys[j]+6, NULL, 10);
                    tmp_double = strtod(val, NULL);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_MAP_MAX,
                            g_variant_new_double(tmp_double), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "mapMin", 6)) {
                    probenum = strtoul(keys[j]+6, NULL, 10);
                    tmp_double = strtod(val, NULL);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_MAP_MIN,
                            g_variant_new_double(tmp_double), sdi, probe, NULL);
                    }
                }
			}
			g_strfreev(keys);
		}
		devcnt++;
	}
	g_strfreev(sections);
	g_key_file_free(kf);

	return SR_OK;
}

/**
 * Initialize a saved session file.
 *
 * @param filename The name of the filename to save the current session as.
 *                 Must not be NULL.
 * @param samplerate The samplerate to store for this session.
 * @param channels A NULL-terminated array of strings containing the names
 * of all the channels active in this session.
 *
 * @retval SR_OK Success
 * @retval SR_ERR_ARG Invalid arguments
 * @retval SR_ERR Other errors
 *
 * @since 0.3.0
 */
SR_API int sr_session_save_init(const char *filename, const char *metafile, const char *decfile)
{
    struct zip *zipfile;
    struct zip_source *metasrc;
    int ret;

    if (!filename) {
        sr_err("%s: filename was NULL", __func__);
        return SR_ERR_ARG;
    }

    /* Quietly delete it first, libzip wants replace ops otherwise. */
    unlink(filename);
    if (!(zipfile = zip_open(filename, ZIP_CREATE, &ret)))
        return SR_ERR;

    // meta file
    if (!(metasrc = zip_source_file(zipfile, metafile, 0, -1))) {
        unlink(metafile);
        return SR_ERR;
    }

    if (zip_add(zipfile, "header", metasrc) == -1) {
        unlink(metafile);
        return SR_ERR;
    }

    // decoders file
    if (decfile != NULL) {
        if (!(metasrc = zip_source_file(zipfile, decfile, 0, -1))) {
            unlink(decfile);
            return SR_ERR;
        }

        if (zip_add(zipfile, "decoders", metasrc) == -1) {
            unlink(decfile);
            return SR_ERR;
        }
    }

    if ((ret = zip_close(zipfile)) == -1) {
        sr_info("error saving zipfile: %s", zip_strerror(zipfile));
        unlink(metafile);
        if (decfile != NULL)
            unlink(decfile);
        return SR_ERR;
    }

    unlink(metafile);
    if (decfile != NULL)
        unlink(decfile);

    return SR_OK;
}

/**
 * Append data to an existing session file.
 *
 * The session file must have been created with sr_session_save_init()
 * or sr_session_save() beforehand.
 *
 * @param filename The name of the filename to append to. Must not be NULL.
 * @param buf The data to be appended.
 * @param size Buffer size.
 * @param chunk_num chunk number
 * @param index channel index
 * @param type channel type
 *
 * @retval SR_OK Success
 * @retval SR_ERR_ARG Invalid arguments
 * @retval SR_ERR Other errors
 *
 * @since 0.3.0
 */
SR_API int sr_session_append(const char *filename, const unsigned char *buf,
        uint64_t size, int chunk_num, int index, int type, int version)
{
    struct zip *archive;
    struct zip_source *logicsrc;
    int ret;
    char chunk_name[16], *type_name;

//    if ((ret = sr_sessionfile_check(filename)) != SR_OK)
//        return ret;

    if (!(archive = zip_open(filename, 0, &ret)))
        return SR_ERR;

    if (version == 2) {
        type_name = (type == SR_CHANNEL_LOGIC) ? "L" :
                    (type == SR_CHANNEL_DSO) ? "O" :
                    (type == SR_CHANNEL_ANALOG) ? "A" : "U";
        snprintf(chunk_name, 15, "%s-%d/%d", type_name, index, chunk_num);
    } else {
        snprintf(chunk_name, 15, "data");
    }

    if (!(logicsrc = zip_source_buffer(archive, buf, size, FALSE))) {
        return SR_ERR;
    }
    if (zip_file_add(archive, chunk_name, logicsrc, ZIP_FL_OVERWRITE) == -1) {
        return SR_ERR;
    }
    if ((ret = zip_close(archive)) == -1) {
        sr_info("error saving session file: %s", zip_strerror(archive));
        return SR_ERR;
    }

    return SR_OK;
}

/** @} */
