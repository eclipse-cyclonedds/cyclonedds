# Copyright(c) 2006 to 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

idlc_generate(TARGET dcidl FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/durablesupport.idl")
add_library(durability SHARED "${CMAKE_CURRENT_SOURCE_DIR}/src/dds_durability.c")
set_property(TARGET durability PROPERTY C_VISIBILITY_PRESET hidden)
target_include_directories(durability PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/ddsrt/include>"
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/core/ddsc/include>"
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/core/ddsi/include>"
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/core/cdr/include>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/ddsrt/include>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/core/include>"
)
target_link_libraries(durability PRIVATE dcidl)

install(
  TARGETS durability
  EXPORT "${PROJECT_NAME}"
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
