#
# Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
PREPEND(srcs_security_core "${CMAKE_CURRENT_LIST_DIR}/src"
    dds_security_serialize.c
    dds_security_utils.c
    dds_security_plugins.c
    shared_secret.c
    dds_security_fsm.c
    dds_security_timed_cb.c
)

PREPEND(hdrs_public_security_core "${CMAKE_CURRENT_LIST_DIR}/include/security/core"
    dds_security_serialize.h
    dds_security_types.h
    dds_security_utils.h
    dds_security_plugins.h
    dds_security_fsm.h
    shared_secret.h
    dds_security_timed_cb.h
)

if(NOT WIN32)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC" )
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC" )
endif()

add_library(security_core INTERFACE IMPORTED GLOBAL)

target_sources(security_core INTERFACE ${srcs_security_core})

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
