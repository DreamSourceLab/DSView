##
## This file is part of the PulseView project.
##
## Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
## Copyright (C) 2012 Alexandru Gagniuc <mr.nuke.me@gmail.com>
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

# Find the platform's thread library (needed for boost-thread).
# This will set ${CMAKE_THREAD_LIBS_INIT} to the correct, OS-specific value.
find_package(Threads)

find_package(Boost 1.46 COMPONENTS unit_test_framework REQUIRED)

set(pulseview_TEST_SOURCES
	${PROJECT_SOURCE_DIR}/pv/data/analogsnapshot.cpp
	${PROJECT_SOURCE_DIR}/pv/data/snapshot.cpp
	${PROJECT_SOURCE_DIR}/pv/data/logicsnapshot.cpp
	data/analogsnapshot.cpp
	data/logicsnapshot.cpp
	test.cpp
)

add_definitions(-DBOOST_TEST_DYN_LINK)

include_directories(
	${Boost_INCLUDE_DIRS}
)

set(PULSEVIEW_LINK_LIBS
	${Boost_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
)

add_executable(pulseview-test
	${pulseview_TEST_SOURCES}
)

target_link_libraries(pulseview-test ${PULSEVIEW_LINK_LIBS})

