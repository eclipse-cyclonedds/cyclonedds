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

set(headers
  dds_security_api.h
  dds_security_api_access_control.h
  dds_security_api_authentication.h
  dds_security_api_cryptography.h
  dds_security_api_defs.h
  dds_security_api_err.h
  dds_security_api_types.h)
prepend(headers "${CMAKE_CURRENT_LIST_DIR}/include/dds/security" ${headers})

add_library(security_api INTERFACE)
target_sources(security_api INTERFACE ${headers})

target_include_directories(
  security_api INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../core/cdr/include>"
)

install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev)
