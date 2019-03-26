#
# Copyright(c) 2019 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
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
cmake_minimum_required(VERSION 3.5)



# Create a dummy export header. generate_export_header can only be used with
# library targets, but since the targets are linked statically,
# __declspec(dllimport) is not required anyway.
set(export_dir "${CMAKE_CURRENT_BINARY_DIR}/include/dds")
set(export_header "${export_dir}/export.h")
if(NOT EXISTS "${export_header}")
  file(MAKE_DIRECTORY "${export_dir}")
  file(WRITE "${export_header}" "#define DDS_EXPORT\n")
endif()



#
# Create the process tests application and add it to the overall tests.
#
if (${DDSRT_USE_PROCESSCREATION})
  add_executable(ddsrt_process_test process_test.c)
  target_link_libraries(ddsrt_process_test PRIVATE ddsrt)

  target_include_directories(
    ddsrt_process_test PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

  add_test(NAME ddsrt_process_create  COMMAND ddsrt_process_test -t create )
  add_test(NAME ddsrt_process_destroy COMMAND ddsrt_process_test -t destroy)
  add_test(NAME ddsrt_process_pid     COMMAND ddsrt_process_test -t pid    )
  add_test(NAME ddsrt_process_env     COMMAND ddsrt_process_test -t env    )
  add_test(NAME ddsrt_process_invalid COMMAND ddsrt_process_test -t invalid)
endif()

