if(NOT DEFINED BUNDLE_PATH OR BUNDLE_PATH STREQUAL "")
	message(FATAL_ERROR "BUNDLE_PATH is required")
endif()

if(NOT IS_DIRECTORY "${BUNDLE_PATH}")
	message(FATAL_ERROR "App bundle does not exist: ${BUNDLE_PATH}")
endif()

set(CONTENTS_PATH "${BUNDLE_PATH}/Contents")
set(FRAMEWORKS_PATH "${CONTENTS_PATH}/Frameworks")
set(MACOS_PATH "${CONTENTS_PATH}/MacOS")

execute_process(
	COMMAND find
		"${MACOS_PATH}"
		"${FRAMEWORKS_PATH}"
		"${CONTENTS_PATH}/PlugIns"
		-type f -print
	RESULT_VARIABLE FIND_RESULT
	OUTPUT_VARIABLE CANDIDATE_OUTPUT
	ERROR_VARIABLE FIND_ERROR
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT FIND_RESULT EQUAL 0)
	message(FATAL_ERROR "Could not inspect app bundle binaries: ${FIND_ERROR}")
endif()

if(CANDIDATE_OUTPUT STREQUAL "")
	message(FATAL_ERROR "No executable files found in app bundle: ${BUNDLE_PATH}")
endif()

string(REPLACE "\n" ";" CANDIDATES "${CANDIDATE_OUTPUT}")
set(UNRESOLVED_DEPENDENCIES)
set(EXTERNAL_DEPENDENCIES)
set(MACHO_COUNT 0)

foreach(BINARY IN LISTS CANDIDATES)
	execute_process(
		COMMAND /usr/bin/file -b "${BINARY}"
		OUTPUT_VARIABLE FILE_TYPE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(NOT FILE_TYPE MATCHES "Mach-O")
		continue()
	endif()

	math(EXPR MACHO_COUNT "${MACHO_COUNT} + 1")
	execute_process(
		COMMAND /usr/bin/otool -L "${BINARY}"
		RESULT_VARIABLE OTOOL_RESULT
		OUTPUT_VARIABLE OTOOL_OUTPUT
		ERROR_VARIABLE OTOOL_ERROR
	)
	if(NOT OTOOL_RESULT EQUAL 0)
		message(FATAL_ERROR "otool failed for ${BINARY}: ${OTOOL_ERROR}")
	endif()

	get_filename_component(LOADER_PATH "${BINARY}" DIRECTORY)
	string(REPLACE "\n" ";" DEPENDENCY_LINES "${OTOOL_OUTPUT}")
	set(SKIP_INSTALL_NAME FALSE)
	if(FILE_TYPE MATCHES "dynamically linked shared library")
		set(SKIP_INSTALL_NAME TRUE)
	endif()
	foreach(LINE IN LISTS DEPENDENCY_LINES)
		if(NOT LINE MATCHES "^[ \t]+([^ \t]+)[ \t]+\\(")
			continue()
		endif()
		if(SKIP_INSTALL_NAME)
			set(SKIP_INSTALL_NAME FALSE)
			continue()
		endif()
		set(DEPENDENCY "${CMAKE_MATCH_1}")
		set(RESOLVED_PATH "")

		if(DEPENDENCY MATCHES "^@rpath/(.+)$")
			set(RESOLVED_PATH "${FRAMEWORKS_PATH}/${CMAKE_MATCH_1}")
		elseif(DEPENDENCY MATCHES "^@executable_path/(.+)$")
			set(RESOLVED_PATH "${MACOS_PATH}/${CMAKE_MATCH_1}")
		elseif(DEPENDENCY MATCHES "^@loader_path/(.+)$")
			set(RESOLVED_PATH "${LOADER_PATH}/${CMAKE_MATCH_1}")
		elseif(DEPENDENCY MATCHES "^/")
			if(NOT DEPENDENCY MATCHES "^/System/" AND NOT DEPENDENCY MATCHES "^/usr/lib/")
				list(APPEND EXTERNAL_DEPENDENCIES "${BINARY}: ${DEPENDENCY}")
			endif()
		endif()

		if(NOT RESOLVED_PATH STREQUAL "" AND NOT EXISTS "${RESOLVED_PATH}")
			list(APPEND UNRESOLVED_DEPENDENCIES "${BINARY}: ${DEPENDENCY}")
		endif()
	endforeach()
endforeach()

if(MACHO_COUNT EQUAL 0)
	message(FATAL_ERROR "No Mach-O binaries found in app bundle: ${BUNDLE_PATH}")
endif()

if(UNRESOLVED_DEPENDENCIES)
	list(JOIN UNRESOLVED_DEPENDENCIES "\n  " UNRESOLVED_TEXT)
	message(FATAL_ERROR "Unresolved bundled dependencies:\n  ${UNRESOLVED_TEXT}")
endif()

if(EXTERNAL_DEPENDENCIES)
	list(JOIN EXTERNAL_DEPENDENCIES "\n  " EXTERNAL_TEXT)
	message(FATAL_ERROR "Non-system absolute dependencies remain:\n  ${EXTERNAL_TEXT}")
endif()

message(STATUS "Validated ${MACHO_COUNT} Mach-O binaries in the app bundle")
