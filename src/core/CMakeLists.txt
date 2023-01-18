#
# Copyright(c) 2006 to 2022 ZettaScale Technology and others
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

if(BUILD_TESTING)
  set_property(TARGET ddsc PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
else()
  set_property(TARGET ddsc PROPERTY C_VISIBILITY_PRESET hidden)
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

if(ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT error)
  if(ipo_supported)
    if(ANDROID AND "${CMAKE_ANDROID_ARCH_ABI}" STREQUAL "armeabi-v7a")
      # clang android LTO bug (only armv7 affected)
      # https://stackoverflow.com/questions/56238019/linker-error-when-enabling-link-time-optimization-in-ndk
      # Fixed in newer clang versions: https://reviews.llvm.org/D79919
      # But fixed here to keep supported android api level as low as possible
      # Tested with android ndk v20.0.5594570 (android api 29)
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto=full")
    else()
      set_property(TARGET ddsc PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
      set_property(TARGET ddsc PROPERTY INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
      #-flto and debugging info do not seem to go well together
      #set_property(TARGET ddsc PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    endif()
  endif()
endif()

# Support the OMG DDS Security within ddsc adds quite a bit of code.
if(ENABLE_SECURITY)
  target_link_libraries(ddsc PRIVATE "$<BUILD_INTERFACE:security_core>")
  target_include_directories(
    ddsc PUBLIC
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:security_api,INTERFACE_INCLUDE_DIRECTORIES>>)
endif()

if(ENABLE_SHM)
  target_link_libraries(ddsc PRIVATE iceoryx_binding_c::iceoryx_binding_c)
endif()

include(cdr/CMakeLists.txt)
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
if (INSTALL_PDB)
  install(FILES $<TARGET_PDB_FILE:ddsc>
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev
    OPTIONAL
  )
endif()

add_subdirectory(xtests)
