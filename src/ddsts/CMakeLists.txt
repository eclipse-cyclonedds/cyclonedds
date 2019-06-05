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

include(GenerateExportHeader)

PREPEND(srcs_ddsts "${CMAKE_CURRENT_SOURCE_DIR}/src"
    typetree.c
    type_walk.c)
set(headers
    "include/dds/ddsts/typetree.h")

add_library(ddsts SHARED ${srcs_ddsts})

generate_export_header(
    ddsts
    BASE_NAME
      dds
    EXPORT_FILE_NAME
      "${CMAKE_CURRENT_BINARY_DIR}/include/dds/export.h"
)

target_include_directories(
  ddsts
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>")

target_link_libraries(
  ddsts
  PRIVATE
    ddsc
)
