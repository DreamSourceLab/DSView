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
#include <QDir>
#include <QStyle> 
#include <QGuiApplication>
#include <QScreen>
#include "mystyle.h" 
#include "appcore/mainframe.h"
#include "config/appconfig.h"
#include "config.h"
#include "appcore/appcontrol.h"
#include "log.h" 
#include "ui/langresource.h"
#include <QDateTime>
#include <string>
#include <ds_types.h>

#ifdef _WIN32
#include <windows.h>
#endif 

void usage()
{
	printf(
		"Usage:\n"
		"  %s [OPTION...] [FILE] - %s\n"
		"\n"
		"Help Options:\n"
		"  -l, --loglevel                  Set log level, value between 0 to 5\n"
		"  -v, -V, --version               Show release version\n"
		"  -s, --storelog                  Save log to locale file\n"
		"  -h, -?, --help                  Show help option\n"
		"\n", DS_BIN_NAME, DS_DESCRIPTION);
}

int main(int argc, char *argv[])
{   
	//return main2();
	int ret = 0; 
	const char *open_file = NULL;
	int logLevel = -1;
	bool bStoreLog = false;

	//----------------------rebuild command param
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
        for (auto& arg : argvUTF8Q){
            argvUTF8.push_back(arg.data());
		}

        // Don't use the arguments passed to main(), because they're in the local ANSI code page.
        (void*)(argc);
        (void*)(argv);

        int argcFinal = argcUTF16;
        char** argvFinal = argvUTF8.data();
    #else
        int argcFinal = argc;
        char** argvFinal = argv;
    #endif 
 
	//----------------------command param parse
	while (1) {
		static const struct option long_options[] = {
			{"loglevel", required_argument, 0, 'l'},
			{"version", no_argument, 0, 'v'},
			{"storelog", no_argument, 0, 's'},
			{"help", no_argument, 0, 'h'},
			{0, 0, 0, 0}
		};

        const char *shortopts = "l:Vvhs?";
        const int c = getopt_long(argcFinal, argvFinal, shortopts, long_options, NULL);
		if (c == -1)
			break;

		switch (c)
		{
		case 'l': // log level
			logLevel = atoi(optarg);
			break;

		case 's': // the store log flag
			bStoreLog = true;
			break;

		case 'V': // version
		case 'v':
			printf("%s %s\n", DS_TITLE, DS_VERSION_STRING);
			return 0;
 
		case 'h': // get help
		case '?':
			usage();
			return 0;
		}
	}

	if (argcFinal - optind > 1) {
		printf("Only one file can be openened.\n");
		return 1;
    } 
	else if (argcFinal - optind == 1){
        open_file = argvFinal[argcFinal - 1];		
	}

	//----------------------HightDpiScaling
#if QT_VERSION >= QT_VERSION_CHECK(5,6,0)
bool bHighScale = true;

#ifdef _WIN32
	int argc1 = 0;
 	QApplication *a1 = new QApplication(argc1, NULL);
	float sk = QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96;
	int sy = QGuiApplication::primaryScreen()->size().height(); //screen rect height
    delete a1;
	a1 = NULL;

	if (sk >= 1.5 && sy <= 1080){
		QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
        QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
		bHighScale = false;
	} 
#endif
	if (bHighScale){
		QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
      	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
	}
#endif 

	//----------------------init app
    QApplication a(argcFinal, argvFinal);
    a.setStyle(new MyStyle);

    // Set some application metadata
    QApplication::setApplicationVersion(DS_VERSION_STRING);
    QApplication::setApplicationName("DSView");
    QApplication::setOrganizationName("DreamSourceLab");
    QApplication::setOrganizationDomain("www.DreamSourceLab.com");

	//----------------------init log
	dsv::dsv_log_init(); // Don't call before QApplication be inited

	if (bStoreLog && logLevel < XLOG_LEVEL_DBG){
		logLevel = XLOG_LEVEL_DBG;
	}
	if (logLevel != -1){
		dsv::dsv_log_level(logLevel);
	}

	#ifdef DEBUG_INFO
		if (XLOG_LEVEL_INFO > logLevel){
            dsv::dsv_log_level(XLOG_LEVEL_INFO); // on develop mode, set the default log level
			logLevel = XLOG_LEVEL_INFO;
		}
	#endif

	if (bStoreLog){
		dsv::dsv_log_enalbe_logfile(true);
	} 

	auto control = dsv::appcore::AppControl::Instance();	
	dsv::config::AppConfig &app = dsv::config::AppConfig::Instance(); 
	app.LoadAll(); //load app config
	LangResource::Instance()->Load(app.frameOptions.language);

	if (app.appOptions.ableSaveLog){
		dsv::dsv_log_enalbe_logfile(app.appOptions.appendLogMode);

		if (app.appOptions.logLevel >= logLevel){
			dsv::dsv_log_level(app.appOptions.logLevel);
		}
	}

	//----------------------run
	dsv_info("----------------- version: %s-----------------", DS_VERSION_STRING);
	dsv_info("Qt:%s", QT_VERSION_STR);

	QDateTime dateTime = QDateTime::currentDateTime();
	std::string strTime = dateTime .toString("yyyy-MM-dd hh:mm:ss").toStdString();
	dsv_info("%s", strTime.c_str());

	int bit_width = sizeof(u64_t);
	if (bit_width != 8){
		dsv_err("Can only run on 64 bit systems");
		return 0;
	}
 
	//init core
	if (!control->Init()){ 
		dsv_err("init error!"); 
		return 1;
	}
	
	if (open_file != NULL){
		control->_open_file_name = open_file;
	}	

	try
	{   
		dsv::appcore::MainFrame w;
		control->Start();
		w.show(); 
		w.readSettings();
		w.show_doc();  //to show the dailog for open help document
		ret = a.exec(); //Run the application
		control->Stop();

		dsv_info("Main window closed.");
	}
	catch (const std::exception &e)
	{
        dsv_err("main() catch a except!");
		const char *exstr = e.what();
		dsv_err("%s", exstr);
	}

	control->UnInit();  //uninit
	control->Destroy();

	dsv_info("Uninit log.");

	dsv::dsv_log_uninit();
 
	return ret;
}
