#
# Copyright(c) 2019 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
idlc_generate(TARGET RhcTypes FILES RhcTypes.idl WARNINGS no-implicit-extensibility)

add_executable(rhc_torture rhc_torture.c)

target_include_directories(
  rhc_torture PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../cdr/include>")

if(ENABLE_SHM)
  target_include_directories(
    rhc_torture PRIVATE
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:iceoryx_binding_c::iceoryx_binding_c,INTERFACE_INCLUDE_DIRECTORIES>>")
endif()

target_link_libraries(rhc_torture RhcTypes ddsc)

add_test(
  NAME rhc_torture
  COMMAND rhc_torture 314159265 0 5000 0 1 20)
set_property(TEST rhc_torture PROPERTY TIMEOUT 30)
set_test_library_paths(rhc_torture)
