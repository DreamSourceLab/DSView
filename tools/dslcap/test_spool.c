/* Standalone regression harness; no analyzer is accessed. Dead-code
 * stripping drops the CLI's main and with it every hardware reference.
 *
 * macOS:
 *   cc -Ilibsigrok4DSL -Icommon $(pkg-config --cflags glib-2.0) tools/dslcap/test_spool.c \
 *      $(pkg-config --libs glib-2.0) -Wl,-dead_strip -o test_spool
 * Linux:
 *   cc -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections \
 *      -Ilibsigrok4DSL -Icommon $(pkg-config --cflags glib-2.0) tools/dslcap/test_spool.c \
 *      $(pkg-config --libs glib-2.0) -lm -Wl,--gc-sections -o test_spool
 *
 *   ./test_spool /tmp/raw /tmp/output.bin
 */
#define main dslcap_main
#include "dslcap.c"
#undef main
#undef NDEBUG
#include <assert.h>
#include <fcntl.h>

static void reset_state(void)
{
    g_done = g_task_end = g_err = g_data_end = g_pkt_error = g_overflow = 0;
    g_raw_bytes = 0;
    g_io_error = 0;
}

/* parse_args with its JSON error output discarded. */
static int parse(struct options *o, ...)
{
    char *argv[32] = { "dslcap" };
    int argc = 1;
    va_list ap;
    va_start(ap, o);
    for (char *a; (a = va_arg(ap, char *)) && argc < 31;) argv[argc++] = a;
    va_end(ap);
    fflush(stdout);
    int saved = dup(STDOUT_FILENO), null = open("/dev/null", O_WRONLY);
    dup2(null, STDOUT_FILENO);
    int rc = parse_args(argc, argv, o);
    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(null);
    close(saved);
    return rc;
}

static void test_arguments(void)
{
    struct options o;
    assert(parse(&o, "--out", "x", "--channels", "3,0,15", "--samplerate", "100000000",
                 "--samples", "64", "--vth", "2.5", "--mode", "stream", "--trigger", "15:F",
                 "--trigpos", "100", "--timeout", "0.5", NULL) == 0);
    assert(o.nch == 3 && o.enabled[0] == 3 && o.enabled[2] == 15 && o.stream);
    assert(o.rate == 100000000 && o.samples == 64 && o.vth == 2.5 && o.timeout == 0.5);
    assert(o.trig_ch == 15 && o.trig_type == 'F' && o.trigpos == 100);
    assert(parse(&o, "--out", "x", "--trigger", "0", NULL) == 0 && o.trig_type == 'R');
    assert(parse(&o, "--list", NULL) == 0 && o.list_only);
    /* trigpos% of the aligned sample limit must fit the driver's 32-bit position. */
    assert(parse(&o, "--out", "x", "--samples", "8589934592", "--trigpos", "100", NULL) == 2);
    assert(parse(&o, "--out", "x", "--samples", "4294000000", "--trigpos", "100", NULL) == 0);
    assert(parse(&o, "--out", "x", "--samples", "8589934592", "--trigpos", "10", NULL) == 0);
    assert(parse(&o, "--out", "x", "--samples", "8589934592", "--trigpos", "0", NULL) == 0);

    const char *bad[][2] = {
        {"--channels", "garbage"}, {"--channels", "1,,2"}, {"--channels", "1,"},
        {"--channels", "-1"}, {"--channels", "64"}, {"--channels", "1,1"},
        {"--channels", " 1"}, {"--mode", "garbage"}, {"--mode", "Stream"},
        {"--trigger", "0:garbage"}, {"--trigger", "0:Rx"}, {"--trigger", "0:"},
        {"--trigger", "0:r"}, {"--trigger", "32"}, {"--trigger", ":R"}, {"--trigger", "x:R"},
        {"--trigpos", "101"}, {"--trigpos", "-1"}, {"--trigpos", "10%"},
        {"--vth", "nan"}, {"--vth", "inf"}, {"--vth", "5.1"}, {"--vth", "-0.1"},
        {"--vth", "1.6V"}, {"--vth", ""}, {"--samples", "0"}, {"--samples", "-5"},
        {"--samples", "1e6"}, {"--samples", "18446744073709551616"},
        {"--samples", "18446744073709551615"}, {"--samplerate", "0"},
        {"--samplerate", "10M"}, {"--timeout", "0"}, {"--timeout", "nan"},
        {"--timeout", "-1"},
    };
    for (size_t i = 0; i < G_N_ELEMENTS(bad); i++)
        assert(parse(&o, "--out", "x", bad[i][0], bad[i][1], NULL) == 2);
    assert(parse(&o, "--channels", "0", NULL) == 2);           /* no --out */
    assert(parse(&o, "--out", NULL) == 2);                     /* missing value */
    assert(parse(&o, "--out", "x", "--bogus", NULL) == 2);
}

static void test_status(void)
{
    struct ds_trigger_pos t = {.real_pos = 7};
    struct sr_datafeed_packet packet = {.type = SR_DF_TRIGGER, .payload = &t};

    /* A bad trigger header arrives as a packet with an error status. */
    reset_state();
    packet.status = SR_PKT_DATA_ERROR;
    on_data(NULL, &packet);
    assert(g_pkt_error && g_done);

    reset_state();
    uint64_t word = 0;
    struct sr_datafeed_logic logic = {.length = 8, .format = LA_CROSS_DATA, .data = &word,
                                      .data_error = 1};
    packet = (struct sr_datafeed_packet){.type = SR_DF_LOGIC, .payload = &logic};
    on_data(NULL, &packet);
    assert(g_pkt_error && g_done && g_raw_bytes == 0);

    reset_state();
    packet = (struct sr_datafeed_packet){.type = SR_DF_OVERFLOW};
    on_data(NULL, &packet);
    assert(g_overflow && g_done);

    /* Data end and device stop precede the task's final status. */
    reset_state();
    packet = (struct sr_datafeed_packet){.type = SR_DF_END};
    on_data(NULL, &packet);
    on_event(DS_EV_DEVICE_STOPPED);
    assert(g_data_end && !g_done && !g_task_end);
    on_event(DS_EV_COLLECT_TASK_END_BY_ERROR);
    assert(g_done && g_task_end && g_err == DS_EV_COLLECT_TASK_END_BY_ERROR);

    assert(capture_failure(0, 1, 0, 0, 0, 1024, 1000) == NULL);
    assert(capture_failure(1, 1, 0, 0, 0, 1024, 1000));
    assert(capture_failure(0, 0, 0, 0, 0, 1024, 1000));
    assert(capture_failure(0, 1, DS_EV_COLLECT_TASK_END_BY_DETACHED, 0, 0, 1024, 1000));
    assert(capture_failure(0, 1, DS_EV_COLLECT_TASK_END_BY_ERROR, 0, 0, 1024, 1000));
    assert(capture_failure(0, 1, 0, 1, 0, 1024, 1000));
    assert(capture_failure(0, 1, 0, 0, 1, 1024, 1000));
    assert(capture_failure(0, 1, 0, 0, 0, 0, 1000));
    assert(capture_failure(0, 1, 0, 0, 0, 960, 1000));
    reset_state();
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    test_arguments();
    test_status();

    const int channels[] = {1, 2, 16, 32};
    const uint64_t frames = 5003;
    for (size_t test = 0; test < G_N_ELEMENTS(channels); test++) {
        int nch = channels[test];
        size_t bytes = frames * nch * 8;
        uint64_t *raw = g_malloc(bytes + 7);
        for (uint64_t k = 0; k < frames; k++)
            for (int c = 0; c < nch; c++) raw[k * nch + c] = (k << 16) | c;
        memset((char *)raw + bytes, 0xFF, 7); /* Incomplete last word. */
        g_raw = fopen(argv[1], "w+b");
        assert(g_raw);
        reset_state();
        /* Packet boundaries may split words and channel groups. */
        for (size_t pos = 0; pos < bytes + 7;) {
            size_t length = MIN((pos % 4096) + 3, bytes + 7 - pos);
            struct sr_datafeed_logic logic = {.length = length, .format = LA_CROSS_DATA,
                                             .data = (char *)raw + pos};
            struct sr_datafeed_packet packet = {.type = SR_DF_LOGIC, .payload = &logic};
            on_data(NULL, &packet);
            assert(!g_io_error && !g_done);
            pos += length;
        }
        assert(g_raw_bytes == bytes + 7);
        /* Publication never replaces a file, so clear the previous case's output. */
        unlink(argv[2]);
        uint64_t per_ch = g_raw_bytes / 8 / nch;
        assert(write_output(argv[2], nch, per_ch, per_ch * 64) == 0);
        assert(write_output(argv[2], nch, per_ch, per_ch * 64) != 0);
        /* Offsets past what an off_t can address are refused up front. */
        assert(write_output(argv[2], nch, (uint64_t)G_MAXINT64 / 8, 64) != 0);
        FILE *out = fopen(argv[2], "rb");
        assert(out);
        for (int c = 0; c < nch; c++)
            for (uint64_t k = 0; k < frames; k++) {
                uint64_t word;
                assert(fread(&word, 8, 1, out) == 1);
                assert(word == ((k << 16) | c));
            }
        assert(fgetc(out) == EOF);
        fclose(out);

        /* Payload accounting crosses the old GByteArray 32-bit limit. */
        g_raw_bytes = UINT32_MAX - 3ULL;
        struct sr_datafeed_logic logic = {.length = 8, .format = LA_CROSS_DATA, .data = raw};
        struct sr_datafeed_packet packet = {.type = SR_DF_LOGIC, .payload = &logic};
        on_data(NULL, &packet);
        assert(g_raw_bytes == (1ULL << 32) + 4);
        fclose(g_raw);

        /* A spool write failure must stop collection and report failure. */
        g_raw = fopen(argv[1], "rb");
        assert(g_raw);
        g_done = 0;
        on_data(NULL, &packet);
        assert(g_io_error && g_done);
        fclose(g_raw);
        g_raw = NULL;
        g_free(raw);
    }

    /* Large-file interface: seek and write past 4 GiB in a sparse spool. */
    FILE *big = fopen(argv[1], "w+b");
    assert(big);
    const off_t far = ((off_t)1 << 32) + 8;
    uint64_t word = 0x0123456789abcdefULL, back = 0;
    assert(fseeko(big, far, SEEK_SET) == 0 && fwrite(&word, 8, 1, big) == 1);
    assert(fflush(big) == 0 && fseeko(big, far, SEEK_SET) == 0 && ftello(big) == far);
    assert(fread(&back, 8, 1, big) == 1 && back == word);
    fclose(big);

    unlink(argv[1]);
    unlink(argv[2]);
    puts("spool tests passed: arguments, packet status, packet boundaries, channel layout, "
         "64-bit count and offsets, I/O failure");
    return 0;
}
