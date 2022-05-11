#
# Copyright(c) 2006 to 2021 ZettaScale Technology and others
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

set(sources
  src/authentication.c
  src/auth_utils.c
  ../include/auth_tokens.h
  ../include/ac_tokens.h)

set(private_headers
  src/authentication.h
  src/auth_utils.h)

add_library(dds_security_auth SHARED ${sources} ${private_headers})

generate_export_header(
        dds_security_auth
        BASE_NAME SECURITY
        EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/security/export.h"
)

target_link_libraries(dds_security_auth PRIVATE security_openssl)
target_link_libraries(dds_security_auth PUBLIC ddsc)
target_link_libraries(dds_security_auth PUBLIC OpenSSL::SSL)
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set_target_properties(dds_security_auth PROPERTIES LINK_FLAGS "/ignore:4099")
endif()

add_coverage(dds_security_auth)
target_include_directories(dds_security_auth
    PUBLIC
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_api,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_core,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_openssl,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../core/ddsi/include>"
)

install(
  TARGETS dds_security_auth
  EXPORT "${PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib)
if (INSTALL_PDB)
  install(FILES $<TARGET_PDB_FILE:dds_security_auth>
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev
    OPTIONAL
  )
endif()
