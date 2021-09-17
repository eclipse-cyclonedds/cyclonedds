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

if (NOT BUILD_IDLC)
  return()
endif()

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)
include(GenerateExportHeader)

function(idlpp_system_type _name _var)
  string(TOLOWER ${_name} _name)
  if("${_name}" MATCHES "^windows")
    set(${_var} "SYS_WIN32" PARENT_SCOPE)
  elseif("${_name}" MATCHES "^linux")
    set(${_var} "SYS_LINUX" PARENT_SCOPE)
  elseif("${_name}" MATCHES "^freebsd")
    set(${_var} "SYS_FREEBSD" PARENT_SCOPE)
  elseif("${_name}" MATCHES "^darwin")
    set(${_var} "SYS_MAC" PARENT_SCOPE)
  elseif("${_name}" MATCHES "^cygwin")
    set(${_var} "SYS_CYGWIN" PARENT_SCOPE)
  elseif("${_name}" MATCHES "^mingw")
    set(${_var} "SYS_MINGW" PARENT_SCOPE)
  else()
    set(${_var} "SYS_UNIX" PARENT_SCOPE)
  endif()
endfunction()

function(have_type _type _var)
  check_type_size(${_type} _size ${ARGN})
  if(_size)
    set(${_var} 1 PARENT_SCOPE)
  else()
    set(${_var} 0 PARENT_SCOPE)
  endif()
endfunction()

macro(have_symbol _symbol _header _var)
  check_symbol_exists("${_symbol}" "${_header}" ${_var})
  if(${${_var}})
    set(${_var} 1)
  else()
    set(${_var} 0)
  endif()
endmacro()

set(idlpp_gcc_major_version_define "#undef GCC_MAJOR_VERSION")
set(idlpp_gcc_minor_version_define "#undef GCC_MINOR_VERSION")
if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  set(idlpp_host_compiler "MSC")
  set(idlpp_host_cmp_name "Visual C++")
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
  set(idlpp_host_compiler "GNUC")
  set(idlpp_host_cmp_name "GCC")
  if(CMAKE_C_COMPILER_VERSION MATCHES "([0-9]+)\.([0-9]+)")
    set(idlpp_gcc_major_version_define "#define GCC_MAJOR_VERSION \"${CMAKE_MATCH_1}\"")
    set(idlpp_gcc_minor_version_define "#define GCC_MINOR_VERSION \"${CMAKE_MATCH_2}\"")
  else()
    message(FATAL "Cannot parse GCC version number")
  endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(idlpp_host_compiler "GNUC")
  set(idlpp_host_cmp_name "Clang")
  if(CMAKE_C_COMPILER_VERSION MATCHES "([0-9]+)\.([0-9]+)")
    set(idlpp_gcc_major_version_define "#define GCC_MAJOR_VERSION \"${CMAKE_MATCH_1}\"")
    set(idlpp_gcc_minor_version_define "#define GCC_MINOR_VERSION \"${CMAKE_MATCH_2}\"")
  else()
    message(FATAL "Cannot parse Clang version number")
  endif()
endif()

# Generate __CYCLONEDDS__ macro value XXYYZZ. Version number as an integer
# constant so that it may be used in mathmatical equations.
#  XX: Major version
#  YY: Minor version
#  ZZ: Patch version
foreach(var MAJOR MINOR PATCH)
  if("00${PROJECT_VERSION_${var}}" MATCHES "([0-9][0-9])$")
    set(idlpp_version "${idlpp_version}${CMAKE_MATCH_1}")
  endif()
endforeach()

set(idlpp_include_dir "/path/to/cyclonedds/idl/includes")

idlpp_system_type("${CMAKE_HOST_SYSTEM_NAME}" idlpp_host_system)
idlpp_system_type("${CMAKE_SYSTEM_NAME}" idlpp_system)
set(idlpp_system_version "ignored")
if(idlpp_system STREQUAL "SYS_FREEBSD")
  if("${CMAKE_SYSTEM_VERSION}" MATCHES "^([0-9]+)")
    set(idlpp_system_version "${CMAKE_MATCH_1}")
  endif()
endif()

check_include_file(stdint.h idlpp_have_stdint_h)
check_include_file(inttypes.h idlpp_have_inttypes_h)
have_type("intmax_t" idlpp_have_intmax_t)
have_type("long long" idlpp_have_long_long)
have_symbol("stpcpy" "string.h" idlpp_have_stpcpy)
have_symbol("strlcpy" "string.h" idlpp_have_strlcpy)
have_symbol("strlcat" "string.h" idlpp_have_strlcat)

if(idlpp_have_intmax_t)
  set(idlpp_ll_form "j")
elseif(idlpp_have_long_long)
  set(idlpp_ll_form "ll")
else()
  set(idlpp_ll_form "l")
endif()

set(configed_H "${CMAKE_CURRENT_BINARY_DIR}/src/configed.H")
configure_file("src/configed.H.in" "${configed_H}" @ONLY)

set(headers
    "src/internal.H"
    "src/system.H"
    "src/mcpp_lib.h"
    "src/mcpp_out.h"
    "${configed_H}")

set(sources
    "src/directive.c"
    "src/eval.c"
    "src/expand.c"
    "src/main.c"
    "src/mbchar.c"
    "src/support.c"
    "src/system.c")

# idlpp (mcpp) must not be exported as a library by itself and is therefore
# defined as a static library, to be used by idlc only.
add_library(idlpp STATIC ${sources} ${headers})
target_include_directories(
  idlpp PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
target_compile_definitions(
  idlpp PRIVATE HAVE_CONFIG_H=1)
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  target_compile_definitions(idlpp PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
set_target_properties(idlpp PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
set_target_properties(idlpp PROPERTIES OUTPUT_NAME idlpp)

generate_export_header(
  idlpp
  BASE_NAME mcpp
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/src/mcpp_export.h"
)

