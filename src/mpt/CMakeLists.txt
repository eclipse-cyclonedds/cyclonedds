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
cmake_minimum_required(VERSION 3.7)

option(MPT_ENABLE_SELFTEST "Enable multi-process test-framework self test" OFF)

set(MPT_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}/mpt/cmake/MPT.cmake")
set(MPT_SOURCE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(MPT_BINARY_ROOT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(mpt)
add_subdirectory(tests)

