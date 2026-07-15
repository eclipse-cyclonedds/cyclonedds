#
# Copyright(c) 2020 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

include(HashUtilities)

set(confgen_hash_files
  ${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/include/dds/ddsi/ddsi_config.h
  ${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/src/ddsi_config.c
  ${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/src/ddsi__cfgelems.h
  cfgunits.h
  _confgen.h _confgen.c generate_rnc.c generate_md.c generate_rst.c generate_xsd.c generate_defconfig.c
)

check_hashes(
  _confgen_hash_correct
  HASH_FILES
    ${confgen_hash_files}
  APPEND_FILES
    "${generated_defconfig_src}"
)

if (NOT _confgen_hash_correct AND CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR
    "You are crosscompiling but files related to DDS config generation/parsing have been edited."
    "Please rerun CMake in host mode to regenerate the files before attempting a crossbuild"
  )
endif()

if (NOT CMAKE_CROSSCOMPILING)
  add_executable(_confgen-exe
    _confgen.c generate_rnc.c generate_rst.c generate_md.c generate_xsd.c generate_defconfig.c
    ${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/src/ddsi_config.c
  )

  get_cmake_property(_variableNames VARIABLES)
  foreach (_variableName ${_variableNames})
    if(_variableName MATCHES "^ENABLE_([A-Za-z0-9_]+)$")
      target_compile_definitions(_confgen-exe PUBLIC "DDS_HAS_${CMAKE_MATCH_1}=1")
    endif()
  endforeach()

  target_link_libraries(_confgen-exe PRIVATE ddsrt-internal compat)
  target_include_directories(_confgen-exe
    PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../core/ddsc/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/src")

  list(JOIN confgen_hash_files "\;" out_confgen_hash_files)

  function(add_confgen_generate_target target generated_defconfig generated_rnc generated_xsd generated_md generated_rst)
    get_filename_component(_confgen_output_dir "${generated_defconfig}" DIRECTORY)
    add_custom_target(
      ${target}
      COMMENT "Generating configuration-dependent files (defconfig.c, cyclonedds.rnc/xsd and markdown/rst documentation)."
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_confgen_output_dir}"
      COMMAND "$<TARGET_FILE:_confgen-exe>" -f defconfig -o "${generated_defconfig}"
      COMMAND "$<TARGET_FILE:_confgen-exe>" -f rnc -o "${generated_rnc}"
      COMMAND "$<TARGET_FILE:_confgen-exe>" -f xsd -o "${generated_xsd}"
      COMMAND "$<TARGET_FILE:_confgen-exe>" -f md -o "${generated_md}"
      COMMAND "$<TARGET_FILE:_confgen-exe>" -f rst -o "${generated_rst}"
      # Append hashes to defconfig.c
      COMMAND ${CMAKE_COMMAND}
        "-DPREFIX=/*"
        "-DPOSTFIX=*/"
        -DHASH_FILES=${out_confgen_hash_files}
        -DAPPEND_FILES=${generated_defconfig}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/AppendHashScript.cmake
      # Append hashes to rnc
      COMMAND ${CMAKE_COMMAND}
        "-DPREFIX=#"
        "-DPOSTFIX="
        -DHASH_FILES=${out_confgen_hash_files}
        -DAPPEND_FILES=${generated_rnc}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/AppendHashScript.cmake
      # Append hashes to xsd
      COMMAND ${CMAKE_COMMAND}
        "-DPREFIX=<!---"
        "-DPOSTFIX=-->"
        -DHASH_FILES=${out_confgen_hash_files}
        -DAPPEND_FILES=${generated_xsd}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/AppendHashScript.cmake
      # Append hashes to md
      COMMAND ${CMAKE_COMMAND}
        "-DPREFIX=<!---"
        "-DPOSTFIX=-->"
        -DHASH_FILES=${out_confgen_hash_files}
        -DAPPEND_FILES=${generated_md}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/AppendHashScript.cmake
      # Append hashes to rst
      COMMAND ${CMAKE_COMMAND}
        "-DBEFORE=.."
        "-DPREFIX=  "
        "-DPOSTFIX="
        -DHASH_FILES=${out_confgen_hash_files}
        -DAPPEND_FILES=${generated_rst}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/AppendHashScript.cmake
      DEPENDS _confgen-exe
      SOURCES ${confgen_hash_files}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      VERBATIM
    )
  endfunction()

  if (NOT _confgen_hash_correct)
    message(STATUS "Building _confgen because configuration sources were updated.")
    add_confgen_generate_target(
      _confgen
      "${generated_defconfig_src}"
      "${generated_cyclonedds_rnc}"
      "${generated_cyclonedds_xsd}"
      "${generated_options_md}"
      "${generated_options_rst}"
    )
  else()
    add_custom_target(_confgen)
  endif()

  set(_confgen_check_dir "${CMAKE_CURRENT_BINARY_DIR}/generated-source-check/confgen")
  set(_confgen_check_defconfig "${_confgen_check_dir}/defconfig.c")
  set(_confgen_check_rnc "${_confgen_check_dir}/cyclonedds.rnc")
  set(_confgen_check_xsd "${_confgen_check_dir}/cyclonedds.xsd")
  set(_confgen_check_md "${_confgen_check_dir}/options.md")
  set(_confgen_check_rst "${_confgen_check_dir}/config_file_reference.rst")
  add_confgen_generate_target(
    _confgen_check_generate
    "${_confgen_check_defconfig}"
    "${_confgen_check_rnc}"
    "${_confgen_check_xsd}"
    "${_confgen_check_md}"
    "${_confgen_check_rst}"
  )
  add_custom_target(
    _confgen_check
    COMMENT "Checking checked-in configuration generator output."
    COMMAND ${CMAKE_COMMAND}
      "-DGENERATED_FILE=${_confgen_check_defconfig}"
      "-DCHECKED_IN_FILE=${generated_defconfig_src}"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/CheckGeneratedFile.cmake
    COMMAND ${CMAKE_COMMAND}
      "-DGENERATED_FILE=${_confgen_check_rnc}"
      "-DCHECKED_IN_FILE=${generated_cyclonedds_rnc}"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/CheckGeneratedFile.cmake
    COMMAND ${CMAKE_COMMAND}
      "-DGENERATED_FILE=${_confgen_check_xsd}"
      "-DCHECKED_IN_FILE=${generated_cyclonedds_xsd}"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/CheckGeneratedFile.cmake
    COMMAND ${CMAKE_COMMAND}
      "-DGENERATED_FILE=${_confgen_check_md}"
      "-DCHECKED_IN_FILE=${generated_options_md}"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/CheckGeneratedFile.cmake
    COMMAND ${CMAKE_COMMAND}
      "-DGENERATED_FILE=${_confgen_check_rst}"
      "-DCHECKED_IN_FILE=${generated_options_rst}"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/CheckGeneratedFile.cmake
    DEPENDS _confgen_check_generate
    VERBATIM
  )
else()
  add_custom_target(_confgen)
endif()
