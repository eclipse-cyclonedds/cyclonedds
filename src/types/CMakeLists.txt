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
set(GENERATED_SOURCE_DIR "${CMAKE_BINARY_DIR}/src/types/src")
file(MAKE_DIRECTORY "${GENERATED_SOURCE_DIR}")

set(PARSER_BASENAME "idl.parser.c")
set(PARSER_PATH "${GENERATED_SOURCE_DIR}/${PARSER_BASENAME}")
set(LEXER_BASENAME "idl.lexer.c")
set(LEXER_PATH "${GENERATED_SOURCE_DIR}/${LEXER_BASENAME}")

bison_target( parser "src/idl.y" "${PARSER_PATH}")
flex_target(
  lexer "src/idl.l" "${LEXER_PATH}"
  COMPILE_FLAGS
    "--header-file=${GENERATED_SOURCE_DIR}/idl.lexer.h")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set_source_files_properties("${PARSER_PATH}" PROPERTIES COMPILE_FLAGS "-Wno-conversion -Wno-sign-conversion")
  set_source_files_properties("${LEXER_PATH}" PROPERTIES COMPILE_FLAGS "-Wno-conversion -Wno-sign-conversion -Wno-unused-parameter -Wno-unused-function")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set_source_files_properties("${PARSER_PATH}" PROPERTIES COMPILE_FLAGS "-Wno-conversion -Wno-sign-conversion")
  set_source_files_properties("${LEXER_PATH}" PROPERTIES COMPILE_FLAGS "-Wno-conversion -Wno-sign-conversion -Wno-unused-parameter -Wno-unused-function")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set_source_files_properties("${LEXER_PATH}" PROPERTIES COMPILE_FLAGS "/wd4005") # suppress macro redefinition warnings
endif()

add_library(types "src/parser.c" "src/typetree.c" "src/tt_create.c" "src/type_walker.c" "src/stringify.c" "${BISON_parser_OUTPUTS}" "${FLEX_lexer_OUTPUTS}")
target_include_directories(types PRIVATE "include")
target_include_directories(types PRIVATE "src")
target_include_directories(types PRIVATE "${GENERATED_SOURCE_DIR}")

target_link_libraries(types PRIVATE ddsrt)

get_target_property(os_api_src_dir ddsc INCLUDE_DIRECTORIES)
# We need to expose some of the OS headers as well.
target_include_directories(types PUBLIC "$<BUILD_INTERFACE:${os_api_src_dir}/include>")

