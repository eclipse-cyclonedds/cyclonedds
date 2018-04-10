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
find_package(Criterion REQUIRED)

include(Glob)

set(_criterion_dir "${CMAKE_CURRENT_LIST_DIR}/Criterion")

function(add_criterion_executable _target)
  set(s "[ \t\r\n]") # space
  set(w "[0-9a-zA-Z_]") # word
  set(b "[^0-9a-zA-Z_]") # boundary
  set(arg "${s}*(${w}+)${s}*")
  set(test "(^|${b})Test${s}*\\(${arg},${arg}(,[^\\)]+)?\\)") # Test
  set(params "${s}*\\([^\\)]*\\)${s}*")
  set(theory "(^|${b})Theory${s}*\\(${params},${arg},${arg}(,[^\\)]+)?\\)") # Theory
  set(paramtest "(^|${b})ParameterizedTest${s}*\\([^,]+,${arg},${arg}(,[^\\)]+)?\\)") # ParameterizedTest

  glob(_files "c" ${ARGN})

  foreach(_file ${_files})
    file(READ "${_file}" _contents)
    string(REGEX MATCHALL "${test}" _matches "${_contents}")

    list(APPEND _sources "${_file}")
    list(LENGTH _matches _length)
    if(_length)
      foreach(_match ${_matches})
        string(REGEX REPLACE "${test}" "\\2" _suite "${_match}")
        string(REGEX REPLACE "${test}" "\\3" _name "${_match}")
        list(APPEND _tests "${_suite}:${_name}")
      endforeach()
    endif()

    string(REGEX MATCHALL "${theory}" _matches "${_contents}")
    list(LENGTH _matches _length)
    if(_length)
      foreach(_match ${_matches})
        string(REGEX REPLACE "${theory}" "\\2" _suite "${_match}")
        string(REGEX REPLACE "${theory}" "\\3" _name "${_match}")
        list(APPEND _tests "${_suite}:${_name}")
      endforeach()
    endif()

    string(REGEX MATCHALL "${paramtest}" _matches "${_contents}")
    list(LENGTH _matches _length)
    if(_length)
      foreach(_match ${_matches})
        string(REGEX REPLACE "${paramtest}" "\\2" _suite "${_match}")
        string(REGEX REPLACE "${paramtest}" "\\3" _name "${_match}")
        list(APPEND _tests "${_suite}:${_name}")
      endforeach()
    endif()
  endforeach()

  add_executable(${_target} "${_criterion_dir}/src/runner.c" ${_sources})
  target_link_libraries(${_target} Criterion)

  foreach(_entry ${_tests})
    string(REPLACE ":" ";" _entry ${_entry})
    list(GET _entry 0 _suite)
    list(GET _entry 1 _name)

    add_test(
      NAME "Criterion_${_suite}_${_name}"
      COMMAND ${_target} --suite ${_suite} --test ${_name} --cunit=${_suite}-${_name} --quiet)
  endforeach()
endfunction()

