#
# Copyright(c) 2020 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
include(FindPackageHandleStandardArgs)

find_path(Lightfleet_INCLUDE_DIR lf_group_lib.h)

if(Lightfleet_INCLUDE_DIR AND EXISTS "${Lightfleet_INCLUDE_DIR}/lf_config.h")
  #define LFHBA_VERSION "1.4.0"
  set(expr "#define[ \t]+LFHBA_VERSION[ \t]+\"([0-9]+\\.[0-9]+\\.[0-9]+)\"")
  file(STRINGS "${Lightfleet_INCLUDE_DIR}/lf_config.h" ver REGEX "${expr}")
  string(REGEX REPLACE ".*${expr}.*" "\\1" Lightfleet_VERSION_STRING "${ver}")
  mark_as_advanced(expr ver)
endif()

if(NOT Lightfleet_LIBRARY)
  find_library(Lightfleet_LIBRARY lf_group PATH_SUFFIXES lightfleet)
endif()

find_package_handle_standard_args(
  Lightfleet
  REQUIRED_VARS
    Lightfleet_LIBRARY Lightfleet_INCLUDE_DIR
  VERSION_VAR
    Lightfleet_VERSION_STRING)

if(Lightfleet_FOUND)
  if(NOT TARGET Lightfleet::lf_group)
    add_library(Lightfleet::lf_group UNKNOWN IMPORTED)
    set_target_properties(Lightfleet::lf_group PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Lightfleet_INCLUDE_DIR}"
      IMPORTED_LOCATION "${Lightfleet_LIBRARY}")
  endif()
endif()

mark_as_advanced(Lightfleet_INCLUDE_DIR)
