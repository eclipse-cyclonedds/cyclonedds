#
# Copyright(c) 2019 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
cmake_minimum_required(VERSION 3.7)

if(NOT ${PROJECT_NAME} STREQUAL "CycloneDDS")
  get_filename_component(dir ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
  message(FATAL_ERROR "Top-level CMakeLists.txt was moved to the top-level directory.  Please run cmake on ${dir} instead of ${CMAKE_CURRENT_LIST_DIR}")
endif()

add_subdirectory(ddsrt)

# some of the tests in the core rely on preprocessing IDL, so idlc has to
# come first
if(BUILD_IDLC)
  add_subdirectory(idlc)
endif()
add_subdirectory(security)
add_subdirectory(core)
add_subdirectory(tools)
if(BUILD_TESTING AND HAVE_MULTI_PROCESS AND BUILD_IDLC)
  add_subdirectory(mpt)
endif()
