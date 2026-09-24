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

set(srcs_cdr
  "${CMAKE_CURRENT_LIST_DIR}/src/dds_cdrstream.c"
  "${CMAKE_CURRENT_LIST_DIR}/src/dds_cdrstream_keys.part.h"
  "${CMAKE_CURRENT_LIST_DIR}/src/dds_cdrstream_write.part.h")

set(hdrs_private_cdr
  "${CMAKE_CURRENT_LIST_DIR}/include/dds/cdr/dds_cdrstream.h")

if(TARGET ddsc)
  target_sources(ddsc PRIVATE ${srcs_cdr} ${hdrs_private_cdr})
  target_include_directories(ddsc PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>")

  install(
    DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT dev
  )
else()
  set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../cmake/Modules")
  if(NOT DEFINED CycloneDDS_IDLC_GENERATE_CMAKE)
    set(CycloneDDS_IDLC_GENERATE_CMAKE
        "${CMAKE_CURRENT_LIST_DIR}/../../tools/idlc/cmake/Generate.cmake")
  endif()
  include("${CycloneDDS_IDLC_GENERATE_CMAKE}")
  include(GenerateExportHeader)

  # Generate CDR library as a standalone library
  add_library(cdr)
  add_library(${PROJECT_NAME}::cdr ALIAS cdr)

  target_sources(cdr PRIVATE ${srcs_cdr} ${hdrs_private_cdr} ${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/src/bswap.c)
  target_include_directories(cdr PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
                                         "${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/include"
                                         "${CMAKE_CURRENT_LIST_DIR}/../ddsc/include"
                                         "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")                                

  configure_file(${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/include/dds/config.h.in include/dds/config.h)
  configure_file(${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/include/dds/features.h.in include/dds/features.h)
  configure_file(${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/include/dds/version.h.in include/dds/version.h)

  generate_export_header(cdr BASE_NAME DDS EXPORT_FILE_NAME include/dds/export.h)

  install(
    TARGETS cdr
    EXPORT "${PROJECT_NAME}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  )

  install(
    DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
              "${PROJECT_BINARY_DIR}/include/"
              "${CMAKE_CURRENT_LIST_DIR}/../../ddsrt/include/"
              "${CMAKE_CURRENT_LIST_DIR}/../ddsc/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT dev
  )
endif()
