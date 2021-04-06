#
# Copyright(c) 2019 Jeroen Koekkoek <jeroen@koekkoek.nl>
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
get_filename_component(__check_target_cpu_dir "${CMAKE_CURRENT_LIST_FILE}" PATH)

include_guard(GLOBAL)

function(__check_target_cpu_impl _var _lang)
  if(_lang STREQUAL "C")
    set(_src "${CMAKE_BINARY_DIR}/CheckTargetCPU/${_var}.c")
  elseif(lang STREQUAL "CXX")
    set(_src "${CMAKE_BINARY_DIR}/CheckTargetCPU/${_var}.cpp")
  endif()

  set(_bin ${CMAKE_BINARY_DIR}/CheckTargetCPU/${_var}.bin)
  configure_file(${__check_target_cpu_dir}/CheckTargetCPU.c.in ${_src} @ONLY)
  try_compile(HAVE_${_var} ${CMAKE_BINARY_DIR} ${_src}
    COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
    LINK_OPTIONS  ${CMAKE_REQUIRED_LINK_OPTIONS}
    LINK_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES}
    CMAKE_FLAGS
      "-DCOMPILE_DEFINITIONS:STRING=${CMAKE_REQUIRED_FLAGS}"
      "-DINCLUDE_DIRECTORIES:STRING=${CMAKE_REQUIRED_INCLUDES}"
    OUTPUT_VARIABLE _output
    COPY_FILE ${_bin}
  )

  if(HAVE_${_var})
    # Fat binaries may be generated on macOS X and multiple, incompatible,
    # occurrences of INFO:cpu[<cpu>] may be found.
    file(STRINGS ${_bin} _strs LIMIT_COUNT 10 REGEX "INFO:cpu")
    set(_expr_cpu ".*INFO:cpu\\[([^]]+)\\]")
    set(_first TRUE)
    foreach(_info ${_strs})
      if(${_info} MATCHES "${_expr_cpu}")
        set(_temp "${CMAKE_MATCH_1}")
        if(_first)
          set(_cpu "${_temp}")
        elseif(NOT "${_cpu}" STREQUAL "${_temp}")
          set(_mismatch TRUE)
        endif()
        set(_first FALSE)
      endif()
    endforeach()

    if(_mismatch)
      message(SEND_ERROR "CHECK_TARGET_CPU found different results, consider setting CMAKE_OSX_ARCHITECTURES or CMAKE_TRY_COMPILE_OSX_ARCHITECTURES to one or no architecture !")
    endif()

    if(NOT CMAKE_REQUIRED_QUIET)
      message(STATUS "Check target architecture - done")
    endif()
    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
      "Determining target architecture passed with the following output:\n${_output}\n\n")
    set(${_var} "${_cpu}" CACHE INTERNAL "CHECK_TARGET_CPU")
  else()
    if(NOT CMAKE_REQUIRED_QUIET)
      message(STATUS "Check target architecture - failed")
    endif()
    file(READ ${_src} content)
    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
      "Determining target architecture failed with the following output:\n${_output}\n${_src}:\n${_content}\n\n")
    set(${_var} "" CACHE INTERNAL "CHECK_TARGET_CPU: unknown")
  endif()
endfunction()

macro(CHECK_TARGET_CPU VARIABLE)
  set(_opts)
  set(_single_opts LANGUAGE)
  set(_multi_opts)
  cmake_parse_arguments(_opt "${_opts}" "${_single_opts}" "${_multi_opts}" ${ARGN})

  if(NOT DEFINED _opt_LANGUAGE)
    set(_lang "C")
  elseif(NOT "x${_opt_LANGUAGE}" MATCHES "^x(C|CXX)$")
    message(FATAL_ERROR "Unknown language:\n  ${_opt_LANGUAGE}.\nSupported languages: C, CXX.\n")
  else()
    set(_lang "${_opt_LANGUAGE}")
  endif()

  if(NOT DEFINED HAVE_${VARIABLE})
    __check_target_cpu_impl(${VARIABLE} ${_lang})
  endif()
endmacro()

