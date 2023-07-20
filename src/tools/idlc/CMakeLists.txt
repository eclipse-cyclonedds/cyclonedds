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
include(GenerateExportHeader)
configure_file(src/idlc/config.h.in config.h)

set(headers
  include/idlc/plugin.h
  ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set(sources
  src/idlc/idlc.c
  src/idlc/plugin.c
  src/idlc/options.c)

add_executable(idlc ${sources} ${headers})

if(MSVC)
  # ignore warnings C6255 and 6263 about _alloca
  target_compile_options(idlc PRIVATE /wd6255 /wd6263)
endif()
target_link_libraries(idlc PRIVATE compat idl idlpp ddsc idl_common ${CMAKE_DL_LIBS})

target_include_directories(
  idlc PRIVATE
    include/idlc
    include/generator_common
    ${CMAKE_CURRENT_BINARY_DIR}
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsc,INTERFACE_INCLUDE_DIRECTORIES>>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/ddsi/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/cdr/include>)

add_executable(${PROJECT_NAME}::idlc ALIAS idlc)

set(common_hdrs
  include/generator_common/common.h
  )

set(common_srcs
  src/generator_common/common.c
  )

if(ENABLE_TYPELIB)
list(APPEND common_hdrs include/generator_common/descriptor_type_meta.h)
list(APPEND common_hdrs src/generator_common/descriptor_type_meta.c)
endif()

add_library(
  idl_common SHARED ${common_hdrs} ${common_srcs})

set_target_properties(idl_common PROPERTIES
   OUTPUT_NAME "idl_common"
   VERSION ${PROJECT_VERSION}
   SOVERSION ${PROJECT_VERSION_MAJOR}
   C_STANDARD 99)

if(BUILD_TESTING)
  set_property(TARGET idl_common PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
else()
  set_property(TARGET idl_common PROPERTY C_VISIBILITY_PRESET hidden)
endif()

target_include_directories(
  idl_common
  PRIVATE
    include
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/generator_common>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/generator_common>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/cdr/include>")

add_library(${PROJECT_NAME}::idl_common ALIAS idl_common)

generate_export_header(idl_common BASE_NAME IDL_COMMON EXPORT_FILE_NAME include/generator_common/common_export.h)

target_link_libraries(idl_common PRIVATE idl ddsc)

set(
  libidlc_hdrs
  include/libidlc/libidlc_generator.h
  ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(
  libidlc_srcs
  src/libidlc/libidlc__types.h
  src/libidlc/libidlc__descriptor.h
  src/libidlc/libidlc__generator.h
  src/libidlc/libidlc__descriptor.c
  src/libidlc/libidlc__generator.c
  src/libidlc/libidlc__types.c)

add_library(
  libidlc SHARED ${libidlc_srcs} ${libidlc_hdrs})

generate_export_header(libidlc BASE_NAME LIBIDLC EXPORT_FILE_NAME include/libidlc/libidlc_export.h)

set_target_properties(libidlc PROPERTIES
   OUTPUT_NAME "cycloneddsidlc"
   VERSION ${PROJECT_VERSION}
   SOVERSION ${PROJECT_VERSION_MAJOR}
   C_STANDARD 99)

if(BUILD_TESTING)
  set_property(TARGET libidlc PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
else()
  set_property(TARGET libidlc PROPERTY C_VISIBILITY_PRESET hidden)
endif()

target_include_directories(
  libidlc
  PRIVATE
    include
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/libidlc>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/generator_common>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/libidlc>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/cdr/include>")

target_link_libraries(libidlc PRIVATE idl_common idl ddsc)

add_library(${PROJECT_NAME}::libidlc ALIAS libidlc)

install(
  TARGETS libidlc idl_common
  EXPORT "${CMAKE_PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib)

install(
  DIRECTORY
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/libidlc>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/generator_common>"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/generator_common"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/libidlc"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/idlc"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/idlc"
  COMPONENT dev
  FILES_MATCHING PATTERN "*.h")

install(
  TARGETS idlc
  EXPORT "${CMAKE_PROJECT_NAME}"
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT dev)

if (INSTALL_PDB)
  install(FILES $<TARGET_PDB_FILE:idlc>
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev
    OPTIONAL
  )
endif()

install(
  FILES "${CycloneDDS_SOURCE_DIR}/cmake/Modules/Generate.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}/idlc"
  COMPONENT dev)

include("${CycloneDDS_SOURCE_DIR}/cmake/Modules/Generate.cmake")

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(BUILD_IDLC_XTESTS)
  add_subdirectory(xtests)
endif()
