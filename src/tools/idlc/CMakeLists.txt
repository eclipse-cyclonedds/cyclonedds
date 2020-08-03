#
# Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
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

bison_target(parser src/parser.y "${CMAKE_CURRENT_BINARY_DIR}/parser.c")

add_library(idlc-lib STATIC
  src/idl.c
  src/tt_create.c
  src/gen_c99.c
  src/gen_ostream.c
  src/scanner.c
  src/directive.c
  ${BISON_parser_OUTPUTS})

target_include_directories(idlc-lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(idlc-lib PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(idlc-lib PUBLIC ddsts)

add_executable(idlc "src/main.c")
target_link_libraries(idlc PRIVATE idlc-lib idlpp)
