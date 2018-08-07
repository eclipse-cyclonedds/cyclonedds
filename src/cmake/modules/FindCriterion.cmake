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
find_path(CRITERION_INCLUDE_DIR criterion/criterion.h)
find_library(CRITERION_LIBRARY criterion)

mark_as_advanced(CRITERION_INCLUDE_DIR)

# Criterion does not define the version number anywhere.

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Criterion DEFAULT_MSG CRITERION_LIBRARY CRITERION_INCLUDE_DIR)

if(CRITERION_FOUND)
  set(CRITERION_INCLUDE_DIRS ${CRITERION_INCLUDE_DIR})
  set(CRITERION_LIBRARIES ${CRITERION_LIBRARY})

  if(WIN32)
    get_filename_component(CRITERION_LIBRARY_DIR "${CRITERION_LIBRARY}}" PATH)
    get_filename_component(CRITERION_BASENAME "${CRITERION_LIBRARY}}" NAME_WE)
    get_filename_component(CRITERION_PREFIX "${CRITERION_LIBRARY_DIR}" PATH)

    find_program(
      CRITERION_DLL
        "${CMAKE_SHARED_LIBRARY_PREFIX}${CRITERION_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
      HINTS
        ${CRITERION_PREFIX}
      PATH_SUFFIXES
        bin
      NO_DEFAULT_PATH)
    mark_as_advanced(CRITERION_DLL)

    if(CRITERION_DLL)
      add_library(Criterion SHARED IMPORTED)
      set_target_properties(
        Criterion PROPERTIES IMPORTED_IMPLIB "${CRITERION_LIBRARY}")
      set_target_properties(
        Criterion PROPERTIES IMPORTED_LOCATION "${CRITERION_DLL}")
    else()
      add_library(Criterion STATIC IMPORTED)
      set_target_properties(
        Criterion PROPERTIES IMPORTED_LOCATION "${CRITERION_LIBRARY}")
    endif()
  else()
    add_library(Criterion UNKNOWN IMPORTED)
    set_target_properties(
      Criterion PROPERTIES IMPORTED_LOCATION "${CRITERION_LIBRARY}")
  endif()

  set_target_properties(
    Criterion PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CRITERION_INCLUDE_DIR}")
endif()

