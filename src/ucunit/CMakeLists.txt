#
# Copyright(c) 2023 ZettaScale Technology and others
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

if (BUILD_SHARED_LIBS OR NOT DEFINED BUILD_SHARED_LIBS)
  add_library(ucunit SHARED "")
else()
  add_library(ucunit)
endif()
add_library(${PROJECT_NAME}::ucunit ALIAS ucunit)

set(srcs_ucunit
  "${CMAKE_CURRENT_LIST_DIR}/src/ucunit.c")
set(hdrs_ucunit
  "${CMAKE_CURRENT_LIST_DIR}/include/ucunit/ucunit.h")

target_sources(ucunit PRIVATE ${srcs_ucunit} ${hdrs_ucunit})

target_include_directories(ucunit
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
  PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src"
  INTERFACE
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

generate_export_header(
  ucunit BASE_NAME UCUNIT EXPORT_FILE_NAME include/ucunit/export.h)

add_executable(test_ucunit)
target_sources(test_ucunit PRIVATE tests/test_ucunit.c)
target_link_libraries(test_ucunit CycloneDDS::ucunit)

add_test(NAME ucunit COMMAND $<TARGET_FILE_DIR:test_ucunit>/test_ucunit)
