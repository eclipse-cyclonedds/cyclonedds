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
    shared_secret.c
    dds_security_timed_cb.c
)
 
PREPEND(hdrs_public_security_core "${CMAKE_CURRENT_LIST_DIR}/include/security/core"
    dds_security_core_if.h
    dds_security_serialize.h
    dds_security_types.h
    dds_security_utils.h
    shared_secret.h
    dds_security_timed_cb.h
)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC" )
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC" )
add_library(security_core INTERFACE)


target_sources(security_core
    INTERFACE
        ${srcs_security_core}

)

target_include_directories(security_core
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../api/include>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsc,INTERFACE_INCLUDE_DIRECTORIES>>"
)
#target_link_libraries(security_core PRIVATE ddsrt security_api)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/dds/security/core/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/dds/security/core/"
  COMPONENT dev)

#install(
# TARGETS security_core
# EXPORT "${PROJECT_NAME}"
#  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
#  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
#  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
#)
