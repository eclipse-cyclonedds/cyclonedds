#
# Copyright(c) 2021 ADLINK Technology Limited and others
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
include(GenerateDummyExportHeader)

list(APPEND sources
  atomics.c
  environ.c
  heap.c
  ifaddrs.c
  sync.c
  strtoll.c
  thread.c
  thread_cleanup.c
  string.c
  log.c
  hopscotch.c
  random.c
  retcode.c
  strlcpy.c
  socket.c
  select.c)

if(WITH_FREERTOS)
  list(APPEND sources tasklist.c)
endif()

if(HAVE_DYNLIB)
  list(APPEND sources dynlib.c)
  set(test_lib_base "cunit_ddsrt_dynlib")
  if(WIN32)
    file(TO_NATIVE_PATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" test_lib_dir)
    # MinGW prefixes .dll files with lib by default
    set(test_lib_name "${CMAKE_SHARED_LIBRARY_PREFIX}${test_lib_base}")
    set(test_lib_sep "\\\\")
    if(MINGW)
      # CMake uses forward slashed for MinGW by default
      string(REPLACE "/" "\\" test_lib_dir "${test_lib_dir}")
    endif()
  else()
    file(TO_NATIVE_PATH "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}" test_lib_dir)
    set(test_lib_name "${test_lib_base}")
    set(test_lib_sep "/")
  endif()
  set(test_lib_file "${CMAKE_SHARED_LIBRARY_PREFIX}${test_lib_base}${CMAKE_SHARED_LIBRARY_SUFFIX}")
  string(REPLACE "\\" "\\\\" test_lib_dir "${test_lib_dir}")
  configure_file(dl.h.in include/dl.h @ONLY)

  # Create a separate shared library for testing dynamic loading
  add_library(cunit_ddsrt_dynlib SHARED dl.c)
  target_include_directories(
    cunit_ddsrt_dynlib PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
endif()

add_cunit_executable(cunit_ddsrt ${sources})
add_coverage(cunit_ddsrt)
target_link_libraries(
  cunit_ddsrt PRIVATE ddsrt)
target_include_directories(
  cunit_ddsrt PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

generate_dummy_export_header(
  cunit_ddsrt
  BASE_NAME dds
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/export.h")

generate_dummy_export_header(
  cunit_ddsrt
  BASE_NAME dds
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/features.h")
