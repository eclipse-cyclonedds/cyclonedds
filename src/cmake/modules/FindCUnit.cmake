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
set(CUNIT_HEADER "CUnit/CUnit.h")

find_path(CUNIT_INCLUDE_DIR ${CUNIT_HEADER})
mark_as_advanced(CUNIT_INCLUDE_DIR)

if(CUNIT_INCLUDE_DIR AND EXISTS "${CUNIT_INCLUDE_DIR}/${CUNIT_HEADER}")
  set(PATTERN "^#define CU_VERSION \"([0-9]+)\\.([0-9]+)\\-([0-9]+)\"$")
  file(STRINGS "${CUNIT_INCLUDE_DIR}/${CUNIT_HEADER}" CUNIT_H REGEX "${PATTERN}")

  string(REGEX REPLACE "${PATTERN}" "\\1" CUNIT_VERSION_MAJOR "${CUNIT_H}")
  string(REGEX REPLACE "${PATTERN}" "\\2" CUNIT_VERSION_MINOR "${CUNIT_H}")
  string(REGEX REPLACE "${PATTERN}" "\\3" CUNIT_VERSION_PATCH "${CUNIT_H}")

  set(CUNIT_VERSION "${CUNIT_VERSION_MAJOR}.${CUNIT_VERSION_MINOR}-${CUNIT_VERSION_PATCH}")
endif()

find_library(CUNIT_LIBRARY cunit)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  CUnit
  REQUIRED_VARS
    CUNIT_LIBRARY CUNIT_INCLUDE_DIR
  VERSION_VAR
    CUNIT_VERSION)

if(CUNIT_FOUND)
  set(CUNIT_INCLUDE_DIRS ${CUNIT_INCLUDE_DIR})
  set(CUNIT_LIBRARIES ${CUNIT_LIBRARY})

  if(WIN32)
    get_filename_component(CUNIT_LIBRARY_DIR "${CUNIT_LIBRARY}}" PATH)
    get_filename_component(CUNIT_BASENAME "${CUNIT_LIBRARY}}" NAME_WE)
    get_filename_component(CUNIT_PREFIX "${CUNIT_LIBRARY_DIR}" PATH)

    find_program(
      CUNIT_DLL
        "${CMAKE_SHARED_LIBRARY_PREFIX}${CUNIT_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
      HINTS
        ${CUNIT_PREFIX}
      PATH_SUFFIXES
        bin
      NO_DEFAULT_PATH)
    mark_as_advanced(CUNIT_DLL)

    # IMPORTANT:
    # Providing a .dll file as the value for IMPORTED_LOCATION can only be
    # done for "SHARED" libraries, otherwise the location of the .dll will be
    # passed to linker, causing it to fail.
    if(CUNIT_DLL)
      add_library(CUnit SHARED IMPORTED)
      set_target_properties(
        CUnit PROPERTIES IMPORTED_IMPLIB "${CUNIT_LIBRARY}")
      set_target_properties(
        CUnit PROPERTIES IMPORTED_LOCATION "${CUNIT_DLL}")
    else()
      add_library(CUnit STATIC IMPORTED)
      set_target_properties(
        CUnit PROPERTIES IMPORTED_LOCATION "${CUNIT_LIBRARY}")
    endif()
  else()
    add_library(CUnit UNKNOWN IMPORTED)
    set_target_properties(
      CUnit PROPERTIES IMPORTED_LOCATION "${CUNIT_LIBRARY}")
  endif()

  set_target_properties(
    CUnit PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CUNIT_INCLUDE_DIR}")
endif()

