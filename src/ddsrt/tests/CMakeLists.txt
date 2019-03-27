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
include(CUnit)

set(sources
    "atomics.c"
    "environ.c"
    "heap.c"
    "ifaddrs.c"
    "sync.c"
    "strtoll.c"
    "thread.c"
    "thread_cleanup.c"
    "string.c"
    "log.c"
    "strlcpy.c"
    "socket.c"
    "select.c"
    "process.c")

add_cunit_executable(cunit_ddsrt ${sources})
target_link_libraries(cunit_ddsrt PRIVATE ddsrt)

# Create a dummy export header. generate_export_header can only be used with
# library targets, but since the targets are linked statically,
# __declspec(dllimport) is not required anyway.
set(export_dir "${CMAKE_CURRENT_BINARY_DIR}/include/dds")
set(export_header "${export_dir}/export.h")
if(NOT EXISTS "${export_header}")
  file(MAKE_DIRECTORY "${export_dir}")
  file(WRITE "${export_header}" "#define DDS_EXPORT\n")
endif()

target_include_directories(
  cunit_ddsrt PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

# Create a separate test application that will be used to
# test process management.
add_executable(process_app process_app.c)
target_link_libraries(process_app PRIVATE ddsrt)
target_include_directories(
  process_app PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
if (MSVC_IDE)
  set(process_app_name "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/process_app${CMAKE_EXECUTABLE_SUFFIX}")
else()
  set(process_app_name "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/process_app${CMAKE_EXECUTABLE_SUFFIX}")
endif()
configure_file(
  "process_test.h.in" "${CMAKE_CURRENT_BINARY_DIR}/include/process_test.h" @ONLY)

