#
# Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
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

#PREPEND(srcs_platform ${platform} os_platform_errno.c os_platform_heap.c os_platform_init.c os_platform_process.c os_platform_socket.c os_platform_stdlib.c os_platform_sync.c os_platform_thread.c os_platform_time.c)
#add_library(util ut_avl.c ut_crc.c ut_expand_envvars.c ut_fibheap.c ut_handleserver.c ut_hopscotch.c ut_thread_pool.c ut_xmlparser.c)

PREPEND(srcs_util "${CMAKE_CURRENT_SOURCE_DIR}/src" ut_avl.c ut_crc.c ut_expand_envvars.c ut_fibheap.c ut_handleserver.c ut_hopscotch.c ut_thread_pool.c ut_xmlparser.c)
add_library(util  ${srcs_util})
generate_export_header(util EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/exports/util/ut_export.h")
target_link_libraries(util PUBLIC OSAPI)

set_property(TARGET util PROPERTY POSITION_INDEPENDENT_CODE TRUE)

target_include_directories(
  util PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/exports/>"
    "$<INSTALL_INTERFACE:${INSTALL_PREFIX}/include/>"
    "$<INSTALL_INTERFACE:${INSTALL_PREFIX}/exports/>")

# TODO: improve test inclusion.
if((BUILD_TESTING) AND ((NOT DEFINED MSVC_VERSION) OR (MSVC_VERSION GREATER "1800")))
  add_subdirectory(tests)
endif()
