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

if(CUNIT_INC AND EXISTS "${CUNIT_INC}/CUnit/CUnit.h")
  set(PATTERN "^#define CU_VERSION \"([0-9]+)\.([0-9]+)\-([0-9]+)\"$")
  file(STRINGS "${CUNIT_INC}/CUnit/CUnit.h" CUNIT_H REGEX "${PATTERN}")

  string(REGEX REPLACE "${PATTERN}" "\\1" CUNIT_VERSION_MAJOR "${CUNIT_H}")
  string(REGEX REPLACE "${PATTERN}" "\\2" CUNIT_VERSION_MINOR "${CUNIT_H}")
  string(REGEX REPLACE "${PATTERN}" "\\3" CUNIT_VERSION_PATCH "${CUNIT_H}")

  set(CUNIT_VERSION "${CUNIT_VERSION_MAJOR}.${CUNIT_VERSION_MINOR}-${CUNIT_VERSION_PATCH}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  CUnit
  REQUIRED_VARS
    CUNIT_LIB CUNIT_INC
  VERSION_VAR CUNIT_VERSION)

if(CUNIT_FOUND AND NOT TARGET CUnit)
  add_library(CUnit INTERFACE IMPORTED)

  set_property(TARGET CUnit PROPERTY INTERFACE_LINK_LIBRARIES "${CUNIT_LIB}")
  set_property(TARGET CUnit PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CUNIT_INC}")
endif()
