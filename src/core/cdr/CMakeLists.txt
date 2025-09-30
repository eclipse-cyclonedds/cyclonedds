#
# Copyright(c) 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

if(NOT ${CMAKE_PROJECT_NAME} STREQUAL "CycloneDDS")
  cmake_minimum_required(VERSION 3.16)
  include(CheckLanguage)

  # Check if the Compiler in use has C support, otherwise throw error
  check_language(C)
  if(CMAKE_C_COMPILER)
    enable_language(C) # Enables C support
  else()
    message(FATAL_ERROR "Current compiler does not support C language")
  endif()
endif()

set(srcs_cdr
  "${CMAKE_CURRENT_LIST_DIR}/src/dds_cdrstream.c"
  "${CMAKE_CURRENT_LIST_DIR}/src/dds_cdrstream_keys.part.h"
  "${CMAKE_CURRENT_LIST_DIR}/src/dds_cdrstream_write.part.h")

set(hdrs_private_cdr
  "${CMAKE_CURRENT_LIST_DIR}/include/dds/cdr/dds_cdrstream.h")

if(${CMAKE_PROJECT_NAME} STREQUAL "CycloneDDS")
  target_sources(ddsc PRIVATE ${srcs_cdr} ${hdrs_private_cdr})
  target_include_directories(ddsc PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")

  install(
    DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT dev)
else()
  set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../cmake/Modules")
  include(Generate)
  include(GenerateExportHeader)

  add_library(cdr)
  add_library(${PROJECT_NAME}::cdr ALIAS cdr)

  target_sources(cdr PRIVATE ${srcs_cdr} ${hdrs_private_cdr} ${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/src/bswap.c)
  target_include_directories(cdr PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
                                         "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
                                         "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/include>"
                                         "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../ddsc/include>")

  configure_file(${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/include/dds/config.h.in include/dds/config.h)
  configure_file(${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/include/dds/features.h.in include/dds/features.h)
  configure_file(${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/include/dds/version.h.in include/dds/version.h)

  generate_export_header(
  cdr BASE_NAME DDS EXPORT_FILE_NAME include/dds/export.h)

  install(
    TARGETS cdr
    EXPORT "${PROJECT_NAME}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  )

  install(
    DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
              "${CMAKE_CURRENT_BINARY_DIR}/include/"
              "${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/include/"
              "${CMAKE_CURRENT_LIST_DIR}/../ddsc/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT dev)
endif()
