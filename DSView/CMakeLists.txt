##
## This file is part of the DSView project.
##
## Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
## Copyright (C) 2012-2013 Alexandru Gagniuc <mr.nuke.me@gmail.com>
## Copyright (C) 2013-2014 DreamSourceLab <support@dreamsourcelab.com>
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

include(FindPkgConfig)
include(GNUInstallDirs)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

project(DSView)

#===============================================================================
#= User Options
#-------------------------------------------------------------------------------

option(DISABLE_WERROR "Build without -Werror" TRUE)
option(ENABLE_SIGNALS "Build with UNIX signals" TRUE)
option(ENABLE_DECODE "Build with libsigrokdecode" TRUE)
option(ENABLE_COTIRE "Enable cotire" FALSE)
option(ENABLE_TESTS "Enable unit tests" FALSE)
option(STATIC_PKGDEPS_LIBS "Statically link to (pkg-config) libraries" FALSE)
option(FORCE_QT4 "Force use of Qt4 even if Qt5 is available" FALSE)

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
#= Dependencies
#-------------------------------------------------------------------------------

list(APPEND PKGDEPS
	"libsigrok4DSL >= 0.2.0"
	"libusb-1.0 >= 1.0.16"
)
if(ENABLE_DECODE)
	list(APPEND PKGDEPS "libsigrokdecode>=0.3.0")
endif()

find_package(PkgConfig)
pkg_check_modules(PKGDEPS REQUIRED ${PKGDEPS})

if(FORCE_QT4)
	set(Qt5Core_FOUND FALSE)
else()
	find_package(Qt5Core QUIET)
endif()

if(Qt5Core_FOUND)
	message("-- Using Qt5")
	find_package(Qt5Widgets REQUIRED)
	find_package(Qt5Gui REQUIRED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
	set(QT_INCLUDE_DIRS ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS})
	set(QT_LIBRARIES Qt5::Gui Qt5::Widgets)
	add_definitions(${Qt5Gui_DEFINITIONS} ${Qt5Widgets_DEFINITIONS})
else()
	find_program(QT_QMAKE_EXECUTABLE NAMES qmake4 qmake-qt4 qmake-mac)
	find_package(Qt4 REQUIRED)
endif()

find_package(Threads)

find_package(Boost 1.42 COMPONENTS filesystem system thread REQUIRED)
find_package(libusb-1.0 REQUIRED)
find_package(FFTW REQUIRED)

#===============================================================================
#= Config Header
#-------------------------------------------------------------------------------

set(DS_TITLE DSView)
set(DS_DESCRIPTION "A GUI for instruments of DreamSourceLab")

set(DS_VERSION_MAJOR 0)
set(DS_VERSION_MINOR 9)
set(DS_VERSION_MICRO 6)
set(DS_VERSION_STRING
	${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO}
)

configure_file (
	${PROJECT_SOURCE_DIR}/config.h.in
	${PROJECT_BINARY_DIR}/config.h
)

#===============================================================================
#= Sources
#-------------------------------------------------------------------------------

set(DSView_SOURCES
    main.cpp 
    pv/sigsession.cpp 
    pv/mainwindow.cpp 
    pv/devicemanager.cpp 
    pv/data/snapshot.cpp 
    pv/data/signaldata.cpp 
    pv/data/logicsnapshot.cpp 
    pv/data/logic.cpp 
    pv/data/analogsnapshot.cpp 
    pv/data/analog.cpp 
    pv/dialogs/deviceoptions.cpp 
    pv/prop/property.cpp 
    pv/prop/int.cpp 
    pv/prop/enum.cpp 
    pv/prop/double.cpp 
    pv/prop/bool.cpp 
    pv/prop/binding/binding.cpp 
    pv/toolbars/samplingbar.cpp 
    pv/view/viewport.cpp 
    pv/view/view.cpp 
    pv/view/timemarker.cpp 
    pv/view/signal.cpp 
    pv/view/ruler.cpp 
    pv/view/logicsignal.cpp 
    pv/view/header.cpp 
    pv/view/cursor.cpp 
    pv/view/analogsignal.cpp 
    pv/prop/binding/binding_deviceoptions.cpp 
    pv/toolbars/trigbar.cpp 
    pv/toolbars/filebar.cpp 
    pv/dock/protocoldock.cpp 
    pv/dock/triggerdock.cpp 
    pv/dock/measuredock.cpp 
    pv/dock/searchdock.cpp 
    pv/toolbars/logobar.cpp 
    pv/data/groupsnapshot.cpp 
    pv/view/groupsignal.cpp 
    pv/data/group.cpp 
    pv/dialogs/about.cpp 
    pv/dialogs/search.cpp 
    pv/data/dsosnapshot.cpp 
    pv/data/dso.cpp
    pv/view/dsosignal.cpp 
    pv/view/dsldial.cpp 
    pv/dock/dsotriggerdock.cpp 
    pv/view/trace.cpp 
    pv/view/selectableitem.cpp 
    pv/data/decoderstack.cpp 
    pv/data/decode/rowdata.cpp 
    pv/data/decode/row.cpp 
    pv/data/decode/decoder.cpp 
    pv/data/decode/annotation.cpp 
    pv/view/decodetrace.cpp 
    pv/prop/binding/decoderoptions.cpp 
    pv/widgets/fakelineedit.cpp 
    pv/widgets/decodermenu.cpp 
    pv/widgets/decodergroupbox.cpp 
    pv/prop/string.cpp 
    pv/device/sessionfile.cpp 
    pv/device/inputfile.cpp 
    pv/device/file.cpp 
    pv/device/devinst.cpp 
    pv/dialogs/storeprogress.cpp 
    pv/storesession.cpp 
    pv/view/devmode.cpp 
    pv/device/device.cpp 
    pv/dialogs/waitingdialog.cpp 
    pv/dialogs/dsomeasure.cpp 
    pv/dialogs/calibration.cpp 
    pv/data/decodermodel.cpp 
    pv/dialogs/protocollist.cpp 
    pv/dialogs/protocolexp.cpp 
    pv/dialogs/fftoptions.cpp 
    pv/data/mathstack.cpp 
    pv/view/mathtrace.cpp 
    dsapplication.cpp 
    pv/widgets/viewstatus.cpp 
    pv/toolbars/titlebar.cpp 
    pv/mainframe.cpp 
    pv/widgets/border.cpp 
    pv/dialogs/dsmessagebox.cpp 
    pv/dialogs/shadow.cpp 
    pv/dialogs/dsdialog.cpp
)

set(DSView_HEADERS
    pv/sigsession.h 
    pv/mainwindow.h
    pv/dialogs/deviceoptions.h
    pv/prop/property.h
    pv/prop/int.h
    pv/prop/enum.h
    pv/prop/double.h
    pv/prop/bool.h
    pv/toolbars/samplingbar.h
    pv/view/viewport.h
    pv/view/view.h
    pv/view/timemarker.h
    pv/view/ruler.h
    pv/view/header.h
    pv/view/cursor.h
    pv/toolbars/trigbar.h
    pv/toolbars/filebar.h
    pv/dock/protocoldock.h
    pv/dock/triggerdock.h
    pv/dock/measuredock.h
    pv/dock/searchdock.h
    pv/toolbars/logobar.h
    pv/dialogs/about.h
    pv/dialogs/search.h
    pv/dock/dsotriggerdock.h
    pv/view/trace.h
    pv/view/selectableitem.h
    pv/data/decoderstack.h
    pv/view/decodetrace.h
    pv/widgets/fakelineedit.h
    pv/widgets/decodermenu.h
    pv/widgets/decodergroupbox.h
    pv/prop/string.h
    pv/device/devinst.h
    pv/dialogs/storeprogress.h
    pv/storesession.h
    pv/view/devmode.h
    pv/dialogs/waitingdialog.h
    pv/dialogs/dsomeasure.h
    pv/dialogs/calibration.h
    pv/dialogs/protocollist.h
    pv/dialogs/protocolexp.h
    pv/dialogs/fftoptions.h
    pv/data/mathstack.h
    pv/view/mathtrace.h
    pv/widgets/viewstatus.h
    pv/toolbars/titlebar.h
    pv/mainframe.h
    pv/widgets/border.h
    pv/dialogs/dsmessagebox.h
    pv/dialogs/shadow.h
    pv/dialogs/dsdialog.h
)

set(DSView_FORMS
)

set(DSView_RESOURCES
	DSView.qrc
)


if(ENABLE_DECODE)
	list(APPEND DSView_SOURCES
	        pv/dock/protocoldock.cpp
		pv/data/decoderstack.cpp
		pv/data/decode/annotation.cpp
		pv/data/decode/decoder.cpp
		pv/data/decode/row.cpp
		pv/data/decode/rowdata.cpp
		pv/prop/binding/decoderoptions.cpp
		pv/view/decodetrace.cpp
		pv/widgets/decodergroupbox.cpp
		pv/widgets/decodermenu.cpp
	)

	list(APPEND DSView_HEADERS
	        pv/dock/protocoldock.h
		pv/data/decoderstack.h
		pv/view/decodetrace.h
		pv/widgets/decodergroupbox.h
		pv/widgets/decodermenu.h
	)
endif()

if(WIN32)
	# Use the DSView icon for the DSView.exe executable.
	set(CMAKE_RC_COMPILE_OBJECT "${CMAKE_RC_COMPILER} -O coff -I${CMAKE_CURRENT_SOURCE_DIR} <SOURCE> <OBJECT>")
	enable_language(RC)
	list(APPEND DSView_SOURCES DSView.rc)
endif()

if(Qt5Core_FOUND)
	qt5_wrap_cpp(DSView_HEADERS_MOC ${DSView_HEADERS})
	qt5_wrap_ui(DSView_FORMS_HEADERS ${DSView_FORMS})
	qt5_add_resources(DSView_RESOURCES_RCC ${DSView_RESOURCES})
else()
	qt4_wrap_cpp(DSView_HEADERS_MOC ${DSView_HEADERS})
	qt4_wrap_ui(DSView_FORMS_HEADERS ${DSView_FORMS})
	qt4_add_resources(DSView_RESOURCES_RCC ${DSView_RESOURCES})
	include(${QT_USE_FILE})
endif()

#===============================================================================
#= Global Definitions
#-------------------------------------------------------------------------------

add_definitions(${QT_DEFINITIONS})
add_definitions(-std=c++11 -Wall -Wextra -Wno-return-type -Wno-ignored-qualifiers)

if(ENABLE_DECODE)
        add_definitions(-DENABLE_DECODE)
endif()

if(NOT DISABLE_WERROR)
        add_definitions(-Werror)
endif()

#===============================================================================
#= Global Include Directories
#-------------------------------------------------------------------------------

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
#= Linker Configuration
#-------------------------------------------------------------------------------

link_directories(${Boost_LIBRARY_DIRS})

set(DSVIEW_LINK_LIBS
	${Boost_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${QT_LIBRARIES}
        ${LIBUSB_1_LIBRARIES}
        ${FFTW_LIBRARIES}
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
	${DSView_SOURCES}
	${DSView_HEADERS_MOC}
	${DSView_FORMS_HEADERS}
	${DSView_RESOURCES_RCC}
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
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "/usr/local/lib")

#===============================================================================
#= Installation
#-------------------------------------------------------------------------------

# Install the executable.
install(TARGETS ${PROJECT_NAME} DESTINATION bin/)
install(DIRECTORY res DESTINATION share/${PROJECT_NAME})
install(DIRECTORY ../libsigrokdecode4DSL/decoders DESTINATION share/${PROJECT_NAME})
install(FILES icons/logo.png DESTINATION share/${PROJECT_NAME} RENAME logo.png)
install(FILES DreamSourceLab.rules DESTINATION /usr/lib/udev/rules.d/)
install(FILES DSView.desktop DESTINATION /usr/share/applications/)

#===============================================================================
#= Packaging (handled by CPack)
#-------------------------------------------------------------------------------

set(CPACK_PACKAGE_VERSION_MAJOR ${DS_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DS_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DS_VERSION_MICRO})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/README)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/COPYING)
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
	add_test(test ${CMAKE_CURRENT_BINARY_DIR}/test/DSView-test)
endif(ENABLE_TESTS)
