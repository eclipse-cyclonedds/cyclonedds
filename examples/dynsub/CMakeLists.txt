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

set(headers
  dynsub.h)
set(sources
  dynsub.c
  type_cache.c
  print_type.c
  print_sample.c)

add_executable(dynsub ${sources} ${headers})

if(TARGET CycloneDDS::ddsc)
  # shouldn't need this ...
  target_include_directories(dynsub
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/cdr/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/include")
endif()
target_link_libraries(dynsub CycloneDDS::ddsc)

add_executable(variouspub variouspub.c)
idlc_generate(TARGET variouspub_lib FILES "variouspub_types.idl" WARNINGS no-implicit-extensibility)
target_link_libraries(variouspub variouspub_lib CycloneDDS::ddsc)
