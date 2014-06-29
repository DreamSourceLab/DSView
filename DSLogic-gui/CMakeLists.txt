##
## This file is part of the PulseView project.
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

cmake_minimum_required(VERSION 2.6)
include(FindPkgConfig)
include(GNUInstallDirs)

project(DSLogic)

#===============================================================================
#= User Options
#-------------------------------------------------------------------------------

option(DISABLE_WERROR "Build without -Werror" TRUE)
option(ENABLE_SIGNALS "Build with UNIX signals" TRUE)
option(ENABLE_TESTS "Enable unit tests" FALSE)
option(STATIC_PKGDEPS_LIBS "Statically link to (pkgconfig) libraries" FALSE)

if(WIN32)
	# On Windows/MinGW we need to statically link to libraries.
	# This option is user configurable, but enable it by default on win32.
	set(STATIC_PKGDEPS_LIBS TRUE)

	# For boost-thread we need two additional settings on win32:
	set(Boost_USE_STATIC_LIBS on)
	add_definitions(-DBOOST_THREAD_USE_LIB)

	# Windsws does not support UNIX signals
	set(ENABLE_SIGNALS FALSE)
endif()

#===============================================================================
#= Dependencies
#-------------------------------------------------------------------------------

list(APPEND PKGDEPS
	libsigrok4DSLogic>=0.2.0
	libusb-1.0>=1.0.16
)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
 
find_package(PkgConfig)
pkg_check_modules(PKGDEPS REQUIRED ${PKGDEPS})

find_package(Qt4 REQUIRED)

# Find the platform's thread library (needed for boost-thread).
# This will set ${CMAKE_THREAD_LIBS_INIT} to the correct, OS-specific value.
find_package(Threads)

if(WIN32)
# On Windows/MinGW the we need to use 'thread_win32' instead of 'thread'.
# The library is named libboost_thread_win32* (not libboost_thread*).
find_package(Boost 1.42 COMPONENTS system thread_win32 REQUIRED)
else()
find_package(Boost 1.42 COMPONENTS system thread REQUIRED)
endif()

find_package(libusb-1.0 REQUIRED)

#===============================================================================
#= Config Header
#-------------------------------------------------------------------------------

set(DS_TITLE DSLogic)
set(DS_DESCRIPTION "A GUI for DSLogic")

set(DS_VERSION_MAJOR 0)
set(DS_VERSION_MINOR 3)
set(DS_VERSION_MICRO 0)
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

set(DSLogic_SOURCES
	main.cpp
	pv/devicemanager.cpp
	pv/mainwindow.cpp
	pv/sigsession.cpp
	pv/data/analog.cpp
	pv/data/analogsnapshot.cpp
	pv/data/group.cpp
	pv/data/groupsnapshot.cpp
	pv/data/logic.cpp
	pv/data/logicsnapshot.cpp
	pv/data/signaldata.cpp
	pv/data/snapshot.cpp
        pv/data/dso.cpp
        pv/data/dsosnapshot.cpp
	pv/decoder/decoder.cpp
	pv/decoder/decoderfactory.cpp
	pv/decoder/democonfig.cpp
	pv/decoder/ds1wire.cpp
	pv/decoder/dsdmx512.cpp
	pv/decoder/dsi2c.cpp
	pv/decoder/dsserial.cpp
	pv/decoder/dsspi.cpp
	pv/dialogs/about.cpp
	pv/dialogs/connect.cpp
	pv/dialogs/deviceoptions.cpp
	pv/dialogs/search.cpp
	pv/dock/fakelineedit.cpp
	pv/dock/measuredock.cpp
	pv/dock/protocoldock.cpp
	pv/dock/searchdock.cpp
	pv/dock/triggerdock.cpp
	pv/prop/bool.cpp
	pv/prop/double.cpp
	pv/prop/enum.cpp
	pv/prop/int.cpp
	pv/prop/property.cpp
	pv/prop/binding/binding.cpp
	pv/prop/binding/binding_deviceoptions.cpp
	pv/toolbars/devicebar.cpp
	pv/toolbars/filebar.cpp
	pv/toolbars/logobar.cpp
	pv/toolbars/samplingbar.cpp
	pv/toolbars/trigbar.cpp
	pv/view/analogsignal.cpp
	pv/view/cursor.cpp
	pv/view/groupsignal.cpp
	pv/view/header.cpp
	pv/view/logicsignal.cpp
	pv/view/protocolsignal.cpp
	pv/view/ruler.cpp
	pv/view/signal.cpp
	pv/view/timemarker.cpp
	pv/view/view.cpp
	pv/view/viewport.cpp
        pv/view/dsosignal.cpp
	pv/view/dsldial.cpp
	pv/dock/dsotriggerdock.cpp
)

set(DSLogic_HEADERS
	pv/sigsession.h
	pv/mainwindow.h
	pv/decoder/democonfig.h
	pv/dock/fakelineedit.h
	pv/dock/measuredock.h
	pv/dock/protocoldock.h
	pv/dock/searchdock.h
	pv/dock/triggerdock.h
	pv/dialogs/about.h
	pv/dialogs/connect.h
	pv/dialogs/deviceoptions.h
	pv/dialogs/search.h
	pv/toolbars/samplingbar.h
	pv/toolbars/devicebar.h
	pv/toolbars/filebar.h
	pv/toolbars/logobar.h
	pv/toolbars/trigbar.h
        pv/data/dso.h
        pv/data/dsosnapshot.h
	pv/view/cursor.h
	pv/view/header.h
	pv/view/ruler.h
	pv/view/timemarker.h
	pv/view/groupsignal.h
	pv/view/protocolsignal.h
	pv/view/view.h
        pv/view/dsosignal.h
	pv/view/viewport.h
	pv/view/dsldial.h
	pv/dock/dsotriggerdock.h
)

set(DSLogic_FORMS
	pv/dialogs/about.ui
	pv/decoder/dmx512config.ui
	pv/decoder/i2cconfig.ui
	pv/decoder/serialconfig.ui
	pv/decoder/spiconfig.ui
	pv/decoder/wire1config.ui
)

set(DSLogic_RESOURCES
	DSLogic.qrc
)

qt4_wrap_cpp(DSLogic_HEADERS_MOC ${DSLogic_HEADERS})
qt4_wrap_ui(DSLogic_FORMS_HEADERS ${DSLogic_FORMS})
qt4_add_resources(DSLogic_RESOURCES_RCC ${DSLogic_RESOURCES})

include(${QT_USE_FILE})

#===============================================================================
#= Global Definitions
#-------------------------------------------------------------------------------

add_definitions(${QT_DEFINITIONS})
add_definitions(-Wall -Wextra -Wno-return-type -Wno-ignored-qualifiers)

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

set(DSLOGIC_LINK_LIBS
	${Boost_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${QT_LIBRARIES}
        ${LIBUSB_1_LIBRARIES}
)

if(STATIC_PKGDEPS_LIBS)
	link_directories(${PKGDEPS_STATIC_LIBRARY_DIRS})
	list(APPEND DSLOGIC_LINK_LIBS ${PKGDEPS_STATIC_LIBRARIES})
else()
	link_directories(${PKGDEPS_LIBRARY_DIRS})
	list(APPEND DSLOGIC_LINK_LIBS ${PKGDEPS_LIBRARIES})
endif()

add_executable(${PROJECT_NAME}
	${DSLogic_SOURCES}
	${DSLogic_HEADERS_MOC}
	${DSLogic_FORMS_HEADERS}
	${DSLogic_RESOURCES_RCC}
)

target_link_libraries(${PROJECT_NAME} ${DSLOGIC_LINK_LIBS})

if(WIN32)
# Pass -mwindows so that no "DOS box" will open when PulseView is started.
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-mwindows")
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "/usr/local/lib")

#===============================================================================
#= Installation
#-------------------------------------------------------------------------------

# Install the executable.
install(TARGETS ${PROJECT_NAME} DESTINATION bin/)
install(FILES res/DSLogic.fw DESTINATION bin/res/)
install(FILES res/DSLogic.bin DESTINATION bin/res/)

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
	add_test(test ${CMAKE_CURRENT_BINARY_DIR}/test/DSLogic-test)
endif(ENABLE_TESTS)
