#
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
include(CUnit)

set(ddsi_test_sources
    "ipaddr.c"
    "locators.c"
    "plist_generic.c"
    "plist.c"
    "plist_leasedur.c"
    "radmin.c"
    "sysdeps.c"
    "mem_ser.h")

if(ENABLE_SECURITY)
  set(ddsi_test_sources ${ddsi_test_sources} "security_msg.c")
endif()

add_cunit_executable(cunit_ddsi ${ddsi_test_sources})

# need DDSC private header files only for dds_global
target_include_directories(
  cunit_ddsi PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../cdr/include>")
if(ENABLE_SHM)
  target_include_directories(
    cunit_ddsi PRIVATE
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:iceoryx_binding_c::iceoryx_binding_c,INTERFACE_INCLUDE_DIRECTORIES>>")
endif()
target_link_libraries(cunit_ddsi PRIVATE ddsc)
