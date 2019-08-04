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
find_package(Java 1.8 REQUIRED)

if(NOT IDLC_JAR)
  set(IDLC_JAR "${CMAKE_CURRENT_LIST_DIR}/idlc-jar-with-dependencies.jar")
endif()

set(LINE_ENDINGS "UNIX")
if(WIN32)
  set(EXTENSION ".bat")
  set(LINE_ENDINGS "WIN32")
endif()

set(IDLC_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE STRING "")
set(IDLC "dds_idlc${EXTENSION}" CACHE STRING "")
mark_as_advanced(IDLC_DIR IDLC)

set(IDLC_SCRIPT_IN "${CMAKE_CURRENT_LIST_DIR}/dds_idlc${EXTENSION}.in")

configure_file(
    "${IDLC_SCRIPT_IN}" "${IDLC}"
    @ONLY
    NEWLINE_STYLE ${LINE_ENDINGS})

if(NOT ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows"))
    execute_process(COMMAND chmod +x "${IDLC_DIR}/${IDLC}")
endif()

add_custom_target(idlc-jar ALL DEPENDS "${IDLC_JAR}")

function(IDLC_GENERATE _target)
  if(NOT ARGN)
    message(FATAL_ERROR "idlc_generate called without any idl files")
  endif()

  if (NOT IDLC_ARGS)
     set(IDLC_ARGS)
  endif()

  set(_files)
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    list(APPEND _files ${ABS_FIL})
  endforeach()

  set(_dir "${CMAKE_CURRENT_BINARY_DIR}")
  set(_sources)
  set(_headers)
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_WE ${FIL} NAME_WE)

    set(_source "${_dir}/${FIL_WE}.c")
    set(_header "${_dir}/${FIL_WE}.h")
    list(APPEND _sources "${_source}")
    list(APPEND _headers "${_header}")

    add_custom_command(
      OUTPUT   "${_source}" "${_header}"
      COMMAND  "${IDLC_DIR}/${IDLC}"
      ARGS     ${IDLC_ARGS} ${ABS_FIL}
      DEPENDS  "${_files}" idlc-jar
      COMMENT  "Running idlc on ${FIL}"
      VERBATIM)
  endforeach()

  add_custom_target(
    "${_target}_idlc_generate"
    DEPENDS "${_sources}" "${_headers}"
  )

  set_source_files_properties(
    ${_sources} ${_headers} PROPERTIES GENERATED TRUE)
  add_library(${_target} INTERFACE)
  target_sources(${_target} INTERFACE ${_sources} ${_headers})
  target_include_directories(${_target} INTERFACE "${_dir}")
  add_dependencies(${_target} "${_target}_idlc_generate")
endfunction()

