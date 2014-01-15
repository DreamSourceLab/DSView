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

#include <stdio.h>
#include <sys/time.h>
#include <stdint.h>
#include <inttypes.h>
#include <glib.h>

#ifdef WIN32
#define WINVER 0x0500
#define _WIN32_WINNT WINVER
#include <Winsock2.h>
#else
#include <sys/time.h>
#endif

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

/** Status/error codes returned by libsigrok functions. */
enum {
	SR_OK             =  0, /**< No error. */
	SR_ERR            = -1, /**< Generic/unspecified error. */
	SR_ERR_MALLOC     = -2, /**< Malloc/calloc/realloc error. */
	SR_ERR_ARG        = -3, /**< Function argument error. */
	SR_ERR_BUG        = -4, /**< Errors hinting at internal bugs. */
	SR_ERR_SAMPLERATE = -5, /**< Incorrect samplerate. */
	SR_ERR_NA         = -6, /**< Not applicable. */
	SR_ERR_DEV_CLOSED = -7, /**< Device is closed, but needs to be open. */

	/*
	 * Note: When adding entries here, don't forget to also update the
	 * sr_strerror() and sr_strerror_name() functions in error.c.
	 */
};

#define SR_MAX_PROBENAME_LEN 32
#define DS_MAX_ANALOG_PROBES_NUM 8
#define TriggerStages 16
#define TriggerProbes 16
#define TriggerCountBits 16

/* Handy little macros */
#define SR_HZ(n)  (n)
#define SR_KHZ(n) ((n) * (uint64_t)(1000ULL))
#define SR_MHZ(n) ((n) * (uint64_t)(1000000ULL))
#define SR_GHZ(n) ((n) * (uint64_t)(1000000000ULL))

#define SR_HZ_TO_NS(n) ((uint64_t)(1000000000ULL) / (n))

/** libsigrok loglevels. */
enum {
	SR_LOG_NONE = 0, /**< Output no messages at all. */
	SR_LOG_ERR  = 1, /**< Output error messages. */
	SR_LOG_WARN = 2, /**< Output warnings. */
	SR_LOG_INFO = 3, /**< Output informational messages. */
	SR_LOG_DBG  = 4, /**< Output debug messages. */
	SR_LOG_SPEW = 5, /**< Output very noisy debug messages. */
};

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

typedef int (*sr_receive_data_callback_t)(int fd, int revents, const struct sr_dev_inst *sdi);

/** Data types used by sr_config_info(). */
enum {
	SR_T_UINT64 = 10000,
	SR_T_CHAR,
	SR_T_BOOL,
	SR_T_FLOAT,
	SR_T_RATIONAL_PERIOD,
	SR_T_RATIONAL_VOLT,
	SR_T_KEYVALUE,
};

/** Value for sr_datafeed_packet.type. */
enum {
	SR_DF_HEADER = 10000,
	SR_DF_END,
	SR_DF_META,
	SR_DF_TRIGGER,
	SR_DF_LOGIC,
	SR_DF_ANALOG,
	SR_DF_FRAME_BEGIN,
	SR_DF_FRAME_END,
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

struct sr_context;

struct sr_datafeed_packet {
	uint16_t type;
	const void *payload;
};

struct sr_datafeed_header {
	int feed_version;
	struct timeval starttime;
};

struct sr_datafeed_meta {
	GSList *config;
};

struct sr_datafeed_logic {
	uint64_t length;
	uint16_t unitsize;
    uint16_t data_error;
	void *data;
};

struct sr_datafeed_trigger {

};

struct sr_datafeed_analog {
	/** The probes for which data is included in this packet. */
	GSList *probes;
	int num_samples;
	/** Measured quantity (voltage, current, temperature, and so on). */
	int mq;
	/** Unit in which the MQ is measured. */
	int unit;
	/** Bitmap with extra information about the MQ. */
	uint64_t mqflags;
	/** The analog value(s). The data is interleaved according to
	 * the probes list. */
	float *data;
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
	 * @return SR_OK upon success, a negative error code upon failure.
	 */
	int (*loadfile) (struct sr_input *in, const char *filename);
};

/** Output (file) format struct. */
struct sr_output {
	/**
	 * A pointer to this output format's 'struct sr_output_format'.
	 * The frontend can use this to call the module's callbacks.
	 */
	struct sr_output_format *format;

	/**
	 * The device for which this output module is creating output. This
	 * can be used by the module to find out probe names and numbers.
	 */
	struct sr_dev_inst *sdi;

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
	void *internal;
};

struct sr_output_format {
	/**
	 * A unique ID for this output format. Must not be NULL.
	 *
	 * It can be used by frontends to select this output format for use.
	 *
	 * For example, calling sigrok-cli with <code>-O hex</code> will
	 * select the hexadecimal text output format.
	 */
	char *id;

	/**
	 * A short description of the output format. Must not be NULL.
	 *
	 * This can be displayed by frontends, e.g. when selecting the output
	 * format for saving a file.
	 */
	char *description;

	int df_type;

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
	 * @return SR_OK upon success, a negative error code otherwise.
	 */
	int (*init) (struct sr_output *o);

	/**
	 * Whenever a chunk of data comes in, it will be passed to the
	 * output module via this function. The <code>data_in</code> and
	 * <code>length_in</code> values refers to this data; the module
	 * must not alter or g_free() this buffer.
	 *
	 * The function must allocate a buffer for storing its output, and
	 * pass along a pointer to this buffer in the <code>data_out</code>
	 * parameter, as well as storing the length of the buffer in
	 * <code>length_out</code>. The calling frontend will g_free()
	 * this buffer when it's done with it.
	 *
	 * IMPORTANT: The memory allocation much happen using a glib memory
	 * allocation call (not a "normal" malloc) since g_free() will be
	 * used to free the memory!
	 *
	 * If there is no output, this function MUST store NULL in the
	 * <code>data_out</code> parameter, so the caller knows not to try
	 * and g_free() it.
	 *
	 * Note: This API call is obsolete, use receive() instead.
	 *
	 * @param o Pointer to the respective 'struct sr_output'.
	 * @param data_in Pointer to the input data buffer.
	 * @param length_in Length of the input.
	 * @param data_out Pointer to the allocated output buffer.
	 * @param length_out Length (in bytes) of the output.
	 *
	 * @return SR_OK upon success, a negative error code otherwise.
	 */
	int (*data) (struct sr_output *o, const uint8_t *data_in,
		     uint64_t length_in, uint8_t **data_out,
		     uint64_t *length_out);

	/**
	 * This function is called when an event occurs in the datafeed
	 * which the output module may need to be aware of. No data is
	 * passed in, only the fact that the event occurs. The following
	 * events can currently be passed in:
	 *
	 *  - SR_DF_TRIGGER: At this point in the datafeed, the trigger
	 *    matched. The output module may mark this in some way, e.g. by
	 *    plotting a red line on a graph.
	 *
	 *  - SR_DF_END: This marks the end of the datafeed. No more calls
	 *    into the output module will be done, so this is a good time to
	 *    free up any memory used to keep state, for example.
	 *
	 * Any output generated by this function must have a reference to
	 * it stored in the <code>data_out</code> and <code>length_out</code>
	 * parameters, or NULL if no output was generated.
	 *
	 * Note: This API call is obsolete, use receive() instead.
	 *
	 * @param o Pointer to the respective 'struct sr_output'.
	 * @param event_type Type of event that occured.
	 * @param data_out Pointer to the allocated output buffer.
	 * @param length_out Length (in bytes) of the output.
	 *
	 * @return SR_OK upon success, a negative error code otherwise.
	 */
	int (*event) (struct sr_output *o, int event_type, uint8_t **data_out,
			uint64_t *length_out);

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
	 * @return SR_OK upon success, a negative error code otherwise.
	 */
	int (*receive) (struct sr_output *o, const struct sr_dev_inst *sdi,
			const struct sr_datafeed_packet *packet, GString **out);

	/**
	 * This function is called after the caller is finished using
	 * the output module, and can be used to free any internal
	 * resources the module may keep.
	 *
	 * @return SR_OK upon success, a negative error code otherwise.
	 */
	int (*cleanup) (struct sr_output *o);
};

enum {
	SR_PROBE_LOGIC = 10000,
	SR_PROBE_ANALOG,
};

enum {
    LOGIC = 0,
    ANALOG = 1,
};

static const char *mode_strings[] = {
    "Logic Analyzer",
    "Oscilloscope",
};

struct sr_probe {
	/* The index field will go: use g_slist_length(sdi->probes) instead. */
	int index;
	int type;
	gboolean enabled;
	char *name;
	char *trigger;
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
	char *description;
};

enum {
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
    SR_CONF_DEVICE_MODE,

	/** The device supports setting a pattern (pattern generator mode). */
	SR_CONF_PATTERN_MODE,

	/** The device supports Run Length Encoding. */
	SR_CONF_RLE,

	/** The device supports setting trigger slope. */
	SR_CONF_TRIGGER_SLOPE,

	/** Trigger source. */
	SR_CONF_TRIGGER_SOURCE,

	/** Horizontal trigger position. */
	SR_CONF_HORIZ_TRIGGERPOS,

	/** Buffer size. */
	SR_CONF_BUFFERSIZE,

	/** Time base. */
	SR_CONF_TIMEBASE,

	/** Filter. */
	SR_CONF_FILTER,

	/** Volts/div. */
	SR_CONF_VDIV,

	/** Coupling. */
	SR_CONF_COUPLING,

	/** Trigger types.  */
	SR_CONF_TRIGGER_TYPE,

	/** The device supports setting its sample interval, in ms. */
	SR_CONF_SAMPLE_INTERVAL,

	/** Number of timebases, as related to SR_CONF_TIMEBASE.  */
	SR_CONF_NUM_TIMEBASE,

	/** Number of vertical divisions, as related to SR_CONF_VDIV.  */
	SR_CONF_NUM_VDIV,

    /** clock type (internal/external) */
    SR_CONF_CLOCK_TYPE,

	/*--- Special stuff -------------------------------------------------*/

	/** Scan options supported by the driver. */
	SR_CONF_SCAN_OPTIONS = 40000,

	/** Device options for a particular device. */
	SR_CONF_DEVICE_OPTIONS,
    SR_CONF_DEVICE_CONFIGS,

	/** Session filename. */
	SR_CONF_SESSIONFILE,

	/** The device supports specifying a capturefile to inject. */
	SR_CONF_CAPTUREFILE,

	/** The device supports specifying the capturefile unit size. */
	SR_CONF_CAPTURE_UNITSIZE,

	/** The device supports setting the number of probes. */
	SR_CONF_CAPTURE_NUM_PROBES,

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

struct sr_dev_inst {
	struct sr_dev_driver *driver;
	int index;
	int status;
	int inst_type;
    int mode;
	char *vendor;
	char *model;
	char *version;
	GSList *probes;
	void *conn;
	void *priv;
};

/** Types of device instances (sr_dev_inst). */
enum {
	/** Device instance type for USB devices. */
	SR_INST_USB = 10000,
	/** Device instance type for serial port devices. */
	SR_INST_SERIAL,
};

/** Device instance status. */
enum {
	/** The device instance was not found. */
	SR_ST_NOT_FOUND = 10000,
	/** The device instance was found, but is still booting. */
	SR_ST_INITIALIZING,
	/** The device instance is live, but not in use. */
	SR_ST_INACTIVE,
	/** The device instance is actively in use in a session. */
	SR_ST_ACTIVE,
	/** The device is winding down its session. */
	SR_ST_STOPPING,
};

struct sr_dev_driver {
	/* Driver-specific */
	char *name;
	char *longname;
	int api_version;
	int (*init) (struct sr_context *sr_ctx);
	int (*cleanup) (void);
	GSList *(*scan) (GSList *options);
	GSList *(*dev_list) (void);
	int (*dev_clear) (void);
	int (*config_get) (int id, GVariant **data,
			const struct sr_dev_inst *sdi);
	int (*config_set) (int id, GVariant *data,
			const struct sr_dev_inst *sdi);
	int (*config_list) (int info_id, GVariant **data,
			const struct sr_dev_inst *sdi);

	/* Device-specific */
	int (*dev_open) (struct sr_dev_inst *sdi);
	int (*dev_close) (struct sr_dev_inst *sdi);
    int (*dev_test) (struct sr_dev_inst *sdi);
	int (*dev_acquisition_start) (const struct sr_dev_inst *sdi,
			void *cb_data);
	int (*dev_acquisition_stop) (struct sr_dev_inst *sdi,
			void *cb_data);

	/* Dynamic */
	void *priv;
};

struct sr_session {
	/** List of struct sr_dev pointers. */
	GSList *devs;
	/** List of struct datafeed_callback pointers. */
	GSList *datafeed_callbacks;
	GTimeVal starttime;

	unsigned int num_sources;

	/*
	 * Both "sources" and "pollfds" are of the same size and contain pairs
	 * of descriptor and callback function. We can not embed the GPollFD
	 * into the source struct since we want to be able to pass the array
	 * of all poll descriptors to g_poll().
	 */
	struct source *sources;
	GPollFD *pollfds;
	int source_timeout;

	/*
	 * These are our synchronization primitives for stopping the session in
	 * an async fashion. We need to make sure the session is stopped from
	 * within the session thread itself.
	 */
//	GMutex stop_mutex;
	gboolean abort_session;
};

enum {
    SIMPLE_TRIGGER = 0,
    ADV_TRIGGER,
};

struct ds_trigger {
    uint16_t trigger_en;
    uint16_t trigger_mode;
    uint16_t trigger_pos;
    uint16_t trigger_stages;
    unsigned char trigger_logic[TriggerStages+1];
    unsigned char trigger0_inv[TriggerStages+1];
    unsigned char trigger1_inv[TriggerStages+1];
    char trigger0[TriggerStages+1][TriggerProbes];
    char trigger1[TriggerStages+1][TriggerProbes];
    uint16_t trigger0_count[TriggerStages+1];
    uint16_t trigger1_count[TriggerStages+1];
};

struct ds_trigger_pos {
    uint32_t real_pos;
    uint32_t ram_saddr;
    unsigned char first_block[504];
};

//struct libusbhp_t;
typedef void (*libusbhp_hotplug_cb_fn)(struct libusbhp_device_t *device,
                                     void *user_data);
#ifdef __linux__
#include <libudev.h>

struct dev_list_t {
  char *path;
  unsigned short vid;
  unsigned short pid;
  struct dev_list_t *next;
};
#endif/*__linux__*/

struct libusbhp_t {
#ifdef __linux__
  struct udev* hotplug;
  struct udev_monitor* hotplug_monitor;
  struct dev_list_t *devlist;
#endif/*__linux__*/
#ifdef _WIN32
  HWND hwnd;
  HDEVNOTIFY hDeviceNotify;
  WNDCLASSEX wcex;
#endif/*_WIN32*/
  libusbhp_hotplug_cb_fn attach;
  libusbhp_hotplug_cb_fn detach;
  void *user_data;
};

struct libusbhp_device_t {
    unsigned short idVendor;
    unsigned short idProduct;
};

#include "proto.h"
#include "version.h"

#ifdef __cplusplus
}
#endif

#endif
