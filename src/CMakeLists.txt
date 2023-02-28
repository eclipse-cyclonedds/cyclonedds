#
# Copyright(c) 2018 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
if(NOT ${PROJECT_NAME} STREQUAL "CycloneDDS")
  get_filename_component(dir ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
  message(FATAL_ERROR "Top-level CMakeLists.txt was moved to the top-level directory.  Please run cmake on ${dir} instead of ${CMAKE_CURRENT_LIST_DIR}")
endif()

function(PREPEND var prefix)
  set(listVar "")
  foreach(f ${ARGN})
     list(APPEND listVar "${prefix}/${f}")
  endforeach()
  set(${var} "${listVar}" PARENT_SCOPE)
endfunction()

option(ENABLE_SECURITY "Enable OMG DDS Security support" ON)
option(ENABLE_LIFESPAN "Enable Lifespan QoS support" ON)
option(ENABLE_DEADLINE_MISSED "Enable Deadline Missed QoS support" ON)
option(SKIP_DEADLINE_UPDATE "Skip deadline update test" OFF)
option(ENABLE_NETWORK_PARTITIONS "Enable network partition support" ON)
option(ENABLE_SOURCE_SPECIFIC_MULTICAST "Enable support for source-specific multicast" ON)
option(ENABLE_IPV6 "Enable ipv6 support" ON)
option(ENABLE_TYPE_DISCOVERY "Enable Type Discovery support" ON)
option(ENABLE_TOPIC_DISCOVERY "Enable Topic Discovery support" ON)
if(ENABLE_TOPIC_DISCOVERY)
  if(NOT ENABLE_TYPE_DISCOVERY)
    message(FATAL_ERROR "ENABLE_TOPIC_DISCOVERY requires ENABLE_TYPE_DISCOVERY to be enabled")
  endif()
endif()

if(ENABLE_SOURCE_SPECIFIC_MULTICAST)
  set(ENABLE_SSM ON)
endif()

# Prefer iceoryx integration but do not require it.
set(ENABLE_SHM "AUTO" CACHE STRING "Enable shared memory support")

set_property(CACHE ENABLE_SHM PROPERTY STRINGS ON OFF AUTO)
if(ENABLE_SHM)
  if(NOT ENABLE_SHM STREQUAL "AUTO")
    set(iceoryx_required REQUIRED)
  else()
    set(iceoryx_required QUIET)
  endif()
  find_package(iceoryx_binding_c ${iceoryx_required})
  set(ENABLE_SHM ${iceoryx_binding_c_FOUND} CACHE STRING "" FORCE)
endif()
# now ENABLE_SHM is no longer AUTO
if(ENABLE_SHM)
  if(APPLE)
    get_filename_component(iceoryx_libdir "${ICEORYX_LIB}" DIRECTORY)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH};${iceoryx_libdir}")
  elseif(WIN32)
    if (BUILD_SHARED_LIBS OR NOT DEFINED BUILD_SHARED_LIBS)
      message(FATAL_ERROR "CycloneDDS with enabled shared memory support can only be built as a static library on Windows")
    endif()
  endif()
endif()

# OpenSSL is huge, raising the RSS by 1MB or so, and moreover find_package(OpenSSL) causes
# trouble on some older CMake versions that otherwise work fine, so provide an option to avoid
# all OpenSSL related things.
#
# Historically the option was DDSC_ENABLE_OPENSSL so make some allowance for those who are
# currently relying on it.
set(ENABLE_SSL "AUTO" CACHE STRING "Enable OpenSSL support")
set_property(CACHE ENABLE_SSL PROPERTY STRINGS ON OFF AUTO)
if(ENABLE_SSL)
  if(NOT ENABLE_SSL STREQUAL "AUTO")
    find_package(OpenSSL REQUIRED)
  else()
    find_package(OpenSSL QUIET)
    if(OPENSSL_FOUND)
      message(STATUS "Building with OpenSSL support")
      set(ENABLE_SSL "ON")
    else()
      message(STATUS "Building without OpenSSL support")
      set(ENABLE_SSL "OFF")
    endif()
  endif()
endif()

if(NOT ENABLE_SECURITY)
  message(STATUS "Building without OMG DDS Security support")
endif()

add_subdirectory(tools)
add_subdirectory(ddsrt)
if(BUILD_IDLC)
  add_subdirectory(idl)
endif()
add_subdirectory(security)
add_subdirectory(core)
