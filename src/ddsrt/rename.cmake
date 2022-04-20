#
# Copyright(c) 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
if(NOT INPUT_FILE)
  message(FATAL_ERROR "INPUT_FILE not specified")
elseif(NOT EXISTS "${INPUT_FILE}")
  message(FATAL_ERROR "INPUT_FILE (${INPUT_FILE}) does not exist")
endif()

if(NOT OUTPUT_DIRECTORY)
  message(FATAL_ERROR "OUTPUT_DIRECTORY not specified")
elseif(NOT EXISTS "${OUTPUT_DIRECTORY}")
  message(FATAL_ERROR "OUTPUT_DIRECTORY (${OUTPUT_DIRECTORY}) does not exist")
endif()

if(NOT OUTPUT_FILE)
  message(FATAL_ERROR "OUTPUT_FILE not specified")
#elseif(EXISTS "${OUTPUT_FILE}")
#  # ensure no existing file is overwritten (including input)
#  message(FATAL_ERROR "OUTPUT_FILE (${OUTPUT_FILE}) already exists")
endif()

# normalize paths
file(TO_CMAKE_PATH "${INPUT_FILE}" input_file)
file(TO_CMAKE_PATH "${OUTPUT_DIRECTORY}" output_directory)
file(TO_CMAKE_PATH "${OUTPUT_FILE}" output_file)

if(IS_ABSOLUTE "${output_file}")
  string(FIND "${output_file}" "${output_directory}" position)
  if(NOT position EQUAL 0)
    message(FATAL_ERROR "OUTPUT_FILE (${OUTPUT_FILE}) is not relative to OUTPUT_DIRECTORY (${OUTPUT_DIRECTORY})")
  endif()
  file(RELATIVE_PATH output_file "${output_directory}" "${output_file}")
endif()

if(NOT NAMESPACE)
  message(FATAL_ERROR "NAMESPACE not specified")
else()
  string(MAKE_C_IDENTIFIER "${NAMESPACE}" namespace)
  if(NOT namespace STREQUAL NAMESPACE)
    message(FATAL_ERROR "NAMESPACE (${NAMESPACE}) is not a valid identifer")
  endif()
endif()

string(TOLOWER "${NAMESPACE}" lower)
string(TOUPPER "${NAMESPACE}" upper)

# ensure output directory exists
get_filename_component(relative_path "${output_file}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}/${relative_path}")

file(READ "${input_file}" text)

set(W "[^_a-zA-Z0-9]")
# replace paths
string(REGEX REPLACE "(${W}|/)dds/ddsrt/" "\\1${lower}/" text "${text}")
string(REGEX REPLACE "(${W}|/)dds(rt)?/" "\\1${lower}/" text "${text}")
# replace macro prefixes
string(REGEX REPLACE "(^|${W})DDS(RT)?_" "\\1${upper}_" text "${text}")
# replace symbol prefixes
string(REGEX REPLACE "(^|${W})[dD][dD][sS]([rR][tT])?_" "\\1${lower}_" text "${text}")
# replace generic references
string(REGEX REPLACE "(^|${W})DDS(RT)?(${W}|$)" "\\1${upper}\\3" text "${text}")
string(REGEX REPLACE "(^|${W})[dD][dD][sS]([rR][tT])?(${W}|$)" "\\1${lower}\\3" text "${text}")

file(WRITE "${output_directory}/${output_file}" "${text}")
