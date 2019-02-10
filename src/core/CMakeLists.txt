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
include (GenerateExportHeader)

FUNCTION(PREPEND var prefix)
   SET(listVar "")
   FOREACH(f ${ARGN})
      LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

option(DDSC_SHARED "Build DDSC as a shared library" ON)

if(DDSC_SHARED AND ((NOT DEFINED BUILD_SHARED_LIBS) OR BUILD_SHARED_LIBS))
  # BUILD_SHARED_LIBS is set to off by for example VxWorks DKM environment
  add_library(ddsc SHARED "")
else()
  if(DDSC_SHARED)
    message(STATUS "Option DDSC_SHARED ignored. Only static libraries supported on this platform.")
  endif()
  add_library(ddsc "")
endif()

add_definitions(-DDDSI_INCLUDE_NETWORK_PARTITIONS -DDDSI_INCLUDE_SSM)

option(DDSC_ENABLE_OPENSSL "Enable openssl support" ON)
if(DDSC_ENABLE_OPENSSL)
  find_package(OpenSSL)
  if(OPENSSL_FOUND)
    add_definitions(-DDDSI_INCLUDE_SSL)
    target_link_libraries(ddsc PRIVATE OpenSSL::SSL)
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
      set_target_properties(ddsc PROPERTIES LINK_FLAGS "/ignore:4099")
    endif()
  else()
    message(FATAL_ERROR "To build without openssl support, set DDSC_ENABLE_OPENSSL to OFF")
  endif()
endif()

include(ddsi/CMakeLists.txt)
include(ddsc/CMakeLists.txt)

target_link_libraries(ddsc PRIVATE util)
target_link_libraries(ddsc PRIVATE OSAPI)

# SOVERSION should increase on incompatible ABI change
set_target_properties(ddsc PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

get_target_property(os_api_src_dir OSAPI SOURCE_DIR)
# We need to expose some of the OS headers as well.
target_include_directories(ddsc
    PUBLIC
        "$<BUILD_INTERFACE:${os_api_src_dir}/include>")

set_target_file_ids(ddsc)

# Create a pseudo-target that other targets (i.e. examples, tests) can depend
# on and can also be provided as import-target by a package-file when building
# those targets outside the regular Cyclone build-tree (i.e. the installed tree)
add_library(${CMAKE_PROJECT_NAME}::ddsc ALIAS ddsc)

install(
  TARGETS ddsc
  EXPORT "${CMAKE_PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/xtests")
