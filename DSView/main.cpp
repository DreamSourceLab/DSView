/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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


#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h> /* First, so we avoid a _POSIX_C_SOURCE warning. */
#endif

#include <stdint.h>
#include <libsigrok4DSL/libsigrok.h>

#include <getopt.h>

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QIcon>

#include "dsapplication.h"
#include "pv/devicemanager.h"
#include "pv/mainframe.h"

#include "config.h"

void usage()
{
	fprintf(stdout,
		"Usage:\n"
		"  %s [OPTION…] [FILE] — %s\n"
		"\n"
		"Help Options:\n"
		"  -l, --loglevel                  Set libsigrok/libsigrokdecode loglevel\n"
		"  -V, --version                   Show release version\n"
		"  -h, -?, --help                  Show help option\n"
		"\n", DS_BIN_NAME, DS_DESCRIPTION);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	struct sr_context *sr_ctx = NULL;
	const char *open_file = NULL;

    DSApplication a(argc, argv);

    // Set some application metadata
    QApplication::setApplicationVersion(DS_VERSION_STRING);
    QApplication::setApplicationName("DSView");
    QApplication::setOrganizationDomain("http://www.DreamSourceLab.com");

	// Parse arguments
	while (1) {
		static const struct option long_options[] = {
			{"loglevel", required_argument, 0, 'l'},
			{"version", no_argument, 0, 'V'},
			{"help", no_argument, 0, 'h'},
			{0, 0, 0, 0}
		};

		const int c = getopt_long(argc, argv,
			"l:Vh?", long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'l':
		{
			const int loglevel = atoi(optarg);
			sr_log_loglevel_set(loglevel);

#ifdef ENABLE_DECODE
			srd_log_loglevel_set(loglevel);
#endif

			break;
		}

		case 'V':
			// Print version info
			fprintf(stdout, "%s %s\n", DS_TITLE, DS_VERSION_STRING);
			return 0;

		case 'h':
		case '?':
			usage();
			return 0;
		}
	}

	if (argc - optind > 1) {
		fprintf(stderr, "Only one file can be openened.\n");
		return 1;
	} else if (argc - optind == 1)
		open_file = argv[argc - 1];

	// Initialise libsigrok
	if (sr_init(&sr_ctx) != SR_OK) {
		qDebug() << "ERROR: libsigrok init failed.";
		return 1;
	}

	do {
#ifdef ENABLE_DECODE
		// Initialise libsigrokdecode
		if (srd_init(NULL) != SRD_OK) {
			qDebug() << "ERROR: libsigrokdecode init failed.";
			break;
		}

		// Load the protocol decoders
		srd_decoder_load_all();
#endif

		try {
			// Create the device manager, initialise the drivers
			pv::DeviceManager device_manager(sr_ctx);

            // Initialise the main frame
            pv::MainFrame w(device_manager, open_file);
            //QFile qss(":/stylesheet.qss");
            QFile qss(":darkstyle/style.qss");
            qss.open(QFile::ReadOnly);
            a.setStyleSheet(qss.readAll());
            qss.close();
			w.show();

			// Run the application
			ret = a.exec();

		} catch(std::exception e) {
			qDebug() << e.what();
		}

#ifdef ENABLE_DECODE
		// Destroy libsigrokdecode
		srd_exit();
#endif

	} while (0);

	// Destroy libsigrok
	if (sr_ctx)
		sr_exit(sr_ctx);

	return ret;
}
