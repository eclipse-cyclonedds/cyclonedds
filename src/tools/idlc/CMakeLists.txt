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
  include/idlc/generator.h
  ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set(sources
  src/idlc/idlc.c
  src/idlc/generator.c
  src/idlc/options.c)

add_executable(idlc ${sources} ${headers})

if(MSVC)
  # ignore warnings C6255 and 6263 about _alloca
  # and 5286 and 5287 about enum type conversion
  target_compile_options(idlc PRIVATE /wd6255 /wd6263 /wd5286 /wd5287)
endif()
target_link_libraries(idlc PRIVATE compat idl idlpp ddsc ${CMAKE_DL_LIBS})

target_include_directories(
  idlc PRIVATE
    include/idlc
    include/generator_common
    ${CMAKE_CURRENT_BINARY_DIR}
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsc,INTERFACE_INCLUDE_DIRECTORIES>>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/ddsi/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/cdr/include>)

add_executable(${PROJECT_NAME}::idlc ALIAS idlc)

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

if(MSVC)
  # ignore warning 5286 and 5287 about enum type conversion
  target_compile_options(libidlc PRIVATE /wd5286 /wd5287)
endif()

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
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/libidlc>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/cdr/include>")

target_link_libraries(libidlc PRIVATE idl ddsc)

add_library(${PROJECT_NAME}::libidlc ALIAS libidlc)

install(
  TARGETS libidlc
  EXPORT "${CMAKE_PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib)

install(
  DIRECTORY
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/libidlc>"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/libidlc"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/idlc"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
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

include("${CycloneDDS_SOURCE_DIR}/cmake/Modules/Generate.cmake")

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(BUILD_IDLC_XTESTS)
  add_subdirectory(xtests)
endif()
