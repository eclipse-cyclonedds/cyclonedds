#
# Copyright(c) 2020 ADLINK Technology Limited and others
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
project(cyclonedds-idl LANGUAGES C)

include(GenerateExportHeader)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(TestBigEndian)

find_package(BISON REQUIRED)

bison_target(
  parser src/parser.y "${CMAKE_CURRENT_BINARY_DIR}/src/parser.c"
  DEFINES_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/src/parser.h")

add_library(
  idl SHARED
    src/scanner.c
    src/directive.c
    src/processor.c
    src/tree.c
    src/string.c
    src/expression.c
    src/scope.c
    src/hashid.c
    src/annotation.c
    ${BISON_parser_OUTPUT_SOURCE})

generate_export_header(idl EXPORT_FILE_NAME include/idl/export.h)

test_big_endian(ARCH_IS_BIG_ENDIAN)

# Locale-independent C runtime functions, like strtoull_l and strtold_l, are
# available on modern operating systems albeit with some quirks.
#
# Linux exports newlocale and freelocale from locale.h if _GNU_SOURCE is
# defined. strtoull_l and strtold_l are exported from stdlib.h, again if
# _GNU_SOURCE is defined.
#
# FreeBSD and macOS export newlocale and freelocale from xlocale.h and
# export strtoull_l and strtold_l from xlocale.h if stdlib.h is included
# before.
#
# Windows exports _create_locale and _free_locale from locale.h and exports
# _strtoull_l and _strtold_l from stdlib.h.
check_include_file(xlocale.h HAVE_XLOCALE_H)
check_include_file(locale.h HAVE_LOCALE_H)

check_c_source_compiles(
  "#include <features.h>\n\
  #ifndef __GNU_LIBRARY__\n\
  #error not GNU\n\
  #endif\n\
  int main(int argc, char *argv[]) { return 0; }\n"
  _GNU_SOURCE)

if(_GNU_SOURCE)
  set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
endif()
if(HAVE_XLOCALE_H)
  set(LOCALE_H "xlocale.h")
elseif(HAVE_LOCALE_H)
  set(LOCALE_H "locale.h")
endif()

check_symbol_exists(newlocale "${LOCALE_H}" HAVE_NEWLOCALE)
check_symbol_exists(freelocale "${LOCALE_H}" HAVE_FREELOCALE)
if(NOT HAVE_NEWLOCALE)
  check_symbol_exists(_create_locale "${LOCALE_H}" HAVE__CREATE_LOCALE)
endif()
if(NOT HAVE_FREELOCALE)
  check_symbol_exists(_free_locale "${LOCALE_H}" HAVE__FREE_LOCALE)
endif()

check_symbol_exists(strtoull_l "stdlib.h;${LOCALE_H}" HAVE_STRTOULL_L)
check_symbol_exists(strtold_l "stdlib.h;${LOCALE_H}" HAVE_STRTOLD_L)
if(NOT HAVE_STRTOULL_L)
  check_symbol_exists(_strtoull_l "stdlib.h;${LOCALE_H}" HAVE__STRTOULL_L)
endif()
if(NOT HAVE_STRTOLD_L)
  check_symbol_exists(_strtold_l "stdlib.h;${LOCALE_H}" HAVE__STRTOLD_L)
endif()

configure_file(src/config.h.in src/config.h @ONLY)
configure_file(include/idl/version.h.in include/idl/version.h @ONLY)

target_include_directories(
  idl
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
  INTERFACE
    "$<INSTALL_INTERFACE:include>")

install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/idl"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev
  FILES_MATCHING PATTERN "*.h")

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/include/idl/export.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/idl/version.h"
    "${BISON_parser_OUTPUT_HEADER}"
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
