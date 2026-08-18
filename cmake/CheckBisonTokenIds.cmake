# Copyright(c) 2026 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

if(NOT DEFINED HAND_WRITTEN_HEADER)
  message(FATAL_ERROR "HAND_WRITTEN_HEADER is not set")
endif()
if(NOT DEFINED BISON_HEADER)
  message(FATAL_ERROR "BISON_HEADER is not set")
endif()

function(read_token_ids OUTVAR HEADER)
  file(STRINGS "${HEADER}" _lines)
  set(_tokens "")
  set(_in_enum 0)

  foreach(_line IN LISTS _lines)
    if(NOT _in_enum)
      if(_line MATCHES "enum[ \t]+idl_yytokentype")
        set(_in_enum 1)
      endif()
      continue()
    endif()

    if(_line MATCHES "^[ \t]*};")
      break()
    endif()

    string(REGEX REPLACE "/\\*.*\\*/" "" _line "${_line}")
    if(_line MATCHES "^[ \t]*([A-Za-z_][A-Za-z0-9_]*)[ \t]*=[ \t]*(-?[0-9]+)")
      list(APPEND _tokens "${CMAKE_MATCH_1}=${CMAKE_MATCH_2}")
    endif()
  endforeach()

  if(_tokens STREQUAL "")
    message(FATAL_ERROR "No token IDs found in ${HEADER}")
  endif()

  set(${OUTVAR} "${_tokens}" PARENT_SCOPE)
endfunction()

read_token_ids(_hand_tokens "${HAND_WRITTEN_HEADER}")
read_token_ids(_bison_tokens "${BISON_HEADER}")

list(LENGTH _hand_tokens _hand_count)
list(LENGTH _bison_tokens _bison_count)
if(NOT _hand_count EQUAL _bison_count)
  message(FATAL_ERROR
    "Bison token ID count differs from handwritten token ID count: "
    "${_bison_count} != ${_hand_count}")
endif()

math(EXPR _last "${_hand_count} - 1")
foreach(_idx RANGE 0 ${_last})
  list(GET _hand_tokens ${_idx} _hand_token)
  list(GET _bison_tokens ${_idx} _bison_token)
  if(NOT _hand_token STREQUAL _bison_token)
    message(FATAL_ERROR
      "Bison token ID mismatch at index ${_idx}: "
      "handwritten ${_hand_token}, bison ${_bison_token}")
  endif()
endforeach()

message(STATUS "Bison token IDs match handwritten token IDs (${_hand_count} tokens).")
