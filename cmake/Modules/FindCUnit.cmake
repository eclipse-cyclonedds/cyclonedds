#
# Copyright(c) 2006 to 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
find_package(CUnit CONFIG QUIET)
if(CUnit_FOUND)
  message(STATUS "Found CUnit via Config file: ${CUnit_DIR}")
  set(CUNIT_FOUND ${CUnit_FOUND})
else()
  set(CUNIT_HEADER "CUnit/CUnit.h")

  if(CONAN_INCLUDE_DIRS)
    find_path(CUNIT_INCLUDE_DIR ${CUNIT_HEADER} HINTS ${CONAN_INCLUDE_DIRS})
  else()
    find_path(CUNIT_INCLUDE_DIR ${CUNIT_HEADER})
  endif()

  mark_as_advanced(CUNIT_INCLUDE_DIR)

  if(CUNIT_INCLUDE_DIR AND EXISTS "${CUNIT_INCLUDE_DIR}/${CUNIT_HEADER}")
    set(PATTERN "^#define CU_VERSION \"([0-9]+)\\.([0-9]+)\\-([0-9]+)\"$")
    file(STRINGS "${CUNIT_INCLUDE_DIR}/${CUNIT_HEADER}" CUNIT_H REGEX "${PATTERN}")

    string(REGEX REPLACE "${PATTERN}" "\\1" CUNIT_VERSION_MAJOR "${CUNIT_H}")
    string(REGEX REPLACE "${PATTERN}" "\\2" CUNIT_VERSION_MINOR "${CUNIT_H}")
    string(REGEX REPLACE "${PATTERN}" "\\3" CUNIT_VERSION_PATCH "${CUNIT_H}")

    set(CUNIT_VERSION "${CUNIT_VERSION_MAJOR}.${CUNIT_VERSION_MINOR}-${CUNIT_VERSION_PATCH}")
  endif()

  if(CONAN_LIB_DIRS_CUNIT)
    # CUnit package in ConanCenter contains a cunit.dll.lib on Windows if
    # shared=True. Probably a bug not to simply name it cunit.lib, but strip
    # the last extension from each library and use it as input.
    foreach(path ${CONAN_LIBS_CUNIT})
      get_filename_component(name "${path}" NAME)
      # NAME_WLE mode for get_filename_component available since CMake >= 3.14
      string(REGEX REPLACE "\.[^\.]+$" "" name_wle "${name}")
      list(APPEND names ${name_wle})
    endforeach()
    find_library(CUNIT_LIBRARY NAMES cunit ${names} HINTS ${CONAN_LIB_DIRS_CUNIT})
  else()
    find_library(CUNIT_LIBRARY NAMES cunit)
  endif()
endif()

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

    if(CONAN_LIB_DIRS_CUNIT)
      # CUnit package in ConanCenter shipped cunit-1.dll on Windows with
      # shared=True. Probably a bug. Especially since the other file was named
      # cunit.dll.lib.
      set(_expr "${CUNIT_PREFIX}/${CUNIT_BASENAME}*${CMAKE_SHARED_LIBRARY_SUFFIX}")
      file(GLOB_RECURSE _paths FALSE "${_expr}")
      foreach(_path "${CMAKE_SHARED_LIBRARY_PREFIX}${CUNIT_BASENAME}" ${_paths})
        get_filename_component(_name "${_path}" NAME)
        string(REGEX REPLACE "\.[^\.]+$" "" _name_wle "${_name}")
        find_program(
          CUNIT_DLL
            "${_name_wle}${CMAKE_SHARED_LIBRARY_SUFFIX}"
          HINTS
            "${CUNIT_PREFIX}"
          PATH_SUFFIXES
            bin
          NO_DEFAULT_PATH)
        if(CUNIT_DLL)
          break()
        endif()
      endforeach()
    endif()

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

