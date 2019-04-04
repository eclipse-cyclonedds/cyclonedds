#
# Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
include (GenerateExportHeader)

PREPEND(srcs_util "${CMAKE_CURRENT_SOURCE_DIR}/src" ut_avl.c ut_crc.c ut_expand_envvars.c ut_fibheap.c ut_handleserver.c ut_hopscotch.c ut_thread_pool.c ut_xmlparser.c)

add_library(util ${srcs_util})
set_property(TARGET util PROPERTY POSITION_INDEPENDENT_CODE TRUE)

generate_export_header(
  util
  BASE_NAME
    dds
  EXPORT_FILE_NAME
    "${CMAKE_CURRENT_BINARY_DIR}/include/dds/export.h")

# util must NOT link with ddsrt to avoid duplicate symbols in ddsc!
# ddsrt include directories are required though.
target_compile_definitions(
  util
  PUBLIC
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_COMPILE_DEFINITIONS>>)
target_include_directories(
  util
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>")

install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/dds"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev)

# TODO: improve test inclusion.
if((BUILD_TESTING) AND ((NOT DEFINED MSVC_VERSION) OR (MSVC_VERSION GREATER "1800")))
  add_subdirectory(tests)
endif()
