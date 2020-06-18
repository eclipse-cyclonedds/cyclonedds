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
include(ListJoin)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

# Create directory where Bison and Flex generated sources will be placed.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src")

set(idl_l_h "${CMAKE_CURRENT_BINARY_DIR}/src/idl.l.h")
set(idl_l_c "${CMAKE_CURRENT_BINARY_DIR}/src/idl.l.c")
set(idl_y_c "${CMAKE_CURRENT_BINARY_DIR}/src/idl.y.c")

flex_target(lexer "src/idl.l" "${idl_l_c}" COMPILE_FLAGS "--header-file=${idl_l_h}")
bison_target(parser "src/idl.y" "${idl_y_c}")

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  # Microsoft Visual Studio does not define __STDC_VERSION__. As a result,
  # Flex defines the C99 limit macros, which causes redefinition warnings.
  set_source_files_properties(
    "${idl_l_c}" PROPERTIES COMPILE_DEFINITIONS __STDC_VERSION__=199901L)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR
       CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(lexer_cflags "-Wno-sign-conversion"
                   "-Wno-conversion"
                   "-Wno-sign-compare"
                   "-Wno-unused-parameter"
                   "-Wno-unused-function"
                   "-Wno-documentation"
                   "-Wno-missing-prototypes")
  set(parser_cflags "-Wno-sign-conversion"
                    "-Wno-conversion")
  list_join(lexer_cflags " " lexer_cflags)
  list_join(parser_cflags " " parser_cflags)
  set_source_files_properties(
    "${idl_l_c}" PROPERTIES COMPILE_FLAGS "${lexer_cflags}")
  set_source_files_properties(
    "${idl_y_c}" PROPERTIES COMPILE_FLAGS "${parser_cflags}")
endif()

set(source_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(idlc-lib STATIC
    "src/idl.c"
    "src/tt_create.c"
    "src/gen_c99.c"
    "src/gen_ostream.c"
    "${BISON_parser_OUTPUTS}"
    "${FLEX_lexer_OUTPUTS}")

target_include_directories(idlc-lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(idlc-lib PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")

target_link_libraries(idlc-lib PUBLIC ddsc)
target_link_libraries(idlc-lib PUBLIC ddsts)

add_executable(idlc "src/main.c")
target_link_libraries(idlc PRIVATE idlc-lib)

