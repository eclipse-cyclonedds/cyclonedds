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
project(listtopics LANGUAGES C)
cmake_minimum_required(VERSION 3.5)

if(NOT TARGET CycloneDDS::ddsc)
  if(YOCTO_CROSSCOMPILING)
    find_package(CycloneDDS REQUIRED PATHS ${STAGING_DIR_NATIVE} NO_CMAKE_FIND_ROOT_PATH)
  else()
    find_package(CycloneDDS REQUIRED)
  endif()
endif()

add_executable(listtopics listtopics.c)

if(YOCTO_CROSSCOMPILING)
  unset(DDSC_LIB CACHE)
  find_library(DDSC_LIB NAMES ddsc REQUIRED PATHS ${STAGING_DIR_HOST}/usr/lib NO_DEFAULT_PATH)
  target_link_libraries(listtopics ${DDSC_LIB})
else()
  target_link_libraries(listtopics CycloneDDS::ddsc)
endif()
