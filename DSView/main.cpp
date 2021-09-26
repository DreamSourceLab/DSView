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

#include <libsigrok4DSL/libsigrok.h>
#ifdef ENABLE_DECODE
#include <libsigrokdecode4DSL/libsigrokdecode.h>
#endif

#include <stdint.h>
#include <getopt.h>

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTranslator>
#include <QSettings>
#include <QDesktopServices>
#include <QStyle>

#include "dsapplication.h"
#include "mystyle.h"
#include "pv/devicemanager.h"
#include "pv/mainframe.h"

#include "config.h"

char DS_RES_PATH[256];

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

    #if QT_VERSION >= QT_VERSION_CHECK(5,6,0)
        QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    #endif // QT_VERSION

    #ifdef _WIN32
        // Under Windows, we need to manually retrieve the command-line arguments and convert them from UTF-16 to UTF-8.
        // This prevents data loss if there are any characters that wouldn't fit in the local ANSI code page.
        int argcUTF16 = 0;
        LPWSTR* argvUTF16 = CommandLineToArgvW(GetCommandLineW(), &argcUTF16);

        std::vector<QByteArray> argvUTF8Q;

        std::for_each(argvUTF16, argvUTF16 + argcUTF16, [&argvUTF8Q](const LPWSTR& arg) {
            argvUTF8Q.emplace_back(QString::fromUtf16(reinterpret_cast<const char16_t*>(arg), -1).toUtf8());
        });

        LocalFree(argvUTF16);

        // Ms::runApplication() wants an argv-style array of raw pointers to the arguments, so let's create a vector of them.
        std::vector<char*> argvUTF8;

        for (auto& arg : argvUTF8Q)
              argvUTF8.push_back(arg.data());

        // Don't use the arguments passed to main(), because they're in the local ANSI code page.
        Q_UNUSED(argc);
        Q_UNUSED(argv);

        int argcFinal = argcUTF16;
        char** argvFinal = argvUTF8.data();
    #else
        int argcFinal = argc;
        char** argvFinal = argv;
    #endif

    QApplication a(argcFinal, argvFinal);
    a.setStyle(new MyStyle);

    // Set some application metadata
    QApplication::setApplicationVersion(DS_VERSION_STRING);
    QApplication::setApplicationName("DSView");
    QApplication::setOrganizationName("DreamSourceLab");
    QApplication::setOrganizationDomain("www.DreamSourceLab.com");

	// Parse arguments
	while (1) {
		static const struct option long_options[] = {
			{"loglevel", required_argument, 0, 'l'},
			{"version", no_argument, 0, 'V'},
			{"help", no_argument, 0, 'h'},
			{0, 0, 0, 0}
		};

        const int c = getopt_long(argcFinal, argvFinal,
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

    if (argcFinal - optind > 1) {
		fprintf(stderr, "Only one file can be openened.\n");
		return 1;
    } else if (argcFinal - optind == 1)
        open_file = argvFinal[argcFinal - 1];

	// Initialise DS_RES_PATH
    QDir dir(QCoreApplication::applicationDirPath());
    if (dir.cd("..") &&
        dir.cd("share") &&
        dir.cd(QApplication::applicationName()) &&
        dir.cd("res")) {
        QString res_dir = dir.absolutePath() + "/";
        strcpy(DS_RES_PATH, res_dir.toUtf8().data());
    } else {
        qDebug() << "ERROR: config files don't exist.";
        return 1;
    }

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
			w.show();
            w.readSettings();
            w.show_doc();

			// Run the application
            ret = a.exec();

        } catch(const std::exception &e) {
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
