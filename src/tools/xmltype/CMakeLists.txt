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

add_executable(xmltype xmltype.c)
target_link_libraries(xmltype PRIVATE dyntypelib ddsc compat)

target_include_directories(xmltype
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../core/ddsc/src")
target_compile_definitions(xmltype PRIVATE ENABLE_RAW_SAMPLE_BYPASS=1)

if(INSTALL_XMLTYPE)
  install(
    TARGETS xmltype
    EXPORT "${PROJECT_NAME}"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev)

  if(INSTALL_PDB)
    install(FILES $<TARGET_PDB_FILE:xmltype>
      DESTINATION "${CMAKE_INSTALL_BINDIR}"
      COMPONENT dev
      OPTIONAL)
  endif()
endif()
