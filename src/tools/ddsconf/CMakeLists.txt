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
if(CMAKE_CROSSCOMPILING)
  find_program(DDSCONF_EXECUTABLE ddsconf REQUIRED)
  if(NOT DDSCONF_EXECUTABLE)
    message(FATAL_ERROR "ddsconf not found!")
  endif()
  add_executable(ddsconf IMPORTED GLOBAL)
  set_property(TARGET ddsconf PROPERTY IMPORTED_LOCATION ${DDSCONF_EXECUTABLE})
else()
  include(GenerateDummyExportHeader)

  add_executable(ddsconf
    ddsconf.c rnc.c md.c xsd.c defconfig.c
    ${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/src/q_config.c
    )

  target_link_libraries(ddsconf ddsrt)

  # Windows wants its pound of flesh ...
  generate_dummy_export_header(
    ddsconf
    BASE_NAME dds
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/export.h")

  target_include_directories(ddsconf
    PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/include"
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsc,INCLUDE_DIRECTORIES>>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
endif()
