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

configure_file(src/config.h.in config.h)

set(headers
  include/idlc/generator.h
  include/idlc/options.h
  ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(sources
  src/idlc.c
  src/generator.c
  src/options.c)

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
    ${CMAKE_CURRENT_BINARY_DIR}
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsc,INTERFACE_INCLUDE_DIRECTORIES>>
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../core/ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../core/cdr/include>")

add_executable(${PROJECT_NAME}::idlc ALIAS idlc)

install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/idlc"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev
  FILES_MATCHING PATTERN "*.h")

install(
  TARGETS idlc
  EXPORT "${PROJECT_NAME}"
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT dev)

if (INSTALL_PDB)
  install(FILES $<TARGET_PDB_FILE:idlc>
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev
    OPTIONAL
  )
endif()
