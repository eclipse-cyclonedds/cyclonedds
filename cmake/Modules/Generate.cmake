#
# Copyright(c) 2021 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

function(IDLC_GENERATE)
  set(options NO_TYPE_INFO WERROR)
  set(one_value_keywords TARGET DEFAULT_EXTENSIBILITY BASE_DIR OUTPUT_DIR)
  set(multi_value_keywords FILES FEATURES INCLUDES WARNINGS)
  cmake_parse_arguments(
    IDLC "${options}" "${one_value_keywords}" "${multi_value_keywords}" "" ${ARGN})

  set(gen_args
    ${IDLC_UNPARSED_ARGUMENTS}
    TARGET ${IDLC_TARGET}
    BASE_DIR ${IDLC_BASE_DIR}
    FILES ${IDLC_FILES}
    FEATURES ${IDLC_FEATURES}
    INCLUDES ${IDLC_INCLUDES}
    WARNINGS ${IDLC_WARNINGS}
    OUTPUT_DIR ${IDLC_OUTPUT_DIR}
    DEFAULT_EXTENSIBILITY ${IDLC_DEFAULT_EXTENSIBILITY})
  if(${IDLC_NO_TYPE_INFO})
    list(APPEND gen_args NO_TYPE_INFO)
  endif()
  if(${IDLC_WERROR})
    list(APPEND gen_args WERROR)
  endif()
  idlc_generate_generic(${gen_args})
endfunction()

function(IDLC_GENERATE_GENERIC)
  set(options NO_TYPE_INFO WERROR)
  set(one_value_keywords TARGET BACKEND DEFAULT_EXTENSIBILITY BASE_DIR OUTPUT_DIR)
  set(multi_value_keywords FILES FEATURES INCLUDES WARNINGS SUFFIXES DEPENDS)
  cmake_parse_arguments(
    IDLC "${options}" "${one_value_keywords}" "${multi_value_keywords}" "" ${ARGN})

  # find idlc binary
  if(CMAKE_CROSSCOMPILING OR NOT BUILD_IDLC)
    find_program(_idlc_executable idlc NO_CMAKE_FIND_ROOT_PATH REQUIRED)

    if(_idlc_executable)
      set(_idlc_depends "")
    else()
      message(FATAL_ERROR "Cannot find idlc executable")
    endif()
  else()
    if (CMAKE_PROJECT_NAME STREQUAL "CycloneDDS")
      # By using the internal target when building CycloneDDS itself, prevent using an external idlc
      # This prevents a problem when an installed cyclone is on your prefix path when building cyclone again
      set(_idlc_executable idlc)
      set(_idlc_depends idlc)
    else()
      set(_idlc_executable CycloneDDS::idlc)
      set(_idlc_depends CycloneDDS::idlc)
    endif()
  endif()

  if(NOT IDLC_TARGET AND NOT IDLC_FILES)
    # assume deprecated invocation: TARGET FILE [FILE..]
    list(GET IDLC_UNPARSED_ARGUMENTS 0 IDLC_TARGET)
    list(REMOVE_AT IDLC_UNPARSED_ARGUMENTS 0 IDLC_)
    set(IDLC_FILES ${IDLC_UNPARSED_ARGUMENTS})
    if (IDLC_TARGET AND IDLC_FILES)
      message(WARNING " Deprecated use of idlc_generate. \n"
                      " Consider switching to keyword based invocation.")
    endif()
    # Java based compiler used to be case sensitive
    list(APPEND IDLC_FEATURES "case-sensitive")
  endif()

  if(NOT IDLC_TARGET)
    message(FATAL_ERROR "idlc_generate called without TARGET")
  elseif(NOT IDLC_FILES)
    message(FATAL_ERROR "idlc_generate called without FILES")
  endif()

  # remove duplicate features
  if(IDLC_FEATURES)
    list(REMOVE_DUPLICATES IDLC_FEATURES)
  endif()
  foreach(_feature ${IDLC_FEATURES})
    list(APPEND IDLC_ARGS "-f" ${_feature})
  endforeach()

  # add directories to include search list
  if(IDLC_INCLUDES)
    foreach(_dir ${IDLC_INCLUDES})
      list(APPEND IDLC_INCLUDE_DIRS "-I" ${_dir})
    endforeach()
  endif()

  # generate using which language (defaults to c)?
  if(IDLC_BACKEND)
    string(APPEND _language "-l" ${IDLC_BACKEND})
  endif()

  # set dependencies
  if(IDLC_DEPENDS)
    list(APPEND _depends ${_idlc_depends} ${IDLC_DEPENDS})
  else()
    set(_depends ${_idlc_depends})
  endif()

  if(IDLC_DEFAULT_EXTENSIBILITY)
    set(_default_extensibility ${IDLC_DEFAULT_EXTENSIBILITY})
    list(APPEND IDLC_ARGS "-x" ${_default_extensibility})
  endif()

  if(IDLC_WARNINGS)
    foreach(_warn ${IDLC_WARNINGS})
      list(APPEND IDLC_ARGS "-W${_warn}")
    endforeach()
  endif()

  if(IDLC_NO_TYPE_INFO)
    list(APPEND IDLC_ARGS "-t")
  endif()

  if(IDLC_WERROR)
    list(APPEND IDLC_ARGS "-Werror")
  endif()

  if(IDLC_BASE_DIR)
    if(${CMAKE_VERSION} VERSION_LESS "3.19.0")
      get_filename_component(_base_dir_abs ${IDLC_BASE_DIR} REALPATH)
    else()
      file(REAL_PATH ${IDLC_BASE_DIR} _base_dir_abs)
    endif()
    list(APPEND IDLC_ARGS "-b${_base_dir_abs}")
  endif()

  if(IDLC_OUTPUT_DIR)
    set(_dir ${IDLC_OUTPUT_DIR})
  else()
    set(_dir ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  list(APPEND IDLC_ARGS "-o${_dir}")
  set(_target ${IDLC_TARGET})
  foreach(_file ${IDLC_FILES})
    get_filename_component(_path ${_file} ABSOLUTE)
    list(APPEND _files "${_path}")
  endforeach()

  # set source suffixes (defaults to .c and .h)
  if(NOT IDLC_SUFFIXES)
    set(IDLC_SUFFIXES ".c" ".h")
  endif()
  set(_outputs "")
  foreach(_file ${_files})
    get_filename_component(_name ${_file} NAME_WLE)
    get_filename_component(_name_ext ${_file} NAME)

    # Determine middle path for directory reconstruction
    if(IDLC_BASE_DIR)
      file(RELATIVE_PATH _file_path_rel ${_base_dir_abs} ${_file})
      # Hard Fail
      if(_file_path_rel MATCHES "^\\.\\.")
        message(FATAL_ERROR "Cannot use base dir with different file tree from input file (${_base_dir_abs} to ${_file} yields ${_file_path_rel})")
      endif()
      string(REPLACE ${_name_ext} "" _mid_dir_path ${_file_path_rel})
      string(REGEX REPLACE "[\\/]$" "" _mid_dir_path ${_mid_dir_path})
    endif()

    set(_file_outputs "")
    foreach(_suffix ${IDLC_SUFFIXES})
      if(IDLC_BASE_DIR)
        list(APPEND _file_outputs "${_dir}/${_mid_dir_path}/${_name}${_suffix}")
      else()
        list(APPEND _file_outputs "${_dir}/${_name}${_suffix}")
      endif()
    endforeach()

    list(APPEND _outputs ${_file_outputs})
    add_custom_command(
      OUTPUT   ${_file_outputs}
      COMMAND  ${_idlc_executable}
      ARGS     ${_language} ${IDLC_ARGS} ${IDLC_INCLUDE_DIRS} ${_file}
      DEPENDS  ${_files} ${_depends})
  endforeach()

  add_custom_target("${_target}_generate" DEPENDS "${_outputs}")
  add_library(${_target} INTERFACE)
  target_sources(${_target} INTERFACE ${_outputs})
  target_include_directories(${_target} INTERFACE "${_dir}")
  add_dependencies(${_target} "${_target}_generate")
endfunction()
