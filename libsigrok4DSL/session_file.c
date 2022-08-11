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

#include "libsigrok-internal.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <minizip/unzip.h>
#include "config.h" /* Needed for PACKAGE_VERSION and others. */
#include "log.h"

#undef LOG_PREFIX
#define LOG_PREFIX "session-file: "

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
	unzFile archive = NULL; 
    char szFilePath[15];
	unz_file_info64 fileInfo;
 
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

    archive = unzOpen64(filename);
	if (NULL == archive) {
		sr_err("load zip file error:%s", filename);
		return SR_ERR;
	}
	if (unzLocateFile(archive, "header", 0) != UNZ_OK){
        unzClose(archive);
        sr_err("unzLocateFile error:'header', %s", filename);
		return SR_ERR;
    } 
    if (unzGetCurrentFileInfo64(archive, &fileInfo, szFilePath, 
                sizeof(szFilePath), NULL, 0, NULL, 0) != UNZ_OK){
        unzClose(archive);
        sr_err("unzGetCurrentFileInfo64 error,'header', %s", filename);
		return SR_ERR;
    }
    if(unzOpenCurrentFile(archive) != UNZ_OK){
        sr_err("cant't open zip inner file:'header',%s", filename);
        unzClose(archive);
        return SR_ERR;
    }

	if (!(metafile = g_try_malloc(fileInfo.uncompressed_size))) {
		sr_err("%s: metafile malloc failed", __func__);
		return SR_ERR_MALLOC;
	}

	unzReadCurrentFile(archive, metafile, fileInfo.uncompressed_size);
    unzCloseCurrentFile(archive);

    if (unzClose(archive) != UNZ_OK){
        sr_err("close zip archive error:%s", filename);
        return SR_ERR;
    }
    archive = NULL;

	kf = g_key_file_new();
	if (!g_key_file_load_from_data(kf, metafile, fileInfo.uncompressed_size, 0, NULL)) {
		sr_err("Failed to parse metadata.");
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
                    sdi->dev_type = DEV_TYPE_FILELOG;
					if (devcnt == 0)
						/* first device, init the driver */
						sdi->driver->init(NULL);
					sr_dev_open(sdi);
					//sr_session_dev_add(sdi);
                    g_assert(0);

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
                } else if (!strcmp(keys[j], "hDiv min")) {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_MIN_TIMEBASE,
                            g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                } else if (!strcmp(keys[j], "hDiv max")) {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_MAX_TIMEBASE,
                            g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                } else if (!strcmp(keys[j], "bits")) {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_UNIT_BITS,
                            g_variant_new_byte(tmp_u64), sdi, NULL, NULL);
                } else if (!strcmp(keys[j], "ref min")) {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_REF_MIN,
                            g_variant_new_uint32(tmp_u64), sdi, NULL, NULL);
                } else if (!strcmp(keys[j], "ref max")) {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_REF_MAX,
                            g_variant_new_uint32(tmp_u64), sdi, NULL, NULL);
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
                } else if (!strncmp(keys[j], "vOffset", 7)) {
                    probenum = strtoul(keys[j]+7, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_HW_OFFSET,
                            g_variant_new_uint16(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "vTrig", 5)) {
                    probenum = strtoul(keys[j]+5, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
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
                            g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "pcnt", 4)) {
                    probenum = strtoul(keys[j]+4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_PCNT,
                            g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "max", 3)) {
                    probenum = strtoul(keys[j]+3, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_MAX,
                            g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "min", 3)) {
                    probenum = strtoul(keys[j]+3, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_MIN,
                            g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "plen", 4)) {
                    probenum = strtoul(keys[j]+4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_PLEN,
                            g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "llen", 4)) {
                    probenum = strtoul(keys[j]+4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_LLEN,
                            g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "level", 5)) {
                    probenum = strtoul(keys[j]+5, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_LEVEL,
                            g_variant_new_boolean(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "plevel", 6)) {
                    probenum = strtoul(keys[j]+6, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_PLEVEL,
                            g_variant_new_boolean(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "low", 3)) {
                    probenum = strtoul(keys[j]+3, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_LOW,
                            g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "high", 4)) {
                    probenum = strtoul(keys[j]+4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_HIGH,
                            g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "rlen", 4)) {
                    probenum = strtoul(keys[j]+4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_RLEN,
                            g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "flen", 4)) {
                    probenum = strtoul(keys[j]+4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_FLEN,
                            g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "rms", 3)) {
                    probenum = strtoul(keys[j]+3, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_RMS,
                            g_variant_new_uint64(tmp_u64), sdi, probe, NULL);
                    }
                } else if (!strncmp(keys[j], "mean", 4)) {
                    probenum = strtoul(keys[j]+4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels)) {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_MEAN,
                            g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
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

/** @} */
