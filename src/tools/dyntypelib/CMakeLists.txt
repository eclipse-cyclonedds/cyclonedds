#
# Copyright(c) 2026 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

set(dyntypelib_headers
  domtree.h
  dyntypelib.h
  print_type.h
  size_and_align.h
  type_cache.h)

set(dyntypelib_sources
  compare_samples.c
  disassemble.c
  domtree.c
  dyntypelib.c
  float128_io.c
  float128_io.h
  print_sample.c
  print_type.c
  scan_sample.c
  size_and_align.c
  type_cache.c)

add_library(dyntypelib ${dyntypelib_sources} ${dyntypelib_headers})
add_library(${PROJECT_NAME}::dyntypelib ALIAS dyntypelib)

set_target_properties(dyntypelib PROPERTIES
  OUTPUT_NAME "cycloneddsdyntypelib"
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR})

if(WIN32 AND BUILD_SHARED_LIBS)
  set_property(TARGET dyntypelib PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()

target_include_directories(dyntypelib
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/cdr/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/ddsi/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/dds/dyntypelib>")

target_link_libraries(dyntypelib PUBLIC ddsc)

install(
  TARGETS dyntypelib
  EXPORT "${PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib)

install(
  FILES ${dyntypelib_headers}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/dds/dyntypelib"
  COMPONENT dev)

if(INSTALL_PDB)
  install(FILES $<TARGET_PDB_FILE:dyntypelib>
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev
    OPTIONAL)
endif()
