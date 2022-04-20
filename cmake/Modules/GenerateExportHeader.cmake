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
if(__GENERATE_EXPORT_HEADER_INCLUDED__)
  return()
endif()
set(__GENERATE_EXPORT_HEADER_INCLUDED__ TRUE)

get_filename_component(_current_list_dir "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

foreach(_module_dir ${CMAKE_MODULE_PATH} ${CMAKE_ROOT}/Modules)
  get_filename_component(_module_dir "${_module_dir}" ABSOLUTE)
  if(NOT _current_list_dir STREQUAL _module_dir)
    if(EXISTS "${_module_dir}/GenerateExportHeader.cmake")
      set(_include_file "${_module_dir}/GenerateExportHeader.cmake")
      break()
    endif()
  endif()
endforeach()

if(_include_file)
  include("${_include_file}")

  macro(GENERATE_EXPORT_HEADER TARGET_LIBRARY)
    set(_opts)
    set(_single_opts PREFIX_NAME BASE_NAME CUSTOM_CONTENT_FROM_VARIABLE)
    set(_multi_opts)

    cmake_parse_arguments(_GEHW "${_opts}" "${_single_opts}" "${_multi_opts}" ${ARGN})

    string(MAKE_C_IDENTIFIER "${TARGET_LIBRARY}" _target_name)
    set(_prefix_name "${_GEHW_PREFIX_NAME}")
    set(_base_name "${TARGET_LIBRARY}")

    if(_GEHW_PREFIX_NAME)
      set(_prefix_name_args PREFIX_NAME "${_GEHW_PREFIX_NAME}")
    endif()
    if(_GEHW_BASE_NAME)
      set(_base_name_args BASE_NAME "${_GEHW_BASE_NAME}")
      set(_base_name ${_GEHW_BASE_NAME})
    endif()

    string(TOUPPER ${_base_name} _base_name_upper)

    # Exporting inline functions using the __declspec(dllexport) keyword in
    # dynamic-link libraries results in "multiple definitions of xxx" if
    # compiled with MinGW. Export the extern inline declaration instead.
    # See https://gitlab.kitware.com/cmake/cmake/-/issues/21940 for details.
    set(_custom_content
"
#ifndef ${_prefix_name}${_base_name_upper}_INLINE_EXPORT
#  if __MINGW32__ && (!defined(__clang__) || !defined(${_target_name}_EXPORTS))
#    define ${_prefix_name}${_base_name_upper}_INLINE_EXPORT
#  else
#    define ${_prefix_name}${_base_name_upper}_INLINE_EXPORT ${_prefix_name}${_base_name_upper}_EXPORT
#  endif
#endif
")

    if(_GEHW_CUSTOM_CONTENT_FROM_VARIABLE)
      set(_custom_content
        "${_custom_content}\n${${_GEHW_CUSTOM_CONTENT_FROM_VARIABLE}}")
    endif()

    _GENERATE_EXPORT_HEADER(
      ${TARGET_LIBRARY}
      ${_prefix_name_args}
      ${_base_name_args}
      CUSTOM_CONTENT_FROM_VARIABLE _custom_content
      ${_GEHW_UNPARSED_ARGUMENTS})
  endmacro()
endif()
