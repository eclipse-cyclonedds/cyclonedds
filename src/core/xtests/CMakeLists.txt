#
# Copyright(c) 2019 to 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
if(BUILD_TESTING AND BUILD_IDLC)
    include(CUnit)
    add_subdirectory(rhc_torture)
    add_subdirectory(initsampledeliv)
endif()

if(NOT CMAKE_CROSSCOMPILING AND NOT CMAKE_SYSTEM_NAME MATCHES "iOS")
    add_subdirectory(symbol_export)
endif()
