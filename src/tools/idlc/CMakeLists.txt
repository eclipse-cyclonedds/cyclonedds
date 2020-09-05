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
include(CheckIncludeFile)

check_include_file(getopt.h HAVE_GETOPT_H)

configure_file(config.h.in config.h)

add_executable(idlc idlc.c generator.c)
target_link_libraries(idlc PRIVATE idl idlpp ${CMAKE_DL_LIBS})
if(NOT HAVE_GETOPT_H)
  # use getopt.h from ddsrt
  file(READ "${CMAKE_SOURCE_DIR}/src/ddsrt/include/getopt.h.in" getopt_h)
  # remove occurrences of DDS_EXPORT
  string(REGEX REPLACE "\n[ \t]*DDS_EXPORT[ \t]+" "\n" getopt_h "${getopt_h}")
  # remove dds/* includes
  string(REGEX REPLACE "\n[ \t]*#[ \t]*include[ \t]+[<\"]dds/[^\n]*" "" getopt_h "${getopt_h}")
  # generate getopt.h
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/getopt.h" CONTENT "${getopt_h}")
  target_include_directories(idlc PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
  # add getopt.c
  target_sources(idlc PUBLIC ${CMAKE_SOURCE_DIR}/src/ddsrt/src/getopt.c)
endif()
