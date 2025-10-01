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

# default to unknown
set(CYCLONEDDS_GIT_HASH "unknown")

# find latest hash and set it
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND git log -1 --pretty=format:%h
        OUTPUT_VARIABLE CYCLONEDDS_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

option(ENABLE_SECURITY "Enable OMG DDS Security support" ON)
option(ENABLE_LIFESPAN "Enable Lifespan QoS support" ON)
option(ENABLE_DEADLINE_MISSED "Enable Deadline Missed QoS support" ON)
option(SKIP_DEADLINE_UPDATE "Skip deadline update test" OFF)
option(ENABLE_NETWORK_PARTITIONS "Enable network partition support" ON)
set(ENABLE_SOURCE_SPECIFIC_MULTICAST "AUTO" CACHE STRING "Enable support for source-specific multicast")
set_property(CACHE ENABLE_SOURCE_SPECIFIC_MULTICAST PROPERTY STRINGS ON OFF AUTO)
set(ENABLE_IPV6 "AUTO" CACHE STRING "Enable ipv6 support")
set_property(CACHE ENABLE_IPV6 PROPERTY STRINGS ON OFF AUTO)
option(ENABLE_TYPELIB "Enable Type Library support" ON)
option(ENABLE_TYPE_DISCOVERY "Enable Type Discovery support" ON)
option(ENABLE_TOPIC_DISCOVERY "Enable Topic Discovery support" ON)
option(ENABLE_QOS_PROVIDER "Enable Qos Provider support" ON)
if(ENABLE_TYPE_DISCOVERY)
  if(NOT ENABLE_TYPELIB)
    message(FATAL_ERROR "ENABLE_TYPE_DISCOVERY requires ENABLE_TYPELIB to be enabled")
  endif()
endif()
if(ENABLE_TOPIC_DISCOVERY)
  if(NOT ENABLE_TYPE_DISCOVERY)
    message(FATAL_ERROR "ENABLE_TOPIC_DISCOVERY requires ENABLE_TYPE_DISCOVERY to be enabled")
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

set(ENABLE_TCP_TLS "AUTO" CACHE STRING "Enable TCP+TLS support (depends on ENABLE_SSL)")
set_property(CACHE ENABLE_TCP_TLS PROPERTY STRINGS ON OFF AUTO)
if(ENABLE_TCP_TLS)
  if(ENABLE_TCP_TLS STREQUAL "AUTO")
    set(ENABLE_TCP_TLS "${ENABLE_SSL}")
  elseif(ENABLE_TCP_TLS AND NOT ENABLE_SSL)
    message(FATAL "ENABLE_TCP_TLS requires ENABLE_SSL")
  endif()
endif()

if(NOT ENABLE_SECURITY)
  message(STATUS "Building without OMG DDS Security support")
endif()

# Prefer Iceoryx2 integration but do not require it
set(ENABLE_ICEORYX2 "AUTO" CACHE STRING "Enable Iceoryx2 integration")
set_property(CACHE ENABLE_ICEORYX2 PROPERTY STRINGS ON OFF AUTO)
if(ENABLE_ICEORYX2)
  if(NOT ENABLE_ICEORYX2 STREQUAL "AUTO")
    set(iceoryx2_required REQUIRED)
  else()
    set(iceoryx2_required QUIET)
  endif()
  find_package(iceoryx2-c ${iceoryx2_required})
  if(${iceoryx2-c_FOUND})
    set(ENABLE_ICEORYX2 "ON")
  else()
    set(ENABLE_ICEORYX2 "OFF")
  endif()
endif()
# Prefer Iceoryx integration but do not require it
set(ENABLE_ICEORYX "AUTO" CACHE STRING "Enable Iceoryx integration")
set_property(CACHE ENABLE_ICEORYX PROPERTY STRINGS ON OFF AUTO)
# backwards compatibility: ENABLE_SHM
set(ENABLE_SHM "AUTO" CACHE STRING "Deprecated, use ENABLE_ICEORYX instead")
set_property(CACHE ENABLE_SHM PROPERTY STRINGS ON OFF AUTO)
if(ENABLE_ICEORYX STREQUAL "AUTO")
  # ENABLE_ICEORYX not set explicitly, check ENABLE_SHM
  if(NOT ENABLE_SHM STREQUAL "AUTO")
    # ENABLE_SHM set explicitly, copy into ENABLE_ICEORYX
    message(WARNING "Copying deprecated ENABLE_SHM setting into ENABLE_ICEORYX")
    set(ENABLE_ICEORYX ${ENABLE_SHM})
  endif()
endif()
if(ENABLE_ICEORYX)
  if(NOT ENABLE_ICEORYX STREQUAL "AUTO")
    set(iceoryx_required REQUIRED)
  else()
    set(iceoryx_required QUIET)
  endif()
  find_package(iceoryx_hoofs ${iceoryx_required})
  find_package(iceoryx_posh ${iceoryx_required})
  if(${iceoryx_hoofs_FOUND} AND ${iceoryx_posh_FOUND})
    set(ENABLE_ICEORYX "ON")
  else()
    set(ENABLE_ICEORYX "OFF")
  endif()
endif()

if(BUILD_TESTING)
  add_subdirectory(ucunit)
endif()
add_subdirectory(tools)
add_subdirectory(ddsrt)
if(BUILD_IDLC)
  add_subdirectory(idl)
endif()
add_subdirectory(security)
add_subdirectory(psmx_iox)
add_subdirectory(core)
