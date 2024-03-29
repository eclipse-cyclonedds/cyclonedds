#
# Copyright(c) 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

project(fuzz_config_init LANGUAGES C)
cmake_minimum_required(VERSION 3.5)

if(NOT TARGET CycloneDDS::ddsc)
  # Find the CycloneDDS package.
  find_package(CycloneDDS REQUIRED)
endif()

add_executable(fuzz_config_init fuzz_config_init.c)
target_include_directories(
  fuzz_config_init PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/cdr/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/src>")

set_target_properties(fuzz_config_init PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(fuzz_config_init CycloneDDS::ddsc $ENV{LIB_FUZZING_ENGINE})
