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
 
#include <stdint.h>
#include <getopt.h>

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTranslator>
#include <QDesktopServices>
#include <QStyle> 

#include "dsapplication.h"
#include "mystyle.h" 
#include "pv/mainframe.h"
#include "pv/config/appconfig.h"
#include "config.h"
#include "pv/appcontrol.h"

#ifdef _WIN32
#include <windows.h>
#endif

//#include <libsigrok4DSL/libsigrok.h>

void usage()
{
	printf(
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
   // sr_test_usb_api();return 0;
	 
	int ret = 0; 
	const char *open_file = NULL;
 
    #if QT_VERSION >= QT_VERSION_CHECK(5,6,0)
		//On Windows, need to compile with the QT5 version of the library, which makes the interface slightly larger.
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

	qDebug()<<"\n----------------- version:"<<DS_VERSION_STRING<<"-----------------\n";

#ifdef Q_OS_LINUX
	// Use low version qt plugins, for able to debug
	QDir qtdir;
	qtdir.setPath(GetAppDataDir() + "/qt5.0/plugins");

	if (qtdir.exists()){
		printf("qt plugins root:%s\n", qtdir.absolutePath().toLatin1().data());
		QCoreApplication::addLibraryPath(qtdir.absolutePath());
	}	
#endif

	AppControl *control = AppControl::Instance();

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
			control->SetLogLevel(loglevel);
			break;
		}

		case 'V':
			// Print version info
			printf("%s %s\n", DS_TITLE, DS_VERSION_STRING);
			return 0;

		case 'h':
		case '?':
			usage();
			return 0;
		}
	}

    if (argcFinal - optind > 1) {
		printf("Only one file can be openened.\n");
		return 1;
    } else if (argcFinal - optind == 1){
        open_file = argvFinal[argcFinal - 1];
		control->_open_file_name = open_file;
	}

	
//#ifdef Q_OS_DARWIN 
//#endif

	//load app config
	AppConfig::Instance().LoadAll();

	//init core
	if (!control->Init()){
        printf("init error!");
        qDebug() << control->GetLastError();
		return 1;
	}

	try
	{   
		control->Start();
		
		// Initialise the main frame
        pv::MainFrame w;
		w.show(); 
 
		w.readSettings();
		 
		//to show the dailog for open help document
		w.show_doc();

		//Run the application
		ret = a.exec();

		control->Stop();
	}
	catch (const std::exception &e)
	{
        printf("main() catch a except!");
		const char *exstr = e.what();
		qDebug() << exstr;
	}

	//uninit
	control->UnInit();  
	control->Destroy();

	return ret;
}
