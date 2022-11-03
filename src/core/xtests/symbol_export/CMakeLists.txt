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

add_executable(symbol_export_test symbol_export.c)

target_link_libraries(symbol_export_test PRIVATE ddsc)
target_include_directories(symbol_export_test PRIVATE
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../cdr/include>")

if (ENABLE_SHM)
  target_include_directories(
    symbol_export_test PRIVATE
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:iceoryx_binding_c::iceoryx_binding_c,INTERFACE_INCLUDE_DIRECTORIES>>")
endif()
if (ENABLE_SECURITY)
    target_link_libraries(symbol_export_test PRIVATE security_api)
    target_include_directories(symbol_export_test PRIVATE
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_api,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_core,INTERFACE_INCLUDE_DIRECTORIES>>")
endif()
