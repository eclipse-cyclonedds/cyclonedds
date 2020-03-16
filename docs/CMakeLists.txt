#
# Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

# These are all various expressions of the configuration schema
get_filename_component(config_c_PATH "../src/core/ddsi/src/q_config.c" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# although the following files are generated, we check them into source control for convenience
get_filename_component(config_rnc_PATH "../etc/cyclonedds.rnc" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(config_xsd_PATH "../etc/cyclonedds.xsd" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component( config_md_PATH "./manual/options.md"   REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(Perl REQUIRED)
add_custom_command(
  OUTPUT
    "${config_md_PATH}" "${config_rnc_PATH}"
  COMMAND
    ${PERL_EXECUTABLE} -w "${CMAKE_CURRENT_SOURCE_DIR}/makernc.pl" "${config_c_PATH}" "${config_md_PATH}" "${config_rnc_PATH}"
  COMMENT "Generating Relax NG schema and Markdown documentation for config options"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/makernc.pl"
    "${config_c_PATH}")
add_custom_target(options_doc ALL DEPENDS "${config_md_PATH}" "${config_rnc_PATH}")

find_program(TRANG_EXECUTABLE trang)
if(NOT TRANG_EXECUTABLE)
  message(FATAL_ERROR "Trang not found - needed for generating xsd files")
endif()
add_custom_command(
  OUTPUT
    cyclonedds.xsd
  COMMAND
    "${TRANG_EXECUTABLE}" -I rnc -O xsd "${config_rnc_PATH}" "${config_xsd_PATH}"
  COMMENT "Translating schema into XSD"
  DEPENDS "${config_rnc_PATH}")
add_custom_target(options_xsd ALL DEPENDS "cyclonedds.xsd")

find_package(Sphinx REQUIRED breathe)
sphinx_add_docs(
  docs
  BREATHE_PROJECTS ddsc_api_docs
  BUILDER html
  SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
add_dependencies(docs options_doc)

install(
  DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  DESTINATION "${CMAKE_INSTALL_DOCDIR}/manual"
  COMPONENT dev)

