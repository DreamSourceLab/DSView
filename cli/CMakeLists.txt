##
## cli/CMakeLists.txt - Build dsview-cli (headless command-line bridge)
##
## This target reuses libsigrok4DSL and common sources from the parent
## DSView tree but does NOT require Qt, Boost, or FFTW.
##

# The parent-scope source lists use paths relative to CMAKE_SOURCE_DIR.
# CMake resolves source paths relative to the current source directory,
# so we must prepend CMAKE_SOURCE_DIR to each entry.
set(_cli_lib_sources "")
foreach(_src ${libsigrok4DSL_SOURCES})
    list(APPEND _cli_lib_sources "${CMAKE_SOURCE_DIR}/${_src}")
endforeach()

set(_cli_common_sources "")
foreach(_src ${common_SOURCES})
    list(APPEND _cli_common_sources "${CMAKE_SOURCE_DIR}/${_src}")
endforeach()

add_executable(dsview-cli
    dsview_cli.c
    ${_cli_lib_sources}
    ${_cli_common_sources}
)

target_include_directories(dsview-cli PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libsigrok4DSL
    ${CMAKE_SOURCE_DIR}/libsigrokdecode4DSL
    ${CMAKE_SOURCE_DIR}/common
    ${GLIB_INCLUDE_DIRS}
    ${LIBUSB_1_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

# Python3 include dirs - handle both find_package(Python3) and find_package(PythonLibs)
if(Python3_FOUND)
    target_include_directories(dsview-cli PRIVATE ${Python3_INCLUDE_DIRS})
elseif(PYTHONLIBS_FOUND)
    target_include_directories(dsview-cli PRIVATE ${PYTHON_INCLUDE_DIRS})
endif()

target_compile_definitions(dsview-cli PRIVATE
    HAVE_LIBUSB_1_0=1
    HAVE_DSL_DEVICE=1
    HAVE_LA_DEMO=1
)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_compile_definitions(dsview-cli PRIVATE _DEFAULT_SOURCE)
endif()

target_compile_options(dsview-cli PRIVATE
    -std=c99
    -Wno-unused-result
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-implicit-function-declaration
)

target_link_libraries(dsview-cli
    -lz
    -lglib-2.0
    -lm
    ${CMAKE_THREAD_LIBS_INIT}
    ${LIBUSB_1_LIBRARIES}
    ${PY_LIB}
)

# Place dsview-cli alongside the DSView GUI binary
set_target_properties(dsview-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build.dir"
)

install(TARGETS dsview-cli DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install MCP server and Python dependencies alongside the binary
install(FILES dsview_mcp.py requirements.txt
        DESTINATION share/DSView/cli)
