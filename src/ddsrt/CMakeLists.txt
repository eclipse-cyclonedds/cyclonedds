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
include(CheckCSourceCompiles)
include(CheckLibraryExists)
include(GenerateDummyExportHeader)

# Lightweight IP stack can be used on non-embedded targets too, but the
# runtime must be instructed to use it instead of the native stack. Of course
# for embedded targets there is no "native" stack and the runtime module must
# always be instructed to use an "alternative" stack.
option(WITH_LWIP "Use lightweight IP stack" OFF)
option(WITH_DNS "Enable domain name lookups" ON)
option(WITH_FREERTOS "Build for FreeRTOS" OFF)

function(check_runtime_feature SOURCE_FILE)
  get_target_property(_defs ddsrt INTERFACE_COMPILE_DEFINITIONS)
  foreach(_def ${_defs})
    set(_strdefs "${_strdefs} -D${_def}")
  endforeach()

  # Generate dummy export header required by feature tests.
  generate_dummy_export_header(
    ddsrt
    BASE_NAME dds
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/cmake/include/dds/export.h")
  set(_strincs "${CMAKE_CURRENT_BINARY_DIR}/cmake/include")

  get_target_property(_incs ddsrt INTERFACE_INCLUDE_DIRECTORIES)
  foreach(_inc ${_incs})
    set(_strincs "${_strincs};${_inc}")
  endforeach()

  if(_strincs)
    set(_strincs "-DINCLUDE_DIRECTORIES:STRING=${_strincs}")
  endif()

  set(expr "cmake_([_a-zA-Z0-9]+)=([_a-zA-Z0-9]+)")
  try_compile(
    foo "${CMAKE_BINARY_DIR}"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}"
    CMAKE_FLAGS "${_strincs}"
    COMPILE_DEFINITIONS "${_strdefs}"
    OUTPUT_VARIABLE output)
  string(REGEX MATCHALL "${expr}" matches "${output}")
  foreach(match ${matches})
    string(REGEX REPLACE "${expr}" "\\1" option "${match}")
    string(REGEX REPLACE "${expr}" "\\2" value "${match}")
    set(${option} ${value} PARENT_SCOPE)
  endforeach()
endfunction()

if(WITH_FREERTOS)
  set(system_name freertos)
elseif(APPLE)
  set(system_name darwin)
elseif(ANDROID)
  # FIXME: Not correct, but will do for the short-term. A better way would be
  #        fallback to linux, and then posix.
  set(system_name linux)
else()
  string(TOLOWER ${CMAKE_SYSTEM_NAME} system_name)
endif()

# A better choice is to use a so-called object library for ddsrt, but it was
# not possible to use target_link_libraries with object libraries until CMake
# 3.12. At the time of this writing most long-term stable distributions still
# ship an older version, so an interface library with public sources is used
# as a workaround for now.
add_library(ddsrt INTERFACE IMPORTED GLOBAL)

foreach(opt WITH_LWIP WITH_DNS WITH_FREERTOS)
  if(${opt})
    target_compile_definitions(ddsrt INTERFACE DDSRT_${opt}=1)
  else()
    target_compile_definitions(ddsrt INTERFACE DDSRT_${opt}=0)
  endif()
endforeach()

target_include_directories(
  ddsrt INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

# Generate version header.
configure_file("include/dds/version.h.in" "include/dds/version.h")
target_sources(
  ddsrt INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include/dds/version.h")

if(WIN32)
  configure_file("include/getopt.h.in" "include/getopt.h" COPYONLY)
  list(APPEND headers "${CMAKE_CURRENT_BINARY_DIR}/include/getopt.h")
  list(APPEND sources "${CMAKE_CURRENT_SOURCE_DIR}/src/getopt.c")
endif()

set(include_path "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(source_path "${CMAKE_CURRENT_SOURCE_DIR}/src")

list(APPEND headers
  "${include_path}/dds/ddsrt/avl.h"
  "${include_path}/dds/ddsrt/fibheap.h"
  "${include_path}/dds/ddsrt/hopscotch.h"
  "${include_path}/dds/ddsrt/log.h"
  "${include_path}/dds/ddsrt/retcode.h"
  "${include_path}/dds/ddsrt/attributes.h"
  "${include_path}/dds/ddsrt/endian.h"
  "${include_path}/dds/ddsrt/arch.h"
  "${include_path}/dds/ddsrt/misc.h"
  "${include_path}/dds/ddsrt/mh3.h"
  "${include_path}/dds/ddsrt/io.h"
  "${include_path}/dds/ddsrt/process.h"
  "${include_path}/dds/ddsrt/sched.h"
  "${include_path}/dds/ddsrt/strtod.h"
  "${include_path}/dds/ddsrt/strtol.h"
  "${include_path}/dds/ddsrt/types.h"
  "${include_path}/dds/ddsrt/countargs.h"
  "${include_path}/dds/ddsrt/static_assert.h"
  "${include_path}/dds/ddsrt/circlist.h")

list(APPEND sources
  "${source_path}/bswap.c"
  "${source_path}/io.c"
  "${source_path}/log.c"
  "${source_path}/retcode.c"
  "${source_path}/strtod.c"
  "${source_path}/strtol.c"
  "${source_path}/mh3.c"
  "${source_path}/avl.c"
  "${source_path}/environ.c"
  "${source_path}/expand_vars.c"
  "${source_path}/fibheap.c"
  "${source_path}/hopscotch.c"
  "${source_path}/xmlparser.c"
  "${source_path}/circlist.c")

# Not every target offers the same set of features. For embedded targets the
# set of features may even be different between builds. e.g. a FreeRTOS build
# could use the lightweight IP stack, but later change to FreeRTOS+TCP.
#
# Most features and target specific settings can be determined at compile time
# by a combination of pre-defined macros. However, some features require input
# from the build system. e.g. that the target operating system is FreeRTOS or
# that the network stack to be used is lwIP as opposed to the target native
# network stack. In order to mix-and-match various compilers, architectures,
# operating systems, etc input from the build system is required.
foreach(feature atomics cdtors environ heap ifaddrs random rusage
        sockets string sync threads time md5 process netstat dynlib filesystem)
  if(EXISTS "${include_path}/dds/ddsrt/${feature}.h")
    list(APPEND headers "${include_path}/dds/ddsrt/${feature}.h")
    file(GLOB_RECURSE
           files
         CONFIGURE_DEPENDS
           "${include_path}/dds/ddsrt/${feature}/*.h")
    list(APPEND headers ${files})

    # Do not add any sources if a feature is not offered by the target. The
    # headers will define any macros needed by the source code by combining
    # build system exports and pre-defined macros. To determine if a certain
    # feature is offered by the target and whether or not to compile any
    # source files, that information must be made available to CMake.
    #
    # Tests that export the required information can be written in
    # cmake/${feature}.c. A test consists of a source file that includes the
    # required header files and results in a specifically crafted compiler
    # error that is picked up by CMake. By default, if a file named after the
    # feature does not exist in cmake, the feature is expected to be
    # implemented for all targets.
    string(TOUPPER "${feature}" feature_uc)
    set(HAVE_${feature_uc} TRUE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${feature}.c")
      check_runtime_feature("cmake/${feature}.c")
    endif()

    if(HAVE_${feature_uc})
      # Code that is more-or-less the same for all targets can placed in
      # src/<feature>.c. An example would be ddsrt_freeifaddrs.
      if(EXISTS "${source_path}/${feature}.c")
        list(APPEND sources "${source_path}/${feature}.c")
      endif()
      set(system_exists FALSE)

      # Headers that must remain private but are required by other runtime
      # source files must be located in src/<feature>/dds/ddsrt.
      if(IS_DIRECTORY "${source_path}/${feature}/include")
        file(GLOB_RECURSE
               files
             CONFIGURE_DEPENDS
               "${source_path}/${feature}/include/*.h")
        list(APPEND headers ${files})
        target_include_directories(
          ddsrt INTERFACE
            "$<BUILD_INTERFACE:${source_path}/${feature}/include/>")
      endif()

      # Allow custom implementations for a feature. e.g. lwip as opposed to
      # windows or posix.
      set(_system_name "${system_name}")
      if(NOT HAVE_${feature_uc} MATCHES "[tT][rR][uU][eE]")
        set(_system_name "${HAVE_${feature_uc}}")
      endif()

      foreach(system ${_system_name} posix)
        if(IS_DIRECTORY "${source_path}/${feature}/${system}")
          file(GLOB_RECURSE
                 files
               CONFIGURE_DEPENDS
                 "${source_path}/${feature}/${system}/*.c")
          list(APPEND sources ${files})
          set(system_exists TRUE)
        endif()
        # Break as soon a system-specific headers or sources are found.
        if(system_exists)
          break()
        endif()
      endforeach()
    else()
      message(STATUS "Feature ${feature} disabled")
    endif()
  else()
    message(FATAL_ERROR "Feature ${feature} does not exist")
  endif()
endforeach()

target_sources(ddsrt INTERFACE ${sources} ${headers})

set(HAVE_MULTI_PROCESS ${HAVE_MULTI_PROCESS} PARENT_SCOPE)

if(NOT WITH_FREERTOS)
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  find_package(Threads REQUIRED)
  target_link_libraries(ddsrt INTERFACE Threads::Threads)
  target_link_libraries(ddsrt INTERFACE ${CMAKE_DL_LIBS})
endif()

if(WIN32)
  target_link_libraries(ddsrt INTERFACE ws2_32 iphlpapi bcrypt)
elseif(UNIX)
  check_library_exists(c clock_gettime "" HAVE_CLOCK_GETTIME)
  if(NOT HAVE_CLOCK_GETTIME)
    # Before glibc 2.17, clock_gettime was in librt.
    check_library_exists(rt clock_gettime "time.h" HAVE_CLOCK_GETTIME_RT)
    if(HAVE_CLOCK_GETTIME_RT)
      set(HAVE_CLOCK_GETTIME TRUE)
      target_link_libraries(ddsrt INTERFACE rt)
    endif()
  endif()

  if(NOT HAVE_CLOCK_GETTIME)
    message(FATAL_ERROR "clock_gettime is not available")
  endif()
endif()

if(${CMAKE_C_COMPILER_ID} STREQUAL "SunPro")
  target_link_libraries(ddsrt INTERFACE socket nsl)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

install(
  DIRECTORY
      "include/"
      ${CMAKE_CURRENT_BINARY_DIR}/include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev
  FILES_MATCHING PATTERN "*.h")
