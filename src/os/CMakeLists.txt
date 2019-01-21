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
include(CheckTypeSize)
include(GenerateExportHeader)
include(TestBigEndian)

set(sources
    src/os_atomics.c
    src/os_init.c
    src/os_log.c
    src/os_ifaddrs.c
    src/os_socket.c
    src/os_thread.c
    src/os_time.c
    src/os_errno.c
    src/os_iter.c
    src/os_strlcpy.c
    src/os_dns.c
    src/os_process.c
    src/os_random.c)

string(TOLOWER ${CMAKE_SYSTEM_NAME} system_name)
set(system_sources
    os_platform_errno.c
    os_platform_heap.c
    os_platform_ifaddrs.c
    os_platform_socket.c
    os_platform_stdlib.c
    os_platform_sync.c
    os_platform_thread.c
    os_platform_time.c
    os_platform_init.c
    os_platform_rusage.c)

foreach(source ${system_sources})
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/${system_name}/${source}")
    list(APPEND sources "src/${system_name}/${source}")
  else()
    list(APPEND sources "src/posix/${source}")
  endif()
endforeach()


add_library(OSAPI ${sources})

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
target_link_libraries(OSAPI INTERFACE Threads::Threads)
set_property(TARGET OSAPI PROPERTY POSITION_INDEPENDENT_CODE TRUE)

if(WIN32)
  # Link with Win32 core-libraries
  target_link_libraries(OSAPI INTERFACE wsock32 ws2_32 iphlpapi)

  # Many of the secure versions provided by Microsoft have failure modes
  # which are not supported by our abstraction layer, so efforts trying
  # to use the _s versions aren't typically the proper solution and C11
  # (which contains most of the secure versions) is 'too new'. So we rely
  # on static detection of misuse instead of runtime detection, so all
  # these warnings can be disabled on Windows.
  target_compile_definitions(OSAPI PRIVATE _CRT_SECURE_NO_WARNINGS)
  # Disable warnings for deprecated Winsock API calls in general.
  target_compile_definitions(OSAPI PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
  # Disable warnings for deprecated POSIX names.
  target_compile_definitions(OSAPI PRIVATE -D_CRT_NONSTDC_NO_DEPRECATE)
elseif(UNIX)
  check_library_exists(c clock_gettime "" HAVE_CLOCK_GETTIME)
  if(NOT HAVE_CLOCK_GETTIME)
    # Before glibc 2.17, clock_gettime was in librt.
    check_library_exists(rt clock_gettime "time.h" HAVE_CLOCK_GETTIME_RT)
    if(HAVE_CLOCK_GETTIME_RT)
      set(HAVE_CLOCK_GETTIME "${HAVE_CLOCK_GETTIME_RT}")
      target_link_libraries(OSAPI INTERFACE rt)
    endif()
  endif()

  if(NOT HAVE_CLOCK_GETTIME)
    message(FATAL_ERROR "clock_gettime is not available")
  endif()
endif()

if(${CMAKE_C_COMPILER_ID} STREQUAL "SunPro")
  target_link_libraries(OSAPI INTERFACE -lsocket -lnsl)
endif()

# Determine if platform is big or little endian.
test_big_endian(BIG_ENDIAN)
if(BIG_ENDIAN)
  set(OS_ENDIANNESS_CODE "#define OS_ENDIANNESS OS_BIG_ENDIAN")
else()
  set(OS_ENDIANNESS_CODE "#define OS_ENDIANNESS OS_LITTLE_ENDIAN")
endif()
# Determine if platform is 32 or 64 bit.
check_type_size("void*" OS_SIZEOF_VOID_P BUILTIN_TYPES_ONLY LANGUAGE C)
configure_file("cmake/os_project.h.in" "include/os/os_project.h")
target_sources(OSAPI PRIVATE "include/os/os_project.h")

generate_export_header(OSAPI EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/exports/os/osapi_export.h")

target_include_directories(
  OSAPI PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/exports/>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/>"
    "$<INSTALL_INTERFACE:${INSTALL_PREFIX}/include/>"
    "$<INSTALL_INTERFACE:${INSTALL_PREFIX}/exports/>")

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/os/${system_name}/os_platform_public.h")
  set(os_platform_public_h "${CMAKE_CURRENT_SOURCE_DIR}/include/os/${system_name}/os_platform_public.h")
else()
  set(os_platform_public_h "${CMAKE_CURRENT_SOURCE_DIR}/include/os/posix/os_platform_public.h")
endif()

install(
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/os/os_public.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/os/os_decl_attributes.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/os/os_decl_attributes_sal.h"
    "${os_platform_public_h}"
  DESTINATION
    "${CMAKE_INSTALL_INCLUDEDIR}/ddsc/os"
  COMPONENT
    dev)

