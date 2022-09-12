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
set(sources
  dds_security_serialize.c
  dds_security_utils.c
  dds_security_plugins.c
  dds_security_shared_secret.c
  dds_security_fsm.c
  dds_security_timed_cb.c)
prepend(sources "${CMAKE_CURRENT_LIST_DIR}/src" ${sources})

set(headers
  dds_security_fsm.h
  dds_security_plugins.h
  dds_security_serialize.h
  dds_security_timed_cb.h
  dds_security_types.h
  dds_security_utils.h
  dds_security_shared_secret.h)
prepend(headers "${CMAKE_CURRENT_LIST_DIR}/include/dds/security/core" ${headers})

if(NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

add_library(security_core INTERFACE)

target_sources(security_core INTERFACE ${sources} ${headers})

target_include_directories(security_core
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../api/include>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsc,INTERFACE_INCLUDE_DIRECTORIES>>"
)

if(BUILD_TESTING AND BUILD_IDLC)
  add_subdirectory(tests)
endif()

install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/dds/security/core/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/dds/security/core/"
  COMPONENT dev)
