#
# Copyright(c) 2020 ADLINK Technology Limited and others
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
  ${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/include/dds/ddsi/ddsi_cfgunits.h
  ${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/include/dds/ddsi/ddsi_cfgelems.h
  ${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/src/ddsi_config.c
  _confgen.h _confgen.c generate_rnc.c generate_md.c generate_xsd.c generate_defconfig.c
)

check_hashes(
  _confgen_hash_correct
  HASH_FILES
    ${confgen_hash_files}
  APPEND_FILES
    "${generated_defconfig_src}"
)

if (NOT ${_confgen_hash_correct})
  if (CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
      "You are crosscompiling but files related to DDS config generation/parsing have been edited."
      "Please rerun CMake in host mode to regenerate the files before attempting a crossbuild"
    )
  else()
    message(STATUS "Building _confgen and regenerating defconfig.c, options.md, cyclonedds.rnc and cyclonedds.xsd.")
  endif()

  add_executable(_confgen-exe
    _confgen.c generate_rnc.c generate_md.c generate_xsd.c generate_defconfig.c
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
    "${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/include")

  list(JOIN confgen_hash_files "\;" out_confgen_hash_files)

  add_custom_target(
    _confgen
    COMMAND _confgen-exe -f defconfig -o "${generated_defconfig_src}"
    COMMAND _confgen-exe -f rnc -o "${generated_cyclonedds_rnc}"
    COMMAND _confgen-exe -f xsd -o "${generated_cyclonedds_xsd}"
    COMMAND _confgen-exe -f md -o "${generated_options_md}"
    # Append hashes to defconfig.c
    COMMAND ${CMAKE_COMMAND}
      "-DPREFIX=/*"
      "-DPOSTFIX=*/"
      -DHASH_FILES=${out_confgen_hash_files}
      -DAPPEND_FILES=${generated_defconfig_src}
      -P ${CMAKE_SOURCE_DIR}/cmake/AppendHashScript.cmake
    # Append hashes to rnc
    COMMAND ${CMAKE_COMMAND}
      "-DPREFIX=#"
      "-DPOSTFIX="
      -DHASH_FILES=${out_confgen_hash_files}
      -DAPPEND_FILES=${generated_cyclonedds_rnc}
      -P ${CMAKE_SOURCE_DIR}/cmake/AppendHashScript.cmake
    # Append hashes to xsd
    COMMAND ${CMAKE_COMMAND}
      "-DPREFIX=<!---"
      "-DPOSTFIX=-->"
      -DHASH_FILES=${out_confgen_hash_files}
      -DAPPEND_FILES=${generated_cyclonedds_xsd}
      -P ${CMAKE_SOURCE_DIR}/cmake/AppendHashScript.cmake
    # Append hashes to md
    COMMAND ${CMAKE_COMMAND}
      "-DPREFIX=<!---"
      "-DPOSTFIX=-->"
      -DHASH_FILES=${out_confgen_hash_files}
      -DAPPEND_FILES=${generated_options_md}
      -P ${CMAKE_SOURCE_DIR}/cmake/AppendHashScript.cmake
    DEPENDS _confgen
    SOURCES ${confgen_hash_files}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
  )
else()
  add_custom_target(
    _confgen
  )
endif()
