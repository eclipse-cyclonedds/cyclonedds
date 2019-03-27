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

# Find Bison and Flex
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

# Create directory where Bison and Flex generated sources will be placed.
set(GENERATED_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/src")
file(MAKE_DIRECTORY "${GENERATED_SOURCE_DIR}")

set(PARSER_BASENAME "idl.parser.c")
set(PARSER_PATH "${GENERATED_SOURCE_DIR}/${PARSER_BASENAME}")
set(LEXER_BASENAME "idl.lexer.c")
set(LEXER_PATH "${GENERATED_SOURCE_DIR}/${LEXER_BASENAME}")

bison_target(parser "src/idl.y" "${PARSER_PATH}")
flex_target(
  lexer "src/idl.l" "${LEXER_PATH}"
  COMPILE_FLAGS
    "--header-file=${GENERATED_SOURCE_DIR}/idl.lexer.h")

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  # Microsoft Visual Studio does not define __STDC_VERSION__. As a result,
  # Flex defines the C99 limit macros, which causes redefinition warnings.
  set_source_files_properties(
    "${LEXER_PATH}" PROPERTIES COMPILE_DEFINITIONS __STDC_VERSION__=199901L)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR
       CMAKE_C_COMPILER_ID MATCHES "Clang") # Takes care of AppleClang too.
  set_source_files_properties(
    "${LEXER_PATH}" PROPERTIES COMPILE_FLAGS "-Wno-sign-conversion -Wno-conversion -Wno-sign-compare -Wno-unused-parameter -Wno-unused-function")
  set_source_files_properties(
    "${PARSER_PATH}" PROPERTIES COMPILE_FLAGS "-Wno-sign-conversion")
endif()

set(source_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(idll STATIC
    "${source_dir}/parser.c"
    "${source_dir}/tt_create.c"
    "${BISON_parser_OUTPUTS}"
    "${FLEX_lexer_OUTPUTS}")

target_include_directories(idll PUBLIC "src")
target_include_directories(idll PRIVATE "${GENERATED_SOURCE_DIR}")

target_link_libraries(idll PUBLIC ddsc)
target_link_libraries(idll PUBLIC ddsts)

add_executable(idlpp "src/main.c")
target_link_libraries(idlpp PRIVATE idll)

