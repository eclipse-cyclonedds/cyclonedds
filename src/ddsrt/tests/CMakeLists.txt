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
include(GenerateExportHeader)
include(GenerateDummyExportHeader)

list(APPEND sources
  "atomics.c"
  "dynlib.c"
  "environ.c"
  "heap.c"
  "ifaddrs.c"
  "sync.c"
  "strtoll.c"
  "thread.c"
  "thread_cleanup.c"
  "string.c"
  "log.c"
  "hopscotch.c"
  "random.c"
  "retcode.c"
  "strlcpy.c"
  "socket.c"
  "select.c")

if(HAVE_MULTI_PROCESS)
  list(APPEND sources "process.c")
endif()
if(WITH_FREERTOS)
  list(APPEND sources "tasklist.c")
endif()

add_cunit_executable(cunit_ddsrt ${sources})
target_link_libraries(
  cunit_ddsrt PRIVATE ddsrt)
target_include_directories(
  cunit_ddsrt PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")


# Create a separate shared library that will be used to
# test dynamic library loading.
set(test_lib_name "dltestlib")
add_library(${test_lib_name} SHARED dllib.c)
# Force the lib to be at the same location, no matter what platform or build type.
set_target_properties(
        ${test_lib_name}
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
# Use proper export for this test lib.
#generate_dummy_export_header(${test_lib_name} BASE_NAME dds LIB_TEST)
target_include_directories(${test_lib_name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
# Let the cunit application know the location and name of the library.
file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}" test_lib_native_dir)
file(TO_NATIVE_PATH "/" test_lib_sep)
string(REPLACE "\\" "\\\\" test_lib_dir ${test_lib_native_dir})
string(REPLACE "\\" "\\\\" test_lib_sep ${test_lib_sep})
configure_file("dl_test.h.in" "${CMAKE_CURRENT_BINARY_DIR}/include/dl_test.h" @ONLY)
# Let ctest set the proper library path when executing library tests.
unset(test_lib_tests)
process_cunit_source_file("dynlib.c" test_lib_header test_lib_suites test_lib_tests)
foreach(libtest ${test_lib_tests})
  string(REPLACE ":" ";" libtest ${libtest})
  list(GET libtest 0 suite)
  list(GET libtest 1 test)
  set(libtestname "CUnit_${suite}_${test}")
  if("${CMAKE_HOST_SYSTEM}" MATCHES ".*Windows.*")
    set_property(TEST ${libtestname} APPEND PROPERTY ENVIRONMENT "${test_lib_native_dir}")
  else()
    set_property(TEST ${libtestname} APPEND PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${test_lib_native_dir};$ENV{LD_LIBRARY_PATH}")
  endif()
endforeach()

generate_dummy_export_header(
  cunit_ddsrt
  BASE_NAME dds
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/export.h")

generate_export_header(${test_lib_name} BASE_NAME LIB_TEST)

if(HAVE_MULTI_PROCESS)
  # A separate application is required to test process management.
  add_executable(process_app process_app.c)
  target_link_libraries(process_app PRIVATE ddsrt)
  target_include_directories(
    process_app
    PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
  # Force the app to be at the same location, no matter what platform or build type.
  # FIXME: What if custom targets are added?
  # FIXME: What debug and release builds are mixed on Windows and macOS?
  set_target_properties(
    process_app
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY                ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_CURRENT_BINARY_DIR} )
  # Let the cunit application know the location and name of the test application.
  set(process_app_name "${CMAKE_CURRENT_BINARY_DIR}/process_app${CMAKE_EXECUTABLE_SUFFIX}")
  configure_file(
    "process_test.h.in" "${CMAKE_CURRENT_BINARY_DIR}/include/process_test.h" @ONLY)
endif()

