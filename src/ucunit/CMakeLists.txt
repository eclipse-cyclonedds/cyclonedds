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

set(srcs_ucunit
  "${CMAKE_CURRENT_LIST_DIR}/src/ucunit.c")
set(hdrs_ucunit
  "${CMAKE_CURRENT_LIST_DIR}/include/ucunit/ucunit.h")

add_library(ucunit STATIC ${srcs_ucunit} ${hdrs_ucunit})
add_library(${PROJECT_NAME}::ucunit ALIAS ucunit)

target_include_directories(ucunit
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
  PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src"
  INTERFACE
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_executable(test_ucunit ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ucunit.c)
target_link_libraries(test_ucunit CycloneDDS::ucunit)

add_test(NAME ucunit COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE_DIR:test_ucunit>/test_ucunit)

install(TARGETS ucunit EXPORT "${PROJECT_NAME}")