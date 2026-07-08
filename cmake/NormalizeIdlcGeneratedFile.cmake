#
# Copyright(c) 2026 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

if(NOT DEFINED INPUT)
  message(FATAL_ERROR "INPUT is not set")
endif()
if(NOT DEFINED OUTPUT)
  message(FATAL_ERROR "OUTPUT is not set")
endif()
if(NOT DEFINED SOURCE)
  message(FATAL_ERROR "SOURCE is not set")
endif()

file(READ "${INPUT}" _generated)
get_filename_component(_source_name "${SOURCE}" NAME)
string(REGEX REPLACE
  "  Source: [^\r\n]*"
  "  Source: ${_source_name}"
  _generated
  "${_generated}")

if(DEFINED PUBLIC_INCLUDE_DIR)
  string(REGEX REPLACE
    "#include \"(ddsi_xt_[A-Za-z0-9_]+\\.h)\""
    "#include \"${PUBLIC_INCLUDE_DIR}/\\1\""
    _generated
    "${_generated}")
endif()

if(DEFINED HEADER_GUARD)
  string(REGEX REPLACE
    "#ifndef DDSC_[A-Z0-9_]+_[0-9A-F]+\r?\n#define DDSC_[A-Z0-9_]+_[0-9A-F]+"
    "#ifndef ${HEADER_GUARD}\n#define ${HEADER_GUARD}"
    _generated
    "${_generated}")
  string(REGEX REPLACE
    "#endif /\\* DDSC_[A-Z0-9_]+_[0-9A-F]+ \\*/"
    "#endif /* ${HEADER_GUARD} */"
    _generated
    "${_generated}")
endif()

string(REGEX REPLACE "[ \t]+\r?\n" "\n" _generated "${_generated}")

get_filename_component(_output_dir "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_dir}")
file(WRITE "${OUTPUT}" "${_generated}")
