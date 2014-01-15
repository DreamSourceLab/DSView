/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010-2012 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2012 Peter Stuge <peter@stuge.se>
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

#include <glib.h>
#include "config.h" /* Needed for HAVE_LIBUSB_1_0 and others. */
#include "libsigrok.h"
#include "libsigrok-internal.h"

/**
 * @mainpage libsigrok API
 *
 * @section sec_intro Introduction
 *
 * The <a href="http://sigrok.org">sigrok</a> project aims at creating a
 * portable, cross-platform, Free/Libre/Open-Source signal analysis software
 * suite that supports various device types (such as logic analyzers,
 * oscilloscopes, multimeters, and more).
 *
 * <a href="http://sigrok.org/wiki/Libsigrok">libsigrok</a> is a shared
 * library written in C which provides the basic API for talking to
 * <a href="http://sigrok.org/wiki/Supported_hardware">supported hardware</a>
 * and reading/writing the acquired data into various
 * <a href="http://sigrok.org/wiki/Input_output_formats">input/output
 * file formats</a>.
 *
 * @section sec_api API reference
 *
 * See the "Modules" page for an introduction to various libsigrok
 * related topics and the detailed API documentation of the respective
 * functions.
 *
 * You can also browse the API documentation by file, or review all
 * data structures.
 *
 * @section sec_mailinglists Mailing lists
 *
 * There are two mailing lists for sigrok/libsigrok: <a href="https://lists.sourceforge.net/lists/listinfo/sigrok-devel">sigrok-devel</a> and <a href="https://lists.sourceforge.net/lists/listinfo/sigrok-commits">sigrok-commits</a>.
 *
 * @section sec_irc IRC
 *
 * You can find the sigrok developers in the
 * <a href="irc://chat.freenode.net/sigrok">\#sigrok</a>
 * IRC channel on Freenode.
 *
 * @section sec_website Website
 *
 * <a href="http://sigrok.org/wiki/Libsigrok">sigrok.org/wiki/Libsigrok</a>
 */

/**
 * @file
 *
 * Initializing and shutting down libsigrok.
 */

/**
 * @defgroup grp_init Initialization
 *
 * Initializing and shutting down libsigrok.
 *
 * Before using any of the libsigrok functionality, sr_init() must
 * be called to initialize the library, which will return a struct sr_context
 * when the initialization was successful.
 *
 * When libsigrok functionality is no longer needed, sr_exit() should be
 * called, which will (among other things) free the struct sr_context.
 *
 * Example for a minimal program using libsigrok:
 *
 * @code{.c}
 *   #include <stdio.h>
 *   #include <libsigrok/libsigrok.h>
 *
 *   int main(int argc, char **argv)
 *   {
 *   	int ret;
 *   	struct sr_context *sr_ctx;
 *
 *   	if ((ret = sr_init(&sr_ctx)) != SR_OK) {
 *   		printf("Error initializing libsigrok (%s): %s.",
 *   		       sr_strerror_name(ret), sr_strerror(ret));
 *   		return 1;
 *   	}
 *
 *   	// Use libsigrok functions here...
 *
 *   	if ((ret = sr_exit(sr_ctx)) != SR_OK) {
 *   		printf("Error shutting down libsigrok (%s): %s.",
 *   		       sr_strerror_name(ret), sr_strerror(ret));
 *   		return 1;
 *   	}
 *
 *   	return 0;
 *   }
 * @endcode
 *
 * @{
 */

/**
 * Sanity-check all libsigrok drivers.
 *
 * @return SR_OK if all drivers are OK, SR_ERR if one or more have issues.
 */
static int sanity_check_all_drivers(void)
{
	int i, errors, ret = SR_OK;
	struct sr_dev_driver **drivers;
	const char *d;

	sr_spew("Sanity-checking all drivers.");

	drivers = sr_driver_list();
	for (i = 0; drivers[i]; i++) {
		errors = 0;

		d = (drivers[i]->name) ? drivers[i]->name : "NULL";

		if (!drivers[i]->name) {
			sr_err("No name in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->longname) {
			sr_err("No longname in driver %d ('%s').", i, d);
			errors++;
		}
		if (drivers[i]->api_version < 1) {
			sr_err("API version in driver %d ('%s') < 1.", i, d);
			errors++;
		}
		if (!drivers[i]->init) {
			sr_err("No init in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->cleanup) {
			sr_err("No cleanup in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->scan) {
			sr_err("No scan in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->dev_list) {
			sr_err("No dev_list in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->dev_clear) {
			sr_err("No dev_clear in driver %d ('%s').", i, d);
			errors++;
		}
		/* Note: config_get() is optional. */
		if (!drivers[i]->config_set) {
			sr_err("No config_set in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->config_list) {
			sr_err("No config_list in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->dev_open) {
			sr_err("No dev_open in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->dev_close) {
			sr_err("No dev_close in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->dev_acquisition_start) {
			sr_err("No dev_acquisition_start in driver %d ('%s').",
			       i, d);
			errors++;
		}
		if (!drivers[i]->dev_acquisition_stop) {
			sr_err("No dev_acquisition_stop in driver %d ('%s').",
			       i, d);
			errors++;
		}

		/* Note: 'priv' is allowed to be NULL. */

		if (errors == 0)
			continue;

		ret = SR_ERR;
	}

	return ret;
}

/**
 * Sanity-check all libsigrok input modules.
 *
 * @return SR_OK if all modules are OK, SR_ERR if one or more have issues.
 */
static int sanity_check_all_input_modules(void)
{
	int i, errors, ret = SR_OK;
	struct sr_input_format **inputs;
	const char *d;

	sr_spew("Sanity-checking all input modules.");

	inputs = sr_input_list();
	for (i = 0; inputs[i]; i++) {
		errors = 0;

		d = (inputs[i]->id) ? inputs[i]->id : "NULL";

		if (!inputs[i]->id) {
			sr_err("No ID in module %d ('%s').", i, d);
			errors++;
		}
		if (!inputs[i]->description) {
			sr_err("No description in module %d ('%s').", i, d);
			errors++;
		}
		if (!inputs[i]->format_match) {
			sr_err("No format_match in module %d ('%s').", i, d);
			errors++;
		}
		if (!inputs[i]->init) {
			sr_err("No init in module %d ('%s').", i, d);
			errors++;
		}
		if (!inputs[i]->loadfile) {
			sr_err("No loadfile in module %d ('%s').", i, d);
			errors++;
		}

		if (errors == 0)
			continue;

		ret = SR_ERR;
	}

	return ret;
}

/**
 * Sanity-check all libsigrok output modules.
 *
 * @return SR_OK if all modules are OK, SR_ERR if one or more have issues.
 */
static int sanity_check_all_output_modules(void)
{
	int i, errors, ret = SR_OK;
	struct sr_output_format **outputs;
	const char *d;

	sr_spew("Sanity-checking all output modules.");

	outputs = sr_output_list();
	for (i = 0; outputs[i]; i++) {
		errors = 0;

		d = (outputs[i]->id) ? outputs[i]->id : "NULL";

		if (!outputs[i]->id) {
			sr_err("No ID in module %d ('%s').", i, d);
			errors++;
		}
		if (!outputs[i]->description) {
			sr_err("No description in module %d ('%s').", i, d);
			errors++;
		}
		if (outputs[i]->df_type < 10000 || outputs[i]->df_type > 10007) {
			sr_err("Invalid df_type %d in module %d ('%s').",
			       outputs[i]->df_type, i, d);
			errors++;
		}

		/* All modules must provide a data or recv API callback. */
		if (!outputs[i]->data && !outputs[i]->receive) {
			sr_err("No data/receive in module %d ('%s').", i, d);
			errors++;
		}

		/*
		 * Currently most API calls are optional (their function
		 * pointers can thus be NULL) in theory: init, event, cleanup.
		 */

		if (errors == 0)
			continue;

		ret = SR_ERR;
	}

	return ret;
}

/**
 * Initialize libsigrok.
 *
 * This function must be called before any other libsigrok function.
 *
 * @param ctx Pointer to a libsigrok context struct pointer. Must not be NULL.
 *            This will be a pointer to a newly allocated libsigrok context
 *            object upon success, and is undefined upon errors.
 *
 * @return SR_OK upon success, a (negative) error code otherwise. Upon errors
 *         the 'ctx' pointer is undefined and should not be used. Upon success,
 *         the context will be free'd by sr_exit() as part of the libsigrok
 *         shutdown.
 *
 * @since 0.1.0 (but the API changed in 0.2.0)
 */
SR_API int sr_init(struct sr_context **ctx)
{
	int ret = SR_ERR;
	struct sr_context *context;

	if (!ctx) {
		sr_err("%s(): libsigrok context was NULL.", __func__);
		return SR_ERR;
	}

	if (sanity_check_all_drivers() < 0) {
		sr_err("Internal driver error(s), aborting.");
		return ret;
	}

	if (sanity_check_all_input_modules() < 0) {
		sr_err("Internal input module error(s), aborting.");
		return ret;
	}

	if (sanity_check_all_output_modules() < 0) {
		sr_err("Internal output module error(s), aborting.");
		return ret;
	}

	/* + 1 to handle when struct sr_context has no members. */
	context = g_try_malloc0(sizeof(struct sr_context) + 1);

	if (!context) {
		ret = SR_ERR_MALLOC;
		goto done;
	}

#ifdef HAVE_LIBUSB_1_0
	ret = libusb_init(&context->libusb_ctx);
	if (LIBUSB_SUCCESS != ret) {
		sr_err("libusb_init() returned %s.\n", libusb_error_name(ret));
		ret = SR_ERR;
		goto done;
	}
#endif

	*ctx = context;
	context = NULL;
	ret = SR_OK;

done:
	if (context)
		g_free(context);
	return ret;
}

/**
 * Shutdown libsigrok.
 *
 * @param ctx Pointer to a libsigrok context struct. Must not be NULL.
 *
 * @return SR_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.1.0 (but the API changed in 0.2.0)
 */
SR_API int sr_exit(struct sr_context *ctx)
{
	if (!ctx) {
		sr_err("%s(): libsigrok context was NULL.", __func__);
		return SR_ERR;
	}

	sr_hw_cleanup_all();

#ifdef HAVE_LIBUSB_1_0
	libusb_exit(ctx->libusb_ctx);
#endif

	g_free(ctx);

	return SR_OK;
}

/** @} */
