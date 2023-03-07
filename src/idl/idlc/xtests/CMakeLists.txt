#
# Copyright(c) 2006 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

set(_sources
  test_alias.idl
  test_basic.idl
  test_union.idl
  test_typedef_member.idl
  test_struct_inherit.idl
  test_struct_inherit_appendable.idl
  test_struct_inherit_mutable.idl
  test_struct_external.idl
  test_union_external.idl
  test_optional.idl
  test_optional_mutable.idl
  test_struct_alignment.idl
  test_struct_r.idl
  test_union_member_types_r.idl
  test_union_r.idl
  test_bounded_seq.idl
  test_enum.idl
  test_bool.idl
  test_bitmask.idl
  test_struct_keys.idl
  test_struct_inherit_mutable_keys.idl
)

set(_includes "${_includes}\\;${CMAKE_SOURCE_DIR}/src/core/cdr/include")
set(_includes "${_includes}\\;${CMAKE_SOURCE_DIR}/src/core/ddsc/include")
set(_includes "${_includes}\\;${CMAKE_SOURCE_DIR}/src/core/ddsi/include")
set(_includes "${_includes}\\;${CMAKE_SOURCE_DIR}/src/ddsrt/include")
set(_includes "${_includes}\\;${CMAKE_BINARY_DIR}/src/ddsrt/include")
set(_includes "${_includes}\\;${CMAKE_BINARY_DIR}/src/core/include")
set(_includes "${_includes}\\;${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_VERBOSE_MAKEFILE ON)

if(MSVC)
  if(CMAKE_CL_64)
    if(MSVC_VERSION GREATER 1599)
      # VS 10 and later:
      set(CMAKE_MSVC_ARCH x64)
    else()
      # VS 9 and earlier:
      set(CMAKE_MSVC_ARCH amd64)
    endif()
  else()
    set(CMAKE_MSVC_ARCH x86)
  endif()

  # Add the include dir from the Windows Kits directory
  # code from https://gitlab.kitware.com/cmake/cmake/-/blob/v3.17.3/Modules/InstallRequiredSystemLibraries.cmake
  get_filename_component(windows_kits_dir
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE)
  set(programfilesx86 "ProgramFiles(x86)")
  if(";${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION};$ENV{UCRTVersion};$ENV{WindowsSDKVersion};" MATCHES [=[;(10\.[0-9.]+)[;\]]=])
    set(__ucrt_version "${CMAKE_MATCH_1}/")
  else()
    set(__ucrt_version "")
  endif()
  find_path(WINDOWS_KITS_DIR
    NAMES
      Redist/${__ucrt_version}ucrt/DLLs/${CMAKE_MSVC_ARCH}/ucrtbase.dll
      Redist/ucrt/DLLs/${CMAKE_MSVC_ARCH}/ucrtbase.dll
    PATHS
      $ENV{CMAKE_WINDOWS_KITS_10_DIR}
      "${windows_kits_dir}"
      "$ENV{ProgramFiles}/Windows Kits/10"
      "$ENV{${programfilesx86}}/Windows Kits/10"
    )
  set(_windows_kit_includes "${WINDOWS_KITS_DIR}/include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
  set(_windows_kit_libs "${WINDOWS_KITS_DIR}/lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")

  # get msvc root
  get_filename_component(_msvc_dir ${CMAKE_C_COMPILER} DIRECTORY)
  set(_msvc_dir "${_msvc_dir}/../../..")

  # Add include directory VC/Tools/MSVC/[version]/include
  set(_includes "${_includes}\\;${_msvc_dir}/include")
  set(_includes "${_includes}\\;${_windows_kit_includes}/ucrt")
  set(_includes "${_includes}\\;${_windows_kit_includes}/um")
  set(_includes "${_includes}\\;${_windows_kit_includes}/shared")

  # Add library paths
  set(_libs "${_msvc_dir}/lib/${CMAKE_MSVC_ARCH}")
  set(_libs "${_libs}\\;${_windows_kit_libs}/ucrt/${CMAKE_MSVC_ARCH}")
  set(_libs "${_libs}\\;${_windows_kit_libs}/um/${CMAKE_MSVC_ARCH}")
endif()

if(ENABLE_TYPE_DISCOVERY)
  set(_type_meta 1)
endif()

set(_dir "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(_source ${_sources})
  set(_source "${_dir}/${_source}")
  get_filename_component(_base "${_source}" NAME_WE)
  add_test(
    NAME "idlc_${_base}"
    COMMAND ${CMAKE_COMMAND} -P "${_dir}/Compile.cmake" ${_source})
  set_property(
      TEST "idlc_${_base}"
      APPEND PROPERTY ENVIRONMENT
        "CC=${CMAKE_C_COMPILER}"
        "IDLC=$<TARGET_FILE:CycloneDDS::idlc>"
        "MAINC=${CMAKE_CURRENT_SOURCE_DIR}/main.c"
        "TEST_INCLUDE_PATHS=${_includes}"
        "TEST_LIB_PATHS=${_libs}"
        "CDDS_LIB_PATH=$<TARGET_LINKER_FILE_DIR:CycloneDDS::ddsc>"
        "CDDS_BIN_PATH=$<TARGET_FILE_DIR:CycloneDDS::ddsc>"
        "SANITIZER=${SANITIZER}"
        "HAS_TYPE_META=${_type_meta}")
  if(APPLE)
    set_property(
        TEST "idlc_${_base}"
        APPEND PROPERTY ENVIRONMENT
          "OSX_SYSROOT=${CMAKE_OSX_SYSROOT}")
  endif()
endforeach()
