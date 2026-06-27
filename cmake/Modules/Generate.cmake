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
  set(options NO_TYPE_INFO WERROR DEFAULT_NON_NESTED)
  set(one_value_keywords TARGET DEFAULT_EXTENSIBILITY BASE_DIR OUTPUT_DIR)
  set(multi_value_keywords FILES FEATURES INCLUDES WARNINGS DEFINES)
  cmake_parse_arguments(
    IDLC "${options}" "${one_value_keywords}" "${multi_value_keywords}" "" ${ARGN})

  if (TARGET CycloneDDS::libidlc)
    set(_idlc_backend "$<TARGET_FILE:CycloneDDS::libidlc>")
    if (NOT DEFINED _idlc_generate_skipreport)
      message(STATUS "Building internal IDLC backend")
      set(_idlc_generate_skipreport 1 CACHE INTERNAL "")
    endif()
    set(_idlc_depends CycloneDDS::libidlc)
  else()
    if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND NOT ".so" IN_LIST CMAKE_FIND_LIBRARY_SUFFIXES)
      # When cross-compiling, find_library looks for libraries using naming conventions of the target,
      # But here we're trying to find the idlc backend library to run on the host.
      # For now, building for a Windows target on a Linux host with w64-mingw is supported by this workaround.
      list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".so")
    endif()
    find_library(_idlc_backend "cycloneddsidlc" NO_CMAKE_FIND_ROOT_PATH)
    if (_idlc_backend)
      if (NOT DEFINED _idlc_generate_skipreport)
        message(STATUS "Using external IDLC backend: ${_idlc_backend}")
        set(_idlc_generate_skipreport 1 CACHE INTERNAL "")
      endif()
    else()
      message(FATAL_ERROR "Unable to find IDLC C-backend library: cycloneddsidlc")
    endif()
  endif()

  set(gen_args
    BACKEND ${_idlc_backend}
    ${IDLC_UNPARSED_ARGUMENTS}
    TARGET ${IDLC_TARGET}
    BASE_DIR ${IDLC_BASE_DIR}
    FILES ${IDLC_FILES}
    FEATURES ${IDLC_FEATURES}
    INCLUDES ${IDLC_INCLUDES}
    WARNINGS ${IDLC_WARNINGS}
    OUTPUT_DIR ${IDLC_OUTPUT_DIR}
    DEFAULT_EXTENSIBILITY ${IDLC_DEFAULT_EXTENSIBILITY}
    DEFINES ${IDLC_DEFINES}
    DEPENDS ${_idlc_depends})

  if(${IDLC_NO_TYPE_INFO})
    list(APPEND gen_args NO_TYPE_INFO)
  endif()

  if(${IDLC_WERROR})
    list(APPEND gen_args WERROR)
  endif()

  if(${IDLC_DEFAULT_NON_NESTED})
    list(APPEND gen_args DEFAULT_NON_NESTED)
  endif()

  idlc_generate_generic(${gen_args})
endfunction()

function(IDLC_GENERATE_GENERIC)
  set(options NO_TYPE_INFO WERROR DEFAULT_NON_NESTED)
  set(one_value_keywords TARGET BACKEND DEFAULT_EXTENSIBILITY BASE_DIR OUTPUT_DIR)
  set(multi_value_keywords FILES FEATURES INCLUDES WARNINGS SUFFIXES DEFINES DEPENDS)
  cmake_parse_arguments(
    IDLC "${options}" "${one_value_keywords}" "${multi_value_keywords}" "" ${ARGN})

  # find idlc binary
  if(BUILD_IDLC)
    if (CMAKE_PROJECT_NAME STREQUAL "CycloneDDS")
      # By using the internal target when building CycloneDDS itself, prevent using an external idlc
      # This prevents a problem when an installed cyclone is on your prefix path when building cyclone again
      set(_idlc_executable idlc)
      set(_idlc_depends idlc)
    else()
      set(_idlc_executable CycloneDDS::idlc)
      set(_idlc_depends CycloneDDS::idlc)
    endif()
  else()
    find_program(_idlc_executable "idlc" NO_CMAKE_FIND_ROOT_PATH REQUIRED)
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

  # add macro definitions (-Dname or -Dname=value)
  foreach(def ${IDLC_DEFINES})
    list(APPEND IDLC_ARGS "-D${def}")
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

  if(IDLC_DEFAULT_NON_NESTED)
    list(APPEND IDLC_ARGS "-nfalse")
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

  # Keep generated source mtimes stable only for the in-tree shared build,
  # where rebuilding idlc from core would otherwise force test IDL rebuilds.
  # Static builds consume some generated files as ordinary sources, so use
  # direct outputs there as for application builds.
  if(CMAKE_PROJECT_NAME STREQUAL "CycloneDDS" AND BUILD_IDLC AND BUILD_SHARED_LIBS)
    set(_stable_idlc_outputs ON)
  else()
    set(_stable_idlc_outputs OFF)
    list(APPEND IDLC_ARGS "-o${_dir}")
  endif()

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
  set(_stamps "")
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
      string(REGEX REPLACE "[\\/]$" "" _mid_dir_path "${_mid_dir_path}")
    endif()

    if(_stable_idlc_outputs)
      # Keep generated source mtimes stable when idlc output is unchanged.
      string(MD5 _file_hash "${_file}")
      set(_tmp_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${_target}_idlc/${_name}-${_file_hash}")
      set(_stamp "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${_target}_idlc/${_name}-${_file_hash}.stamp")
    endif()

    set(_file_outputs "")
    if(_stable_idlc_outputs)
      set(_copy_commands "")
      set(_make_dir_commands "")
    endif()
    foreach(_suffix ${IDLC_SUFFIXES})
      if(IDLC_BASE_DIR)
        set(_output "${_dir}/${_mid_dir_path}/${_name}${_suffix}")
        if(_stable_idlc_outputs)
          set(_tmp_output "${_tmp_dir}/${_mid_dir_path}/${_name}${_suffix}")
        endif()
      else()
        set(_output "${_dir}/${_name}${_suffix}")
        if(_stable_idlc_outputs)
          set(_tmp_output "${_tmp_dir}/${_name}${_suffix}")
        endif()
      endif()
      list(APPEND _file_outputs "${_output}")
      if(_stable_idlc_outputs)
        get_filename_component(_output_dir "${_output}" DIRECTORY)
        list(APPEND _make_dir_commands
          COMMAND "${CMAKE_COMMAND}" -E make_directory "${_output_dir}")
        list(APPEND _copy_commands
          COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_tmp_output}" "${_output}")
      endif()
    endforeach()

    list(APPEND _outputs ${_file_outputs})
    if(_stable_idlc_outputs)
      list(APPEND _stamps "${_stamp}")
      add_custom_command(
        OUTPUT   "${_stamp}"
        BYPRODUCTS ${_file_outputs}
        COMMAND  "${CMAKE_COMMAND}" -E remove_directory "${_tmp_dir}"
        COMMAND  "${CMAKE_COMMAND}" -E make_directory "${_tmp_dir}"
        COMMAND  ${_idlc_executable}
        ARGS     ${_language} ${IDLC_ARGS} "-o${_tmp_dir}" ${IDLC_INCLUDE_DIRS} ${_file}
        ${_make_dir_commands}
        ${_copy_commands}
        COMMAND  "${CMAKE_COMMAND}" -E touch "${_stamp}"
        DEPENDS  ${_files} ${_depends}
        VERBATIM)
    else()
      add_custom_command(
        OUTPUT   ${_file_outputs}
        COMMAND  ${_idlc_executable}
        ARGS     ${_language} ${IDLC_ARGS} ${IDLC_INCLUDE_DIRS} ${_file}
        DEPENDS  ${_files} ${_depends})
    endif()
  endforeach()

  if(_stable_idlc_outputs)
    add_custom_target("${_target}_generate" DEPENDS "${_stamps}")
  else()
    add_custom_target("${_target}_generate" DEPENDS "${_outputs}")
  endif()
  if(${CMAKE_GENERATOR} MATCHES "Xcode")
    set_target_properties("${_target}_generate" PROPERTIES XCODE_GENERATE_SCHEME NO)
  endif()

  add_library(${_target} INTERFACE)
  target_sources(${_target} INTERFACE ${_outputs})
  target_include_directories(${_target} INTERFACE "${_dir}")
  add_dependencies(${_target} "${_target}_generate")
  if(${CMAKE_GENERATOR} MATCHES "Xcode")
    set_target_properties("${_target}" PROPERTIES XCODE_GENERATE_SCHEME NO)
  endif()
endfunction()
