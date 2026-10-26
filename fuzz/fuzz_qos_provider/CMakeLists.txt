#
# Copyright(c) 2026 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

project(fuzz_qos_provider LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.16)

if(NOT TARGET CycloneDDS::ddsc)
  # Find the CycloneDDS package.
  find_package(CycloneDDS REQUIRED)
endif()

add_executable(fuzz_qos_provider fuzz_qos_provider.c)

set_target_properties(fuzz_qos_provider PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(fuzz_qos_provider CycloneDDS::ddsc $ENV{LIB_FUZZING_ENGINE})
