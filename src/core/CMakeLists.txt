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

if (BUILD_SHARED_LIBS OR NOT DEFINED BUILD_SHARED_LIBS)
  add_library(ddsc SHARED)
else()
  add_library(ddsc)
endif()

add_definitions(-DDDSI_INCLUDE_NETWORK_PARTITIONS -DDDSI_INCLUDE_SSM)

option(ENABLE_LIFESPAN "Enable Lifespan QoS support" ON)
if(ENABLE_LIFESPAN)
  add_definitions(-DDDSI_INCLUDE_LIFESPAN)
endif()

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
    add_definitions(-DDDSI_INCLUDE_SSL)
    target_link_libraries(ddsc PRIVATE OpenSSL::SSL)
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
      set_target_properties(ddsc PROPERTIES LINK_FLAGS "/ignore:4099")
    endif()
    message(STATUS "Building with OpenSSL support")
  else()
    message(STATUS "Building without OpenSSL support")
  endif()
endif()

# Support the OMG DDS Security within ddsc adds quite a bit of code.
if(ENABLE_SECURITY)
  add_definitions(-DDDSI_INCLUDE_SECURITY)
  target_link_libraries(ddsc PRIVATE security_core)
  target_include_directories(
    ddsc PUBLIC
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:security_api,INTERFACE_INCLUDE_DIRECTORIES>>)
endif()

include(ddsi/CMakeLists.txt)
include(ddsc/CMakeLists.txt)

target_link_libraries(ddsc PRIVATE ddsrt)
target_compile_definitions(
  ddsc PUBLIC
  $<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_COMPILE_DEFINITIONS>>)
target_include_directories(
  ddsc PUBLIC
  $<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>)

# SOVERSION should increase on incompatible ABI change
set_target_properties(ddsc PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

# Create a pseudo-target that other targets (i.e. examples, tests) can depend
# on and can also be provided as import-target by a package-file when building
# those targets outside the regular Cyclone build-tree (i.e. the installed tree)
add_library(${PROJECT_NAME}::ddsc ALIAS ddsc)

install(
  TARGETS ddsc
  EXPORT "${PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
)

if(BUILD_IDLC)
  add_subdirectory(xtests)
endif()

if(BUILD_DOCS)
  set(DOXYGEN_GENERATE_HTML NO)
  set(DOXYGEN_GENERATE_XML YES)
  set(DOXYGEN_EXCLUDE_PATTERNS "*/tests/*")
  set(DOXYGEN_MACRO_EXPANSION YES)
  set(DOXYGEN_PREDEFINED
    "__restrict="
    "__attribute__="
    "__declspec(x)="
    "DDS_EXPORT="
    "DDS_DEPRECATED_EXPORT="
    "DDSRT_STATIC_ASSERT(x)=")
  find_package(Doxygen REQUIRED)
  doxygen_add_docs(ddsc_api_docs "ddsc/include")
endif()
