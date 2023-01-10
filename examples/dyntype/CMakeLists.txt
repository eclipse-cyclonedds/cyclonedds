#
# Copyright(c) 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
project(dyntype LANGUAGES C)
cmake_minimum_required(VERSION 3.16)

if(NOT TARGET CycloneDDS::ddsc)
  find_package(CycloneDDS REQUIRED)
endif()

add_executable(dyntype dyntype.c)

target_link_libraries(dyntype CycloneDDS::ddsc)
