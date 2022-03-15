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

if(BUILD_DOCS)
  find_package(Sphinx REQUIRED breathe exhale)
  sphinx_add_docs(
    docs
    BREATHE_PROJECTS ddsc_api_docs
    BUILDER html
    SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/manual")

  install(
    DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/manual"
    COMPONENT dev)
endif()

install(
  FILES ${CMAKE_CURRENT_SOURCE_DIR}/../CONTRIBUTING.md
        ${CMAKE_CURRENT_SOURCE_DIR}/../CYCLONEDDS_QUALITY_DECLARATION.md
        ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE
        ${CMAKE_CURRENT_SOURCE_DIR}/../NOTICE.md
        ${CMAKE_CURRENT_SOURCE_DIR}/../README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/../CHANGELOG.rst
  DESTINATION ${CMAKE_INSTALL_DOCDIR})
