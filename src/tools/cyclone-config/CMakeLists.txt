#
# Copyright(c) 2021 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

IF (WIN32)
  string(REPLACE "/" "\\\\" CYCLONE_CONFIG_PREFIX ${CMAKE_INSTALL_PREFIX})
ELSE()
  set(CYCLONE_CONFIG_PREFIX ${CMAKE_INSTALL_PREFIX})
ENDIF()

configure_file(src/cyclone-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/cyclone-config.h)
add_executable(cyclone-config src/cyclone-config.c)
set_target_properties(cyclone-config PROPERTIES LINKER_LANGUAGE C)
target_include_directories(cyclone-config PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>)

install(
  TARGETS cyclone-config
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT dev
)
