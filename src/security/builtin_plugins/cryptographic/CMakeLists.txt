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
include (GenerateExportHeader)

set(sources
  src/crypto_cipher.c
  src/crypto_key_exchange.c
  src/crypto_key_factory.c
  src/crypto_objects.c
  src/crypto_transform.c
  src/crypto_utils.c
  src/cryptography.c)
set(private_headers
  src/crypto_cipher.h
  src/crypto_key_exchange.h
  src/crypto_key_factory.h
  src/crypto_objects.h
  src/crypto_transform.h
  src/crypto_utils.h
  src/cryptography.h
  ../include/crypto_tokens.h)

add_library(dds_security_crypto SHARED ${sources} ${private_headers})

generate_export_header(
    dds_security_crypto
        BASE_NAME SECURITY
        EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/security/export.h"
)

target_link_libraries(dds_security_crypto PRIVATE security_openssl)
target_link_libraries(dds_security_crypto PUBLIC ddsc)
target_link_libraries(dds_security_crypto PUBLIC OpenSSL::SSL)
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set_target_properties(dds_security_crypto PROPERTIES LINK_FLAGS "/ignore:4099")
endif()

target_include_directories(dds_security_crypto
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
  TARGETS dds_security_crypto
  EXPORT "${PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
)
if (INSTALL_PDB)
  install(FILES $<TARGET_PDB_FILE:dds_security_crypto>
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev
    OPTIONAL
  )
endif()

add_coverage(dds_security_crypto)
