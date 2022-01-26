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

if (BUILD_SHARED_LIBS OR NOT DEFINED BUILD_SHARED_LIBS)
  add_library(ddsc SHARED "")
else()
  add_library(ddsc)
endif()

if("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
  target_link_libraries(ddsc PRIVATE dbghelp)
endif()

if(ENABLE_SSL AND OPENSSL_FOUND)
  target_link_libraries(ddsc PRIVATE OpenSSL::SSL)
  if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set_target_properties(ddsc PROPERTIES LINK_FLAGS "/ignore:4099")
  endif()
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
  set_property(TARGET ddsc PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  set_property(TARGET ddsc PROPERTY INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
  #-flto and debugging info do not seem to go well together
  #set_property(TARGET ddsc PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
endif()

# Support the OMG DDS Security within ddsc adds quite a bit of code.
if(ENABLE_SECURITY)
  target_link_libraries(ddsc PRIVATE security_core)
  target_include_directories(
    ddsc PUBLIC
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:security_api,INTERFACE_INCLUDE_DIRECTORIES>>)
endif()

if(ENABLE_SHM)
  target_link_libraries(ddsc PRIVATE iceoryx_binding_c::iceoryx_binding_c)
endif()

include(ddsi/CMakeLists.txt)
include(ddsc/CMakeLists.txt)

add_coverage(ddsc)
target_link_libraries(ddsc PRIVATE "$<BUILD_INTERFACE:ddsrt>")
target_compile_definitions(
  ddsc PUBLIC
  $<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_COMPILE_DEFINITIONS>>)
target_include_directories(
  ddsc PUBLIC
  $<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>)

# SOVERSION should increase on incompatible ABI change
set_target_properties(ddsc PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

# define target property to indicate if Cyclone DDS is compiled with SHM support
define_property(TARGET
    PROPERTY SHM_SUPPORT_IS_AVAILABLE
    BRIEF_DOCS "Indicator whether SHM support is available with current Cyclone DDS build configuration."
    FULL_DOCS "Indicator whether SHM support is available with current Cyclone DDS build configuration."
    )
set_target_properties(ddsc PROPERTIES
    SHM_SUPPORT_IS_AVAILABLE "${ENABLE_SHM}")
set_target_properties(ddsc PROPERTIES EXPORT_PROPERTIES
    "SHM_SUPPORT_IS_AVAILABLE")

# define target property to indicate if Cyclone DDS is compiled with type discovery
define_property(TARGET
    PROPERTY TYPE_DISCOVERY_IS_AVAILABLE
    BRIEF_DOCS "Indicator whether type discovery is available with current Cyclone DDS build configuration."
    FULL_DOCS "Indicator whether type discovery is available with current Cyclone DDS build configuration."
    )
set_property(
    TARGET ddsc
    APPEND PROPERTY TYPE_DISCOVERY_IS_AVAILABLE "${ENABLE_TYPE_DISCOVERY}")
set_property(
    TARGET ddsc
    APPEND PROPERTY EXPORT_PROPERTIES "TYPE_DISCOVERY_IS_AVAILABLE")

# define target property to indicate if Cyclone DDS is compiled with topic discovery
define_property(TARGET
    PROPERTY TOPIC_DISCOVERY_IS_AVAILABLE
    BRIEF_DOCS "Indicator whether topic discovery is available with current Cyclone DDS build configuration."
    FULL_DOCS "Indicator whether topic discovery is available with current Cyclone DDS build configuration."
    )
set_property(
    TARGET ddsc
    APPEND PROPERTY TOPIC_DISCOVERY_IS_AVAILABLE "${ENABLE_TOPIC_DISCOVERY}")
set_property(
    TARGET ddsc
    APPEND PROPERTY EXPORT_PROPERTIES "TOPIC_DISCOVERY_IS_AVAILABLE")

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
if (MSVC)
  install(FILES $<TARGET_PDB_FILE:ddsc>
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev
    OPTIONAL
  )
endif()

if(BUILD_TESTING AND BUILD_IDLC)
  add_subdirectory(xtests)
endif()

if(BUILD_DOCS)
  set(DOXYGEN_GENERATE_HTML NO)
  set(DOXYGEN_GENERATE_XML YES)
  set(DOXYGEN_STRIP_FROM_PATH "ddsc/include/dds/")
  set(DOXYGEN_EXCLUDE_PATTERNS "*/tests/*")
  set(DOXYGEN_MACRO_EXPANSION YES)
  set(DOXYGEN_PREDEFINED
    "__restrict="
    "__attribute__="
    "__declspec(x)="
    "DDS_EXPORT="
    "DDS_INLINE_EXPORT="
    "DDS_DEPRECATED_EXPORT="
    "DDSRT_STATIC_ASSERT(x)=")
  find_package(Doxygen REQUIRED)
  doxygen_add_docs(ddsc_api_docs "ddsc/include/dds/")
endif()
