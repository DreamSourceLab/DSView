if(NOT DEFINED BUNDLE_PATH OR BUNDLE_PATH STREQUAL "")
	message(FATAL_ERROR "BUNDLE_PATH is required")
endif()

set(FRAMEWORKS_PATH "${BUNDLE_PATH}/Contents/Frameworks")

if(NOT IS_DIRECTORY "${BUNDLE_PATH}")
	message(FATAL_ERROR "App bundle does not exist: ${BUNDLE_PATH}")
endif()

if(NOT IS_DIRECTORY "${FRAMEWORKS_PATH}")
	message(STATUS "No embedded Frameworks directory; nothing to fix")
	return()
endif()

execute_process(
	COMMAND find "${FRAMEWORKS_PATH}" -type l -print
	RESULT_VARIABLE FIND_RESULT
	OUTPUT_VARIABLE SYMLINK_OUTPUT
	ERROR_VARIABLE FIND_ERROR
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT FIND_RESULT EQUAL 0)
	message(FATAL_ERROR "Could not inspect bundle symlinks: ${FIND_ERROR}")
endif()

if(SYMLINK_OUTPUT STREQUAL "")
	message(STATUS "No framework symlinks found")
	return()
endif()

string(REPLACE "\n" ";" SYMLINKS "${SYMLINK_OUTPUT}")
set(UNRESOLVED_SYMLINKS)

foreach(SYMLINK IN LISTS SYMLINKS)
	if(IS_SYMLINK "${SYMLINK}" AND NOT EXISTS "${SYMLINK}")
		if(SYMLINK MATCHES "/lib/python[^/]+/site-packages$")
			message(STATUS "Removing external Homebrew site-packages link: ${SYMLINK}")
			file(REMOVE "${SYMLINK}")
			file(MAKE_DIRECTORY "${SYMLINK}")
		else()
			list(APPEND UNRESOLVED_SYMLINKS "${SYMLINK}")
		endif()
	endif()
endforeach()

if(UNRESOLVED_SYMLINKS)
	list(JOIN UNRESOLVED_SYMLINKS "\n  " UNRESOLVED_TEXT)
	message(FATAL_ERROR
		"Unexpected broken symlinks remain in the app bundle:\n  ${UNRESOLVED_TEXT}\n"
		"Refusing to produce a potentially corrupt release."
	)
endif()

set(DECODERS_PATH "${BUNDLE_PATH}/Contents/share/libsigrokdecode4DSL/decoders")
if(IS_DIRECTORY "${DECODERS_PATH}")
	execute_process(
		COMMAND find "${DECODERS_PATH}" -type d -name __pycache__ -print
		RESULT_VARIABLE CACHE_FIND_RESULT
		OUTPUT_VARIABLE CACHE_OUTPUT
		ERROR_VARIABLE CACHE_FIND_ERROR
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(NOT CACHE_FIND_RESULT EQUAL 0)
		message(FATAL_ERROR "Could not inspect Python bytecode caches: ${CACHE_FIND_ERROR}")
	endif()
	if(NOT CACHE_OUTPUT STREQUAL "")
		string(REPLACE "\n" ";" CACHE_DIRECTORIES "${CACHE_OUTPUT}")
		foreach(CACHE_DIRECTORY IN LISTS CACHE_DIRECTORIES)
			file(REMOVE_RECURSE "${CACHE_DIRECTORY}")
		endforeach()
		list(LENGTH CACHE_DIRECTORIES CACHE_COUNT)
		message(STATUS "Removed ${CACHE_COUNT} Python bytecode cache directories")
	endif()
endif()

message(STATUS "macOS bundle symlinks validated")
