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

add_cunit_executable(cunit_util "handleserver.c")
target_link_libraries(cunit_util PRIVATE util)
target_link_libraries(cunit_util PRIVATE ddsrt)

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
  cunit_util PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
