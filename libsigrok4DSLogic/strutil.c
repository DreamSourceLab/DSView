/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "strutil: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

/**
 * @file
 *
 * Helper functions for handling or converting libsigrok-related strings.
 */

/**
 * @defgroup grp_strutil String utilities
 *
 * Helper functions for handling or converting libsigrok-related strings.
 *
 * @{
 */

/**
 * Convert a numeric value value to its "natural" string representation.
 * in SI units
 *
 * E.g. a value of 3000000, with units set to "W", would be converted
 * to "3 MW", 20000 to "20 kW", 31500 would become "31.5 kW".
 *
 * @param x The value to convert.
 * @param unit The unit to append to the string, or NULL if the string
 *             has no units.
 *
 * @return A g_try_malloc()ed string representation of the samplerate value,
 *         or NULL upon errors. The caller is responsible to g_free() the
 *         memory.
 */
SR_API char *sr_si_string_u64(uint64_t x, const char *unit)
{
	if (unit == NULL)
		unit = "";

	if ((x >= SR_GHZ(1)) && (x % SR_GHZ(1) == 0)) {
		return g_strdup_printf("%" PRIu64 " G%s", x / SR_GHZ(1), unit);
	} else if ((x >= SR_GHZ(1)) && (x % SR_GHZ(1) != 0)) {
		return g_strdup_printf("%" PRIu64 ".%" PRIu64 " G%s",
				       x / SR_GHZ(1), x % SR_GHZ(1), unit);
	} else if ((x >= SR_MHZ(1)) && (x % SR_MHZ(1) == 0)) {
		return g_strdup_printf("%" PRIu64 " M%s",
				       x / SR_MHZ(1), unit);
	} else if ((x >= SR_MHZ(1)) && (x % SR_MHZ(1) != 0)) {
		return g_strdup_printf("%" PRIu64 ".%" PRIu64 " M%s",
				    x / SR_MHZ(1), x % SR_MHZ(1), unit);
	} else if ((x >= SR_KHZ(1)) && (x % SR_KHZ(1) == 0)) {
		return g_strdup_printf("%" PRIu64 " k%s",
				    x / SR_KHZ(1), unit);
	} else if ((x >= SR_KHZ(1)) && (x % SR_KHZ(1) != 0)) {
		return g_strdup_printf("%" PRIu64 ".%" PRIu64 " k%s",
				    x / SR_KHZ(1), x % SR_KHZ(1), unit);
	} else {
		return g_strdup_printf("%" PRIu64 " %s", x, unit);
	}

	sr_err("%s: Error creating SI units string.", __func__);
	return NULL;
}

/**
 * Convert a numeric samplerate value to its "natural" string representation.
 *
 * E.g. a value of 3000000 would be converted to "3 MHz", 20000 to "20 kHz",
 * 31500 would become "31.5 kHz".
 *
 * @param samplerate The samplerate in Hz.
 *
 * @return A g_try_malloc()ed string representation of the samplerate value,
 *         or NULL upon errors. The caller is responsible to g_free() the
 *         memory.
 */
SR_API char *sr_samplerate_string(uint64_t samplerate)
{
	return sr_si_string_u64(samplerate, "Hz");
}

/**
 * Convert a numeric frequency value to the "natural" string representation
 * of its period.
 *
 * E.g. a value of 3000000 would be converted to "3 us", 20000 to "50 ms".
 *
 * @param frequency The frequency in Hz.
 *
 * @return A g_try_malloc()ed string representation of the frequency value,
 *         or NULL upon errors. The caller is responsible to g_free() the
 *         memory.
 */
SR_API char *sr_period_string(uint64_t frequency)
{
	char *o;
	int r;

	/* Allocate enough for a uint64_t as string + " ms". */
	if (!(o = g_try_malloc0(30 + 1))) {
		sr_err("%s: o malloc failed", __func__);
		return NULL;
	}

	if (frequency >= SR_GHZ(1))
		r = snprintf(o, 30, "%" PRIu64 " ns", frequency / 1000000000);
	else if (frequency >= SR_MHZ(1))
		r = snprintf(o, 30, "%" PRIu64 " us", frequency / 1000000);
	else if (frequency >= SR_KHZ(1))
		r = snprintf(o, 30, "%" PRIu64 " ms", frequency / 1000);
	else
		r = snprintf(o, 30, "%" PRIu64 " s", frequency);

	if (r < 0) {
		/* Something went wrong... */
		g_free(o);
		return NULL;
	}

	return o;
}

/**
 * Convert a numeric voltage value to the "natural" string representation
 * of its voltage value. The voltage is specified as a rational number's
 * numerator and denominator.
 *
 * E.g. a value of 300000 would be converted to "300mV", 2 to "2V".
 *
 * @param v_p The voltage numerator.
 * @param v_q The voltage denominator.
 *
 * @return A g_try_malloc()ed string representation of the voltage value,
 *         or NULL upon errors. The caller is responsible to g_free() the
 *         memory.
 */
SR_API char *sr_voltage_string(uint64_t v_p, uint64_t v_q)
{
	int r;
	char *o;

	if (!(o = g_try_malloc0(30 + 1))) {
		sr_err("%s: o malloc failed", __func__);
		return NULL;
	}

	if (v_q == 1000)
		r = snprintf(o, 30, "%" PRIu64 "mV", v_p);
	else if (v_q == 1)
		r = snprintf(o, 30, "%" PRIu64 "V", v_p);
	else
		r = snprintf(o, 30, "%gV", (float)v_p / (float)v_q);

	if (r < 0) {
		/* Something went wrong... */
		g_free(o);
		return NULL;
	}

	return o;
}

/**
 * Parse a trigger specification string.
 *
 * @param sdi The device instance for which the trigger specification is
 *            intended. Must not be NULL. Also, sdi->driver and
 *            sdi->driver->info_get must not be NULL.
 * @param triggerstring The string containing the trigger specification for
 *        one or more probes of this device. Entries for multiple probes are
 *        comma-separated. Triggers are specified in the form key=value,
 *        where the key is a probe number (or probe name) and the value is
 *        the requested trigger type. Valid trigger types currently
 *        include 'r' (rising edge), 'f' (falling edge), 'c' (any pin value
 *        change), '0' (low value), or '1' (high value).
 *        Example: "1=r,sck=f,miso=0,7=c"
 *
 * @return Pointer to a list of trigger types (strings), or NULL upon errors.
 *         The pointer list (if non-NULL) has as many entries as the
 *         respective device has probes (all physically available probes,
 *         not just enabled ones). Entries of the list which don't have
 *         a trigger value set in 'triggerstring' are NULL, the other entries
 *         contain the respective trigger type which is requested for the
 *         respective probe (e.g. "r", "c", and so on).
 */
SR_API char **sr_parse_triggerstring(const struct sr_dev_inst *sdi,
				     const char *triggerstring)
{
	GSList *l;
	GVariant *gvar;
	struct sr_probe *probe;
	int max_probes, probenum, i;
	char **tokens, **triggerlist, *trigger, *tc;
	const char *trigger_types;
	gboolean error;

	max_probes = g_slist_length(sdi->probes);
	error = FALSE;

	if (!(triggerlist = g_try_malloc0(max_probes * sizeof(char *)))) {
		sr_err("%s: triggerlist malloc failed", __func__);
		return NULL;
	}

	if (sdi->driver->config_list(SR_CONF_TRIGGER_TYPE, &gvar, sdi) != SR_OK) {
		sr_err("%s: Device doesn't support any triggers.", __func__);
		return NULL;
	}
	trigger_types = g_variant_get_string(gvar, NULL);

	tokens = g_strsplit(triggerstring, ",", max_probes);
	for (i = 0; tokens[i]; i++) {
		probenum = -1;
		for (l = sdi->probes; l; l = l->next) {
			probe = (struct sr_probe *)l->data;
			if (probe->enabled
				&& !strncmp(probe->name, tokens[i],
					strlen(probe->name))) {
				probenum = probe->index;
				break;
			}
		}

		if (probenum < 0 || probenum >= max_probes) {
			sr_err("Invalid probe.");
			error = TRUE;
			break;
		}

		if ((trigger = strchr(tokens[i], '='))) {
			for (tc = ++trigger; *tc; tc++) {
				if (strchr(trigger_types, *tc) == NULL) {
					sr_err("Unsupported trigger "
					       "type '%c'.", *tc);
					error = TRUE;
					break;
				}
			}
			if (!error)
				triggerlist[probenum] = g_strdup(trigger);
		}
	}
	g_strfreev(tokens);
	g_variant_unref(gvar);

	if (error) {
		for (i = 0; i < max_probes; i++)
			g_free(triggerlist[i]);
		g_free(triggerlist);
		triggerlist = NULL;
	}

	return triggerlist;
}

/**
 * Convert a "natural" string representation of a size value to uint64_t.
 *
 * E.g. a value of "3k" or "3 K" would be converted to 3000, a value
 * of "15M" would be converted to 15000000.
 *
 * Value representations other than decimal (such as hex or octal) are not
 * supported. Only 'k' (kilo), 'm' (mega), 'g' (giga) suffixes are supported.
 * Spaces (but not other whitespace) between value and suffix are allowed.
 *
 * @param sizestring A string containing a (decimal) size value.
 * @param size Pointer to uint64_t which will contain the string's size value.
 *
 * @return SR_OK upon success, SR_ERR upon errors.
 */
SR_API int sr_parse_sizestring(const char *sizestring, uint64_t *size)
{
	int multiplier, done;
	char *s;

	*size = strtoull(sizestring, &s, 10);
	multiplier = 0;
	done = FALSE;
	while (s && *s && multiplier == 0 && !done) {
		switch (*s) {
		case ' ':
			break;
		case 'k':
		case 'K':
			multiplier = SR_KHZ(1);
			break;
		case 'm':
		case 'M':
			multiplier = SR_MHZ(1);
			break;
		case 'g':
		case 'G':
			multiplier = SR_GHZ(1);
			break;
		default:
			done = TRUE;
			s--;
		}
		s++;
	}
	if (multiplier > 0)
		*size *= multiplier;

	if (*s && strcasecmp(s, "Hz"))
		return SR_ERR;

	return SR_OK;
}

/**
 * Convert a "natural" string representation of a time value to an
 * uint64_t value in milliseconds.
 *
 * E.g. a value of "3s" or "3 s" would be converted to 3000, a value
 * of "15ms" would be converted to 15.
 *
 * Value representations other than decimal (such as hex or octal) are not
 * supported. Only lower-case "s" and "ms" time suffixes are supported.
 * Spaces (but not other whitespace) between value and suffix are allowed.
 *
 * @param timestring A string containing a (decimal) time value.
 * @return The string's time value as uint64_t, in milliseconds.
 *
 * @todo Add support for "m" (minutes) and others.
 * @todo Add support for picoseconds?
 * @todo Allow both lower-case and upper-case? If no, document it.
 */
SR_API uint64_t sr_parse_timestring(const char *timestring)
{
	uint64_t time_msec;
	char *s;

	/* TODO: Error handling, logging. */

	time_msec = strtoull(timestring, &s, 10);
	if (time_msec == 0 && s == timestring)
		return 0;

	if (s && *s) {
		while (*s == ' ')
			s++;
		if (!strcmp(s, "s"))
			time_msec *= 1000;
		else if (!strcmp(s, "ms"))
			; /* redundant */
		else
			return 0;
	}

	return time_msec;
}

SR_API gboolean sr_parse_boolstring(const char *boolstr)
{
	if (!boolstr)
		return FALSE;

	if (!g_ascii_strncasecmp(boolstr, "true", 4) ||
	    !g_ascii_strncasecmp(boolstr, "yes", 3) ||
	    !g_ascii_strncasecmp(boolstr, "on", 2) ||
	    !g_ascii_strncasecmp(boolstr, "1", 1))
		return TRUE;

	return FALSE;
}

SR_API int sr_parse_period(const char *periodstr, uint64_t *p, uint64_t *q)
{
	char *s;

	*p = strtoull(periodstr, &s, 10);
	if (*p == 0 && s == periodstr)
		/* No digits found. */
		return SR_ERR_ARG;

	if (s && *s) {
		while (*s == ' ')
			s++;
		if (!strcmp(s, "fs"))
			*q = 1000000000000000ULL;
		else if (!strcmp(s, "ps"))
			*q = 1000000000000ULL;
		else if (!strcmp(s, "ns"))
			*q = 1000000000ULL;
		else if (!strcmp(s, "us"))
			*q = 1000000;
		else if (!strcmp(s, "ms"))
			*q = 1000;
		else if (!strcmp(s, "s"))
			*q = 1;
		else
			/* Must have a time suffix. */
			return SR_ERR_ARG;
	}

	return SR_OK;
}


SR_API int sr_parse_voltage(const char *voltstr, uint64_t *p, uint64_t *q)
{
	char *s;

	*p = strtoull(voltstr, &s, 10);
	if (*p == 0 && s == voltstr)
		/* No digits found. */
		return SR_ERR_ARG;

	if (s && *s) {
		while (*s == ' ')
			s++;
		if (!strcasecmp(s, "mv"))
			*q = 1000L;
		else if (!strcasecmp(s, "v"))
			*q = 1;
		else
			/* Must have a base suffix. */
			return SR_ERR_ARG;
	}

	return SR_OK;
}

/** @} */
