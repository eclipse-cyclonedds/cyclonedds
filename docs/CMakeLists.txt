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

# although the following files are generated, they are checked into source
# control for convenience, but the user must copy them manually
set(cyclonedds_rnc "${CMAKE_CURRENT_BINARY_DIR}/cyclonedds.rnc")
set(cyclonedds_xsd "${CMAKE_CURRENT_BINARY_DIR}/cyclonedds.xsd")
set(options_md "${CMAKE_CURRENT_BINARY_DIR}/manual/options.md")

if(BUILD_SCHEMA OR BUILD_DOCS)
  add_custom_command(
    OUTPUT "${cyclonedds_rnc}" "${cyclonedds_xsd}" "${options_md}"
    COMMAND ddsconf ARGS -f rnc -o "${cyclonedds_rnc}"
    COMMAND ddsconf ARGS -f xsd -o "${cyclonedds_xsd}"
    COMMAND ${CMAKE_COMMAND} -E make_directory manual
    COMMAND ddsconf ARGS -f md -o "${options_md}"
    DEPENDS ddsconf)
  add_custom_target(
    schema ALL DEPENDS "${cyclonedds_rnc}" "${cyclonedds_xsd}" "${options_md}")
endif()

if(BUILD_DOCS)
  find_package(Sphinx REQUIRED breathe)
  sphinx_add_docs(
    docs
    BREATHE_PROJECTS ddsc_api_docs
    BUILDER html
    SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/manual")
  add_dependencies(docs schema)

  install(
    DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/manual"
    COMPONENT dev)
endif()
