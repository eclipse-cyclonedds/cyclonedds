#
# Copyright(c) 2018 to 2026 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

add_subdirectory(libidl)
add_subdirectory(idlpp)
add_subdirectory(libidlc)
add_subdirectory(idlc)

if(NOT DEFINED CycloneDDS_IDLC_GENERATE_CMAKE)
  set(CycloneDDS_IDLC_GENERATE_CMAKE
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Generate.cmake")
endif()
include("${CycloneDDS_IDLC_GENERATE_CMAKE}")

if(BUILD_IDLC_XTESTS)
  add_subdirectory(xtests)
endif()
