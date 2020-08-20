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

# CMake script to produce a codecov.io submission file.
# Should be compatible with https://codecov.io/bash.

if(NOT PROJECT_ROOT)
  message(FATAL_ERROR "PROJECT_ROOT is not set")
endif()

if(NOT CODECOV_FILE)
  message(FATAL_ERROR "CODECOV_FILE is not set")
endif()

function(read_adjustments ROOT SOURCEFILE ADJUSTMENTS)
  file(READ "${SOURCEFILE}" _source)
  # replace semicolons by colons
  string(REPLACE ";"  ":" _source "${_source}")
  # replace newlines by semicolons
  # space is inserted to ensure blank lines are picked up
  string(REPLACE "\n" " ;" _source "${_source}")

  # include matching lines in adjustments
  set(_line_number 0)
  foreach(_line ${_source})
    math(EXPR _line_number "${_line_number} + 1")
    # empty_line='^[[:space:]]*$'
    if(_line MATCHES "^[ \t]*$")
      list(APPEND _adjustments ${_line_number})
    # syntax_bracket='^[[:space:]]*[\{\}][[:space:]]*(//.*)?$'
    elseif(_line MATCHES "^[ \t]*[{}][ \t]*(//.*)?")
      list(APPEND _adjustments ${_line_number})
    # //LCOV_EXCL
    elseif(_line MATCHES "// LCOV_EXCL")
      list(APPEND _adjustments ${_line_number})
    endif()
  endforeach()

  if(_adjustments)
    string(REPLACE ";" "," _adjustments "${_adjustments}")
    set(${ADJUSTMENTS} "${SOURCEFILE}:${_adjustments}\n" PARENT_SCOPE)
  endif()
endfunction()

function(read_and_reduce_gcov ROOT GCOVFILE SOURCEFILE GCOV)
  file(READ "${GCOVFILE}" _gcov)
  # grab first line
  string(REGEX MATCH "^[^\n]*" _source "${_gcov}")

  # reduce gcov
  # 1. remove source code
  # 2. remove ending bracket lines
  # 3. remove whitespace
  string(REGEX REPLACE " *([^ \n:]*): *([^ \n:]*):?[^\n]*" "\\1:\\2:" _gcov "${_gcov}")
  # 4. remove contextual lines
  string(REGEX REPLACE "-+[^\n]*\n" "" _gcov "${_gcov}")
  # 5. remove function names

  string(REPLACE "${ROOT}" "" _path "${GCOVFILE}")
  string(REGEX REPLACE "^/+" "" _path "${_path}")
  string(PREPEND _gcov "# path=${_path}.reduced\n${_source}\n")
  string(APPEND _gcov "<<<<<< EOF\n")

  # capture source file
  string(REGEX REPLACE "^.*:Source:(.*)$" "\\1" _source "${_source}")

  set(${SOURCEFILE} "${_source}" PARENT_SCOPE)
  set(${GCOV} "${_gcov}" PARENT_SCOPE)
endfunction()

set(project_dir "${PROJECT_ROOT}")
set(build_dir "${CMAKE_CURRENT_BINARY_DIR}")

set(network_block "\n")
set(gcov_block "")
set(adjustments_block "")

# codecov.io uses "git ls-files" and falls back to find, but having no
# dependencies is preferred (for now). the globbing rules are very likely to
# produce the same result
file(GLOB_RECURSE source_files "${PROJECT_ROOT}/*.[hH]"
                               "${PROJECT_ROOT}/*.[Cc]"
                               "${PROJECT_ROOT}/*.[Hh][Pp][Pp]"
                               "${PROJECT_ROOT}/*.[Cc][Pp][Pp]"
                               "${PROJECT_ROOT}/*.[Cc][Xx][Xx]")

file(GLOB_RECURSE gcov_files "${CMAKE_CURRENT_BINARY_DIR}/*.gcov")
foreach(gcov_file ${gcov_files})
  read_and_reduce_gcov("${PROJECT_ROOT}" "${gcov_file}" source_file gcov)
  list(APPEND source_files "${source_file}")
  string(APPEND gcov_block "${gcov}")
endforeach()

list(REMOVE_DUPLICATES source_files)
foreach(source_file ${source_files})
  # ignore any files located in the build directory
  string(FIND "${source_file}" "${build_dir}/" in_build_dir)
  if(in_build_dir GREATER_EQUAL 0)
    continue()
  endif()
  # ignore paths with /CMakeFiles/
  if(source_file MATCHES "/CMakeFiles/")
    continue()
  endif()
  read_adjustments("${PROJECT_ROOT}" "${source_file}" adjustments)
  string(APPEND adjustments_block "${adjustments}")
  string(REPLACE "${PROJECT_ROOT}" "" source_file "${source_file}")
  string(REGEX REPLACE "^/+" "" source_file "${source_file}")
  string(APPEND network_block "${source_file}\n")
endforeach()

string(PREPEND adjustments_block "# path=fixes\n")
string(APPEND  adjustments_block "<<<<<< EOF")
string(APPEND network_block "<<<<<< network\n")

file(WRITE  ${CODECOV_FILE} "${network_block}")
file(APPEND ${CODECOV_FILE} "${gcov_block}")
file(APPEND ${CODECOV_FILE} "${adjustments_block}")
