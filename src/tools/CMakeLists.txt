#
# Copyright(c) 2006 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

set(CMAKE_INSTALL_TOOLSDIR "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/tools")

add_subdirectory(_confgen)

if(ENABLE_TYPELIB)
  if(BUILD_DYNTYPELIB)
    add_subdirectory(dyntypelib)
  endif()
  if(BUILD_XMLTYPE)
    if(NOT BUILD_DYNTYPELIB)
      message(FATAL_ERROR "BUILD_XMLTYPE requires BUILD_DYNTYPELIB")
    endif()
    if(INSTALL_XMLTYPE AND BUILD_SHARED_LIBS AND NOT INSTALL_DYNTYPELIB)
      message(FATAL_ERROR "INSTALL_XMLTYPE requires INSTALL_DYNTYPELIB when BUILD_SHARED_LIBS is enabled")
    endif()
    add_subdirectory(xmltype)
  elseif(INSTALL_XMLTYPE)
    message(FATAL_ERROR "INSTALL_XMLTYPE requires BUILD_XMLTYPE")
  endif()
elseif(INSTALL_XMLTYPE)
  message(FATAL_ERROR "INSTALL_XMLTYPE requires ENABLE_TYPELIB")
endif()

if(BUILD_IDLC)
  add_subdirectory(idlc)
endif()

if(HAVE_IDLC)
  add_subdirectory(ddsperf)
endif()
