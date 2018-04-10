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
find_path(CUNIT_INC CUnit/CUnit.h)
find_library(CUNIT_LIB cunit)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CUnit DEFAULT_MSG CUNIT_LIB CUNIT_INC)

if(CUNIT_FOUND AND NOT TARGET CUnit)
  add_library(CUnit INTERFACE IMPORTED)

  set_property(TARGET CUnit PROPERTY INTERFACE_LINK_LIBRARIES "${CUNIT_LIB}")
  set_property(TARGET CUnit PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CUNIT_INC}")
endif()
