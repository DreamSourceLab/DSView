/*
 * dsview_cli.c - Command-line bridge to libsigrok4DSL for DreamSourceLab devices.
 *
 * Subcommands:
 *   scan     - list connected devices as JSON
 *   info     - print device capabilities as JSON
 *   capture  - capture logic data to binary file, print JSON status
 *
 * Trigger values passed to ds_trigger_probe_set():
 *   'X' = don't care   'R' = rising   'F' = falling   '1' = high   '0' = low
 */

/* Feature-test macro: expose POSIX + GNU extensions (readlink, clock_gettime,
 * CLOCK_REALTIME, strcasecmp, ETIMEDOUT).  Must precede any #include. */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <glib.h>

#include "libsigrok4DSL/libsigrok.h"
#include "libsigrok4DSL/libsigrok-internal.h"

/* -------------------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------------------- */

#define MAX_CH 16

/* -------------------------------------------------------------------------
 * Global capture state
 * ------------------------------------------------------------------------- */

static FILE *g_capture_file = NULL;
static pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_state_cond = PTHREAD_COND_INITIALIZER;
static volatile int g_capture_done = 0;
static volatile int g_capture_error = 0;
static uint64_t g_sample_bytes = 0;
static int g_unitsize = 2;	/* bytes per sample, set from channel count */
static int g_dev_mode = 0;	/* LOGIC=0, DSO=1, ANALOG=2              */
static uint64_t g_limit_samples = 0;	/* requested sample count for DSO stop    */
static int g_hw_nch = 0;	/* hw mode channel count (FPGA data packing) */

/* Leftover buffer for cross-to-parallel conversion.
 * USB transfers and receive_transfer() truncation can deliver data
 * that is not a whole multiple of nch*8 bytes (one cross-format group).
 * We buffer partial groups here and prepend them to the next callback. */
static uint8_t g_cross_leftover[MAX_CH * 8]; /* max group = 16 * 8 = 128 */
static size_t g_cross_leftover_len = 0;

/* -------------------------------------------------------------------------
 * Channel / trigger config (filled by argument parsing)
 * ------------------------------------------------------------------------- */

static int g_enabled_chs[MAX_CH];	/* physical channel indices */
static int g_n_enabled_chs = 0;	/* how many are in the list  */
static char g_ch_names[MAX_CH][64];	/* parallel name array       */

static int g_trig_ch = -1;	/* -1 = no trigger           */
static char g_trig_type[16] = "none";	/* rising/falling/high/low   */
static int g_trig_pos = 50;	/* pre-trigger %             */
static double g_vth = -1.0;	/* -1 = don't change default */

/* DSO / analog oscilloscope config */
static uint64_t g_vdiv[2] = { 0, 0 };	/* mV per div, 0 = don't change  */
static int g_coupling[2] = { -1, -1 };	/* 0=DC, 1=AC, -1=default       */
static uint64_t g_probe_factor[2] = { 0, 0 };	/* 1,2,10,20; 0 = don't change  */

/* -------------------------------------------------------------------------
 * Library callbacks
 * ------------------------------------------------------------------------- */

static void event_callback(int event)
{
	switch (event) {
	case DS_EV_COLLECT_TASK_START:
		break;
	case DS_EV_COLLECT_TASK_END:
	case DS_EV_COLLECT_TASK_END_BY_DETACHED:
		pthread_mutex_lock(&g_state_mutex);
		g_capture_done = 1;
		pthread_cond_signal(&g_state_cond);
		pthread_mutex_unlock(&g_state_mutex);
		break;
	case DS_EV_COLLECT_TASK_END_BY_ERROR:
		pthread_mutex_lock(&g_state_mutex);
		g_capture_done = 1;
		g_capture_error = 1;
		pthread_cond_signal(&g_state_cond);
		pthread_mutex_unlock(&g_state_mutex);
		break;
	default:
		break;
	}
}

/*
 * Convert one complete cross-format group to parallel format and write it.
 *
 * One group = nch * 8 input bytes -> 64 parallel samples of unitsize bytes.
 * The DSLogic FPGA sends LA_CROSS_DATA: data interleaved at 8-byte (64-bit)
 * boundaries per enabled channel.  With N enabled channels, each group is:
 *   [ch0 uint64][ch1 uint64]...[chN-1 uint64]
 * Each uint64 holds 64 consecutive sample bits for that one channel.
 *
 * Parallel format (what sigrok / our Python layer expects):
 * Each sample is unitsize bytes, with bit K = channel K value.
 */
static void convert_one_group(const uint8_t *gp, int nch, int unitsize,
			      FILE *fp, uint64_t *written)
{
	uint8_t out[64 * 2]; /* max unitsize=2, 64 samples per group */

	memset(out, 0, (size_t)(64 * unitsize));

	for (int ch = 0; ch < nch; ch++) {
		/* 8 bytes (64 bits) for this channel in this group */
		const uint8_t *ch_bytes = gp + ch * 8;
		int bit = ch; /* bit position in the output sample */

		for (int b = 0; b < 64; b++) {
			int byte_idx = b / 8;
			int bit_idx = b % 8;
			if (ch_bytes[byte_idx] & (1 << bit_idx)) {
				out[b * unitsize + bit / 8]
				    |= (uint8_t)(1 << (bit % 8));
			}
		}
	}
	fwrite(out, (size_t)unitsize, 64, fp);
	*written += (uint64_t)(64 * unitsize);
}

/*
 * Convert cross-format logic data to parallel format, handling partial
 * groups across callbacks.
 *
 * USB transfers and receive_transfer() truncation (dsl.c:2403) can
 * deliver data that is not a whole multiple of nch*8 bytes.  We buffer
 * leftover bytes in g_cross_leftover[] and prepend them to the next
 * callback's data before processing complete groups.
 */
static void cross_to_parallel(const uint8_t *src, size_t src_len,
			       int nch, int unitsize, FILE *fp,
			       uint64_t *written)
{
	size_t grp_in = (size_t)nch * 8;
	const uint8_t *p = src;
	size_t remain = src_len;

	/* If we have leftover bytes from the previous callback, try to
	 * complete the partial group by appending from the new data. */
	if (g_cross_leftover_len > 0) {
		size_t need = grp_in - g_cross_leftover_len;
		if (remain >= need) {
			memcpy(g_cross_leftover + g_cross_leftover_len,
			       p, need);
			convert_one_group(g_cross_leftover, nch, unitsize,
					  fp, written);
			p += need;
			remain -= need;
			g_cross_leftover_len = 0;
		} else {
			/* Still not enough for a full group -- accumulate */
			memcpy(g_cross_leftover + g_cross_leftover_len,
			       p, remain);
			g_cross_leftover_len += remain;
			return;
		}
	}

	/* Process all complete groups in the current buffer */
	while (remain >= grp_in) {
		convert_one_group(p, nch, unitsize, fp, written);
		p += grp_in;
		remain -= grp_in;
	}

	/* Save any remaining bytes for the next callback */
	if (remain > 0) {
		memcpy(g_cross_leftover, p, remain);
		g_cross_leftover_len = remain;
	}
}

static void datafeed_callback(const struct sr_dev_inst *sdi,
			      const struct sr_datafeed_packet *packet)
{
	(void)sdi;
	if (packet->type == SR_DF_LOGIC && g_capture_file) {
		const struct sr_datafeed_logic *logic =
		    (const struct sr_datafeed_logic *)packet->payload;
		if (logic && logic->data && logic->length > 0) {
			if (logic->format == LA_CROSS_DATA &&
			    g_hw_nch > 0) {
				cross_to_parallel(
				    (const uint8_t *)logic->data,
				    (size_t)logic->length,
				    g_hw_nch,
				    g_unitsize,
				    g_capture_file,
				    &g_sample_bytes);
			} else {
				fwrite(logic->data, 1,
				       (size_t)logic->length,
				       g_capture_file);
				g_sample_bytes += logic->length;
			}
		}
	}
	/* DSO mode: interleaved 8-bit samples [ch0_s0, ch1_s0, ch0_s1, ...] */
	if (packet->type == SR_DF_DSO && g_capture_file) {
		const struct sr_datafeed_dso *dso =
		    (const struct sr_datafeed_dso *)packet->payload;
		unsigned nch = (unsigned)(g_n_enabled_chs > 0 ? g_n_enabled_chs : 1);
		if (dso && dso->data && dso->num_samples > 0 && !g_capture_done) {
			/* Only write up to g_limit_samples per channel */
			uint64_t nsamp = (uint64_t) dso->num_samples;
			uint64_t have = g_sample_bytes / nch;
			uint64_t want = g_limit_samples ? g_limit_samples : nsamp;
			if (have < want) {
				uint64_t remaining = want - have;
				uint64_t take = (nsamp < remaining) ? nsamp : remaining;
				size_t len = (size_t)take * nch;
				fwrite(dso->data, 1, len, g_capture_file);
				g_sample_bytes += len;
			}
			/* Signal done once we have enough */
			have = g_sample_bytes / nch;
			if (have >= want) {
				pthread_mutex_lock(&g_state_mutex);
				g_capture_done = 1;
				pthread_cond_signal(&g_state_cond);
				pthread_mutex_unlock(&g_state_mutex);
			}
		}
	}
	/* ANALOG (DAQ) mode: same interleaved 8-bit format */
	if (packet->type == SR_DF_ANALOG && g_capture_file) {
		const struct sr_datafeed_analog *analog =
		    (const struct sr_datafeed_analog *)packet->payload;
		unsigned nch = (unsigned)(g_n_enabled_chs > 0 ? g_n_enabled_chs : 1);
		if (analog && analog->data && analog->num_samples > 0 && !g_capture_done) {
			uint64_t nsamp = (uint64_t) analog->num_samples;
			uint64_t have = g_sample_bytes / nch;
			uint64_t want = g_limit_samples ? g_limit_samples : nsamp;
			if (have < want) {
				uint64_t remaining = want - have;
				uint64_t take = (nsamp < remaining) ? nsamp : remaining;
				size_t len = (size_t)take * nch;
				fwrite(analog->data, 1, len, g_capture_file);
				g_sample_bytes += len;
			}
			have = g_sample_bytes / nch;
			if (have >= want) {
				pthread_mutex_lock(&g_state_mutex);
				g_capture_done = 1;
				pthread_cond_signal(&g_state_cond);
				pthread_mutex_unlock(&g_state_mutex);
			}
		}
	}
}

/* -------------------------------------------------------------------------
 * Library init
 * ------------------------------------------------------------------------- */

static void get_paths(char *fw_dir, size_t fw_sz, char *ud_dir, size_t ud_sz)
{
	const char *home = getenv("HOME");
	if (!home)
		home = "/tmp";

	char exe_path[512];
	ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
	if (len > 0) {
		exe_path[len] = '\0';
		char *slash = strrchr(exe_path, '/');
		if (slash) {
			*slash = '\0';
			/* Primary: DSView/res/ relative to the binary's directory.
			 * When built in-tree, the binary is at build.dir/dsview-cli
			 * so ../DSView/res/ points to the firmware.
			 * When built in build/cli/, ../../../DSView/res/ is the path,
			 * but we try the shorter path first. */
			snprintf(fw_dir, fw_sz, "%s/../DSView/res", exe_path);

			/* Check if primary path exists, fall back to source tree layout */
			if (access(fw_dir, R_OK) != 0) {
				snprintf(fw_dir, fw_sz, "%s/../../DSView/res", exe_path);
			}
		}
	} else {
		/* Fallback: system install path */
		snprintf(fw_dir, fw_sz, "/usr/share/DSView/res");
	}

	/* Final fallback: system install path if nothing found yet */
	if (access(fw_dir, R_OK) != 0) {
		snprintf(fw_dir, fw_sz, "/usr/share/DSView/res");
	}

	snprintf(ud_dir, ud_sz, "%s/.config/dsview-cli", home);
	g_mkdir_with_parents(ud_dir, 0755);
}

static int lib_init(void)
{
	char fw_dir[512], ud_dir[512];
	get_paths(fw_dir, sizeof(fw_dir), ud_dir, sizeof(ud_dir));

	ds_set_firmware_resource_dir(fw_dir);
	ds_set_user_data_dir(ud_dir);
	ds_set_event_callback(event_callback);
	ds_set_datafeed_callback(datafeed_callback);

	int ret = ds_lib_init();
	if (ret != SR_OK)
		fprintf(stderr, "{\"error\": \"ds_lib_init failed: %d\"}\n", ret);
	return ret;
}

/* -------------------------------------------------------------------------
 * Device activation with exclusive-access check
 * ------------------------------------------------------------------------- */

static const char *device_error_hint(int err_code)
{
	switch (err_code) {
	case SR_ERR_DEVICE_IS_EXCLUSIVE:
		return "device is in use by another application "
		    "(DSView GUI or another dsview-cli instance)";
	case SR_ERR_FIRMWARE_NOT_EXIST:
		return "firmware file not found";
	case SR_ERR_DEVICE_FIRMWARE_VERSION_LOW:
		return "device firmware version too low, update via DSView GUI";
	case SR_ERR_DEVICE_USB_IO_ERROR:
		return "USB I/O error";
	case SR_ERR_DEVICE_NO_DRIVER:
		return "no driver for this device";
	default:
		return NULL;
	}
}

static int activate_device(int dev_index, struct ds_device_base_info *list, int count)
{
	if (dev_index < 0 || dev_index >= count) {
		fprintf(stderr, "device index %d out of range (0-%d)\n",
			dev_index, count - 1);
		return -1;
	}

	ds_device_handle requested_handle = list[dev_index].handle;

	int ret = ds_active_device_by_index(dev_index);
	if (ret != SR_OK) {
		int last_err = ds_get_last_error();
		const char *hint = device_error_hint(last_err);
		if (hint)
			fprintf(stderr, "failed to activate device %d: %s\n",
				dev_index, hint);
		else
			fprintf(stderr, "failed to activate device %d (code %d)\n",
				dev_index, last_err);
		return -1;
	}

	/* The library may silently fall back to the Demo Device when the
	 * requested USB device is exclusively held by another process.
	 * Detect this by comparing the activated handle with what we asked for. */
	struct ds_device_full_info activated;
	memset(&activated, 0, sizeof(activated));
	ds_get_actived_device_info(&activated);

	if (activated.handle != requested_handle) {
		fprintf(stderr, "device %d (\"%s\") is in use by another application "
			"(DSView GUI or another dsview-cli instance); "
			"library fell back to \"%s\"\n",
			dev_index, list[dev_index].name, activated.name);
		return -2;
	}

	return 0;
}

/* -------------------------------------------------------------------------
 * Metadata sidecar  (.bin.meta.json written alongside the capture file)
 * ------------------------------------------------------------------------- */

/* Per-channel DSO metadata, filled during capture for sidecar output */
static uint64_t g_ch_vdiv[MAX_CH];	/* actual vdiv (mV) per ch  */
static uint64_t g_ch_vfactor[MAX_CH];	/* actual probe factor      */
static uint8_t g_ch_coupling[MAX_CH];	/* 0=DC, 1=AC               */
static uint16_t g_ch_hw_offset[MAX_CH];	/* hardware ADC offset      */
static uint8_t g_ch_bits[MAX_CH];	/* bits per sample (8)      */

static void write_metadata(const char *bin_path,
			   uint64_t samplerate, uint64_t n_samples, int unitsize)
{
	char meta_path[600];
	snprintf(meta_path, sizeof(meta_path), "%s.meta.json", bin_path);

	FILE *f = fopen(meta_path, "w");
	if (!f)
		return;

	int trig_en = (g_trig_ch >= 0 && strcmp(g_trig_type, "none") != 0);
	const char *mode_name = (g_dev_mode == DSO) ? "dso" :
	    (g_dev_mode == ANALOG) ? "analog" : "logic";

	fprintf(f, "{\n");
	fprintf(f, "  \"mode\": \"%s\",\n", mode_name);
	fprintf(f, "  \"samplerate\": %llu,\n", (unsigned long long)samplerate);
	fprintf(f, "  \"samples\": %llu,\n", (unsigned long long)n_samples);
	fprintf(f, "  \"unitsize\": %d,\n", unitsize);
	fprintf(f, "  \"channel_map\": [\n");
	for (int i = 0; i < g_n_enabled_chs; i++) {
		int phys = g_enabled_chs[i];
		const char *ch_type = (g_dev_mode == DSO || g_dev_mode == ANALOG)
		    ? "dso" : "logic";
		fprintf(f, "    {\"seq\": %d, \"phys\": %d, \"name\": \"%s\", "
			"\"type\": \"%s\"",
			i, phys, g_ch_names[i][0] ? g_ch_names[i] : "", ch_type);
		if (g_dev_mode == DSO || g_dev_mode == ANALOG) {
			const char *coup = (g_ch_coupling[i] == 1) ? "AC" : "DC";
			fprintf(f, ", \"vdiv_mV\": %llu, \"probe_factor\": %llu, "
				"\"coupling\": \"%s\", \"hw_offset\": %u, \"bits\": %u",
				(unsigned long long)g_ch_vdiv[i],
				(unsigned long long)g_ch_vfactor[i],
				coup,
				(unsigned)g_ch_hw_offset[i], (unsigned)g_ch_bits[i]);
		}
		fprintf(f, "}%s\n", (i < g_n_enabled_chs - 1) ? "," : "");
	}
	fprintf(f, "  ],\n");
	fprintf(f, "  \"trigger\": {\n");
	fprintf(f, "    \"enabled\": %s", trig_en ? "true" : "false");
	if (trig_en) {
		fprintf(f, ",\n    \"channel\": %d", g_trig_ch);
		fprintf(f, ",\n    \"type\": \"%s\"", g_trig_type);
	}
	fprintf(f, ",\n    \"pos_pct\": %d\n", g_trig_pos);
	fprintf(f, "  }\n");
	fprintf(f, "}\n");
	fclose(f);
}

/* -------------------------------------------------------------------------
 * Trigger setup
 * ------------------------------------------------------------------------- */

static void apply_trigger(void)
{
	ds_trigger_reset();

	if (g_trig_ch < 0 || strcmp(g_trig_type, "none") == 0) {
		ds_trigger_set_en(0);
		return;
	}

	char t0;
	if (!strcmp(g_trig_type, "rising"))
		t0 = 'R';
	else if (!strcmp(g_trig_type, "falling"))
		t0 = 'F';
	else if (!strcmp(g_trig_type, "high"))
		t0 = '1';
	else if (!strcmp(g_trig_type, "low"))
		t0 = '0';
	else
		t0 = 'X';

	ds_trigger_set_en(1);
	ds_trigger_set_stage(1);
	ds_trigger_set_pos((uint16_t) g_trig_pos);
	/* second condition ('X') = don't care */
	ds_trigger_probe_set((uint16_t) g_trig_ch, (unsigned char)t0, 'X');
}

/* -------------------------------------------------------------------------
 * Channel setup helper
 * ------------------------------------------------------------------------- */

static int setup_channels(int total_ch)
{
	/* Enable ALL channels within the hardware mode range, matching
	 * the DSView GUI behaviour.  The FPGA packs cross-format data
	 * for every enabled channel.  Disabling a subset within the mode
	 * causes the FPGA packing to differ from what cross_to_parallel()
	 * expects (nch = g_hw_nch = mode channel count).
	 *
	 * Channels beyond the mode range are disabled.
	 * User-requested channels get their names set. */
	for (int ch = 0; ch < total_ch; ch++) {
		gboolean en = (ch < g_hw_nch) ? TRUE : FALSE;
		ds_enable_device_channel_index(ch, en);

		/* Set name for user-requested channels */
		int name_idx = -1;
		for (int j = 0; j < g_n_enabled_chs; j++) {
			if (g_enabled_chs[j] == ch) {
				name_idx = j;
				break;
			}
		}
		if (name_idx >= 0 && g_ch_names[name_idx][0])
			ds_set_device_channel_name(ch, g_ch_names[name_idx]);
	}
	return SR_OK;
}

/* -------------------------------------------------------------------------
 * scan
 * ------------------------------------------------------------------------- */

static int cmd_scan(void)
{
	if (lib_init() != SR_OK)
		return 1;

	ds_reload_device_list();
	g_usleep(800000);

	struct ds_device_base_info *list = NULL;
	int count = 0;
	ds_get_device_list(&list, &count);

	printf("[\n");
	for (int i = 0; i < count; i++) {
		/* Escape quotes in name */
		char safe[200];
		int j = 0;
		for (const char *s = list[i].name; *s && j < 197; s++) {
			if (*s == '"')
				safe[j++] = '\\';
			safe[j++] = *s;
		}
		safe[j] = '\0';
		printf("  {\"index\": %d, \"handle\": %llu, \"name\": \"%s\"}%s\n",
		       i, (unsigned long long)list[i].handle, safe,
		       (i < count - 1) ? "," : "");
	}
	printf("]\n");

	g_free(list);
	ds_lib_exit();
	return 0;
}

/* -------------------------------------------------------------------------
 * info
 * ------------------------------------------------------------------------- */

static int cmd_info(int dev_index)
{
	if (lib_init() != SR_OK)
		return 1;

	ds_reload_device_list();
	g_usleep(800000);

	struct ds_device_base_info *list = NULL;
	int count = 0;
	ds_get_device_list(&list, &count);

	if (count == 0) {
		printf("{\"error\": \"no devices found\"}\n");
		g_free(list);
		ds_lib_exit();
		return 1;
	}

	int act_ret = activate_device(dev_index, list, count);
	if (act_ret != 0) {
		if (act_ret == -2)
			printf
			    ("{\"error\": \"device %d is in use by another application\"}\n",
			     dev_index);
		else
			printf("{\"error\": \"failed to activate device %d\"}\n",
			       dev_index);
		g_free(list);
		ds_lib_exit();
		return 1;
	}

	int init_status = 0;
	for (int i = 0; i < 100; i++) {
		ds_get_actived_device_init_status(&init_status);
		if (init_status)
			break;
		g_usleep(100000);
	}

	struct ds_device_full_info info;
	memset(&info, 0, sizeof(info));
	ds_get_actived_device_info(&info);
	int n_ch = info.di ? (int)g_slist_length(info.di->channels) : 0;

	GVariant *v = NULL;
	uint64_t samplerate = 0, limit_samples = 0;
	double vth = -1.0;
	if (ds_get_actived_device_config(NULL, NULL, SR_CONF_SAMPLERATE, &v) == SR_OK
	    && v) {
		samplerate = g_variant_get_uint64(v);
		g_variant_unref(v);
		v = NULL;
	}
	if (ds_get_actived_device_config(NULL, NULL, SR_CONF_LIMIT_SAMPLES, &v) == SR_OK
	    && v) {
		limit_samples = g_variant_get_uint64(v);
		g_variant_unref(v);
		v = NULL;
	}
	if (ds_get_actived_device_config(NULL, NULL, SR_CONF_VTH, &v) == SR_OK && v) {
		vth = g_variant_get_double(v);
		g_variant_unref(v);
		v = NULL;
	}

	/* Channel modes: each entry describes channels-in-use vs max samplerate */
	GVariant *cm_data = NULL;
	char cm_json[2048] = "[]";
	if (ds_get_actived_device_config_list(NULL, SR_CONF_CHANNEL_MODE, &cm_data) ==
	    SR_OK && cm_data) {
		struct sr_list_item *modes =
		    (struct sr_list_item *)(uintptr_t) g_variant_get_uint64(cm_data);
		g_variant_unref(cm_data);
		int pos = 0;
		pos += snprintf(cm_json + pos, sizeof(cm_json) - (size_t)pos, "[");
		int first = 1;
		for (int i = 0; modes[i].id >= 0; i++) {
			/* escape any quotes in description */
			char safe_desc[256];
			int j = 0;
			const char *src = modes[i].name ? modes[i].name : "";
			for (; *src && j < 253; src++) {
				if (*src == '"')
					safe_desc[j++] = '\\';
				safe_desc[j++] = *src;
			}
			safe_desc[j] = '\0';
			if (!first)
				pos +=
				    snprintf(cm_json + pos, sizeof(cm_json) - (size_t)pos,
					     ", ");
			pos +=
			    snprintf(cm_json + pos, sizeof(cm_json) - (size_t)pos,
				     "{\"id\": %d, \"desc\": \"%s\"}", modes[i].id,
				     safe_desc);
			first = 0;
		}
		snprintf(cm_json + pos, sizeof(cm_json) - (size_t)pos, "]");
	}

	/* Library / DSView version */
	const char *lib_ver = sr_get_lib_version_string();

	int dev_mode = ds_get_actived_device_mode();
	const char *mode_name = (dev_mode == DSO) ? "DSO" :
	    (dev_mode == ANALOG) ? "ANALOG" : "LOGIC";

	printf("{\n");
	printf("  \"index\": %d,\n", dev_index);
	printf("  \"handle\": %llu,\n", (unsigned long long)info.handle);
	printf("  \"name\": \"%s\",\n", info.name);
	printf("  \"driver\": \"%s\",\n", info.driver_name);
	printf("  \"dsview_version\": \"1.3.2\",\n");
	printf("  \"libsigrok4dsl_version\": \"%s\",\n", lib_ver ? lib_ver : "unknown");
	printf("  \"channels\": %d,\n", n_ch);
	printf("  \"channel_range\": [0, %d],\n", n_ch - 1);
	printf("  \"mode\": %d,\n", dev_mode);
	printf("  \"mode_name\": \"%s\",\n", mode_name);
	printf("  \"samplerate\": %llu,\n", (unsigned long long)samplerate);
	printf("  \"limit_samples\": %llu,\n", (unsigned long long)limit_samples);

	if (dev_mode == DSO || dev_mode == ANALOG) {
		/* Per-channel analog info */
		printf("  \"analog_channels\": [\n");
		GSList *channels = ds_get_actived_device_channels();
		int ch_idx = 0;
		for (GSList * l = channels; l; l = l->next, ch_idx++) {
			struct sr_channel *ch = (struct sr_channel *)l->data;
			const char *coup_str =
			    (ch->coupling == SR_AC_COUPLING) ? "AC" : "DC";
			printf("    {\"index\": %d, \"name\": \"%s\", \"enabled\": %s, " "\"vdiv_mV\": %llu, \"probe_factor\": %llu, " "\"coupling\": \"%s\", \"bits\": %u, \"hw_offset\": %u}%s\n", (int)ch->index, ch->name ? ch->name : "", ch->enabled ? "true" : "false", (unsigned long long)ch->vdiv, (unsigned long long)ch->vfactor, coup_str, (unsigned)ch->bits, (unsigned)ch->offset,	/* use software offset; hw_offset only valid after acq */
			       (l->next) ? "," : "");
		}
		printf("  ],\n");
		printf("  \"vdiv_options\": [10, 20, 50, 100, 200, 500, 1000, 2000],\n");
		printf("  \"coupling_options\": [\"DC\", \"AC\"],\n");
		printf("  \"probe_factor_options\": [1, 2, 10, 20],\n");
		printf("  \"trigger_types\": [\"none\", \"rising\", \"falling\"],\n");
	} else {
		printf("  \"vth\": %.2f,\n", vth);
		printf
		    ("  \"trigger_types\": [\"none\", \"rising\", \"falling\", \"high\", \"low\"],\n");
	}

	printf("  \"channel_modes\": %s\n", cm_json);
	printf("}\n");

	g_free(list);
	ds_lib_exit();
	return 0;
}

/* -------------------------------------------------------------------------
 * capture
 * ------------------------------------------------------------------------- */

static int cmd_capture(int dev_index, uint64_t samplerate, uint64_t limit_samples,
		       const char *outfile)
{
	g_capture_done = g_capture_error = 0;
	g_sample_bytes = 0;
	g_cross_leftover_len = 0;
	g_hw_nch = 0;
	memset(g_ch_vdiv, 0, sizeof(g_ch_vdiv));
	memset(g_ch_vfactor, 0, sizeof(g_ch_vfactor));
	memset(g_ch_coupling, 0, sizeof(g_ch_coupling));
	memset(g_ch_hw_offset, 0, sizeof(g_ch_hw_offset));
	memset(g_ch_bits, 0, sizeof(g_ch_bits));

	if (lib_init() != SR_OK)
		return 1;

	ds_reload_device_list();
	g_usleep(800000);

	struct ds_device_base_info *list = NULL;
	int count = 0;
	ds_get_device_list(&list, &count);

	if (count == 0) {
		printf("{\"success\": false, \"error\": \"no devices found\"}\n");
		g_free(list);
		ds_lib_exit();
		return 1;
	}

	int act_ret = activate_device(dev_index, list, count);
	if (act_ret != 0) {
		if (act_ret == -2)
			printf("{\"success\": false, \"error\": \"device %d is in use by "
			       "another application\"}\n", dev_index);
		else
			printf("{\"success\": false, \"error\": \"failed to activate "
			       "device %d\"}\n", dev_index);
		g_free(list);
		ds_lib_exit();
		return 1;
	}

	/* Device list no longer needed after activation */
	g_free(list);
	list = NULL;

	/* Wait for device init */
	int init_status = 0;
	for (int i = 0; i < 100; i++) {
		ds_get_actived_device_init_status(&init_status);
		if (init_status)
			break;
		g_usleep(100000);
	}
	if (!init_status) {
		printf("{\"success\": false, \"error\": \"device init timed out\"}\n");
		ds_lib_exit();
		return 1;
	}

	/* Detect device mode */
	g_dev_mode = ds_get_actived_device_mode();
	int is_dso = (g_dev_mode == DSO || g_dev_mode == ANALOG);

	/* LOGIC mode: select the best hardware channel mode for the number
	 * of requested channels.  The DSLogic FPGA must be told which
	 * channel mode to use -- simply enabling/disabling individual
	 * channels is not enough.  Without this, the FPGA stays in the
	 * default 16-channel mode and packs data for 16 channels even if
	 * only 2 are requested, causing misaligned bit extraction. */
	int ch_mode_num = 0;   /* channel count of the selected hw mode */
	if (!is_dso) {
		GVariant *cm_data = NULL;
		if (ds_get_actived_device_config_list(NULL,
		    SR_CONF_CHANNEL_MODE, &cm_data) == SR_OK && cm_data) {
			struct sr_list_item *modes =
			    (struct sr_list_item *)(uintptr_t) g_variant_get_uint64(cm_data);
			g_variant_unref(cm_data);

			/* The hw mode must cover ALL requested channel indices.
			 * For contiguous channels 0-6 (count=7), max index is 6,
			 * so min_hw_channels = 7.  For non-contiguous channels
			 * like 0,3,7 (count=3), max index is 7, so
			 * min_hw_channels = 8.  The mode must have >= this many
			 * channels because the FPGA addresses channels by index. */
			int max_ch_idx = 0;
			for (int i = 0; i < g_n_enabled_chs; i++) {
				if (g_enabled_chs[i] > max_ch_idx)
					max_ch_idx = g_enabled_chs[i];
			}
			int min_hw_chs = max_ch_idx + 1;
			if (min_hw_chs < g_n_enabled_chs)
				min_hw_chs = g_n_enabled_chs;

			/* Find the mode with the smallest channel count
			 * that can still accommodate all requested channels.
			 * Parse channel count from desc: "Use N Channels ..." */
			int best_id = -1;
			int best_nch = 9999;
			for (int i = 0; modes[i].id >= 0; i++) {
				int nch = 0;
				const char *p = modes[i].name;
				if (p) {
					/* Look for "N Channel" pattern */
					const char *u = strstr(p, "se ");
					if (u) {
						nch = atoi(u + 3);
					} else {
						/* Try "Channels 0~N" pattern */
						const char *t = strstr(p, "0~");
						if (t)
							nch = atoi(t + 2) + 1;
					}
				}
				if (nch >= min_hw_chs && nch < best_nch) {
					best_nch = nch;
					best_id = modes[i].id;
				}
			}
			if (best_id >= 0) {
				GVariant *v = g_variant_new_int16((int16_t)best_id);
				if (ds_set_actived_device_config(NULL, NULL,
				    SR_CONF_CHANNEL_MODE, v) != SR_OK)
					fprintf(stderr,
						"Warning: set channel mode %d failed\n",
						best_id);
				else
					ch_mode_num = best_nch;
			}
		}
	}

	/* Store the hw channel mode count globally.  The FPGA packs cross-
	 * format data for ALL enabled channels.  Like the DSView GUI, we
	 * keep all channels in the mode enabled (not just user-requested
	 * ones) so the FPGA packing matches our expectation exactly. */
	g_hw_nch = ch_mode_num > 0 ? ch_mode_num : g_n_enabled_chs;

	/* bytes-per-sample: DSO = 1 byte per channel (8-bit ADC),
	 * LOGIC = use the hw channel mode's channel count for unitsize */
	if (is_dso)
		g_unitsize = 1;	/* 8-bit ADC, 1 byte per channel per sample */
	else
		g_unitsize = (g_hw_nch <= 8) ? 1 : 2;

	/* Samplerate -- sr_config_set() takes ownership of the GVariant
	 * (ref_sink + unref), so we must NOT unref after the call. */
	GVariant *v = g_variant_new_uint64(samplerate);
	if (ds_set_actived_device_config(NULL, NULL, SR_CONF_SAMPLERATE, v) != SR_OK)
		fprintf(stderr, "Warning: set samplerate returned error\n");

	/* Limit samples */
	v = g_variant_new_uint64(limit_samples);
	if (ds_set_actived_device_config(NULL, NULL, SR_CONF_LIMIT_SAMPLES, v) != SR_OK)
		fprintf(stderr, "Warning: set limit_samples returned error\n");

	/* For DSO/ANALOG: datafeed callback uses this to stop after enough data */
	g_limit_samples = is_dso ? limit_samples : 0;

	/* Logic-analyzer-specific: Voltage threshold */
	double vth_actual = -1.0;
	if (!is_dso) {
		if (g_vth >= 0.0) {
			if (g_vth > 5.0)
				fprintf(stderr,
					"Warning: VTH %.2f exceeds 5.0V maximum\n",
					g_vth);
			v = g_variant_new_double(g_vth);
			if (ds_set_actived_device_config(NULL, NULL, SR_CONF_VTH, v) !=
			    SR_OK)
				fprintf(stderr, "Warning: set VTH returned error\n");
		}
		v = NULL;
		if (ds_get_actived_device_config(NULL, NULL, SR_CONF_VTH, &v) == SR_OK
		    && v) {
			vth_actual = g_variant_get_double(v);
			g_variant_unref(v);
			v = NULL;
		}
	}

	/* DSO buffered mode requires both channels enabled; force-enable both
	 * if user only requested one, adding a default name for the extra. */
	if (is_dso && g_n_enabled_chs < 2) {
		int need[2] = { 0, 1 };
		int found[2] = { 0, 0 };
		for (int j = 0; j < g_n_enabled_chs; j++)
			if (g_enabled_chs[j] < 2)
				found[g_enabled_chs[j]] = 1;
		g_n_enabled_chs = 0;
		for (int j = 0; j < 2; j++) {
			g_enabled_chs[g_n_enabled_chs] = need[j];
			if (!found[need[j]] && !g_ch_names[g_n_enabled_chs][0])
				snprintf(g_ch_names[g_n_enabled_chs],
					 sizeof(g_ch_names[0]), "CH%d", need[j]);
			g_n_enabled_chs++;
		}
	}

	/* Channel enable / name.
	 *
	 * After the channel mode change, dsl_adjust_probes() may have
	 * resized the probe list (e.g. 16 -> 8 for "Use 8 Channels").
	 * Re-query the channel count and enable ALL channels within the
	 * hw mode range (matching DSView GUI behaviour).  The FPGA packs
	 * cross-format data for every enabled channel -- selectively
	 * disabling channels within the mode causes the data packing to
	 * differ from what cross_to_parallel() expects.
	 * dsl_en_ch_num() will return g_hw_nch after setup_channels(). */
	struct ds_device_full_info finfo;
	memset(&finfo, 0, sizeof(finfo));
	ds_get_actived_device_info(&finfo);
	int total_ch = finfo.di ? (int)g_slist_length(finfo.di->channels) : MAX_CH;
	setup_channels(total_ch);

	/* DSO-specific: apply per-channel analog config (vdiv, coupling, probe) */
	if (is_dso) {
		GSList *channels = ds_get_actived_device_channels();
		for (GSList * l = channels; l; l = l->next) {
			struct sr_channel *ch = (struct sr_channel *)l->data;
			int idx = (int)ch->index;
			if (idx < 0 || idx >= 2)
				continue;

			if (g_vdiv[idx] > 0) {
				v = g_variant_new_uint64(g_vdiv[idx]);
				ds_set_actived_device_config(ch, NULL, SR_CONF_PROBE_VDIV,
							     v);
			}
			if (g_coupling[idx] >= 0) {
				v = g_variant_new_byte((uint8_t) g_coupling[idx]);
				ds_set_actived_device_config(ch, NULL,
							     SR_CONF_PROBE_COUPLING, v);
			}
			if (g_probe_factor[idx] > 0) {
				v = g_variant_new_uint64(g_probe_factor[idx]);
				ds_set_actived_device_config(ch, NULL,
							     SR_CONF_PROBE_FACTOR, v);
			}
		}

		/* Read back actual per-channel settings for metadata */
		channels = ds_get_actived_device_channels();
		int seq = 0;
		for (GSList * l = channels; l; l = l->next) {
			struct sr_channel *ch = (struct sr_channel *)l->data;
			if (!ch->enabled)
				continue;
			if (seq < MAX_CH) {
				g_ch_vdiv[seq] = ch->vdiv;
				g_ch_vfactor[seq] = ch->vfactor;
				g_ch_coupling[seq] = ch->coupling;
				g_ch_hw_offset[seq] = ch->offset;	/* use software offset (128 for 8-bit ADC); hw_offset only valid after acq start */
				g_ch_bits[seq] = ch->bits;
				seq++;
			}
		}
	}

	/* Trigger setup */
	if (is_dso) {
		/* DSO trigger: use SR_CONF_TRIGGER_SOURCE and SR_CONF_TRIGGER_SLOPE */
		uint8_t trig_src;
		if (g_trig_ch < 0 || strcmp(g_trig_type, "none") == 0)
			trig_src = DSO_TRIGGER_AUTO;
		else if (g_trig_ch == 0)
			trig_src = DSO_TRIGGER_CH0;
		else
			trig_src = DSO_TRIGGER_CH1;

		v = g_variant_new_byte(trig_src);
		ds_set_actived_device_config(NULL, NULL, SR_CONF_TRIGGER_SOURCE, v);

		if (g_trig_ch >= 0 && strcmp(g_trig_type, "none") != 0) {
			uint8_t slope = DSO_TRIGGER_RISING;
			if (!strcmp(g_trig_type, "falling"))
				slope = DSO_TRIGGER_FALLING;
			v = g_variant_new_byte(slope);
			ds_set_actived_device_config(NULL, NULL, SR_CONF_TRIGGER_SLOPE,
						     v);
		}

		/* Trigger position for DSO (percentage) */
		v = g_variant_new_byte((uint8_t) g_trig_pos);
		ds_set_actived_device_config(NULL, NULL, SR_CONF_HORIZ_TRIGGERPOS, v);
	} else {
		/* Logic-analyzer trigger */
		apply_trigger();
	}

	/* Open output file -- 12-byte header: [uint64 samplerate][uint32 n_channels] */
	g_capture_file = fopen(outfile, "wb");
	if (!g_capture_file) {
		printf("{\"success\": false, \"error\": \"cannot open: %s\"}\n", outfile);
		ds_lib_exit();
		return 1;
	}
	uint64_t hdr_sr = samplerate;
	/* Write the hw mode channel count so the Python layer computes
	 * the correct unitsize for the parallel-format data we produce. */
	uint32_t hdr_ch = is_dso ? (uint32_t)g_n_enabled_chs
				 : (uint32_t)g_hw_nch;
	fwrite(&hdr_sr, sizeof(hdr_sr), 1, g_capture_file);
	fwrite(&hdr_ch, sizeof(hdr_ch), 1, g_capture_file);

	/* Start */
	pthread_mutex_lock(&g_state_mutex);
	g_capture_done = 0;

	if (ds_start_collect() != SR_OK) {
		pthread_mutex_unlock(&g_state_mutex);
		fclose(g_capture_file);
		g_capture_file = NULL;
		printf("{\"success\": false, \"error\": \"ds_start_collect failed\"}\n");
		ds_lib_exit();
		return 1;
	}

	/* Wait for DS_EV_COLLECT_TASK_END (120 s max) */
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 120;
	while (!g_capture_done) {
		if (pthread_cond_timedwait(&g_state_cond, &g_state_mutex, &ts) ==
		    ETIMEDOUT) {
			g_capture_error = 1;
			break;
		}
	}
	pthread_mutex_unlock(&g_state_mutex);

	ds_stop_collect();
	fclose(g_capture_file);
	g_capture_file = NULL;

	/* For DSO, n_samples = total_bytes / n_enabled_channels (1 byte per ch) */
	uint64_t n_samples;
	if (is_dso)
		n_samples = (g_n_enabled_chs > 0) ?
		    (g_sample_bytes / (uint64_t) g_n_enabled_chs) : 0;
	else
		n_samples = (g_unitsize > 0) ?
		    (g_sample_bytes / (uint64_t) g_unitsize) : 0;

	/* Write sidecar metadata */
	if (!g_capture_error)
		write_metadata(outfile, samplerate, n_samples, g_unitsize);

	/* Build channel_map JSON inline */
	char ch_map[2048];
	int pos = 0;
	pos += snprintf(ch_map + pos, sizeof(ch_map) - (size_t)pos, "[");
	for (int i = 0; i < g_n_enabled_chs; i++) {
		if (is_dso) {
			const char *coup = (g_ch_coupling[i] == 1) ? "AC" : "DC";
			pos += snprintf(ch_map + pos, sizeof(ch_map) - (size_t)pos,
					"{\"seq\":%d,\"phys\":%d,\"name\":\"%s\","
					"\"type\":\"dso\","
					"\"vdiv_mV\":%llu,\"probe_factor\":%llu,"
					"\"coupling\":\"%s\",\"hw_offset\":%u,\"bits\":%u}%s",
					i, g_enabled_chs[i],
					g_ch_names[i][0] ? g_ch_names[i] : "",
					(unsigned long long)g_ch_vdiv[i],
					(unsigned long long)g_ch_vfactor[i],
					coup,
					(unsigned)g_ch_hw_offset[i],
					(unsigned)g_ch_bits[i],
					(i < g_n_enabled_chs - 1) ? "," : "");
		} else {
			pos += snprintf(ch_map + pos, sizeof(ch_map) - (size_t)pos,
					"{\"seq\":%d,\"phys\":%d,\"name\":\"%s\"}%s",
					i, g_enabled_chs[i],
					g_ch_names[i][0] ? g_ch_names[i] : "",
					(i < g_n_enabled_chs - 1) ? "," : "");
		}
	}
	pos += snprintf(ch_map + pos, sizeof(ch_map) - (size_t)pos, "]");

	int trig_en = (g_trig_ch >= 0 && strcmp(g_trig_type, "none") != 0);
	const char *mode_str = is_dso ? (g_dev_mode == DSO ? "dso" : "analog") : "logic";

	if (g_capture_error) {
		printf("{\"success\": false, \"error\": \"capture error or timeout\","
		       " \"samples\": %llu}\n", (unsigned long long)n_samples);
		ds_lib_exit();
		return 1;
	}

	printf("{\"success\": true,"
	       " \"mode\": \"%s\","
	       " \"samples\": %llu,"
	       " \"samplerate\": %llu,"
	       " \"unitsize\": %d,",
	       mode_str,
	       (unsigned long long)n_samples, (unsigned long long)samplerate, g_unitsize);

	if (!is_dso)
		printf(" \"vth\": %.2f,", vth_actual);

	printf(" \"channel_map\": %s,"
	       " \"trigger\": {\"enabled\": %s, \"channel\": %d,"
	       "               \"type\": \"%s\", \"pos_pct\": %d},"
	       " \"file\": \"%s\","
	       " \"meta\": \"%s.meta.json\"}\n",
	       ch_map,
	       trig_en ? "true" : "false", g_trig_ch, g_trig_type, g_trig_pos,
	       outfile, outfile);

	ds_lib_exit();
	return 0;
}

/* -------------------------------------------------------------------------
 * Argument parsing
 * ------------------------------------------------------------------------- */

static uint64_t parse_si(const char *s)
{
	char *end;
	double val = strtod(s, &end);
	if (*end == 'k' || *end == 'K')
		val *= 1e3;
	else if (*end == 'M' || *end == 'm')
		val *= 1e6;
	else if (*end == 'G' || *end == 'g')
		val *= 1e9;
	return (uint64_t) val;
}

/* Parse "CH:VALUE" into array[CH] = VALUE, e.g. "0:500" -> arr[0]=500 */
static int parse_ch_uint64(const char *s, uint64_t arr[2])
{
	int ch;
	uint64_t val;
	if (sscanf(s, "%d:%llu", &ch, (unsigned long long *)&val) == 2) {
		if (ch < 0 || ch > 1) {
			fprintf(stderr, "Error: channel index %d out of range (0-1)\n",
				ch);
			return -1;
		}
		arr[ch] = val;
		return 0;
	}
	fprintf(stderr, "Error: expected CH:VALUE format, got '%s'\n", s);
	return -1;
}

/* Parse "CH:MODE" for coupling, e.g. "0:DC" -> arr[0]=0, "1:AC" -> arr[1]=1 */
static int parse_ch_coupling(const char *s, int arr[2])
{
	char mode[16];
	int ch;
	if (sscanf(s, "%d:%15s", &ch, mode) == 2) {
		if (ch < 0 || ch > 1) {
			fprintf(stderr, "Error: channel index %d out of range (0-1)\n",
				ch);
			return -1;
		}
		if (!strcasecmp(mode, "DC"))
			arr[ch] = 0;
		else if (!strcasecmp(mode, "AC"))
			arr[ch] = 1;
		else {
			fprintf(stderr, "Error: unknown coupling '%s' (use DC or AC)\n",
				mode);
			return -1;
		}
		return 0;
	}
	fprintf(stderr, "Error: expected CH:MODE format, got '%s'\n", s);
	return -1;
}

/* Parse "0,1,4,7" into g_enabled_chs / g_n_enabled_chs */
static void parse_channels(const char *s)
{
	char buf[256];
	strncpy(buf, s, sizeof(buf) - 1);
	char *tok = strtok(buf, ",");
	while (tok && g_n_enabled_chs < MAX_CH) {
		int ch = atoi(tok);
		if (ch >= 0 && ch < MAX_CH)
			g_enabled_chs[g_n_enabled_chs++] = ch;
		tok = strtok(NULL, ",");
	}
}

/* Parse "SDA,SCL,TX,RX" into g_ch_names (parallel to g_enabled_chs) */
static void parse_names(const char *s)
{
	char buf[512];
	strncpy(buf, s, sizeof(buf) - 1);
	char *tok = strtok(buf, ",");
	int i = 0;
	while (tok && i < MAX_CH) {
		strncpy(g_ch_names[i], tok, sizeof(g_ch_names[i]) - 1);
		i++;
		tok = strtok(NULL, ",");
	}
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s scan\n"
		"      List connected DreamSourceLab devices.\n\n"
		"  %s info [-d|--dev N]\n"
		"      Show capabilities (channels, samplerates, trigger types).\n\n"
		"  %s capture [options] -o|--out FILE\n"
		"    -d, --dev N            Device index (default 0)\n"
		"    -s, --samplerate RATE  e.g. 1M, 10M, 100M (default 1M)\n"
		"    -n, --samples COUNT    e.g. 100k, 1M (default 1M)\n"
		"    -c, --enable-chs LIST  Comma-sep channel indices to enable (default 0-15)\n"
		"                           e.g.  -c 0,1,4,7\n"
		"    -N, --ch-names LIST    Comma-sep names matching --enable-chs order\n"
		"                           e.g.  -N SDA,SCL,TX,RX\n"
		"    -t, --trig-ch N        Channel to trigger on (default -1 = free-run)\n"
		"    -T, --trig-type TYPE   rising|falling|high|low|none (default none)\n"
		"    -p, --trig-pos PCT     Pre-trigger %% 0-100 (default 50)\n"
		"    -V, --vth VOLTS        Voltage threshold 0.0-5.0 (logic analyzers only)\n"
		"    -o, --out FILE         Output binary file path\n"
		"\n  DSO/oscilloscope options (DSCope devices):\n"
		"    --vdiv CH:VAL          Voltage per division in mV. CH is 0 or 1.\n"
		"                           e.g. --vdiv 0:500 --vdiv 1:1000\n"
		"                           Valid: 10,20,50,100,200,500,1000,2000\n"
		"    --coupling CH:MODE     DC or AC coupling. CH is 0 or 1.\n"
		"                           e.g. --coupling 0:DC --coupling 1:AC\n"
		"    --probe CH:FACTOR      Probe attenuation factor. CH is 0 or 1.\n"
		"                           e.g. --probe 0:1 --probe 1:10\n"
		"                           Valid: 1,2,10,20\n"
		"\n    -h, --help             Show this help message\n",
		prog, prog, prog);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}
	const char *cmd = argv[1];

	/* Defaults */
	int dev_index = 0;
	uint64_t samplerate = 1000000ULL;
	uint64_t limit_samples = 1000000ULL;
	char outfile[512] = "/tmp/dsview_capture.bin";

	/* Default: all 16 channels */
	for (int i = 0; i < MAX_CH; i++) {
		g_enabled_chs[i] = i;
		g_ch_names[i][0] = '\0';
	}
	g_n_enabled_chs = MAX_CH;

	/* Long-only options use IDs >= 256 to avoid clashing with short opts */
	enum { OPT_VDIV = 256, OPT_COUPLING, OPT_PROBE };

	static const struct option long_options[] = {
		{ "dev", required_argument, NULL, 'd' },
		{ "samplerate", required_argument, NULL, 's' },
		{ "samples", required_argument, NULL, 'n' },
		{ "enable-chs", required_argument, NULL, 'c' },
		{ "ch-names", required_argument, NULL, 'N' },
		{ "trig-ch", required_argument, NULL, 't' },
		{ "trig-type", required_argument, NULL, 'T' },
		{ "trig-pos", required_argument, NULL, 'p' },
		{ "out", required_argument, NULL, 'o' },
		{ "vth", required_argument, NULL, 'V' },
		{ "vdiv", required_argument, NULL, OPT_VDIV },
		{ "coupling", required_argument, NULL, OPT_COUPLING },
		{ "probe", required_argument, NULL, OPT_PROBE },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	optind = 2;		/* skip argv[0] (program) and argv[1] (subcommand) */
	int opt;
	while ((opt = getopt_long(argc, argv, "d:s:n:c:N:t:T:p:o:V:h",
				  long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			dev_index = atoi(optarg);
			break;
		case 's':
			samplerate = parse_si(optarg);
			break;
		case 'n':
			limit_samples = parse_si(optarg);
			break;
		case 'c':
			g_n_enabled_chs = 0;
			memset(g_ch_names, 0, sizeof(g_ch_names));
			parse_channels(optarg);
			break;
		case 'N':
			parse_names(optarg);
			break;
		case 't':
			g_trig_ch = atoi(optarg);
			break;
		case 'T':
			strncpy(g_trig_type, optarg, sizeof(g_trig_type) - 1);
			break;
		case 'p':
			g_trig_pos = atoi(optarg);
			break;
		case 'o':
			strncpy(outfile, optarg, sizeof(outfile) - 1);
			break;
		case 'V':
			g_vth = atof(optarg);
			if (g_vth < 0.0) {
				fprintf(stderr, "Error: --vth must be >= 0 (got %.2f)\n",
					g_vth);
				return 1;
			}
			break;
		case OPT_VDIV:
			if (parse_ch_uint64(optarg, g_vdiv) != 0)
				return 1;
			break;
		case OPT_COUPLING:
			if (parse_ch_coupling(optarg, g_coupling) != 0)
				return 1;
			break;
		case OPT_PROBE:
			if (parse_ch_uint64(optarg, g_probe_factor) != 0)
				return 1;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (!strcmp(cmd, "scan")) {
		return cmd_scan();
	} else if (!strcmp(cmd, "info")) {
		return cmd_info(dev_index);
	} else if (!strcmp(cmd, "capture")) {
		return cmd_capture(dev_index, samplerate, limit_samples, outfile);
	} else {
		fprintf(stderr, "Unknown command: %s\n\n", cmd);
		usage(argv[0]);
		return 1;
	}
}
