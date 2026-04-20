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

add_library(dyntypelib STATIC
  dyntypelib.c
  dyntypelib.h
  domtree.c
  domtree.h
  type_cache.c
  type_cache.h
  print_type.c
  print_sample.c
  compare_samples.c
  compare_samples.h
  size_and_align.c
  size_and_align.h
  scan_sample.c)
target_link_libraries(dyntypelib CycloneDDS::ddsc compat)

add_executable(dynsub dynsub.c)
add_executable(xmltype xmltype.c)

if(TARGET CycloneDDS::ddsc)
  target_include_directories(dyntypelib
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/cdr/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/include")
  target_include_directories(xmltype
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsc/src")
  target_compile_definitions(xmltype PUBLIC ENABLE_RAW_SAMPLE_BYPASS=1)
endif()
target_link_libraries(dynsub dyntypelib CycloneDDS::ddsc)
target_link_libraries(xmltype dyntypelib CycloneDDS::ddsc)

add_executable(variouspub variouspub.c)
idlc_generate(TARGET variouspub_lib FILES "variouspub_types.idl" WARNINGS no-implicit-extensibility)
target_link_libraries(variouspub variouspub_lib CycloneDDS::ddsc)
