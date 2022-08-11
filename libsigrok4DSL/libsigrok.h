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

#ifndef LIBSIGROK_SIGROK_H
#define LIBSIGROK_SIGROK_H

#include <sys/time.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <glib.h>  
#include <log/xlog.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * The public libsigrok header file to be used by frontends.
 *
 * This is the only file that libsigrok users (frontends) are supposed to
 * use and \#include. There are other header files which get installed with
 * libsigrok, but those are not meant to be used directly by frontends.
 *
 * The correct way to get/use the libsigrok API functions is:
 *
 * @code{.c}
 *   #include <libsigrok/libsigrok.h>
 * @endcode
 */

/*
 * All possible return codes of libsigrok functions must be listed here.
 * Functions should never return hardcoded numbers as status, but rather
 * use these enum values. All error codes are negative numbers.
 *
 * The error codes are globally unique in libsigrok, i.e. if one of the
 * libsigrok functions returns a "malloc error" it must be exactly the same
 * return value as used by all other functions to indicate "malloc error".
 * There must be no functions which indicate two different errors via the
 * same return code.
 *
 * Also, for compatibility reasons, no defined return codes are ever removed
 * or reused for different errors later. You can only add new entries and
 * return codes, but never remove or redefine existing ones.
 */

#define SR_LIB_NAME		"libsigrok"

/** Status/error codes returned by libsigrok functions. */
enum {
	SR_OK             =  0, /**< No error. */
	SR_ERR            =  1, /**< Generic/unspecified error. */
	SR_ERR_MALLOC     =  2, /**< Malloc/calloc/realloc error. */
	SR_ERR_ARG        =  3, /**< Function argument error. */
	SR_ERR_BUG        =  4, /**< Errors hinting at internal bugs. */
	SR_ERR_SAMPLERATE =  5, /**< Incorrect samplerate. */
	SR_ERR_NA         =  6, /**< Not applicable. */
	SR_ERR_DEV_CLOSED =  7, /**< Device is closed, but needs to be open. */
	SR_ERR_CALL_STATUS = 8, /**< Function call status error. */
	SR_ERR_HAVE_DONE  = 9, /**< The Function have called.*/

	/*
	 * Note: When adding entries here, don't forget to also update the
	 * sr_error_str() and sr_error_name() functions in error.c.
	 */
};

#define SR_MAX_PROBENAME_LEN 32
#define DS_MAX_ANALOG_PROBES_NUM 4
#define DS_MAX_DSO_PROBES_NUM 2

#define TriggerStages 16
#define TriggerProbes 16
#define MaxTriggerProbes 32
#define TriggerCountBits 16
#define STriggerDataStage 3
#define DS_MAX_TRIG_PERCENT 90

#define DS_CONF_DSO_HDIVS 10
#define DS_CONF_DSO_VDIVS 10

#define SAMPLES_ALIGN 1023ULL


/* Handy little macros */
#define SR_HZ(n)  (n)
#define SR_KHZ(n) ((n) * (uint64_t)(1000ULL))
#define SR_MHZ(n) ((n) * (uint64_t)(1000000ULL))
#define SR_GHZ(n) ((n) * (uint64_t)(1000000000ULL))

#define SR_HZ_TO_NS(n) ((uint64_t)(1000000000ULL) / (n))

#define SR_NS(n)   (n)
#define SR_US(n)   ((n) * (uint64_t)(1000ULL))
#define SR_MS(n)   ((n) * (uint64_t)(1000000ULL))
#define SR_SEC(n)  ((n) * (uint64_t)(1000000000ULL))
#define SR_MIN(n)  ((n) * (uint64_t)(60000000000ULL))
#define SR_HOUR(n) ((n) * (uint64_t)(3600000000000ULL))
#define SR_DAY(n)  ((n) * (uint64_t)(86400000000000ULL))

#define SR_n(n)  (n)
#define SR_Kn(n) ((n) * (uint64_t)(1000ULL))
#define SR_Mn(n) ((n) * (uint64_t)(1000000ULL))
#define SR_Gn(n) ((n) * (uint64_t)(1000000000ULL))

#define SR_B(n)  (n)
#define SR_KB(n) ((n) * (uint64_t)(1024ULL))
#define SR_MB(n) ((n) * (uint64_t)(1048576ULL))
#define SR_GB(n) ((n) * (uint64_t)(1073741824ULL))

#define SR_mV(n) (n)
#define SR_V(n)  ((n) * (uint64_t)(1000ULL))
#define SR_KV(n) ((n) * (uint64_t)(1000000ULL))
#define SR_MV(n) ((n) * (uint64_t)(1000000000ULL))
 
/*
 * Use SR_API to mark public API symbols, and SR_PRIV for private symbols.
 *
 * Variables and functions marked 'static' are private already and don't
 * need SR_PRIV. However, functions which are not static (because they need
 * to be used in other libsigrok-internal files) but are also not meant to
 * be part of the public libsigrok API, must use SR_PRIV.
 *
 * This uses the 'visibility' feature of gcc (requires gcc >= 4.0).
 *
 * This feature is not available on MinGW/Windows, as it is a feature of
 * ELF files and MinGW/Windows uses PE files.
 *
 * Details: http://gcc.gnu.org/wiki/Visibility
 */

/* Marks public libsigrok API symbols. */
#ifndef _WIN32
#define SR_API __attribute__((visibility("default")))
#else
#define SR_API
#endif

/* Marks private, non-public libsigrok symbols (not part of the API). */
#ifndef _WIN32
#define SR_PRIV __attribute__((visibility("hidden")))
#else
#define SR_PRIV
#endif

enum sr_device_type{
	DEV_TYPE_UNKOWN = 0,
	DEV_TYPE_DEMO = 1,
	DEV_TYPE_FILELOG = 2,
	DEV_TYPE_USB = 3,
	DEV_TYPE_SERIAL = 4,
};
  
/** Data types used by sr_config_info(). */
enum {
	SR_T_UINT64 = 10000,
    SR_T_UINT8,
    SR_T_CHAR,
	SR_T_BOOL,
	SR_T_FLOAT,
	SR_T_RATIONAL_PERIOD,
	SR_T_RATIONAL_VOLT,
	SR_T_KEYVALUE,
};

/** Value for sr_datafeed_packet.type. */
enum sr_datafeed_packet_type {
	SR_DF_HEADER = 10000,
	SR_DF_END,
	SR_DF_META,
	SR_DF_TRIGGER,
	SR_DF_LOGIC,
    SR_DF_DSO,
	SR_DF_ANALOG,
	SR_DF_FRAME_BEGIN,
	SR_DF_FRAME_END,
    SR_DF_OVERFLOW,
};

/** Values for sr_datafeed_analog.mq. */
enum {
	SR_MQ_VOLTAGE = 10000,
	SR_MQ_CURRENT,
	SR_MQ_RESISTANCE,
	SR_MQ_CAPACITANCE,
	SR_MQ_TEMPERATURE,
	SR_MQ_FREQUENCY,
	SR_MQ_DUTY_CYCLE,
	SR_MQ_CONTINUITY,
	SR_MQ_PULSE_WIDTH,
	SR_MQ_CONDUCTANCE,
	/** Electrical power, usually in W, or dBm. */
	SR_MQ_POWER,
	/** Gain (a transistor's gain, or hFE, for example). */
	SR_MQ_GAIN,
	/** Logarithmic representation of sound pressure relative to a
	 * reference value. */
	SR_MQ_SOUND_PRESSURE_LEVEL,
	SR_MQ_CARBON_MONOXIDE,
	SR_MQ_RELATIVE_HUMIDITY,
};

/** Values for sr_datafeed_analog.unit. */
enum {
	SR_UNIT_VOLT = 10000,
	SR_UNIT_AMPERE,
	SR_UNIT_OHM,
	SR_UNIT_FARAD,
	SR_UNIT_KELVIN,
	SR_UNIT_CELSIUS,
	SR_UNIT_FAHRENHEIT,
	SR_UNIT_HERTZ,
	SR_UNIT_PERCENTAGE,
	SR_UNIT_BOOLEAN,
	SR_UNIT_SECOND,
	/** Unit of conductance, the inverse of resistance. */
	SR_UNIT_SIEMENS,
	/**
	 * An absolute measurement of power, in decibels, referenced to
	 * 1 milliwatt (dBu).
	 */
	SR_UNIT_DECIBEL_MW,
	/** Voltage in decibel, referenced to 1 volt (dBV). */
	SR_UNIT_DECIBEL_VOLT,
	/**
	 * Measurements that intrinsically do not have units attached, such
	 * as ratios, gains, etc. Specifically, a transistor's gain (hFE) is
	 * a unitless quantity, for example.
	 */
	SR_UNIT_UNITLESS,
	/** Sound pressure level relative so 20 micropascals. */
	SR_UNIT_DECIBEL_SPL,
	/**
	 * Normalized (0 to 1) concentration of a substance or compound with 0
	 * representing a concentration of 0%, and 1 being 100%. This is
	 * represented as the fraction of number of particles of the substance.
	 */
	SR_UNIT_CONCENTRATION,
};

/** Values for sr_datafeed_analog.flags. */
enum {
	/** Voltage measurement is alternating current (AC). */
	SR_MQFLAG_AC = 0x01,
	/** Voltage measurement is direct current (DC). */
	SR_MQFLAG_DC = 0x02,
	/** This is a true RMS measurement. */
	SR_MQFLAG_RMS = 0x04,
	/** Value is voltage drop across a diode, or NAN. */
	SR_MQFLAG_DIODE = 0x08,
	/** Device is in "hold" mode (repeating the last measurement). */
	SR_MQFLAG_HOLD = 0x10,
	/** Device is in "max" mode, only updating upon a new max value. */
	SR_MQFLAG_MAX = 0x20,
	/** Device is in "min" mode, only updating upon a new min value. */
	SR_MQFLAG_MIN = 0x40,
	/** Device is in autoranging mode. */
	SR_MQFLAG_AUTORANGE = 0x80,
	/** Device is in relative mode. */
	SR_MQFLAG_RELATIVE = 0x100,
	/** Sound pressure level is A-weighted in the frequency domain,
	 * according to IEC 61672:2003. */
	SR_MQFLAG_SPL_FREQ_WEIGHT_A = 0x200,
	/** Sound pressure level is C-weighted in the frequency domain,
	 * according to IEC 61672:2003. */
	SR_MQFLAG_SPL_FREQ_WEIGHT_C = 0x400,
	/** Sound pressure level is Z-weighted (i.e. not at all) in the
	 * frequency domain, according to IEC 61672:2003. */
	SR_MQFLAG_SPL_FREQ_WEIGHT_Z = 0x800,
	/** Sound pressure level is not weighted in the frequency domain,
	 * albeit without standards-defined low and high frequency limits. */
	SR_MQFLAG_SPL_FREQ_WEIGHT_FLAT = 0x1000,
	/** Sound pressure level measurement is S-weighted (1s) in the
	 * time domain. */
	SR_MQFLAG_SPL_TIME_WEIGHT_S = 0x2000,
	/** Sound pressure level measurement is F-weighted (125ms) in the
	 * time domain. */
	SR_MQFLAG_SPL_TIME_WEIGHT_F = 0x4000,
	/** Sound pressure level is time-averaged (LAT), also known as
	 * Equivalent Continuous A-weighted Sound Level (LEQ). */
	SR_MQFLAG_SPL_LAT = 0x8000,
	/** Sound pressure level represented as a percentage of measurements
	 * that were over a preset alarm level. */
	SR_MQFLAG_SPL_PCT_OVER_ALARM = 0x10000,
};

enum DSO_MEASURE_TYPE {
    DSO_MS_BEGIN = 0,
    DSO_MS_FREQ,
    DSO_MS_PERD,
    DSO_MS_PDUT,
    DSO_MS_NDUT,
    DSO_MS_PCNT,
    DSO_MS_RISE,
    DSO_MS_FALL,
    DSO_MS_PWDT,
    DSO_MS_NWDT,
    DSO_MS_BRST,
    DSO_MS_AMPT,
    DSO_MS_VHIG,
    DSO_MS_VLOW,
    DSO_MS_VRMS,
    DSO_MS_VMEA,
    DSO_MS_VP2P,
    DSO_MS_VMAX,
    DSO_MS_VMIN,
    DSO_MS_POVR,
    DSO_MS_NOVR,
    DSO_MS_END,
};

enum {
    SR_PKT_OK,
    SR_PKT_SOURCE_ERROR,
    SR_PKT_DATA_ERROR,
};

struct sr_context; //hidden all field
struct sr_dev_inst;
struct sr_dev_driver;

struct sr_datafeed_packet {
	uint16_t type; //see enum sr_datafeed_packet_type
    uint16_t status;
	const void *payload;
	int bExportOriginalData;
};

struct sr_datafeed_header {
	int feed_version;
	struct timeval starttime;
};

struct sr_datafeed_meta {
	GSList *config;
};

enum LA_DATA_FORMAT {
    LA_CROSS_DATA,
    LA_SPLIT_DATA,
};

struct sr_datafeed_logic {
	uint64_t length;
    /** data format */
    int format;
    /** for LA_SPLIT_DATA, indicate the channel index */
    uint16_t index;
    uint16_t order;
	uint16_t unitsize;
    uint16_t data_error;
    uint64_t error_pattern;
	void *data;
};

struct sr_datafeed_dso {
    /** The probes for which data is included in this packet. */
    GSList *probes;
    int num_samples;
    /** Measured quantity (voltage, current, temperature, and so on). */
    int mq;
    /** Unit in which the MQ is measured. */
    int unit;
    /** Bitmap with extra information about the MQ. */
    uint64_t mqflags;
    /** samplerate different from last packet */
    gboolean samplerate_tog;
    /** trig flag */
    gboolean trig_flag;
    /** trig channel */
    uint8_t trig_ch;
    /** The analog value(s). The data is interleaved according to
     * the probes list. */
    void *data;
};

struct sr_datafeed_analog {
	/** The probes for which data is included in this packet. */
	GSList *probes;
	int num_samples;
    /** How many bits for each sample */
    uint8_t unit_bits;
    /** Interval between two valid samples */
    uint16_t unit_pitch;
	/** Measured quantity (voltage, current, temperature, and so on). */
	int mq;
	/** Unit in which the MQ is measured. */
	int unit;
	/** Bitmap with extra information about the MQ. */
	uint64_t mqflags;
	/** The analog value(s). The data is interleaved according to
	 * the probes list. */
    void *data;
};

/** Input (file) format struct. */
struct sr_input {
	/**
	 * A pointer to this input format's 'struct sr_input_format'.
	 * The frontend can use this to call the module's callbacks.
	 */
	struct sr_input_format *format;

	GHashTable *param;

	struct sr_dev_inst *sdi;

	void *internal;
};

struct sr_input_format {
	/** The unique ID for this input format. Must not be NULL. */
	char *id;

	/**
	 * A short description of the input format, which can (for example)
	 * be displayed to the user by frontends. Must not be NULL.
	 */
	char *description;

	/**
	 * Check if this input module can load and parse the specified file.
	 *
	 * @param filename The name (and path) of the file to check.
	 *
	 * @return TRUE if this module knows the format, FALSE if it doesn't.
	 */
	int (*format_match) (const char *filename);

	/**
	 * Initialize the input module.
	 *
	 * @param in A pointer to a valid 'struct sr_input' that the caller
	 *           has to allocate and provide to this function. It is also
	 *           the responsibility of the caller to free it later.
	 * @param filename The name (and path) of the file to use.
	 *
	 * @return SR_OK upon success, a negative error code upon failure.
	 */
	int (*init) (struct sr_input *in, const char *filename);

	/**
	 * Load a file, parsing the input according to the file's format.
     *
	 * This function will send datafeed packets to the session bus, so
	 * the calling frontend must have registered its session callbacks
	 * beforehand.
	 *
	 * The packet types sent across the session bus by this function must
	 * include at least SR_DF_HEADER, SR_DF_END, and an appropriate data
	 * type such as SR_DF_LOGIC. It may also send a SR_DF_TRIGGER packet
	 * if appropriate.
	 *
	 * @param in A pointer to a valid 'struct sr_input' that the caller
	 *           has to allocate and provide to this function. It is also
	 *           the responsibility of the caller to free it later.
	 * @param filename The name (and path) of the file to use.
	 *
     * @return SR_OK upon succcess, a negative error code upon failure.
	 */
	int (*loadfile) (struct sr_input *in, const char *filename);
};

/** Output (file) format struct. */
struct sr_output {
	/**
	 * A pointer to this output format's 'struct sr_output_format'.
	 * The frontend can use this to call the module's callbacks.
	 */
    const struct sr_output_module *module;

	/**
	 * The device for which this output module is creating output. This
	 * can be used by the module to find out probe names and numbers.
	 */
    const struct sr_dev_inst *sdi;

	/**
	 * An optional parameter which the frontend can pass in to the
	 * output module. How the string is interpreted is entirely up to
	 * the module.
	 */
	char *param;

	/**
	 * A generic pointer which can be used by the module to keep internal
	 * state between calls into its callback functions.
	 *
	 * For example, the module might store a pointer to a chunk of output
	 * there, and only flush it when it reaches a certain size.
	 */
	void *priv;
};

/** Generic option struct used by various subsystems. */
struct sr_option {
	/* Short name suitable for commandline usage, [a-z0-9-]. */
	char *id;
	/* Short name suitable for GUI usage, can contain UTF-8. */
	char *name;
	/* Description of the option, in a sentence. */
	char *desc;
	/* Default value for this option. */
	GVariant *def;
	/* List of possible values, if this is an option with few values. */
	GSList *values;
};

/** Output module driver. */
struct sr_output_module {
	/**
	 * A unique ID for this output module, suitable for use in command-line
	 * clients, [a-z0-9-]. Must not be NULL.
	 */
	char *id;

	/**
	 * A unique name for this output module, suitable for use in GUI
	 * clients, can contain UTF-8. Must not be NULL.
	 */
	const char *name;

	/**
	 * A short description of the output module. Must not be NULL.
	 *
	 * This can be displayed by frontends, e.g. when selecting the output
	 * module for saving a file.
	 */
	char *desc;

	/**
	 * A NULL terminated array of strings containing a list of file name
	 * extensions typical for the input file format, or NULL if there is
	 * no typical extension for this file format.
	 */
	const char *const *exts;

	/**
	 * Returns a NULL-terminated list of options this module can take.
	 * Can be NULL, if the module has no options.
	 */
	const struct sr_option *(*options) (void);

	/**
	 * This function is called once, at the beginning of an output stream.
	 *
	 * The device struct will be available in the output struct passed in,
	 * as well as the param field -- which may be NULL or an empty string,
	 * if no parameter was passed.
	 *
	 * The module can use this to initialize itself, create a struct for
	 * keeping state and storing it in the <code>internal</code> field.
	 *
	 * @param o Pointer to the respective 'struct sr_output'.
	 *
	 * @retval SR_OK Success
	 * @retval other Negative error code.
	 */
	int (*init) (struct sr_output *o, GHashTable *options);

	/**
	 * This function is passed a copy of every packed in the data feed.
	 * Any output generated by the output module in response to the
	 * packet should be returned in a newly allocated GString
	 * <code>out</code>, which will be freed by the caller.
	 *
	 * Packets not of interest to the output module can just be ignored,
	 * and the <code>out</code> parameter set to NULL.
	 *
	 * @param o Pointer to the respective 'struct sr_output'.
	 * @param sdi The device instance that generated the packet.
	 * @param packet The complete packet.
	 * @param out A pointer where a GString * should be stored if
	 * the module generates output, or NULL if not.
	 *
	 * @retval SR_OK Success
	 * @retval other Negative error code.
	 */
	int (*receive) (const struct sr_output *o,
			const struct sr_datafeed_packet *packet, GString **out);

	/**
	 * This function is called after the caller is finished using
	 * the output module, and can be used to free any internal
	 * resources the module may keep.
	 *
	 * @retval SR_OK Success
	 * @retval other Negative error code.
	 */
	int (*cleanup) (struct sr_output *o);
};


enum CHANNEL_TYPE {
    SR_CHANNEL_DECODER = 9998,
    SR_CHANNEL_GROUP = 9999,
    SR_CHANNEL_LOGIC = 10000,
    SR_CHANNEL_DSO,
    SR_CHANNEL_ANALOG,
    SR_CHANNEL_FFT,
    SR_CHANNEL_LISSAJOUS,
    SR_CHANNEL_MATH,
};

enum OPERATION_MODE {
    LOGIC = 0,
    DSO = 1,
    ANALOG = 2,
};

struct sr_channel {
    /* The index field will go: use g_slist_length(sdi->channels) instead. */
    uint16_t index;
    int type;
	gboolean enabled;
	char *name;
	char *trigger;
    uint8_t bits;
    uint64_t vdiv;
    uint64_t vfactor;
    uint16_t offset;
    uint16_t zero_offset;
    uint16_t hw_offset;
    uint16_t vpos_trans;
    uint8_t coupling;
    uint8_t trig_value;
    int8_t comb_diff_top;
    int8_t comb_diff_bom;
    int8_t comb_comp;
    uint16_t digi_fgain;

    double cali_fgain0;
    double cali_fgain1;
    double cali_fgain2;
    double cali_fgain3;
    double cali_comb_fgain0;
    double cali_comb_fgain1;
    double cali_comb_fgain2;
    double cali_comb_fgain3;

    gboolean map_default;
    const char *map_unit;
    double map_min;
    double map_max;
    struct DSL_vga *vga_ptr;
};

/** Structure for groups of channels that have common properties. */
struct sr_channel_group {
    /** Name of the channel group. */
    char *name;
    /** List of sr_channel structs of the channels belonging to this group. */
    GSList *channels;
    /** Private data for driver use. */
    void *priv;
};

struct sr_config {
	int key;
	GVariant *data;
};

struct sr_config_info {
	int key;
	int datatype;
	char *id;
	char *name;
    char *label;
    char *label_cn;
	char *description;
};

struct sr_status {
    uint8_t trig_hit;
    uint8_t captured_cnt3;
    uint8_t captured_cnt2;
    uint8_t captured_cnt1;
    uint8_t captured_cnt0;

    uint16_t pkt_id;
    uint32_t vlen;
    gboolean stream_mode;
    gboolean measure_valid;
    uint32_t sample_divider;
    gboolean sample_divider_tog;
    gboolean trig_flag;
    uint8_t  trig_ch;
    uint8_t  trig_offset;

    uint8_t ch0_max;
    uint8_t ch0_min;
    uint32_t ch0_cyc_cnt;
    uint32_t ch0_cyc_tlen;
    uint32_t ch0_cyc_plen;
    uint32_t ch0_cyc_llen;
    gboolean ch0_level_valid;
    gboolean ch0_plevel;
    uint8_t ch0_low_level;
    uint8_t ch0_high_level;
    uint32_t ch0_cyc_rlen;
    uint32_t ch0_cyc_flen;
    uint64_t ch0_acc_square;
    uint32_t ch0_acc_mean;
    uint32_t ch0_acc_mean_p1;
    uint32_t ch0_acc_mean_p2;
    uint32_t ch0_acc_mean_p3;

    uint8_t ch1_max;
    uint8_t ch1_min;
    uint32_t ch1_cyc_cnt;
    uint32_t ch1_cyc_tlen;
    uint32_t ch1_cyc_plen;
    uint32_t ch1_cyc_llen;
    gboolean ch1_level_valid;
    gboolean ch1_plevel;
    uint8_t ch1_low_level;
    uint8_t ch1_high_level;
    uint32_t ch1_cyc_rlen;
    uint32_t ch1_cyc_flen;
    uint64_t ch1_acc_square;
    uint32_t ch1_acc_mean;
    uint32_t ch1_acc_mean_p1;
    uint32_t ch1_acc_mean_p2;
    uint32_t ch1_acc_mean_p3;
};

enum sr_config_option_id{
	/*--- Device classes ------------------------------------------------*/

	/** The device can act as logic analyzer. */
	SR_CONF_LOGIC_ANALYZER = 10000,

	/** The device can act as an oscilloscope. */
	SR_CONF_OSCILLOSCOPE,

	/** The device can act as a multimeter. */
	SR_CONF_MULTIMETER,

	/** The device is a demo device. */
	SR_CONF_DEMO_DEV,

	/** The device can act as a sound level meter. */
	SR_CONF_SOUNDLEVELMETER,

	/** The device can measure temperature. */
	SR_CONF_THERMOMETER,

	/** The device can measure humidity. */
	SR_CONF_HYGROMETER,

	/*--- Driver scan options -------------------------------------------*/

	/**
	 * Specification on how to connect to a device.
	 *
	 * In combination with SR_CONF_SERIALCOMM, this is a serial port in
	 * the form which makes sense to the OS (e.g., /dev/ttyS0).
	 * Otherwise this specifies a USB device, either in the form of
	 * @verbatim <bus>.<address> @endverbatim (decimal, e.g. 1.65) or
	 * @verbatim <vendorid>.<productid> @endverbatim
	 * (hexadecimal, e.g. 1d6b.0001).
	 */
	SR_CONF_CONN = 20000,

	/**
	 * Serial communication specification, in the form:
	 *
	 *   @verbatim <baudrate>/<databits><parity><stopbits> @endverbatim
	 *
	 * Example: 9600/8n1
	 *
	 * The string may also be followed by one or more special settings,
	 * in the form "/key=value". Supported keys and their values are:
	 *
	 * rts    0,1    set the port's RTS pin to low or high
	 * dtr    0,1    set the port's DTR pin to low or high
	 * flow   0      no flow control
	 *        1      hardware-based (RTS/CTS) flow control
	 *        2      software-based (XON/XOFF) flow control
	 * 
	 * This is always an optional parameter, since a driver typically
	 * knows the speed at which the device wants to communicate.
	 */
	SR_CONF_SERIALCOMM,

	/*--- Device configuration ------------------------------------------*/

	/** The device supports setting its samplerate, in Hz. */
	SR_CONF_SAMPLERATE = 30000,

	/** The device supports setting a pre/post-trigger capture ratio. */
	SR_CONF_CAPTURE_RATIO,

    /** */
    SR_CONF_USB_SPEED,
    SR_CONF_USB30_SUPPORT,
    SR_CONF_DEVICE_MODE,
    SR_CONF_INSTANT,
    SR_CONF_STATUS,

	/** The device supports setting a pattern (pattern generator mode). */
	SR_CONF_PATTERN_MODE,

	/** The device supports Run Length Encoding. */
	SR_CONF_RLE,

    /** Need wait to uplad captured data */
    SR_CONF_WAIT_UPLOAD,

	/** The device supports setting trigger slope. */
	SR_CONF_TRIGGER_SLOPE,

	/** Trigger source. */
	SR_CONF_TRIGGER_SOURCE,

    /** Trigger channel */
    SR_CONF_TRIGGER_CHANNEL,

    /** Trigger Value. */
    SR_CONF_TRIGGER_VALUE,

	/** Horizontal trigger position. */
	SR_CONF_HORIZ_TRIGGERPOS,

    /** Trigger hold off time */
    SR_CONF_TRIGGER_HOLDOFF,

    /** Trigger Margin */
    SR_CONF_TRIGGER_MARGIN,

	/** Buffer size. */
	SR_CONF_BUFFERSIZE,

	/** Time base. */
    SR_CONF_MAX_TIMEBASE,
    SR_CONF_MIN_TIMEBASE,
	SR_CONF_TIMEBASE,

	/** Filter. */
	SR_CONF_FILTER,

    /** DSO configure sync */
    SR_CONF_DSO_SYNC,

    /** How many bits for each sample */
    SR_CONF_UNIT_BITS,
    SR_CONF_REF_MIN,
    SR_CONF_REF_MAX,

    /** Valid channel number */
    SR_CONF_TOTAL_CH_NUM,

    /** Valid channel number */
    SR_CONF_VLD_CH_NUM,

    /** 32 channel support */
    SR_CONF_LA_CH32,

    /** Zero */
    SR_CONF_HAVE_ZERO,
    SR_CONF_ZERO,
    SR_CONF_ZERO_SET,
    SR_CONF_ZERO_LOAD,
    SR_CONF_ZERO_DEFAULT,
    SR_CONF_ZERO_COMB_FGAIN,
    SR_CONF_ZERO_COMB,
    SR_CONF_VOCM,
    SR_CONF_CALI,

    /** status for dso channel */
    SR_CONF_STATUS_PERIOD,
    SR_CONF_STATUS_PCNT,
    SR_CONF_STATUS_MAX,
    SR_CONF_STATUS_MIN,
    SR_CONF_STATUS_PLEN,
    SR_CONF_STATUS_LLEN,
    SR_CONF_STATUS_LEVEL,
    SR_CONF_STATUS_PLEVEL,
    SR_CONF_STATUS_LOW,
    SR_CONF_STATUS_HIGH,
    SR_CONF_STATUS_RLEN,
    SR_CONF_STATUS_FLEN,
    SR_CONF_STATUS_RMS,
    SR_CONF_STATUS_MEAN,

    /** Stream */
    SR_CONF_STREAM,

    /** DSO Roll */
    SR_CONF_ROLL,

    /** Test */
    SR_CONF_TEST,
    SR_CONF_EEPROM,
    SR_CONF_TUNE,
    SR_CONF_TUNE_SEL,
    SR_CONF_EXTEND_ID,
    SR_CONF_EXTEND_DATA,

	/** The device supports setting its sample interval, in ms. */
	SR_CONF_SAMPLE_INTERVAL,

	/** Number of timebases, as related to SR_CONF_TIMEBASE.  */
	SR_CONF_NUM_TIMEBASE,

    /** Number of vertical divisions, as related to SR_CONF_PROBE_VDIV.  */
	SR_CONF_NUM_VDIV,

    /** clock type (internal/external) */
    SR_CONF_CLOCK_TYPE,

    /** clock edge (posedge/negedge) */
    SR_CONF_CLOCK_EDGE,

    /** Device operation mode */
    SR_CONF_OPERATION_MODE,

    /** Device buffer options */
    SR_CONF_BUFFER_OPTIONS,

    /** Device channel mode */
    SR_CONF_CHANNEL_MODE,

    /** RLE compress support */
    SR_CONF_RLE_SUPPORT,

    /** Signal max height **/
    SR_CONF_MAX_HEIGHT,
    SR_CONF_MAX_HEIGHT_VALUE,

    /** Device sample threshold */
    SR_CONF_THRESHOLD,
    SR_CONF_VTH,

    /** Hardware capacity **/
    SR_CONF_MAX_DSO_SAMPLERATE,
    SR_CONF_MAX_DSO_SAMPLELIMITS,
    SR_CONF_HW_DEPTH,

    /** bandwidth */
    SR_CONF_BANDWIDTH,
    SR_CONF_BANDWIDTH_LIMIT,

    /*--- Probe configuration -------------------------------------------*/
    /** Probe options */
    SR_CONF_PROBE_CONFIGS,

    /** Probe options */
    SR_CONF_PROBE_SESSIONS,

    /** Enable */
    SR_CONF_PROBE_EN,

    /** Coupling */
    SR_CONF_PROBE_COUPLING,

    /** Volts/div */
    SR_CONF_PROBE_VDIV,

    /** Factor */
    SR_CONF_PROBE_FACTOR,

    /** Mapping */
    SR_CONF_PROBE_MAP_DEFAULT,
    SR_CONF_PROBE_MAP_UNIT,
    SR_CONF_PROBE_MAP_MIN,
    SR_CONF_PROBE_MAP_MAX,

    /** Vertical offset */
    SR_CONF_PROBE_OFFSET,
    SR_CONF_PROBE_HW_OFFSET,
    SR_CONF_PROBE_PREOFF,
    SR_CONF_PROBE_PREOFF_DEFAULT,
    SR_CONF_PROBE_PREOFF_MARGIN,

    /** VGain */
    SR_CONF_PROBE_VGAIN,
    SR_CONF_PROBE_VGAIN_DEFAULT,
    SR_CONF_PROBE_VGAIN_RANGE,
    SR_CONF_PROBE_COMB_COMP_EN,
    SR_CONF_PROBE_COMB_COMP,

	/*--- Special stuff -------------------------------------------------*/

	/** Device options for a particular device. */
	SR_CONF_DEVICE_OPTIONS,

    /** Sessions */
    SR_CONF_DEVICE_SESSIONS,

	/** Session filename. */
	SR_CONF_SESSIONFILE,

	/** The device supports specifying a capturefile to inject. */
	SR_CONF_CAPTUREFILE,

    /** Session file version */
    SR_CONF_FILE_VERSION,

	/** The device supports setting the number of probes. */
	SR_CONF_CAPTURE_NUM_PROBES,

    /** The device supports setting the number of data blocks. */
    SR_CONF_NUM_BLOCKS,

    /** language (string code) **/
    SR_CONF_LANGUAGE,

	/*--- Acquisition modes ---------------------------------------------*/

	/**
	 * The device supports setting a sample time limit (how long
	 * the sample acquisition should run, in ms).
	 */
	SR_CONF_LIMIT_MSEC = 50000,

	/**
	 * The device supports setting a sample number limit (how many
	 * samples should be acquired).
	 */
	SR_CONF_LIMIT_SAMPLES,

    /**
     * Absolute time record for session driver
     */
    SR_CONF_TRIGGER_TIME,

    /**
     * Trigger position for session driver
     */
    SR_CONF_TRIGGER_POS,

    /**
     * The actual sample count received
     */
    SR_CONF_ACTUAL_SAMPLES,

	/**
	 * The device supports setting a frame limit (how many
	 * frames should be acquired).
	 */
	SR_CONF_LIMIT_FRAMES,

	/**
	 * The device supports continuous sampling. Neither a time limit
	 * nor a sample number limit has to be supplied, it will just acquire
	 * samples continuously, until explicitly stopped by a certain command.
	 */
	SR_CONF_CONTINUOUS,

	/** The device has internal storage, into which data is logged. This
	 * starts or stops the internal logging. */
	SR_CONF_DATALOG,
};

/** Device instance status. */
enum {
	/** The device instance was not found. */
	SR_ST_NOT_FOUND = 10000,
	/** The device instance was found, but is still booting. */
	SR_ST_INITIALIZING,
	/** The device instance is live, but not in use. */
	SR_ST_INACTIVE,
    /** The device instance has an imcompatible firmware */
    SR_ST_INCOMPATIBLE,
	/** The device instance is actively in use in a session. */
	SR_ST_ACTIVE,
	/** The device is winding down its session. */
	SR_ST_STOPPING,
};

/** Device test modes. */
enum {
    /** No test mode */
    SR_TEST_NONE,
    /** Internal pattern test mode */
    SR_TEST_INTERNAL,
    /** External pattern test mode */
    SR_TEST_EXTERNAL,
    /** SDRAM loopback test mode */
    SR_TEST_LOOPBACK,
};

/** Device buffer mode */
enum {
    /** Stop immediately */
    SR_BUF_STOP = 0,
    /** Upload captured data */
    SR_BUF_UPLOAD = 1,
};

/** Device threshold level. */
enum {
    /** 1.8/2.5/3.3 level */
    SR_TH_3V3 = 0,
    /** 5.0 level */
    SR_TH_5V0 = 1,
};

/** Device input filter. */
enum {
    /** None */
    SR_FILTER_NONE = 0,
    /** One clock cycle */
    SR_FILTER_1T = 1,
};

/** Coupling. */
enum {
    /** DC */
    SR_DC_COUPLING = 0,
    /** AC */
    SR_AC_COUPLING = 1,
    /** Ground */
    SR_GND_COUPLING = 2,
};

struct sr_dev_mode {
    int mode;
	const char *name;
    const char *acronym;
};
 
enum {
    SIMPLE_TRIGGER = 0,
    ADV_TRIGGER,
    SERIAL_TRIGGER,
};

enum {
    DSO_TRIGGER_AUTO = 0,
    DSO_TRIGGER_CH0,
    DSO_TRIGGER_CH1,
    DSO_TRIGGER_CH0A1,
    DSO_TRIGGER_CH0O1,
};
enum {
    DSO_TRIGGER_RISING = 0,
    DSO_TRIGGER_FALLING,
};

struct ds_trigger_pos {
    uint32_t check_id;
    uint32_t real_pos;
    uint32_t ram_saddr;
    uint32_t remain_cnt_l;
    uint32_t remain_cnt_h;
    uint32_t status;
};
  
/**
 * @file
 *
 * Header file containing API function prototypes.
 */

/*--- backend.c -------------------------------------------------------------*/

//@event, 1:attach, 2:left
SR_PRIV int sr_init(struct sr_context **ctx);
SR_PRIV int sr_exit(struct sr_context *ctx);

/*--- device.c --------------------------------------------------------------*/

SR_API int sr_dev_probe_name_set(const struct sr_dev_inst *sdi,
		int probenum, const char *name);
SR_API int sr_dev_probe_enable(const struct sr_dev_inst *sdi, int probenum,
		gboolean state);
SR_API int sr_dev_trigger_set(const struct sr_dev_inst *sdi, uint16_t probenum,
		const char *trigger);
SR_API GSList *sr_dev_list(const struct sr_dev_driver *driver);
SR_API const GSList *sr_dev_mode_list(const struct sr_dev_inst *sdi);
SR_API int sr_dev_clear(const struct sr_dev_driver *driver);
SR_API int sr_dev_open(struct sr_dev_inst *sdi);
SR_API int sr_dev_close(struct sr_dev_inst *sdi);

/*--- hwdriver.c ------------------------------------------------------------*/

SR_API struct sr_dev_driver **sr_driver_list(void);
SR_API int sr_driver_init(struct sr_context *ctx,
		struct sr_dev_driver *driver);
SR_API GSList *sr_driver_scan(struct sr_dev_driver *driver, GSList *options);
SR_API int sr_config_get(const struct sr_dev_driver *driver,
                         const struct sr_dev_inst *sdi,
                         const struct sr_channel *ch,
                         const struct sr_channel_group *cg,
                         int key, GVariant **data);
SR_API int sr_config_set(struct sr_dev_inst *sdi,
                         struct sr_channel *ch,
                         struct sr_channel_group *cg,
                         int key, GVariant *data);
SR_API int sr_config_list(const struct sr_dev_driver *driver,
                          const struct sr_dev_inst *sdi,
                          const struct sr_channel_group *cg,
                          int key, GVariant **data);
SR_API const struct sr_config_info *sr_config_info_get(int key);
SR_API const struct sr_config_info *sr_config_info_name_get(const char *optname);
SR_API int sr_status_get(const struct sr_dev_inst *sdi, struct sr_status *status, gboolean prg);
SR_API struct sr_config *sr_config_new(int key, GVariant *data);
SR_API void sr_config_free(struct sr_config *src);

//SR_API void sr_test_usb_api();

/*--------------------session.c----------------*/
typedef void (*sr_datafeed_callback_t)(const struct sr_dev_inst *sdi,
					const struct sr_datafeed_packet *packet, void *cb_data);
          

/* Session setup */
SR_API int sr_session_load(const char *filename);
SR_API struct sr_session *sr_session_new(void);
SR_API int sr_session_destroy(void); 

/* Session control */ 
SR_API int sr_session_run(void);
SR_API int sr_session_stop(void); 
 

/*--- input/input.c ---------------------------------------------------------*/

SR_API struct sr_input_format **sr_input_list(void);

/*--- output/output.c -------------------------------------------------------*/

SR_API const struct sr_output_module **sr_output_list(void);

/*--- strutil.c -------------------------------------------------------------*/

SR_API char *sr_si_string_u64(uint64_t x, const char *unit);
SR_API char *sr_iec_string_u64(uint64_t x, const char *unit);
SR_API char *sr_samplerate_string(uint64_t samplerate);
SR_API char *sr_samplecount_string(uint64_t samplecount);
SR_API char *sr_period_string(uint64_t frequency);
SR_API char *sr_time_string(uint64_t time);
SR_API char *sr_voltage_string(uint64_t v_p, uint64_t v_q);
SR_API int sr_parse_sizestring(const char *sizestring, uint64_t *size);
SR_API uint64_t sr_parse_timestring(const char *timestring);
SR_API gboolean sr_parse_boolstring(const char *boolstring);
SR_API int sr_parse_period(const char *periodstr, uint64_t *p, uint64_t *q);
SR_API int sr_parse_voltage(const char *voltstr, uint64_t *p, uint64_t *q);

/*--- version.c -------------------------------------------------------------*/

SR_API const char *sr_get_lib_version_string();

/*--- error.c ---------------------------------------------------------------*/

SR_API const char *sr_error_str(int error_code);
SR_API const char *sr_error_name(int error_code);

/*--- trigger.c ------------------------------------------------------------*/
SR_API int ds_trigger_stage_set_value(uint16_t stage, uint16_t probes, char *trigger0, char *trigger1);
SR_API int ds_trigger_stage_set_logic(uint16_t stage, uint16_t probes, unsigned char trigger_logic);
SR_API int ds_trigger_stage_set_inv(uint16_t stage, uint16_t probes, unsigned char trigger0_inv, unsigned char trigger1_inv);
SR_API int ds_trigger_stage_set_count(uint16_t stage, uint16_t probes, uint32_t trigger0_count, uint32_t trigger1_count);
SR_API int ds_trigger_probe_set(uint16_t probe, unsigned char trigger0, unsigned char trigger1);
SR_API int ds_trigger_set_stage(uint16_t stages);
SR_API int ds_trigger_set_pos(uint16_t position);
SR_API uint16_t ds_trigger_get_pos();
SR_API int ds_trigger_set_en(uint16_t enable);
SR_API uint16_t ds_trigger_get_en();
SR_API int ds_trigger_set_mode(uint16_t mode);
 
/*--- log.c -----------------------------------------------------------------*/

/**
 * Use a shared context, and drop the private log context
 */
SR_API void ds_log_set_context(xlog_context *ctx); 

/**
 * Set the private log context level
 */
SR_API void ds_log_level(int level);


/*---event define ---------------------------------------------*/
enum dslib_event_type
{
	// A new device attached, user need to call ds_device_get_list to get the list,
	// the last one is new.
	// User can call ds_device_select() to switch to the current device.
	DS_EV_NEW_DEVICE_ATTACH = 0, 

	// The current device detached, user need to call ds_device_get_list to get the list,
	// and call ds_device_select() to switch to the current device.
	DS_EV_CURRENT_DEVICE_DETACH = 1, 

	// A inactive device detached.
	// User can call ds_device_get_list() to get the new list, and update the list view.
	DS_EV_INACTIVE_DEVICE_DETACH = 2,

	// The current device switch success.
	// User can call ds_device_get_list() to get new list, and update the data view.
	DS_EV_CURRENT_DEVICE_CHANGED = 3,
};

typedef unsigned long long ds_device_handle;

/**
 * Device base info
 */
struct ds_device_info
{
	ds_device_handle handle;
	char 	name[50];
	int 	dev_type; // enum sr_device_type
	int 	is_current; //is actived
};

struct ds_task_progress
{
	int progress;
	int is_end;
};

struct ds_store_extra_data
{
	char name[50];
	char *data;
	int  data_length;
};

/*-----------------trigger---------------*/
int ds_trigger_is_enabled();

/*-----------------channel---------------*/
/**
 *  heck that at least one probe is enabled
 */
int ds_channel_is_enabled();


/*---lib_main.c -----------------------------------------------*/

/**
 * event see enum libsigrok_event_type
 */
typedef void (*dslib_event_callback_t)(int event);

/**
 * Data forwarding callback collected by the device.
 */
typedef void (*ds_datafeed_callback_t)(const struct sr_dev_inst *sdi,
						const struct sr_datafeed_packet *packet);

/**
 * Must call first
 */
SR_API int ds_lib_init();

/**
 * Free all resource before program exits
 */
SR_API int ds_lib_exit(); 

/**
 * Set event callback, event type see enum libsigrok_event_type
 */
SR_API void ds_set_event_callback(dslib_event_callback_t cb);

/**
 * Set the data receive callback.
 */
SR_API int ds_set_datafeed_callback(ds_datafeed_callback_t cb);

/**
 * Set the firmware binary file directory,
 * User must call it to set the firmware resource directory
 */
SR_API void ds_set_firmware_resource_dir(const char *dir);

/**
 * Get the device list, if the field _handle is 0, the list visited to end.
 * User need call free() to release the buffer. If the list is empty, the out_list is null.
 */
SR_API int ds_device_get_list(struct ds_device_info** out_list, int *out_count);

/**
 * Active a device, if success, it will trigs the event of SR_EV_CURRENT_DEVICE_CHANGED.
 */
SR_API int ds_device_select(ds_device_handle handle);

/**
 * Active a device, if success, it will trigs the event of SR_EV_CURRENT_DEVICE_CHANGED.
 * @index is -1, will select the last one.
 */
SR_API int ds_device_select_by_index(int index);

/**
 * Create a device from session file, it auto load the data.
 */
SR_API int ds_device_from_file(const char *file_path);

/**
 * Remove one device from the list, and destory it.
 * User need to call ds_device_get_list() to get the new list.
 */
SR_API int ds_remove_device(ds_device_handle handle);

/**
 * Get the current device info.
 * If the current device is not exists, the handle filed will be set null.
 */
SR_API int ds_get_current_device_info(struct ds_device_info *info);

/**
 * Start collect data
 */
SR_API int ds_device_start_collect();

/**
 * Stop collect data
 */
SR_API int ds_device_stop_collect();


#ifdef __cplusplus
}
#endif

#endif
