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

# This will define the following variables
#
#    OpenSplice_FOUND
#
if(OpenSplice_FOUND)
  return()
endif()

if(NOT "$ENV{OSPL_HOME_NORMALIZED}" STREQUAL "")
  # An OpenSplice source/build tree: include files need to be gathered from all over the place,
  # but that's a small price to pay for not having to install it
  file(TO_CMAKE_PATH "$ENV{OSPL_HOME_NORMALIZED}" OSPL_HOME)
  file(TO_CMAKE_PATH "$ENV{SPLICE_TARGET}" OSPL_SPLICE_TARGET)
  set(OSPL_HOME ${OSPL_HOME} CACHE INTERNAL "")
  set(OSPL_LIB ${OSPL_HOME}/lib/${OSPL_SPLICE_TARGET} CACHE INTERNAL "")
  set(OSPL_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OSPL_HOME}/src/abstraction/os/include
    ${OSPL_HOME}/src/utilities/include
    ${OSPL_HOME}/src/database/database/include
    ${OSPL_HOME}/src/database/serialization/include/
    ${OSPL_HOME}/src/kernel/include
    ${OSPL_HOME}/src/osplcore/bld/${OSPL_SPLICE_TARGET}
    ${OSPL_HOME}/src/kernel/code
    ${OSPL_HOME}/src/user/include
    ${OSPL_HOME}/src/api/dcps/common/include
    CACHE INTERNAL "")
  if(MSVC)
    set(OSPL_INCLUDE_DIRS ${OSPL_INCLUDE_DIRS} "${OSPL_HOME}/src/abstraction/os/win32")
  elseif(APPLE)
    set(OSPL_INCLUDE_DIRS ${OSPL_INCLUDE_DIRS} "${OSPL_HOME}/src/abstraction/os/darwin10")
  else()
    set(OSPL_INCLUDE_DIRS ${OSPL_INCLUDE_DIRS} "${OSPL_HOME}/src/abstraction/os/linux")
  endif()
  set(OSPL_SAC_DIRS
    "${OSPL_HOME}/src/api/dcps/sac/code"
    "${OSPL_HOME}/src/api/dcps/sac/include"
    "${OSPL_HOME}/src/api/dcps/sac/bld/${OSPL_SPLICE_TARGET}"
    CACHE INTERNAL "")
  set(OSPL_IDL_PATH "${OSPL_HOME}/etc/idl" CACHE INTERNAL "")
  set(OSPL_BIN "${OSPL_HOME}/exec/${OSPL_SPLICE_TARGET}" CACHE INTERNAL "")
elseif(NOT "$ENV{OSPL_HOME}" STREQUAL "")
  # An installed version of OpenSplice
  file(TO_CMAKE_PATH "$ENV{OSPL_HOME}" OSPL_HOME)
  set(OSPL_HOME ${OSPL_HOME} CACHE INTERNAL "")
  set(OSPL_LIB ${OSPL_HOME}/lib CACHE INTERNAL "")
  set(OSPL_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${OSPL_HOME}/include"
    "${OSPL_HOME}/include/sys"
    CACHE INTERNAL "")
  set(OSPL_SAC_DIRS "${OSPL_HOME}/include/dcps/C/SAC" CACHE INTERNAL "")
  set(OSPL_IDL_PATH "${OSPL_HOME}/etc/idl" CACHE INTERNAL "")
  set(OSPL_BIN "${OSPL_HOME}/bin" CACHE INTERNAL "")
endif()

# linking /MDd causes an memory alignment error when using a release build of OpenSplice
IF(MSVC)
  SET(CMAKE_CXX_FLAGS_DEBUG "/Md")
ENDIF()

find_library(OSPL_DDSKERNEL_LIBRARY NAMES ddskernel PATHS "${OSPL_LIB}")
set(OSPL_INCLUDE_LIBS ${OSPL_DDSKERNEL_LIBRARY})

find_library(OSPL_SAC_LIB NAMES dcpssac PATHS "${OSPL_LIB}")
find_path(OSPL_INCLUDE_SAC NAMES dds/dds.h PATHS ${OSPL_SAC_DIRS})
if(OSPL_LIB AND NOT (OSPL_SAC_LIB AND OSPL_INCLUDE_SAC))
  message(STATUS "OpenSplice kernel found, but not the C binding")
endif()

find_program(OSPL_IDLPP idlpp "${OSPL_BIN}")
if(OSPL_LIB AND NOT OSPL_IDLPP)
  message(STATUS "OpenSplice kernel/C binding found, but not idlpp")
endif()

if(OSPL_LIB AND OSPL_SAC_LIB AND OSPL_IDLPP)
  set(OpenSplice_FOUND TRUE)
  mark_as_advanced(OpenSplice_FOUND)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenSplice REQUIRED_VARS OSPL_INCLUDE_LIBS)

if(OSPL_LIB AND OSPL_SAC_LIB AND OSPL_IDLPP)
  add_library(OpenSplice::kernel SHARED IMPORTED)
  set_target_properties(OpenSplice::kernel
    PROPERTIES
    IMPORTED_LOCATION ${OSPL_DDSKERNEL_LIBRARY}
    IMPORTED_IMPLIB ${OSPL_DDSKERNEL_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES "${OSPL_INCLUDE_DIRS}")

  add_library(OpenSplice::sac SHARED IMPORTED)
  set_target_properties(OpenSplice::sac
    PROPERTIES
    IMPORTED_LOCATION ${OSPL_SAC_LIB}
    IMPORTED_IMPLIB ${OSPL_SAC_LIB}
    INTERFACE_INCLUDE_DIRECTORIES "${OSPL_SAC_DIRS}"
    INTERFACE_LINK_LIBRARIES OpenSplice::kernel)
endif()

function(osplidl_generate _target)
  if(NOT ARGN)
    message(FATAL_ERROR "idlpp_generate called without any idl files")
  endif()
  set(_ospl_idlfiles "${ARGN}")
  foreach(_ospl_idlfile IN LISTS _ospl_idlfiles)
    get_filename_component(_ospl_idlbasename ${_ospl_idlfile} NAME_WE)
    set(_ospl_idloutput
      "${CMAKE_CURRENT_BINARY_DIR}/ospl/${_ospl_idlbasename}.h"
      "${CMAKE_CURRENT_BINARY_DIR}/ospl/${_ospl_idlbasename}Dcps.h"
      "${CMAKE_CURRENT_BINARY_DIR}/ospl/${_ospl_idlbasename}SacDcps.c"
      "${CMAKE_CURRENT_BINARY_DIR}/ospl/${_ospl_idlbasename}SacDcps.h"
      "${CMAKE_CURRENT_BINARY_DIR}/ospl/${_ospl_idlbasename}SplDcps.c"
      "${CMAKE_CURRENT_BINARY_DIR}/ospl/${_ospl_idlbasename}SplDcps.h")
    add_custom_command(
      OUTPUT  ${_ospl_idloutput}
      COMMAND ${OSPL_IDLPP}
      ARGS    -I "${OSPL_IDL_PATH}" -S -l c -d ${CMAKE_CURRENT_BINARY_DIR}/ospl ${_ospl_idlfile}
      DEPENDS ${_ospl_idlfile})
  endforeach()
  add_custom_target("${_target}_osplidl_generate" DEPENDS "${_ospl_idloutput}")
  set_source_files_properties(${_ospl_idloutput} PROPERTIES GENERATED TRUE)
  add_library(${_target} INTERFACE)
  target_sources(${_target} INTERFACE ${_ospl_idloutput})
  target_include_directories(${_target} INTERFACE "${_dir}")
  add_dependencies(${_target} "${_target}_osplidl_generate")
endfunction()
