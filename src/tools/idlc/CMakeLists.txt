#
# Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

# Create directory where Bison and Flex generated sources will be placed.
set(GENERATED_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/src")
file(MAKE_DIRECTORY "${GENERATED_SOURCE_DIR}")

set(parser_path "${GENERATED_SOURCE_DIR}/idl.parser.c")
set(lexer_path "${GENERATED_SOURCE_DIR}/idl.lexer.c")

bison_target(parser "src/idl.y" "${parser_path}")
flex_target(
  lexer "src/idl.l" "${lexer_path}"
  COMPILE_FLAGS
    "--header-file=${GENERATED_SOURCE_DIR}/idl.lexer.h")

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  # Microsoft Visual Studio does not define __STDC_VERSION__. As a result,
  # Flex defines the C99 limit macros, which causes redefinition warnings.
  set_source_files_properties(
    "${lexer_path}" PROPERTIES COMPILE_DEFINITIONS __STDC_VERSION__=199901L)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR
       CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(lexer_cflags "-Wno-sign-conversion"
                   "-Wno-conversion"
                   "-Wno-sign-compare"
                   "-Wno-unused-parameter"
                   "-Wno-unused-function"
                   "-Wno-documentation"
                   "-Wno-missing-prototypes")
  set(parser_cflags "-Wno-sign-conversion")
  list(JOIN lexer_cflags " " lexer_cflags)
  list(JOIN parser_cflags " " parser_cflags)
  set_source_files_properties(
    "${lexer_path}" PROPERTIES COMPILE_FLAGS "${lexer_cflags}")
  set_source_files_properties(
    "${parser_path}" PROPERTIES COMPILE_FLAGS "${parser_cflags}")
endif()

set(source_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(idlc-lib STATIC
    "src/parser.c"
    "src/tt_create.c"
    "src/gen_c99.c"
    "src/gen_ostream.c"
    "${BISON_parser_OUTPUTS}"
    "${FLEX_lexer_OUTPUTS}")

target_include_directories(idlc-lib PUBLIC "src")
target_include_directories(idlc-lib PRIVATE "${GENERATED_SOURCE_DIR}")

target_link_libraries(idlc-lib PUBLIC ddsc)
target_link_libraries(idlc-lib PUBLIC ddsts)

add_executable(idlc "src/main.c")
target_link_libraries(idlc PRIVATE idlc-lib)

