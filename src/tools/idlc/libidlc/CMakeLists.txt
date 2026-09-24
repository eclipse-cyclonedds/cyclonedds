#
# Copyright(c) 2020 to 2026 ZettaScale Technology and others
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

set(
  libidlc_hdrs
  include/libidlc/libidlc_generator.h)

set(
  libidlc_srcs
  src/libidlc__types.h
  src/libidlc__descriptor.h
  src/libidlc__generator.h
  src/libidlc__descriptor.c
  src/libidlc__generator.c
  src/libidlc__types.c)

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
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../idlc/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/libidlc>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/libidlc>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../core/ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../core/cdr/include>")

target_link_libraries(libidlc PRIVATE idl ddsc)

add_library(${PROJECT_NAME}::libidlc ALIAS libidlc)

install(
  TARGETS libidlc
  EXPORT "${PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib)

install(
  DIRECTORY
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/libidlc>"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/libidlc"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev
  FILES_MATCHING PATTERN "*.h")

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
