/*
 * dslcap: headless DSLogic capture on libsigrok4DSL (GPL-3.0, as DSView).
 *
 *   dslcap --list
 *   dslcap --channels 0,1 --samplerate 10000000 --samples 1000000
 *          [--vth 1.6] [--mode buffer|stream] [--trigger CH[:R|F|C|1|0]]
 *          [--trigpos PERCENT] [--timeout SEC] [--res DIR] --out /path/base
 *
 * Writes <base>.bin: for each enabled channel in ascending order, the
 * channel's samples packed LSB-first, ceil(samples/64)*8 bytes per channel.
 * Prints one JSON object describing the capture on stdout; every failure
 * prints an object with an "error" key instead, and <base>.bin is only
 * created for a complete capture.
 *
 * Exit status: 0 success, 1 runtime or I/O error, 2 invalid arguments or
 * unavailable settings, 3 the capture itself failed.
 */
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#include <glib.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include "libsigrok.h"

/* The spool and the output may exceed 2 GiB. */
G_STATIC_ASSERT(sizeof(off_t) >= 8);

#define MAX_CHANNELS 64
#define STOP_GRACE_US (5 * G_TIME_SPAN_SECOND)
#define POLL_US 2000

/*
 * Capture state. The library delivers data and events on its own threads,
 * so everything below is guarded by g_lock (a pthread mutex, as the library
 * itself uses).
 */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_done = 0;                   /* main thread should stop waiting */
static int g_task_end = 0;               /* terminal collect-task event seen */
static int g_err = 0;                    /* DS_EV_COLLECT_TASK_END_BY_ERROR/_DETACHED */
static int g_data_end = 0;               /* SR_DF_END seen */
static int g_pkt_error = 0;              /* packet status or logic data error */
static int g_overflow = 0;               /* SR_DF_OVERFLOW seen */
static FILE *g_raw = NULL;               /* disk spool of logic payload */
static uint64_t g_raw_bytes = 0;
static int g_io_error = 0;
static int g_format = -1;
static long long g_trig_pos = -1;
static int g_split_seen = 0;

static void on_event(int ev)
{
    pthread_mutex_lock(&g_lock);
    /* SR_DF_END and DS_EV_DEVICE_STOPPED come before the final status;
     * only the collect-task end events say how the capture finished. */
    if (ev == DS_EV_COLLECT_TASK_END ||
            ev == DS_EV_COLLECT_TASK_END_BY_ERROR ||
            ev == DS_EV_COLLECT_TASK_END_BY_DETACHED) {
        if (ev != DS_EV_COLLECT_TASK_END)
            g_err = ev;
        g_task_end = 1;
        g_done = 1;
    }
    pthread_mutex_unlock(&g_lock);
}

static void on_data(const struct sr_dev_inst *sdi, const struct sr_datafeed_packet *p)
{
    (void)sdi;
    pthread_mutex_lock(&g_lock);
    if (p->status != SR_PKT_OK) {
        g_pkt_error = 1;
        g_done = 1;
        if (p->type == SR_DF_END)
            g_data_end = 1;
    } else if (p->type == SR_DF_LOGIC) {
        const struct sr_datafeed_logic *l = p->payload;
        if (l->data_error) {
            g_pkt_error = 1;
            g_done = 1;
        } else {
            g_format = l->format;
            if (l->format == LA_SPLIT_DATA)
                g_split_seen = 1;
            if (g_raw && !g_io_error) {
                if (fwrite(l->data, 1, l->length, g_raw) != l->length) {
                    g_io_error = 1;
                    g_done = 1;
                } else {
                    g_raw_bytes += l->length;
                }
            }
        }
    } else if (p->type == SR_DF_TRIGGER && p->payload) {
        const struct ds_trigger_pos *t = p->payload;
        g_trig_pos = t->real_pos;
    } else if (p->type == SR_DF_OVERFLOW) {
        g_overflow = 1;
        g_done = 1;
    } else if (p->type == SR_DF_END) {
        g_data_end = 1;
    }
    pthread_mutex_unlock(&g_lock);
}

/* Wait until a callback sets *flag or the monotonic deadline passes. */
static int wait_flag(const int *flag, gint64 deadline)
{
    for (;;) {
        pthread_mutex_lock(&g_lock);
        int set = *flag;
        pthread_mutex_unlock(&g_lock);
        gint64 now = g_get_monotonic_time();
        if (set || now >= deadline)
            return set;
        g_usleep(MIN(POLL_US, deadline - now));
    }
}

static void json_str(const char *s)
{
    putchar('"');
    for (; *s; s++) {
        if ((unsigned char)*s < 0x20)
            printf("\\u%04x", (unsigned char)*s);
        else {
            if (*s == '"' || *s == '\\') putchar('\\');
            putchar(*s);
        }
    }
    putchar('"');
}

/* The JSON result is the tool's output; failing to deliver it is an error. */
static int finish_stdout(int rc)
{
    if (fflush(stdout) || ferror(stdout)) {
        fprintf(stderr, "dslcap: cannot write the result to stdout\n");
        return rc ? rc : 1;
    }
    return rc;
}

static void arg_error(const char *what, const char *option, const char *value)
{
    printf("{\"error\":");
    json_str(what);
    if (option) {
        printf(",\"option\":");
        json_str(option);
    }
    if (value) {
        printf(",\"value\":");
        json_str(value);
    }
    printf("}\n");
}

/* Whole-string unsigned decimal; no sign, whitespace or trailing text. */
static int parse_u64(const char *s, uint64_t *out)
{
    char *end;
    if (!g_ascii_isdigit(*s)) return -1;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno || *end) return -1;
    *out = v;
    return 0;
}

/* Whole-string finite number, independent of the locale. */
static int parse_double(const char *s, double *out)
{
    char *end;
    if (!*s || g_ascii_isspace(*s)) return -1;
    errno = 0;
    double v = g_ascii_strtod(s, &end);
    if (errno || end == s || *end || !isfinite(v)) return -1;
    *out = v;
    return 0;
}

struct options {
    const char *res, *out, *chans, *mode, *trig;
    uint64_t rate, samples;
    double vth, timeout;
    int trigpos, list_only, stream, vth_given;
    int enabled[MAX_CHANNELS], nch;
    int trig_ch;
    char trig_type;
};

/* Validates every option before the library or the device is touched.
 * Returns 0, or 2 after printing a JSON error. */
static int parse_args(int argc, char **argv, struct options *o)
{
    uint64_t u;
    memset(o, 0, sizeof *o);
    o->res = getenv("DSLCAP_RES");
    o->chans = "0";
    o->mode = "buffer";
    o->rate = 10000000;
    o->samples = 1000000;
    o->vth = 1.6;
    o->timeout = 30;
    o->trigpos = 10;
    o->trig_ch = -1;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--list")) {
            o->list_only = 1;
            continue;
        }
        static const char *const valued[] = {
            "--res", "--out", "--channels", "--samplerate", "--samples", "--vth",
            "--mode", "--trigger", "--trigpos", "--timeout",
        };
        int known = 0;
        for (size_t k = 0; k < G_N_ELEMENTS(valued); k++)
            if (!strcmp(a, valued[k])) known = 1;
        if (!known) {
            arg_error("unknown argument", a, NULL);
            return 2;
        }
        if (i + 1 >= argc) {
            arg_error("missing option value", a, NULL);
            return 2;
        }
        const char *v = argv[++i];
        int bad = 0;
        if (!strcmp(a, "--res")) o->res = v;
        else if (!strcmp(a, "--out")) o->out = v;
        else if (!strcmp(a, "--channels")) o->chans = v;
        else if (!strcmp(a, "--mode")) o->mode = v;
        else if (!strcmp(a, "--trigger")) o->trig = v;
        else if (!strcmp(a, "--samplerate")) bad = parse_u64(v, &o->rate) || o->rate == 0;
        else if (!strcmp(a, "--samples"))
            /* The driver rounds the limit up to SAMPLES_ALIGN + 1. */
            bad = parse_u64(v, &o->samples) || o->samples == 0 ||
                  o->samples > UINT64_MAX - SAMPLES_ALIGN;
        else if (!strcmp(a, "--vth")) {
            /* The range DSView offers for the threshold voltage. */
            bad = parse_double(v, &o->vth) || o->vth < 0.0 || o->vth > 5.0;
            o->vth_given = 1;
        }
        else if (!strcmp(a, "--trigpos")) {
            bad = parse_u64(v, &u) || u > 100;
            if (!bad) o->trigpos = (int)u;
        } else if (!strcmp(a, "--timeout"))
            bad = parse_double(v, &o->timeout) || o->timeout <= 0 || o->timeout > 1e9;
        if (bad) {
            arg_error("invalid option value", a, v);
            return 2;
        }
    }

    if (strcmp(o->mode, "buffer") && strcmp(o->mode, "stream")) {
        arg_error("invalid option value", "--mode", o->mode);
        return 2;
    }
    o->stream = !strcmp(o->mode, "stream");
    if (o->list_only)
        return 0;
    /* The driver converts trigpos% of the (aligned) sample limit to a 32-bit
     * trigger position before applying its depth limit, so keep it in range. */
    if (o->trigpos > 0 &&
        o->samples + SAMPLES_ALIGN > (uint64_t)UINT32_MAX * 100 / (uint64_t)o->trigpos) {
        printf("{\"error\":\"--trigpos too large for --samples\",\"samples\":%llu,\"trigpos\":%d}\n",
               (unsigned long long)o->samples, o->trigpos);
        return 2;
    }
    if (!o->out) {
        arg_error("--out is required", NULL, NULL);
        return 2;
    }

    /* Comma-separated channel indexes, each a plain decimal number. */
    const char *s = o->chans;
    for (;;) {
        const char *comma = strchr(s, ',');
        size_t len = comma ? (size_t)(comma - s) : strlen(s);
        char token[8];
        int bad = len == 0 || len >= sizeof token;
        if (!bad) {
            memcpy(token, s, len);
            token[len] = '\0';
            bad = parse_u64(token, &u) || u >= MAX_CHANNELS;
        }
        if (bad) {
            arg_error("invalid option value", "--channels", o->chans);
            return 2;
        }
        for (int i = 0; i < o->nch; i++) {
            if (o->enabled[i] == (int)u) {
                printf("{\"error\":\"duplicate channel\",\"channel\":%d}\n", (int)u);
                return 2;
            }
        }
        if (o->nch == MAX_CHANNELS) {
            printf("{\"error\":\"too many channels\"}\n");
            return 2;
        }
        o->enabled[o->nch++] = (int)u;
        if (!comma) break;
        s = comma + 1;
    }

    /* CH, or CH:T with T exactly one of R F C 1 0. */
    if (o->trig) {
        const char *colon = strchr(o->trig, ':');
        size_t len = colon ? (size_t)(colon - o->trig) : strlen(o->trig);
        char token[8];
        int bad = len == 0 || len >= sizeof token;
        if (!bad) {
            memcpy(token, o->trig, len);
            token[len] = '\0';
            bad = parse_u64(token, &u) || u >= MaxTriggerProbes;
        }
        o->trig_type = 'R';
        if (!bad && colon) {
            o->trig_type = colon[1];
            bad = !o->trig_type || !strchr("RFC10", o->trig_type) || colon[2];
        }
        if (bad) {
            arg_error("invalid option value", "--trigger", o->trig);
            return 2;
        }
        o->trig_ch = (int)u;
    }
    return 0;
}

static int pick_device(int list_only)
{
    struct ds_device_base_info *list = NULL;
    int count = 0, pick = -1;
    if (ds_get_device_list(&list, &count) != SR_OK) {
        g_free(list);
        return -3;
    }
    if (list_only) printf("{\"devices\":[");
    for (int i = 0; i < count; i++) {
        if (list_only) {
            printf("%s", i ? "," : "");
            json_str(list[i].name);
        }
        if (pick < 0 && strstr(list[i].name, "DSLogic"))
            pick = i;
    }
    if (list_only) printf("]}\n");
    if (pick >= 0 && !list_only) {
        if (ds_active_device(list[pick].handle) != SR_OK)
            pick = -2;
    }
    g_free(list);
    return pick;
}

static int set_u64(int key, uint64_t v) { return ds_set_actived_device_config(NULL, NULL, key, g_variant_new_uint64(v)); }

/* Read back a configuration value of the expected type. */
static GVariant *get_config(int key, const GVariantType *type)
{
    GVariant *gv = NULL;
    if (ds_get_actived_device_config(NULL, NULL, key, &gv) != SR_OK || !gv)
        return NULL;
    if (!g_variant_is_of_type(gv, type)) {
        g_variant_unref(gv);
        return NULL;
    }
    return gv;
}

/* Whether the active device lists key among its SR_CONF_DEVICE_OPTIONS. */
static int device_has_option(int key)
{
    GVariant *gv = NULL;
    int found = 0;
    if (ds_get_actived_device_config_list(NULL, SR_CONF_DEVICE_OPTIONS, &gv) != SR_OK || !gv)
        return 0;
    if (g_variant_is_of_type(gv, G_VARIANT_TYPE("ai"))) {
        gsize n = 0;
        const gint32 *keys = g_variant_get_fixed_array(gv, &n, sizeof(gint32));
        for (gsize i = 0; i < n; i++)
            if (keys[i] == key) found = 1;
    }
    g_variant_unref(gv);
    return found;
}

static void config_error(const char *setting, int code)
{
    printf("{\"error\":\"device configuration failed\",\"setting\":\"%s\",\"code\":%d}\n",
           setting, code);
}

/* Keep the channel-major file layout, using fixed-size conversion buffers. */
static int write_output(const char *path, int nch, uint64_t per_ch, uint64_t got)
{
    uint64_t raw[4096], channel[4096];
    if (nch <= 0 || nch > (int)G_N_ELEMENTS(raw)) return -1;
    /* Every channel's last byte must be addressable as an off_t. */
    if (per_ch > (uint64_t)G_MAXINT64 / 8 / (uint64_t)nch) return -1;
    if (fflush(g_raw) || fseeko(g_raw, 0, SEEK_SET)) return -1;
    /* Write to a temporary file and publish it only on success. */
    char *tmp = g_strdup_printf("%s.XXXXXX", path);
    int fd = g_mkstemp(tmp);
    if (fd < 0) { g_free(tmp); return -1; }
    FILE *f = fdopen(fd, "wb");
    if (!f) { close(fd); unlink(tmp); g_free(tmp); return -1; }
    int failed = 0;
    for (uint64_t k = 0; k < per_ch && !failed;) {
        size_t count = MIN(per_ch - k, G_N_ELEMENTS(raw) / nch);
        if (fread(raw, sizeof(uint64_t) * nch, count, g_raw) != count) {
            failed = 1;
            break;
        }
        for (int c = 0; c < nch; c++) {
            for (size_t i = 0; i < count; i++) channel[i] = raw[i * nch + c];
            /* Clear samples past the requested count in the last word. */
            if (k + count == per_ch && got % 64)
                channel[count - 1] &= (1ULL << (got % 64)) - 1;
            off_t offset = (off_t)(((uint64_t)c * per_ch + k) * 8);
            if (fseeko(f, offset, SEEK_SET) || fwrite(channel, 8, count, f) != count) {
                failed = 1;
                break;
            }
        }
        k += count;
    }
    if (fclose(f)) failed = 1;
    /* link() publishes atomically and, unlike rename(), refuses to replace an existing capture. */
    if (!failed && link(tmp, path)) failed = 1;
    unlink(tmp);  /* after link() the published name keeps the data; drop the temporary one either way */
    g_free(tmp);
    return failed ? -1 : 0;
}

/* Why a finished acquisition must not be published, or NULL if it may. */
static const char *capture_failure(int timed_out, int task_end, int err, int pkt_error,
                                   int overflow, uint64_t got, uint64_t samples)
{
    if (timed_out) return "capture timed out";
    if (!task_end) return "capture did not finish";
    if (err == DS_EV_COLLECT_TASK_END_BY_DETACHED) return "device detached during capture";
    if (err) return "capture ended with a device error";
    if (pkt_error) return "device reported a data error";
    if (overflow) return "device buffer overflow";
    if (got == 0) return "no samples captured";
    if (got < samples) return "capture ended before the requested sample count";
    return NULL;
}

static int set_samplerate(uint64_t rate)
{
    GVariant *dict = NULL;
    if (ds_get_actived_device_config_list(NULL, SR_CONF_SAMPLERATE, &dict) != SR_OK || !dict)
        return SR_ERR;
    GVariant *rates = g_variant_lookup_value(dict, "samplerates", G_VARIANT_TYPE("at"));
    g_variant_unref(dict);
    if (!rates)
        return SR_ERR;

    gsize count;
    const uint64_t *values = g_variant_get_fixed_array(rates, &count, sizeof(uint64_t));
    int supported = 0;
    for (gsize i = 0; i < count; i++)
        if (values[i] == rate) { supported = 1; break; }
    g_variant_unref(rates);

    /* The driver stores arbitrary rates but rounds the hardware divider. */
    return supported ? set_u64(SR_CONF_SAMPLERATE, rate) : SR_ERR_ARG;
}

static int select_channel_mode(uint64_t rate)
{
    GVariant *gv = NULL;
    if (ds_get_actived_device_config_list(NULL, SR_CONF_CHANNEL_MODE, &gv) != SR_OK || !gv)
        return SR_ERR;
    const struct sr_list_item *modes = (const struct sr_list_item *)g_variant_get_uint64(gv);
    g_variant_unref(gv);

    /* The driver lists modes from most channels to fewest. */
    for (; modes && modes->id >= 0; modes++) {
        if (ds_set_actived_device_config(NULL, NULL, SR_CONF_CHANNEL_MODE,
                g_variant_new_int16(modes->id)) != SR_OK)
            return SR_ERR;
        gv = NULL;
        if (ds_get_actived_device_config_list(NULL, SR_CONF_SAMPLERATE, &gv) != SR_OK || !gv)
            return SR_ERR;
        GVariant *rates = g_variant_lookup_value(gv, "samplerates", G_VARIANT_TYPE("at"));
        g_variant_unref(gv);
        if (!rates) return SR_ERR;
        gsize count = 0;
        const uint64_t *values = g_variant_get_fixed_array(rates, &count, sizeof(uint64_t));
        int found = 0;
        for (gsize i = 0; i < count; i++)
            if (values[i] == rate) found = 1;
        g_variant_unref(rates);
        if (found) return SR_OK;
    }
    return SR_ERR;
}

static int configure_trigger(const struct options *o)
{
    int rc;
    if ((rc = ds_trigger_reset()) != SR_OK) { config_error("trigger", rc); return rc; }
    if ((rc = ds_trigger_set_mode(SIMPLE_TRIGGER)) != SR_OK) { config_error("trigger_mode", rc); return rc; }
    if ((rc = ds_trigger_set_pos((uint16_t)o->trigpos)) != SR_OK) { config_error("trigpos", rc); return rc; }
    if (o->trig && (rc = ds_trigger_probe_set((uint16_t)o->trig_ch,
            (unsigned char)o->trig_type, 'X')) != SR_OK) {
        config_error("trigger", rc);
        return rc;
    }
    if ((rc = ds_trigger_set_en(o->trig ? TRUE : FALSE)) != SR_OK) { config_error("trigger_enable", rc); return rc; }
    return SR_OK;
}

/* Firmware directory: next to this executable (app bundle Contents/Resources/res,
 * build tree DSView/res, installed ../share/DSView/res), then the configured
 * install location. */
static char *default_res_dir(void)
{
    char exe[4096] = {0};
    char *dir = NULL;
#ifdef __APPLE__
    uint32_t size = sizeof exe;
    if (_NSGetExecutablePath(exe, &size) != 0) exe[0] = '\0';
#else
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n < 0 || n >= (ssize_t)sizeof exe - 1) exe[0] = '\0';
#endif
    char *real = exe[0] ? realpath(exe, NULL) : NULL;
    if (real) {
        dir = g_path_get_dirname(real);
        free(real);
    }
    const char *candidates[] = { "../Resources/res", "../DSView/res", "res", "../share/DSView/res" };
    char *found = NULL;
    for (size_t i = 0; dir && i < G_N_ELEMENTS(candidates) && !found; i++) {
        char *path = g_build_filename(dir, candidates[i], NULL);
        if (g_file_test(path, G_FILE_TEST_IS_DIR)) found = path;
        else g_free(path);
    }
    g_free(dir);
#ifdef DSLCAP_INSTALL_RES_DIR
    if (!found && g_file_test(DSLCAP_INSTALL_RES_DIR, G_FILE_TEST_IS_DIR))
        found = g_strdup(DSLCAP_INSTALL_RES_DIR);
#endif
    return found;
}

struct report {
    const char *device;
    uint64_t rate, samples, got, per_ch, limit;
    const int *channels;
    int nch;
    double vth;
    const char *mode, *trig;
    int format;
    long long trig_pos;
    int timed_out, err, pkt_error, overflow;
    double secs;
    const char *bin;
};

/* One JSON object; error, when set, makes it a failure report without a file. */
static void print_report(const char *error, const struct report *r)
{
    printf("{");
    if (error) {
        printf("\"error\":");
        json_str(error);
        printf(",");
    }
    printf("\"device\":");
    json_str(r->device);
    printf(",\"samplerate\":%llu,\"samples_requested\":%llu,\"samples\":%llu,\"words_per_channel\":%llu,"
           "\"channels\":[", (unsigned long long)r->rate, (unsigned long long)r->samples,
           (unsigned long long)r->got, (unsigned long long)r->per_ch);
    for (int i = 0; i < r->nch; i++) printf("%s%d", i ? "," : "", r->channels[i]);
    /* vth is null on devices without a threshold voltage. */
    if (isfinite(r->vth)) printf("],\"vth\":%.3f,\"mode\":", r->vth);
    else printf("],\"vth\":null,\"mode\":");
    json_str(r->mode);
    printf(",\"format\":\"%s\",\"trigger\":", r->format == LA_SPLIT_DATA ? "split" : "cross");
    if (r->trig) json_str(r->trig); else printf("null");
    printf(",\"trigger_pos\":%lld,\"timed_out\":%s,\"lib_error_event\":%d,\"elapsed_s\":%.3f,\"bin\":",
           r->trig_pos, r->timed_out ? "true" : "false", r->err, r->secs);
    if (r->bin) json_str(r->bin); else printf("null");
    printf(",\"limit_samples\":%llu,\"packet_error\":%s,\"overflow\":%s}\n",
           (unsigned long long)r->limit, r->pkt_error ? "true" : "false",
           r->overflow ? "true" : "false");
}

int main(int argc, char **argv)
{
    struct options o;
    int rc = parse_args(argc, argv, &o);
    if (rc) return finish_stdout(rc);

    char *res_found = NULL;
    const char *res = o.res;
    if (!res) res = res_found = default_res_dir();
    if (!res) {
        printf("{\"error\":\"firmware directory not found; set --res or DSLCAP_RES\"}\n");
        return finish_stdout(2);
    }
    if (strlen(res) >= 500) {
        printf("{\"error\":\"firmware directory path is too long (limit 499 bytes)\"}\n");
        g_free(res_found);
        return finish_stdout(2);
    }

    uint64_t hw_samples = o.samples;
    if (!o.stream) {
        /* Buffer delivery is aligned to 1024 samples in the driver. */
        hw_samples = (o.samples + SAMPLES_ALIGN) & ~SAMPLES_ALIGN;
    }

    ds_log_level(1);
    ds_set_firmware_resource_dir(res);
    g_free(res_found);
    ds_set_event_callback(on_event);
    ds_set_datafeed_callback(on_data);
    if (ds_lib_init() != SR_OK) { printf("{\"error\":\"lib init failed\"}\n"); return finish_stdout(1); }

    int dev = pick_device(o.list_only);
    if (dev == -3) {
        printf("{\"error\":\"cannot list devices\"}\n");
        ds_lib_exit();
        return finish_stdout(1);
    }
    if (o.list_only) { ds_lib_exit(); return finish_stdout(0); }
    if (dev < 0) {
        printf("{\"error\":\"%s\"}\n", dev == -1 ? "no DSLogic found" : "device activation failed");
        ds_lib_exit();
        return finish_stdout(1);
    }

    struct ds_device_full_info info;
    memset(&info, 0, sizeof info);
    ds_get_actived_device_info(&info);
    info.name[sizeof info.name - 1] = '\0';

    /* Mode first: it changes the channel mode and the allowed rates. */
    if (ds_set_actived_device_config(NULL, NULL, SR_CONF_OPERATION_MODE,
            g_variant_new_int16(o.stream ? LO_OP_STREAM : LO_OP_BUFFER)) != SR_OK ||
            select_channel_mode(o.rate) != SR_OK) {
        printf("{\"error\":\"samplerate unavailable in operation mode\",\"samplerate\":%llu}\n",
               (unsigned long long)o.rate);
        ds_lib_exit();
        return finish_stdout(2);
    }
    GVariant *gv = get_config(SR_CONF_VLD_CH_NUM, G_VARIANT_TYPE_INT16);
    if (!gv) {
        printf("{\"error\":\"cannot read channel limit\"}\n");
        ds_lib_exit();
        return finish_stdout(1);
    }
    int max_channels = g_variant_get_int16(gv);
    g_variant_unref(gv);

    for (GSList *l = ds_get_actived_device_channels(); l; l = l->next)
        ds_enable_device_channel(l->data, FALSE);
    for (int i = 0; i < o.nch; i++) {
        /* The selected channel mode limits which channels exist at this rate. */
        if (i >= max_channels || ds_enable_device_channel_index(o.enabled[i], TRUE) != SR_OK) {
            printf("{\"error\":\"channel/rate combination unavailable\",\"channel\":%d,\"samplerate\":%llu,\"max_channels\":%d}\n",
                   o.enabled[i], (unsigned long long)o.rate, max_channels);
            ds_lib_exit();
            return finish_stdout(2);
        }
    }

    /* Devices with a threshold voltage list SR_CONF_VTH; older ones only have a
     * 3.3 V / 5 V threshold setting, which is left as it is. */
    int has_vth = device_has_option(SR_CONF_VTH);
    if (!has_vth && o.vth_given) {
        printf("{\"error\":\"this device has no threshold voltage setting\",\"device\":");
        json_str(info.name);
        printf("}\n");
        ds_lib_exit();
        return finish_stdout(2);
    }
    if (has_vth && (rc = ds_set_actived_device_config(NULL, NULL, SR_CONF_VTH, g_variant_new_double(o.vth))) != SR_OK) {
        config_error("vth", rc);
        ds_lib_exit();
        return finish_stdout(1);
    }
    int rate_rc = set_samplerate(o.rate);
    if (rate_rc != SR_OK) {
        printf("{\"error\":\"%s\",\"samplerate\":%llu}\n",
            rate_rc == SR_ERR_ARG ? "unsupported samplerate" : "samplerate configuration failed",
            (unsigned long long)o.rate);
        ds_lib_exit();
        return finish_stdout(rate_rc == SR_ERR_ARG ? 2 : 1);
    }
    if ((rc = set_u64(SR_CONF_LIMIT_SAMPLES, hw_samples)) != SR_OK) {
        config_error("samples", rc);
        ds_lib_exit();
        return finish_stdout(1);
    }

    int enabled[MAX_CHANNELS], nch = 0;
    for (GSList *l = ds_get_actived_device_channels(); l; l = l->next) {
        const struct sr_channel *ch = l->data;
        if (!ch->enabled) continue;
        if (nch == MAX_CHANNELS) {
            printf("{\"error\":\"too many enabled channels\"}\n");
            ds_lib_exit();
            return finish_stdout(1);
        }
        enabled[nch++] = ch->index;
    }
    /* ascending order, as the device sends them */
    for (int i = 0; i < nch; i++) for (int j = i + 1; j < nch; j++)
        if (enabled[j] < enabled[i]) { int t = enabled[i]; enabled[i] = enabled[j]; enabled[j] = t; }

    if (o.trig) {
        int trigger_enabled = 0;
        for (int i = 0; i < nch; i++)
            if (enabled[i] == o.trig_ch) trigger_enabled = 1;
        if (!trigger_enabled) {
            printf("{\"error\":\"trigger channel must be an enabled device channel\"}\n");
            ds_lib_exit();
            return finish_stdout(2);
        }
    }
    if (configure_trigger(&o) != SR_OK) {
        ds_lib_exit();
        return finish_stdout(1);
    }

    /* Report what the device holds, not what was requested. */
    GVariant *rate_gv = get_config(SR_CONF_SAMPLERATE, G_VARIANT_TYPE_UINT64);
    GVariant *limit_gv = get_config(SR_CONF_LIMIT_SAMPLES, G_VARIANT_TYPE_UINT64);
    GVariant *vth_gv = has_vth ? get_config(SR_CONF_VTH, G_VARIANT_TYPE_DOUBLE) : NULL;
    uint64_t act_rate = rate_gv ? g_variant_get_uint64(rate_gv) : 0;
    uint64_t act_limit = limit_gv ? g_variant_get_uint64(limit_gv) : 0;
    double act_vth = vth_gv ? g_variant_get_double(vth_gv) : NAN;
    const char *unread = !rate_gv ? "samplerate" : !limit_gv ? "samples" : has_vth && !vth_gv ? "vth" : NULL;
    if (rate_gv) g_variant_unref(rate_gv);
    if (limit_gv) g_variant_unref(limit_gv);
    if (vth_gv) g_variant_unref(vth_gv);
    if (unread || act_limit != hw_samples || (has_vth && !isfinite(act_vth))) {
        printf("{\"error\":\"cannot verify device configuration\",\"setting\":\"%s\"}\n",
               unread ? unread : act_limit != hw_samples ? "samples" : "vth");
        ds_lib_exit();
        return finish_stdout(1);
    }

    char *spool = g_strdup_printf("%s.raw-XXXXXX", o.out);
    int fd = g_mkstemp(spool);
    if (fd >= 0) {
        g_raw = fdopen(fd, "w+b");
        if (!g_raw) close(fd);
        unlink(spool);
    }
    g_free(spool);
    if (!g_raw) { printf("{\"error\":\"cannot create capture spool\"}\n"); ds_lib_exit(); return finish_stdout(1); }

    /* One monotonic clock for both the deadline and the reported time. */
    gint64 t0 = g_get_monotonic_time();
    gint64 deadline = t0 + (gint64)(o.timeout * G_TIME_SPAN_SECOND);
    rc = ds_start_collect();
    if (rc != SR_OK) {
        printf("{\"error\":\"start failed\",\"code\":%d}\n", rc);
        fclose(g_raw);
        ds_lib_exit();
        return finish_stdout(1);
    }
    int timed_out = !wait_flag(&g_done, deadline);

    /* Stop early on timeout or a reported error; either way wait for the
     * collect task's own end event before trusting the data and status. */
    if (ds_is_collecting()) ds_stop_collect();
    gint64 stop_deadline = g_get_monotonic_time() + STOP_GRACE_US;
    wait_flag(&g_task_end, stop_deadline);
    pthread_mutex_lock(&g_lock);
    int task_end = g_task_end, err = g_err, pkt_error = g_pkt_error, overflow = g_overflow;
    int io_error = g_io_error, format = g_split_seen ? LA_SPLIT_DATA : g_format;
    uint64_t raw_bytes = g_raw_bytes;
    long long trig_pos = g_trig_pos;
    pthread_mutex_unlock(&g_lock);
    gint64 t1 = g_get_monotonic_time();

    /* De-interleave LA_CROSS_DATA: 64-sample words rotate through channels. */
    uint64_t words = raw_bytes / 8;
    uint64_t per_ch = nch ? words / nch : 0;
    uint64_t got = per_ch > o.samples / 64 ? o.samples : per_ch * 64;
    per_ch = got / 64 + (got % 64 != 0);

    struct report r = {
        .device = info.name, .rate = act_rate, .samples = o.samples, .got = got,
        .per_ch = per_ch, .limit = act_limit, .channels = enabled, .nch = nch,
        .vth = act_vth, .mode = o.mode, .trig = o.trig, .format = format,
        .trig_pos = trig_pos, .timed_out = timed_out, .err = err,
        .pkt_error = pkt_error, .overflow = overflow,
        .secs = (t1 - t0) / (double)G_TIME_SPAN_SECOND, .bin = NULL,
    };
    const char *failure = io_error ? NULL : capture_failure(timed_out, task_end, err,
            pkt_error, overflow, got, o.samples);
    char *path = g_strdup_printf("%s.bin", o.out);
    int exit_code = 0;
    if (io_error || (!failure && write_output(path, nch, per_ch, got))) {
        printf("{\"error\":\"cannot write capture data\"}\n");
        exit_code = 1;
    } else if (failure) {
        print_report(failure, &r);
        exit_code = 3;
    } else {
        r.bin = path;
        print_report(NULL, &r);
    }
    g_free(path);

    /* Late callbacks, if the task never reported its end, must not see a closed spool. */
    pthread_mutex_lock(&g_lock);
    FILE *raw = g_raw;
    g_raw = NULL;
    pthread_mutex_unlock(&g_lock);
    fclose(raw);
    ds_release_actived_device();
    ds_lib_exit();
    return finish_stdout(exit_code);
}
