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
set(DS_VERSION_MICRO 0)
set(DS_VERSION_STRING  ${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO} )

configure_file (
	${PROJECT_SOURCE_DIR}/DSView/src/config.h.in
	${PROJECT_BINARY_DIR}/DSView/src/config.h
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
	#find_package(Qt5WinExtras REQUIRED)
	find_package(Qt5Widgets REQUIRED)
	find_package(Qt5Gui REQUIRED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
	#set(QT_INCLUDE_DIRS ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5WinExtras_INCLUDE_DIRS})
	#set(QT_LIBRARIES Qt5::Gui Qt5::Widgets Qt5::WinExtras)
	set(QT_INCLUDE_DIRS ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS})
	set(QT_LIBRARIES Qt5::Gui Qt5::Widgets)
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
	DSView/src/log.cpp
	DSView/src/main.cpp
	DSView/src/utility/path.cpp
	DSView/src/utility/array.cpp
	DSView/src/utility/encoding.cpp
	DSView/src/config/appconfig.cpp
	DSView/src/decode/rowdata.cpp
	DSView/src/decode/annotationrestable.cpp
	DSView/src/decode/decoder.cpp
	DSView/src/decode/row.cpp
	DSView/src/decode/annotation.cpp
	DSView/src/decode/decoderstatus.cpp
	DSView/src/prop/property.cpp
	DSView/src/prop/double.cpp
	DSView/src/prop/int.cpp
	DSView/src/prop/bool.cpp
	DSView/src/prop/enum.cpp
	DSView/src/prop/string.cpp
	DSView/src/prop/binding/deviceoptions.cpp
	DSView/src/prop/binding/decoderoptions.cpp
	DSView/src/prop/binding/probeoptions.cpp
	DSView/src/prop/binding/binding.cpp
	DSView/src/dialogs/protocollist.cpp
	DSView/src/dialogs/interval.cpp
	DSView/src/dialogs/search.cpp
	DSView/src/dialogs/fftoptions.cpp
	DSView/src/dialogs/waitingdialog.cpp
	DSView/src/dialogs/regionoptions.cpp
	DSView/src/dialogs/deviceoptions.cpp
	DSView/src/dialogs/decoderoptionsdlg.cpp
	DSView/src/dialogs/applicationpardlg.cpp
	DSView/src/dialogs/storeprogress.cpp
	DSView/src/dialogs/shadow.cpp
	DSView/src/dialogs/protocolexp.cpp
	DSView/src/dialogs/calibration.cpp
	DSView/src/dialogs/dsdialog.cpp
	DSView/src/dialogs/about.cpp
	DSView/src/dialogs/mathoptions.cpp
	DSView/src/dialogs/dsmessagebox.cpp
	DSView/src/dialogs/lissajousoptions.cpp
	DSView/src/dialogs/dsomeasure.cpp
	DSView/src/view/viewport.cpp
	DSView/src/view/view.cpp
	DSView/src/view/groupsignal.cpp
	DSView/src/view/devmode.cpp
	DSView/src/view/dsosignal.cpp
	DSView/src/view/timemarker.cpp
	DSView/src/view/viewstatus.cpp
	DSView/src/view/mathtrace.cpp
	DSView/src/view/trace.cpp
	DSView/src/view/logicsignal.cpp
	DSView/src/view/dsldial.cpp
	DSView/src/view/spectrumtrace.cpp
	DSView/src/view/analogsignal.cpp
	DSView/src/view/signal.cpp
	DSView/src/view/cursor.cpp
	DSView/src/view/header.cpp
	DSView/src/view/ruler.cpp
	DSView/src/view/decodetrace.cpp
	DSView/src/view/lissajoustrace.cpp
	DSView/src/view/xcursor.cpp
	DSView/src/appcore/mainframe.cpp
	DSView/src/appcore/appcontrol.cpp
	DSView/src/appcore/deviceagent.cpp
	DSView/src/appcore/mainwindow.cpp
	DSView/src/appcore/eventobject.cpp
	DSView/src/appcore/sigsession.cpp
	DSView/src/com/storesession.cpp
	DSView/src/com/zipmaker.cpp
	DSView/src/com/dstimer.cpp
	DSView/src/widgets/decodergroupbox.cpp
	DSView/src/widgets/decodermenu.cpp
	DSView/src/widgets/border.cpp
	DSView/src/widgets/fakelineedit.cpp
	DSView/src/ui/dscombobox.cpp
	DSView/src/ui/langresource.cpp
	DSView/src/ui/fn.cpp
	DSView/src/ui/msgbox.cpp
	DSView/src/dock/triggerdock.cpp
	DSView/src/dock/keywordlineedit.cpp
	DSView/src/dock/searchcombobox.cpp
	DSView/src/dock/measuredock.cpp
	DSView/src/dock/dsotriggerdock.cpp
	DSView/src/dock/searchdock.cpp
	DSView/src/dock/protocolitemlayer.cpp
	DSView/src/dock/protocoldock.cpp
	DSView/src/toolbars/samplingbar.cpp
	DSView/src/toolbars/logobar.cpp
	DSView/src/toolbars/trigbar.cpp
	DSView/src/toolbars/titlebar.cpp
	DSView/src/toolbars/filebar.cpp
	DSView/src/data/analogsnapshot.cpp
	DSView/src/data/decodermodel.cpp
	DSView/src/data/signaldata.cpp
	DSView/src/data/snapshot.cpp
	DSView/src/data/dsosnapshot.cpp
	DSView/src/data/mathstack.cpp
	DSView/src/data/logicsnapshot.cpp
	DSView/src/data/decoderstack.cpp
	DSView/src/data/spectrumstack.cpp
)

set(DSView_HEADERS
	DSView/src/log.h
	DSView/src/config.h
	DSView/src/mystyle.h
	DSView/src/basedef.h
	DSView/src/utility/encoding.h
	DSView/src/utility/path.h
	DSView/src/utility/array.h
	DSView/src/config/appconfig.h
	DSView/src/decode/annotation.h
	DSView/src/decode/displaydataformat.h
	DSView/src/decode/annotationrestable.h
	DSView/src/decode/row.h
	DSView/src/decode/rowdata.h
	DSView/src/decode/decoderstatus.h
	DSView/src/decode/decoder.h
	DSView/src/prop/double.h
	DSView/src/prop/bool.h
	DSView/src/prop/enum.h
	DSView/src/prop/property.h
	DSView/src/prop/int.h
	DSView/src/prop/string.h
	DSView/src/prop/binding/deviceoptions.h
	DSView/src/prop/binding/probeoptions.h
	DSView/src/prop/binding/decoderoptions.h
	DSView/src/prop/binding/binding.h
	DSView/src/dialogs/deviceoptions.h
	DSView/src/dialogs/calibration.h
	DSView/src/dialogs/protocollist.h
	DSView/src/dialogs/waitingdialog.h
	DSView/src/dialogs/applicationpardlg.h
	DSView/src/dialogs/search.h
	DSView/src/dialogs/regionoptions.h
	DSView/src/dialogs/fftoptions.h
	DSView/src/dialogs/dsmessagebox.h
	DSView/src/dialogs/dsdialog.h
	DSView/src/dialogs/dsomeasure.h
	DSView/src/dialogs/lissajousoptions.h
	DSView/src/dialogs/about.h
	DSView/src/dialogs/shadow.h
	DSView/src/dialogs/decoderoptionsdlg.h
	DSView/src/dialogs/interval.h
	DSView/src/dialogs/protocolexp.h
	DSView/src/dialogs/storeprogress.h
	DSView/src/dialogs/mathoptions.h
	DSView/src/view/viewport.h
	DSView/src/view/mathtrace.h
	DSView/src/view/xcursor.h
	DSView/src/view/spectrumtrace.h
	DSView/src/view/decodetrace.h
	DSView/src/view/devmode.h
	DSView/src/view/ruler.h
	DSView/src/view/logicsignal.h
	DSView/src/view/lissajoustrace.h
	DSView/src/view/groupsignal.h
	DSView/src/view/signal.h
	DSView/src/view/dsldial.h
	DSView/src/view/cursor.h
	DSView/src/view/viewstatus.h
	DSView/src/view/timemarker.h
	DSView/src/view/trace.h
	DSView/src/view/analogsignal.h
	DSView/src/view/dsosignal.h
	DSView/src/view/header.h
	DSView/src/view/view.h
	DSView/src/interface/icallbacks.h
	DSView/src/appcore/sigsession.h
	DSView/src/appcore/mainframe.h
	DSView/src/appcore/eventobject.h
	DSView/src/appcore/appcontrol.h
	DSView/src/appcore/deviceagent.h
	DSView/src/appcore/mainwindow.h
	DSView/src/com/dstimer.h
	DSView/src/com/zipmaker.h
	DSView/src/com/storesession.h
	DSView/src/widgets/fakelineedit.h
	DSView/src/widgets/decodermenu.h
	DSView/src/widgets/decodergroupbox.h
	DSView/src/widgets/border.h
	DSView/src/ui/langresource.h
	DSView/src/ui/msgbox.h
	DSView/src/ui/string_ids.h
	DSView/src/ui/dscombobox.h
	DSView/src/ui/fn.h
	DSView/src/dock/searchdock.h
	DSView/src/dock/protocolitemlayer.h
	DSView/src/dock/searchcombobox.h
	DSView/src/dock/triggerdock.h
	DSView/src/dock/keywordlineedit.h
	DSView/src/dock/protocoldock.h
	DSView/src/dock/measuredock.h
	DSView/src/dock/dsotriggerdock.h
	DSView/src/toolbars/samplingbar.h
	DSView/src/toolbars/trigbar.h
	DSView/src/toolbars/titlebar.h
	DSView/src/toolbars/filebar.h
	DSView/src/toolbars/logobar.h
	DSView/src/data/logicsnapshot.h
	DSView/src/data/analogsnapshot.h
	DSView/src/data/decoderstack.h
	DSView/src/data/dsosnapshot.h
	DSView/src/data/snapshot.h
	DSView/src/data/decodermodel.h
	DSView/src/data/mathstack.h
	DSView/src/data/signaldata.h
	DSView/src/data/spectrumstack.h
)
 
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
add_compile_options(-O3)

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


