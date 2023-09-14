#
# Copyright(c) 2023 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
cmake_minimum_required(VERSION 3.10)
project(CycloneDDS-PSMX-IOX VERSION 0.11.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

message(STATUS "Building Iceoryx PSMX plugin")

add_library(psmx_iox SHARED "src/psmx_iox_impl.cpp")

set_target_properties(psmx_iox PROPERTIES VERSION ${PROJECT_VERSION})

target_include_directories(psmx_iox PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(psmx_iox PRIVATE ddsc iceoryx_hoofs::iceoryx_hoofs iceoryx_posh::iceoryx_posh)

install(TARGETS psmx_iox
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
