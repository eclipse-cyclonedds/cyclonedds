#
# Copyright(c) 2021 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
cmake_minimum_required(VERSION 3.7)

include(GenerateExportHeader)

find_package(BISON 3.0.4 REQUIRED)
find_package(Threads QUIET REQUIRED)

set(binary_dir "${CMAKE_CURRENT_BINARY_DIR}")
set(source_dir "${CMAKE_CURRENT_SOURCE_DIR}")

bison_target(parser src/parser.y "${binary_dir}/parser.c")

# generate hashid.c from md5.h and md5.c to avoid code duplication
file(READ "${CMAKE_SOURCE_DIR}/src/ddsrt/include/dds/ddsrt/endian.h" endian_h)
file(READ "${CMAKE_SOURCE_DIR}/src/ddsrt/include/dds/ddsrt/md5.h" md5_h)
file(READ "${CMAKE_SOURCE_DIR}/src/ddsrt/src/md5.c" md5_c)
string(CONCAT MD5 "${endian_h}" "${md5_h}" "${md5_c}")
# remove occurrences of DDS_EXPORT
string(REGEX REPLACE "\n[ \t]*DDS_EXPORT[ \t]+" "\n" MD5 "${MD5}")
# remove dds/* includes
string(REGEX REPLACE "\n[ \t]*#[ \t]*include[ \t]+[<\"]dds/[^\n]*" "" MD5 "${MD5}")
# switch functions to static
foreach(func ddsrt_md5_init ddsrt_md5_append ddsrt_md5_finish)
  string(REGEX REPLACE "\n[ \t]*void[ \n\t]+${func}" "\nstatic void ${func}" MD5 "${MD5}")
endforeach()

configure_file(src/hashid.c.in ${CMAKE_CURRENT_BINARY_DIR}/hashid.c @ONLY)

add_library(
  idl SHARED
    src/symbol.c
    src/directive.c
    src/expression.c
    src/file.c
    src/processor.c
    src/scanner.c
    src/string.c
    src/annotation.c
    src/scope.c
    src/hashid.c
    src/string.c
    src/tree.c
    src/visit.c
    src/thread.c
    ${CMAKE_CURRENT_BINARY_DIR}/hashid.c
    ${BISON_parser_OUTPUT_SOURCE})

set_target_properties(
  idl PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR})

generate_export_header(idl EXPORT_FILE_NAME include/idl/export.h)

configure_file(include/idl/version.h.in include/idl/version.h @ONLY)

target_include_directories(
  idl
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  INTERFACE
    "$<INSTALL_INTERFACE:include>")

target_link_libraries(idl PRIVATE Threads::Threads)

install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/idl"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev
  FILES_MATCHING PATTERN "*.h")

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/include/idl/export.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/idl/version.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/idl"
  COMPONENT dev)

install(
  TARGETS idl
  EXPORT "${CMAKE_PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
