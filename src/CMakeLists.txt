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
cmake_minimum_required(VERSION 3.7)

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

# Generate a header file listing compile-time options relevant to the API.  Define to
# "1" if enabled so that the generated features.h ends up having either
#
# - #define DDS_HAS_SECURITY 1
# or
# - /* #undef DDS_HAS_SECURITY */
#
# which caters both for those who prefer #ifdef DDS_HAS_SECURITY and for those who prefer
# #if DDS_HAS_SECURITY.
option(ENABLE_SECURITY "Enable OMG DDS Security support" ON)
option(ENABLE_LIFESPAN "Enable Lifespan QoS support" ON)
option(ENABLE_DEADLINE_MISSED "Enable Deadline Missed QoS support" ON)
option(ENABLE_NETWORK_PARTITIONS "Enable network partition support" ON)
option(ENABLE_SOURCE_SPECIFIC_MULTICAST "Enable support for source-specific multicast" ON)
option(ENABLE_TYPE_DISCOVERY "Enable Type Discovery support" OFF)
if(ENABLE_SECURITY)
  set(DDS_HAS_SECURITY "1")
endif()
if(ENABLE_LIFESPAN)
  set(DDS_HAS_LIFESPAN "1")
endif()
if(ENABLE_DEADLINE_MISSED)
  set(DDS_HAS_DEADLINE_MISSED "1")
endif()
if(ENABLE_NETWORK_PARTITIONS)
  set(DDS_HAS_NETWORK_PARTITIONS "1")
endif()
if(ENABLE_SOURCE_SPECIFIC_MULTICAST)
  set(DDS_HAS_SSM "1")
endif()
# ones that linger in the sources
# - DDS_HAS_BANDWIDTH_LIMITING
# - DDS_HAS_NETWORK_CHANNELS
if(ENABLE_TYPE_DISCOVERY)
  set(DDS_HAS_TYPE_DISCOVERY "1")
endif()
configure_file(features.h.in "${CMAKE_CURRENT_BINARY_DIR}/core/include/dds/features.h")

# OpenSSL is huge, raising the RSS by 1MB or so, and moreover find_package(OpenSSL) causes
# trouble on some older CMake versions that otherwise work fine, so provide an option to avoid
# all OpenSSL related things.
#
# Historically the option was DDSC_ENABLE_OPENSSL so make some allowance for those who are
# currently relying on it.
option(ENABLE_SSL "Enable openssl support" ON)
option(DDSC_ENABLE_OPENSSL "Deprecated: please use ENABLE_SSL instead" ON)
if(NOT DDSC_ENABLE_OPENSSL)
  message(ERROR "DDSC_ENABLE_OPENSSL is deprecated, please use ENABLE_SSL instead")
  set(ENABLE_SSL OFF)
endif()
if(ENABLE_SSL)
  find_package(OpenSSL)
  if(OPENSSL_FOUND)
    set(DDS_HAS_SSL "1")
    message(STATUS "Building with OpenSSL support")
  else()
    message(STATUS "Building without OpenSSL support")
  endif()
endif()

if(NOT ENABLE_SECURITY)
  message(STATUS "Building without OMG DDS Security support")
endif()

configure_file(features.h.in "${CMAKE_CURRENT_BINARY_DIR}/core/include/dds/features.h")

add_subdirectory(tools)
add_subdirectory(ddsrt)
if(BUILD_IDLC)
  add_subdirectory(idl)
endif()
add_subdirectory(security)
add_subdirectory(core)
