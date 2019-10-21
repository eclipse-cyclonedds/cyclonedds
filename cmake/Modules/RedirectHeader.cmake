# Copyright(c) 2019 Rover Robotics via Dan Rose
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#[[.rst:
RedirectHeader
-------

.. command:: redirect_header(old_file new_file [SOURCE src_includes_dir] DESTINATION build_includes_dir [ERROR])

  Generate a stub headers to gently inform developers that a public header file has moved

  ``old_file`` Path to #include the old header file. Should end in '.h' and file **should not** exist.
  ``new_file`` Path to #include the new header file. Should end in '.h' and file **should** exist.
  ``src_includes_dir`` If specified, we will check that ``new_path`` exists at `src_includes_dir`/`new_file`
  ``build_includes_dir`` Path under which to generate stub header. Default "${CMAKE_CURRENT_BINARY_DIR}/include/".
  ``ERROR`` If passed, using the stub headers is an error. Otherwise, it is just a message.

.. command:: redirect_header_directory(old_dir new_dir SOURCE src_includes_dir DESTINATION build_includes_dir [ERROR])

  Generate a bunch of stub headers to gently inform developers an entire header directory has moved

  ``old_dir`` Path that used to contain a bunch of headers.
  ``new_dir`` Path that now contains those headers. Should be a directory and should exist.
  ``src_includes_dir`` Include root which has `new_dir` as a subdirectory, containing header files
  ``build_includes_dir`` Path to an 'include' folder, under which to generate stub header files
  ``ERROR`` If passed, using the stub headers is an error. Otherwise, it is just a message.
#]]

function(dump)
  get_cmake_property(_variableNames VARIABLES)
  list(SORT _variableNames)
  foreach(_variableName ${_variableNames})
    message("${_variableName}=${${_variableName}}")
  endforeach()
endfunction()

function(redirect_header old_file new_file)
  set(options ERROR)
  set(oneValueArgs DESTINATION SOURCE)
  cmake_parse_arguments(PARSE_ARGV 2 REDIRECT_HEADER "${options}" "${oneValueArgs}" "")

  if(("${old_file}" STREQUAL "") OR ("${new_file}" STREQUAL ""))
    message(FATAL_ERROR "Must specify both an old and a new include path")
  endif()

  if(REDIRECT_HEADER_SOURCE)
    if(NOT EXISTS "${REDIRECT_HEADER_SOURCE}/${new_path}")
      message(FATAL_ERROR "Expected file at ${REDIRECT_HEADER_SOURCE}/${new_path}")
    endif()
  endif()

  if(NOT REDIRECT_HEADER_DESTINATION)
    message(FATAL_ERROR "Missing required argument: DESTINATION")
  endif()

  if(EXISTS "${REDIRECT_HEADER_DESTINATION}/${old_file}")
    get_property(src_gen SOURCE "${REDIRECT_HEADER_DESTINATION}/${new_file}" PROPERTY GENERATED)

    get_property(DEST_IS_GENERATED SOURCE "${REDIRECT_HEADER_DESTINATION}/${old_file}" PROPERTY GENERATED)
    if(NOT ${DEST_IS_GENERATED})
      dump()
      message(FATAL_ERROR "Non-generated file already exists at generated header path ${REDIRECT_HEADER_DESTINATION}/${old_file}")
    endif()
  endif()

  if(REDIRECT_HEADER_ERROR)
    set(directive "#error")
  else()
    set(directive "#pragma message")
  endif()

  if("${old_file}" STREQUAL "${new_file}")
    message(FATAL_ERROR "Old and new include paths must be different")
  endif()

  string(CONFIGURE "\
// Generated header from project @PROJECT_NAME@, file RedirectHeader.cmake

@directive@(\"Please update your #include path to point to \\\"@new_file@\\\".\\nThis file has been permanently moved from \\\"@old_file@\\\".\\n\")

#include \"@new_file@\"
" content @ONLY ESCAPE_QUOTES)

  get_filename_component(generated_header_path "${old_file}" ABSOLUTE BASE_DIR "${REDIRECT_HEADER_DESTINATION}")
  file(GENERATE OUTPUT "${generated_header_path}" CONTENT "${content}")
endfunction()

function(redirect_header_directory old_dir new_dir)
  set(options ERROR)
  set(oneValueArgs DESTINATION SOURCE)
  cmake_parse_arguments(PARSE_ARGV 2 REDIRECT_HEADER "${options}" "${oneValueArgs}" "")

  if(NOT REDIRECT_HEADER_SOURCE)
    message(FATAL_ERROR "Missing required argument 'SOURCE'")
  endif()
  if(NOT REDIRECT_HEADER_DESTINATION)
    message(FATAL_ERROR "Missing required argument: DESTINATION")
  endif()

  get_filename_component(subdir "${new_dir}" ABSOLUTE BASE_DIR "${REDIRECT_HEADER_SOURCE}")
  if(NOT EXISTS "${REDIRECT_HEADER_SOURCE}/${new_dir}")
    message(FATAL_ERROR "Provided path to headers '${subdir}' is not an existing directory")
  endif()

  if(REDIRECT_HEADER_ERROR)
    set(flag ERROR)
  else()
    set(flag "")
  endif()

  file(GLOB_RECURSE include_files
    RELATIVE "${REDIRECT_HEADER_SOURCE}/${new_dir}"
    "${REDIRECT_HEADER_SOURCE}/${new_dir}/*.h")
  foreach(path_to_header IN LISTS include_files)
    redirect_header("${old_dir}/${path_to_header}" "${new_dir}/${path_to_header}"
      SOURCE "${REDIRECT_HEADER_SOURCE}"
      DESTINATION "${REDIRECT_HEADER_DESTINATION}"
      ${flag}
      )
  endforeach()
endfunction()