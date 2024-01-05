# Copyright(c) 2006 to 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
set(source_dir "${CMAKE_CURRENT_SOURCE_DIR}")
set(binary_dir "${CMAKE_CURRENT_BINARY_DIR}")

add_library(durability INTERFACE)

target_include_directories(
  durability INTERFACE
    "$<BUILD_INTERFACE:${source_dir}/include>"
    "$<BUILD_INTERFACE:${source_dir}/src>"
    "$<BUILD_INTERFACE:${binary_dir}/include>")

set(headers
  "${source_dir}/include/dds/durability/dds_durability.h"
  "${source_dir}/include/dds/durability/durablesupport.h")

set(sources
  "${source_dir}/src/dds_durability.c"
  "${source_dir}/src/durablesupport.c")
  
target_sources(durability INTERFACE ${headers} ${sources})

install(
  DIRECTORY
      "${source_dir}/include/"
      "${binary_dir}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev
  FILES_MATCHING PATTERN "*.h")