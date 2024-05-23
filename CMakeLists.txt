##
## This file is part of the DSView project.
##
## Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
## Copyright (C) 2012-2013 Alexandru Gagniuc <mr.nuke.me@gmail.com>
## Copyright (C) 2013-2022 DreamSourceLab <support@dreamsourcelab.com>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

cmake_minimum_required(VERSION 2.8.6)

project(DSView)

#===============================================================================
#= Config Header
#-------------------------------------------------------------------------------

set(DS_TITLE DSView)
set(DS_DESCRIPTION "A GUI for instruments of DreamSourceLab")

set(DS_VERSION_MAJOR 1)
set(DS_VERSION_MINOR 3)
set(DS_VERSION_MICRO 2)
set(DS_VERSION_STRING  ${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO})

configure_file (
	${PROJECT_SOURCE_DIR}/DSView/config.h.in
	${PROJECT_BINARY_DIR}/DSView/config.h
)

#===============================================================================
#=pkg config
#-------------------------------------------------------------------------------
include(FindPkgConfig)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake") 
#list(APPEND CMAKE_PREFIX_PATH "xxx.cmake find path") 

find_package(PkgConfig)

if(NOT PKG_CONFIG_FOUND)
	message(FATAL_ERROR  "Please install pkg-config!")
endif()

#===============================================================================
#= User Options
#-------------------------------------------------------------------------------

set(DISABLE_WERROR TRUE) #Build without -Werror
set(ENABLE_SIGNALS TRUE) #Build with UNIX signals
set(ENABLE_COTIRE FALSE) #Enable cotire
set(ENABLE_TESTS  FALSE) #Enable unit tests
set(STATIC_PKGDEPS_LIBS FALSE) #Statically link to (pkg-config) libraries

if(WIN32)
	# On Windows/MinGW we need to statically link to libraries.
	# This option is user configurable, but enable it by default on win32.
	set(STATIC_PKGDEPS_LIBS TRUE)

	# Windows does not support UNIX signals.
	set(ENABLE_SIGNALS FALSE)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif()

#===============================================================================
#= first search path
#-------------------------------------------------------------------------------
include_directories(
   ./DSView
   ./libsigrok4DSL
   ./libsigrokdecode4DSL
   ./common
)

#===============================================================================
#= glib-2.0
#-------------------------------------------------------------------------------
pkg_search_module(GLIB glib-2.0)

if(NOT GLIB_FOUND)
	message(FATAL_ERROR  "Please install glib!")
endif()

message("----- glib-2.0:")
message(STATUS "	 includes:" ${GLIB_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${GLIB_LIBDIR}/libglib-2.0.*)
include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBDIR})

#===============================================================================
#= python3
#-------------------------------------------------------------------------------

find_package(Python3 COMPONENTS Development QUIET)

if (Python3_FOUND)
	message("----- python3:")
	message(STATUS "	 includes:" ${Python3_INCLUDE_DIRS})
	message(STATUS "	 libraries:" ${Python3_LIBRARIES})
	include_directories(${Python3_INCLUDE_DIRS})
	set(PY_LIB ${Python3_LIBRARIES})
else()
     find_package(PythonLibs 3 QUIET)

     if(PYTHONLIBS_FOUND)
     	message("----- python(${PYTHONLIBS_VERSION_STRING}):")
	message(STATUS "	 includes:" ${PYTHON_INCLUDE_DIRS})
	message(STATUS "	 libraries:" ${PYTHON_LIBRARIES})
	include_directories(${PYTHON_INCLUDE_DIRS})
	set(PY_LIB ${PYTHON_LIBRARIES})
     else()
     	message(FATAL_ERROR  "Please install lib python3!")
     endif()
endif()
  
#===============================================================================
#= FFTW
#-------------------------------------------------------------------------------
find_package(FFTW)

if(NOT FFTW_FOUND)
	message(FATAL_ERROR  "Please install lib fftw!")
endif()

message("----- FFTW:")
message(STATUS "	 includes:" ${FFTW_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${FFTW_LIBRARIES})
include_directories(${FFTW_INCLUDE_DIRS})

#===============================================================================
#= libusb-1.0
#-------------------------------------------------------------------------------
find_package(libusb-1.0)

if(NOT LIBUSB_1_FOUND)
	message(FATAL_ERROR  "Please install libusb!")
endif()

message("----- libusb-1.0:")
message(STATUS "	 includes:" ${LIBUSB_1_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${LIBUSB_1_LIBRARIES})
include_directories(${LIBUSB_1_INCLUDE_DIRS})

#===============================================================================
#= zlib
#-------------------------------------------------------------------------------
find_package(ZLIB QUIET)

if(NOT ZLIB_FOUND)
	message(FATAL_ERROR  "Please install zlib!")
endif()

message("----- zlib:")
message(STATUS "	 includes:" ${ZLIB_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${ZLIB_LIBRARIES})
include_directories(${ZLIB_INCLUDE_DIRS})

#===============================================================================
#= Qt5 or Qt6
#-------------------------------------------------------------------------------

find_package(Qt5Core QUIET)

if(Qt5Core_FOUND)
	message("----- Qt5:")
	message(STATUS "	 includes:" ${Qt5Core_INCLUDE_DIRS})
	if (WIN32)
		find_package(Qt5WinExtras REQUIRED)
	endif ()
	find_package(Qt5Svg REQUIRED)
	find_package(Qt5Widgets REQUIRED)
	find_package(Qt5Gui REQUIRED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
	if (WIN32)
		set(QT_INCLUDE_DIRS ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5WinExtras_INCLUDE_DIRS})
		set(QT_LIBRARIES Qt5::Gui Qt5::Widgets Qt5::WinExtras Qt5::WinMain Qt5::Svg)
	else ()
		set(QT_INCLUDE_DIRS ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS})
		set(QT_LIBRARIES Qt5::Gui Qt5::Widgets Qt5::Svg)
	endif ()
	add_definitions(${Qt5Gui_DEFINITIONS} ${Qt5Widgets_DEFINITIONS})
else()
	find_package(Qt6Core QUIET)
endif()

if(Qt6Core_FOUND)
    	message("----- Qt6:")
	message(STATUS "	 includes:" ${Qt6Core_INCLUDE_DIRS})
	find_package(Qt6Widgets REQUIRED)
	find_package(Qt6Gui REQUIRED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt6Widgets_EXECUTABLE_COMPILE_FLAGS}")
	set(QT_INCLUDE_DIRS ${Qt6Gui_INCLUDE_DIRS} ${Qt6Widgets_INCLUDE_DIRS})
	set(QT_LIBRARIES Qt6::Gui Qt6::Widgets)
	add_definitions(${Qt6Gui_DEFINITIONS} ${Qt6Widgets_DEFINITIONS})
endif()

if(NOT Qt5Core_FOUND AND NOT Qt6Core_FOUND)
	message("Error!The qt5 or qt6 can't find, if them has been installed, please append the install path to CMAKE_PREFIX_PATH, the command as:")
	message("list(APPEND CMAKE_PREFIX_PATH \"xxx install path\")" )
	message(FATAL_ERROR  "Can't find Qt5 or Qt6!")
endif()

#===============================================================================
#= boost
#-------------------------------------------------------------------------------
find_package(Boost 1.42 QUIET)

if(NOT Boost_FOUND)
	message(FATAL_ERROR  "Please install boost!")
endif()

message("----- boost:")
message(STATUS "	 includes:" ${Boost_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

#===============================================================================
#= Dependencies
#-------------------------------------------------------------------------------

find_package(Threads)

#===============================================================================
#= DSView sources
#-------------------------------------------------------------------------------

set(DSView_SOURCES
    DSView/main.cpp
    DSView/dsapplication.cpp
    DSView/pv/log.cpp
    DSView/pv/sigsession.cpp
    DSView/pv/mainwindow.cpp
    DSView/pv/data/snapshot.cpp
    DSView/pv/data/signaldata.cpp
    DSView/pv/data/logicsnapshot.cpp
    DSView/pv/data/analogsnapshot.cpp
    DSView/pv/dialogs/deviceoptions.cpp
    DSView/pv/prop/property.cpp
    DSView/pv/prop/int.cpp
    DSView/pv/prop/enum.cpp
    DSView/pv/prop/double.cpp
    DSView/pv/prop/bool.cpp
    DSView/pv/prop/binding/binding.cpp
    DSView/pv/toolbars/samplingbar.cpp
    DSView/pv/view/viewport.cpp
    DSView/pv/view/view.cpp
    DSView/pv/view/timemarker.cpp
    DSView/pv/view/signal.cpp
    DSView/pv/view/ruler.cpp
    DSView/pv/view/logicsignal.cpp
    DSView/pv/view/header.cpp
    DSView/pv/view/cursor.cpp
    DSView/pv/view/analogsignal.cpp
    DSView/pv/prop/binding/deviceoptions.cpp
    DSView/pv/toolbars/trigbar.cpp
    DSView/pv/toolbars/filebar.cpp
    DSView/pv/dock/protocoldock.cpp
    DSView/pv/dock/triggerdock.cpp
    DSView/pv/dock/measuredock.cpp
    DSView/pv/dock/searchdock.cpp
    DSView/pv/toolbars/logobar.cpp
    DSView/pv/dialogs/about.cpp
    DSView/pv/dialogs/search.cpp
    DSView/pv/data/dsosnapshot.cpp
    DSView/pv/view/dsosignal.cpp
    DSView/pv/view/dsldial.cpp
    DSView/pv/dock/dsotriggerdock.cpp
    DSView/pv/view/trace.cpp
    DSView/pv/view/selectableitem.cpp
    DSView/pv/data/decoderstack.cpp
    DSView/pv/data/decode/rowdata.cpp
    DSView/pv/data/decode/row.cpp
    DSView/pv/data/decode/decoder.cpp
    DSView/pv/data/decode/annotation.cpp
    DSView/pv/view/decodetrace.cpp
    DSView/pv/prop/binding/decoderoptions.cpp
    DSView/pv/widgets/fakelineedit.cpp
    DSView/pv/widgets/decodermenu.cpp
    DSView/pv/widgets/decodergroupbox.cpp
    DSView/pv/prop/string.cpp
    DSView/pv/dialogs/storeprogress.cpp
    DSView/pv/storesession.cpp
    DSView/pv/view/devmode.cpp
    DSView/pv/dialogs/waitingdialog.cpp
    DSView/pv/dialogs/dsomeasure.cpp
    DSView/pv/dialogs/calibration.cpp
    DSView/pv/data/decodermodel.cpp
    DSView/pv/dialogs/protocollist.cpp
    DSView/pv/dialogs/protocolexp.cpp
    DSView/pv/dialogs/fftoptions.cpp
    DSView/pv/data/mathstack.cpp
    DSView/pv/view/mathtrace.cpp   
    DSView/pv/toolbars/titlebar.cpp
    DSView/pv/mainframe.cpp
    DSView/pv/widgets/border.cpp
    DSView/pv/dialogs/dsmessagebox.cpp
    DSView/pv/dialogs/shadow.cpp
    DSView/pv/dialogs/dsdialog.cpp
    DSView/pv/dialogs/interval.cpp
    DSView/pv/prop/binding/probeoptions.cpp
    DSView/pv/view/viewstatus.cpp
    DSView/pv/dialogs/lissajousoptions.cpp
    DSView/pv/view/lissajoustrace.cpp
    DSView/pv/view/spectrumtrace.cpp
    DSView/pv/data/spectrumstack.cpp
    DSView/pv/dialogs/mathoptions.cpp
    DSView/pv/dialogs/regionoptions.cpp
    DSView/pv/view/xcursor.cpp
    DSView/pv/dock/protocoldock.cpp
    DSView/pv/data/decoderstack.cpp
    DSView/pv/data/decode/annotation.cpp
    DSView/pv/data/decode/decoder.cpp
    DSView/pv/data/decode/row.cpp
    DSView/pv/data/decode/rowdata.cpp
    DSView/pv/prop/binding/decoderoptions.cpp
    DSView/pv/view/decodetrace.cpp
    DSView/pv/widgets/decodergroupbox.cpp
    DSView/pv/widgets/decodermenu.cpp
    DSView/pv/config/appconfig.cpp
    DSView/pv/appcontrol.cpp
    DSView/pv/dstimer.cpp
    DSView/pv/eventobject.cpp
    DSView/pv/ZipMaker.cpp
    DSView/pv/data/decode/annotationrestable.cpp
    DSView/pv/data/decode/decoderstatus.cpp
    DSView/pv/dock/protocolitemlayer.cpp
    DSView/pv/ui/msgbox.cpp
    DSView/pv/ui/dscombobox.cpp
    DSView/pv/dsvdef.cpp
    DSView/pv/dialogs/applicationpardlg.cpp
    DSView/pv/dock/keywordlineedit.cpp
    DSView/pv/dock/searchcombobox.cpp
    DSView/pv/dialogs/decoderoptionsdlg.cpp
    DSView/pv/utility/encoding.cpp
    DSView/pv/utility/path.cpp
    DSView/pv/utility/array.cpp
    DSView/pv/deviceagent.cpp
    DSView/pv/ui/langresource.cpp
    DSView/pv/ui/fn.cpp
    DSView/pv/ui/xtoolbutton.cpp
    DSView/pv/ui/uimanager.cpp
    DSView/pv/ui/popupdlglist.cpp
)

# Windows-specific QT source files
if(WIN32)
	list(APPEND DSView_SOURCES
		DSView/pv/winnativewidget.cpp
		DSView/pv/winshadow.cpp
	)
endif ()

set(DSView_HEADERS
    DSView/mystyle.h
    DSView/pv/log.h
    DSView/pv/sigsession.h
    DSView/pv/mainwindow.h
    DSView/pv/dialogs/deviceoptions.h
    DSView/pv/prop/property.h
    DSView/pv/prop/int.h
    DSView/pv/prop/enum.h
    DSView/pv/prop/double.h
    DSView/pv/prop/bool.h
    DSView/pv/toolbars/samplingbar.h
    DSView/pv/view/viewport.h
    DSView/pv/view/view.h
    DSView/pv/view/timemarker.h
    DSView/pv/view/ruler.h
    DSView/pv/view/header.h
    DSView/pv/view/cursor.h
    DSView/pv/toolbars/trigbar.h
    DSView/pv/toolbars/filebar.h
    DSView/pv/dock/protocoldock.h
    DSView/pv/dock/triggerdock.h
    DSView/pv/dock/measuredock.h
    DSView/pv/dock/searchdock.h
    DSView/pv/toolbars/logobar.h
    DSView/pv/dialogs/about.h
    DSView/pv/dialogs/search.h
    DSView/pv/dock/dsotriggerdock.h
    DSView/pv/view/trace.h
    DSView/pv/view/selectableitem.h
    DSView/pv/data/decoderstack.h
    DSView/pv/view/decodetrace.h
    DSView/pv/widgets/fakelineedit.h
    DSView/pv/widgets/decodermenu.h
    DSView/pv/widgets/decodergroupbox.h
    DSView/pv/prop/string.h
    DSView/pv/dialogs/storeprogress.h
    DSView/pv/storesession.h
    DSView/pv/view/devmode.h
    DSView/pv/dialogs/waitingdialog.h
    DSView/pv/dialogs/dsomeasure.h
    DSView/pv/dialogs/calibration.h
    DSView/pv/dialogs/protocollist.h
    DSView/pv/dialogs/protocolexp.h
    DSView/pv/dialogs/fftoptions.h
    DSView/pv/data/mathstack.h
    DSView/pv/view/mathtrace.h
    DSView/pv/view/viewstatus.h
    DSView/pv/toolbars/titlebar.h
    DSView/pv/mainframe.h
    DSView/pv/widgets/border.h
    DSView/pv/dialogs/dsmessagebox.h
    DSView/pv/dialogs/shadow.h
    DSView/pv/dialogs/dsdialog.h
    DSView/pv/dialogs/interval.h
    DSView/pv/dialogs/lissajousoptions.h
    DSView/pv/view/lissajoustrace.h
    DSView/pv/view/spectrumtrace.h
    DSView/pv/data/spectrumstack.h
    DSView/pv/dialogs/mathoptions.h
    DSView/pv/dialogs/regionoptions.h
    DSView/pv/view/xcursor.h
    DSView/pv/view/signal.h
    DSView/pv/view/logicsignal.h
    DSView/pv/view/analogsignal.h
    DSView/pv/view/dsosignal.h 
    DSView/pv/dock/protocoldock.h
    DSView/pv/data/decoderstack.h
    DSView/pv/view/decodetrace.h
    DSView/pv/widgets/decodergroupbox.h
    DSView/pv/widgets/decodermenu.h
    DSView/pv/config/appconfig.h
    DSView/pv/appcontrol.h
    DSView/pv/dstimer.h
    DSView/pv/eventobject.h
    DSView/pv/ZipMaker.h
    DSView/pv/data/decode/annotationrestable.h
    DSView/pv/data/decode/decoderstatus.h
    DSView/pv/dock/protocolitemlayer.h
    DSView/pv/ui/msgbox.h
    DSView/pv/ui/dscombobox.h
    DSView/pv/dsvdef.h
    DSView/pv/dialogs/applicationpardlg.h
    DSView/pv/dock/keywordlineedit.h
    DSView/pv/dock/searchcombobox.h
    DSView/pv/dialogs/decoderoptionsdlg.h
    DSView/pv/utility/encoding.h
    DSView/pv/utility/path.h
    DSView/pv/utility/array.h
    DSView/pv/deviceagent.h
    DSView/pv/ui/fn.h
    DSView/pv/ui/xtoolbutton.h
)
 
# Windows-specific QT headers
if(WIN32)
	list(APPEND DSView_HEADERS
		DSView/pv/winnativewidget.h
		DSView/pv/winshadow.h
	)
endif ()

#===============================================================================
#= libsigrok4DSL source
#-------------------------------------------------------------------------------
set(libsigrok4DSL_SOURCES
    libsigrok4DSL/version.c
    libsigrok4DSL/strutil.c
    libsigrok4DSL/std.c
    libsigrok4DSL/session_driver.c
    libsigrok4DSL/session.c
    libsigrok4DSL/log.c
    libsigrok4DSL/hwdriver.c
    libsigrok4DSL/error.c
    libsigrok4DSL/backend.c
    libsigrok4DSL/output/output.c
    libsigrok4DSL/input/input.c
    libsigrok4DSL/hardware/demo/demo.c
    libsigrok4DSL/input/in_binary.c
    libsigrok4DSL/input/in_vcd.c
    libsigrok4DSL/input/in_wav.c
    libsigrok4DSL/output/csv.c
    libsigrok4DSL/output/gnuplot.c
    libsigrok4DSL/output/srzip.c
    libsigrok4DSL/output/vcd.c
    libsigrok4DSL/hardware/DSL/dslogic.c
    libsigrok4DSL/hardware/common/usb.c
    libsigrok4DSL/hardware/common/ezusb.c
    libsigrok4DSL/trigger.c
    libsigrok4DSL/dsdevice.c
    libsigrok4DSL/hardware/DSL/dscope.c
    libsigrok4DSL/hardware/DSL/command.c
    libsigrok4DSL/hardware/DSL/dsl.c
    libsigrok4DSL/lib_main.c
)

set(libsigrok4DSL_HEADERS
    libsigrok4DSL/version.h
    libsigrok4DSL/libsigrok-internal.h
    libsigrok4DSL/libsigrok.h
    libsigrok4DSL/config.h
    libsigrok4DSL/hardware/DSL/command.h
    libsigrok4DSL/hardware/DSL/dsl.h
)
 
#===============================================================================
#= libsigrokdecode4DSL source
#-------------------------------------------------------------------------------
set(libsigrokdecode4DSL_SOURCES
    libsigrokdecode4DSL/type_decoder.c
    libsigrokdecode4DSL/srd.c
    libsigrokdecode4DSL/module_sigrokdecode.c
    libsigrokdecode4DSL/decoder.c
    libsigrokdecode4DSL/error.c
    libsigrokdecode4DSL/exception.c
    libsigrokdecode4DSL/instance.c
    libsigrokdecode4DSL/log.c
    libsigrokdecode4DSL/session.c
    libsigrokdecode4DSL/util.c
    libsigrokdecode4DSL/version.c
)

set(libsigrokdecode4DSL_HEADERS
    libsigrokdecode4DSL/libsigrokdecode-internal.h
    libsigrokdecode4DSL/libsigrokdecode.h
    libsigrokdecode4DSL/config.h
    libsigrokdecode4DSL/version.h
)

#===============================================================================
#= common source
#-------------------------------------------------------------------------------

set(common_SOURCES
    common/minizip/zip.c
    common/minizip/unzip.c
    common/minizip/ioapi.c
    common/log/xlog.c
)

set(common_HEADERS
    common/minizip/zip.h
    common/minizip/unzip.h
    common/minizip/ioapi.h
    common/log/xlog.h
)

#===============================================================================
#= compile config
#-------------------------------------------------------------------------------

set(DSView_FORMS
)

set(DSView_RESOURCES
    DSView/DSView.qrc
    DSView/themes/breeze.qrc
    DSView/languages/language.qrc
)


if(WIN32)
	# Use the DSView icon for the DSView.exe executable.
	set(CMAKE_RC_COMPILE_OBJECT "${CMAKE_RC_COMPILER} -O coff -I${CMAKE_CURRENT_SOURCE_DIR} <SOURCE> <OBJECT>")
	enable_language(RC)
	# app icon
        list(APPEND DSView_SOURCES applogo.rc)
	# MinGW Unicode support
	add_definitions(-municode)
endif()

if(Qt5Core_FOUND)
	qt5_wrap_cpp(DSView_HEADERS_MOC ${DSView_HEADERS})
	qt5_wrap_ui(DSView_FORMS_HEADERS ${DSView_FORMS})
	qt5_add_resources(DSView_RESOURCES_RCC ${DSView_RESOURCES})
endif()

if(Qt6Core_FOUND)
	qt6_wrap_cpp(DSView_HEADERS_MOC ${DSView_HEADERS})
	qt6_wrap_ui(DSView_FORMS_HEADERS ${DSView_FORMS})
	qt6_add_resources(DSView_RESOURCES_RCC ${DSView_RESOURCES})
endif()


add_definitions(${QT_DEFINITIONS})
add_definitions(-Wall -Wextra -Wno-return-type -Wno-ignored-qualifiers)

if(NOT DISABLE_WERROR)
        add_definitions(-Werror)
endif()

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")


include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${Boost_INCLUDE_DIRS}
	${QT_INCLUDE_DIRS}
)


if(STATIC_PKGDEPS_LIBS)
	include_directories(${PKGDEPS_STATIC_INCLUDE_DIRS})
else()
	include_directories(${PKGDEPS_INCLUDE_DIRS})
endif()

#===============================================================================
#= Release flags
#-------------------------------------------------------------------------------
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

#===============================================================================
#= Linker Configuration
#-------------------------------------------------------------------------------


set(DSVIEW_LINK_LIBS
	-lz
	-lglib-2.0
	${CMAKE_THREAD_LIBS_INIT}
	${QT_LIBRARIES}
	${LIBUSB_1_LIBRARIES}
	${FFTW_LIBRARIES}
	${PY_LIB}
)

if(STATIC_PKGDEPS_LIBS)
	link_directories(${PKGDEPS_STATIC_LIBRARY_DIRS})
	list(APPEND DSVIEW_LINK_LIBS ${PKGDEPS_STATIC_LIBRARIES})
if(WIN32)
	# Workaround for a MinGW linking issue.
	list(APPEND PULSEVIEW_LINK_LIBS "-llzma -llcms2")
endif()
else()
	link_directories(${PKGDEPS_LIBRARY_DIRS})
	list(APPEND DSVIEW_LINK_LIBS ${PKGDEPS_LIBRARIES})
endif()

add_executable(${PROJECT_NAME}
	${common_SOURCES}
	${DSView_SOURCES}
	${DSView_HEADERS_MOC}
	${DSView_FORMS_HEADERS}
	${DSView_RESOURCES_RCC}
	${libsigrok4DSL_SOURCES}
	${libsigrokdecode4DSL_SOURCES}
)

target_link_libraries(${PROJECT_NAME} ${DSVIEW_LINK_LIBS})

if(WIN32)
# Pass -mwindows so that no "DOS box" will open when PulseView is started.
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-mwindows")
endif()

if(ENABLE_COTIRE)
	include(cotire)
	cotire(${PROJECT_NAME})
endif()

message(STATUS "Output dir: ${CMAKE_CURRENT_SOURCE_DIR}/build.dir")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build.dir")

#===============================================================================
#= Installation
#-------------------------------------------------------------------------------

# Install the executable.
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY DSView/res DESTINATION share/DSView)
install(DIRECTORY DSView/demo DESTINATION share/DSView)
install(FILES DSView/icons/logo.svg DESTINATION share/DSView RENAME logo.svg)
install(FILES DSView/icons/logo.svg DESTINATION share/icons/hicolor/scalable/apps RENAME dsview.svg)
install(FILES DSView/icons/logo.svg DESTINATION share/pixmaps RENAME dsview.svg)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")	
	install(FILES DSView/DSView.desktop DESTINATION /usr/share/applications RENAME dsview.desktop)
	
	add_compile_definitions(_DEFAULT_SOURCE)

	if(IS_DIRECTORY /usr/lib/udev/rules.d)
		install(FILES DSView/DreamSourceLab.rules DESTINATION /usr/lib/udev/rules.d RENAME 60-dreamsourcelab.rules)
	elseif(IS_DIRECTORY /lib/udev/rules.d)
		install(FILES DSView/DreamSourceLab.rules DESTINATION /lib/udev/rules.d RENAME 60-dreamsourcelab.rules)
	elseif(IS_DIRECTORY /etc/udev/rules.d)
		install(FILES DSView/DreamSourceLab.rules DESTINATION /etc/udev/rules.d RENAME 60-dreamsourcelab.rules)
	endif()

endif()

install(FILES NEWS25 DESTINATION share/DSView RENAME NEWS25)
install(FILES NEWS31 DESTINATION share/DSView RENAME NEWS31)
install(FILES ug25.pdf DESTINATION share/DSView RENAME ug25.pdf)
install(FILES ug31.pdf DESTINATION share/DSView RENAME ug31.pdf)

install(DIRECTORY libsigrokdecode4DSL/decoders DESTINATION share/libsigrokdecode4DSL)
install(DIRECTORY lang DESTINATION share/DSView)

#===============================================================================
#= Packaging (handled by CPack)
#-------------------------------------------------------------------------------

set(CPACK_PACKAGE_VERSION_MAJOR ${DS_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DS_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DS_VERSION_MICRO})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/DSView/README)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/DSView/COPYING)
set(CPACK_SOURCE_IGNORE_FILES ${CMAKE_CURRENT_BINARY_DIR} ".gitignore" ".git")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
	"${CMAKE_PROJECT_NAME}-${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO}")
set(CPACK_SOURCE_GENERATOR "TGZ")

include(CPack)

#===============================================================================
#= Tests
#-------------------------------------------------------------------------------

if(ENABLE_TESTS)
	add_subdirectory(test)
	enable_testing()
	add_test(test ${CMAKE_CURRENT_BINARY_DIR}/DSView/test/DSView-test)
endif(ENABLE_TESTS)


