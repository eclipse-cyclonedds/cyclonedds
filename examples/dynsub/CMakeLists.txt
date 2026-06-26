# Copyright(c) 2023 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(dynsub LANGUAGES C)

if(NOT TARGET CycloneDDS::ddsc)
  find_package(CycloneDDS REQUIRED)
endif()

set(dynsub_compat_target)
if(TARGET compat)
  set(dynsub_compat_target compat)
else()
  include(CheckIncludeFile)
  check_include_file(getopt.h HAVE_GETOPT_H)
  if(NOT HAVE_GETOPT_H)
    message(FATAL_ERROR "dynsub requires getopt.h or the CycloneDDS compat target")
  endif()
endif()

if(TARGET dyntypelib)
  set(dynsub_dyntypelib_target dyntypelib)
elseif(TARGET CycloneDDS::dyntypelib)
  set(dynsub_dyntypelib_target CycloneDDS::dyntypelib)
else()
  message(FATAL_ERROR "dynsub requires the dyntypelib target")
endif()

add_executable(dynsub dynsub.c)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsc/src")
  target_include_directories(dynsub
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/cdr/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsc/src"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/include")
endif()
target_link_libraries(dynsub PRIVATE ${dynsub_dyntypelib_target} CycloneDDS::ddsc ${dynsub_compat_target})

add_executable(variouspub variouspub.c)
idlc_generate(TARGET variouspub_lib FILES "variouspub_types.idl" WARNINGS no-implicit-extensibility)
target_link_libraries(variouspub variouspub_lib CycloneDDS::ddsc)
