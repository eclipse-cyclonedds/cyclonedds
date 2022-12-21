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
include(CUnit)

set(sources
  atomics.c
  bits.c
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
  # Create a separate shared library for testing dynamic loading
  add_library(cunit_ddsrt_dynlib SHARED dl.c)
  target_include_directories(
    cunit_ddsrt_dynlib PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

  # Make sure we know where to find the library, even on multi-target generators
  # which seem to usually append Debug/Release/... to the directory, but not, it
  # seems, on Azure.
  set_target_properties(
    cunit_ddsrt_dynlib
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY                ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY                ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_CURRENT_BINARY_DIR} )
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}" test_lib_dir)

  # Make the location of the test library available to the test program
  set(test_lib_base "cunit_ddsrt_dynlib")
  if(WIN32)
    # MinGW prefixes .dll files with lib by default
    set(test_lib_name "${CMAKE_SHARED_LIBRARY_PREFIX}${test_lib_base}")
    set(test_lib_sep "\\\\")
    if(MINGW)
      # CMake uses forward slashed for MinGW by default
      string(REPLACE "/" "\\" test_lib_dir "${test_lib_dir}")
    endif()
  else()
    set(test_lib_name "${test_lib_base}")
    set(test_lib_sep "/")
  endif()
  set(test_lib_file "${CMAKE_SHARED_LIBRARY_PREFIX}${test_lib_base}${CMAKE_SHARED_LIBRARY_SUFFIX}")
  string(REPLACE "\\" "\\\\" test_lib_dir "${test_lib_dir}")
  configure_file(dl.h.in include/dl.h @ONLY)
endif()

add_cunit_executable(cunit_ddsrt ${sources})

# Ensure the directory containing the test library for the dlopen tests is in
# PATH/LD_LIBRARY_PATH (macOS is fine by virtue of rpath) so that the tests that do not
# specify the full file name can work.
#
# (Needs to be done after `add_cunit_executable`)
if(HAVE_DYNLIB)
  unset(test_lib_tests)
  process_cunit_source_file("dynlib.c" test_lib_header test_lib_suites test_lib_tests)
  foreach(libtest ${test_lib_tests})
    string(REPLACE ":" ";" libtest ${libtest})
    list(GET libtest 0 suite)
    list(GET libtest 1 test)
    set(libtestname "CUnit_${suite}_${test}")
    if("${CMAKE_HOST_SYSTEM}" MATCHES ".*Windows.*")
      set_property(TEST ${libtestname} APPEND PROPERTY ENVIRONMENT "${test_lib_dir}")
    else()
      set_property(TEST ${libtestname} APPEND PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${test_lib_dir};$ENV{LD_LIBRARY_PATH}")
    endif()
  endforeach()
endif()

add_coverage(cunit_ddsrt)
target_link_libraries(
  cunit_ddsrt PRIVATE ddsrt-internal)
target_include_directories(
  cunit_ddsrt PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
