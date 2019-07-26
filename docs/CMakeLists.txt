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

# TODO depending on requirements we can add/remove options as needed,
# these are examples of generators we'll need as a minimum.
# Perhaps we should also consider options for building subset of all docs.
# When a certain doc is related to a target, no option is needed; you can simply check if the target exists
# (i.e. if a target 'ddsc' exists, build ddsc api docs). And possibly make the target definition dependent on an option.

find_program(SPHINX_EXECUTABLE NAMES sphinx-build DOC "Sphinx documentation builder")
if (NOT SPHINX_EXECUTABLE)
  message(FATAL_ERROR "${CMAKE_PROJECT_NAME} documentation: unable to find sphinx-build executable")
endif()

find_package(Doxygen)
if (NOT Doxygen_FOUND)
  message(FATAL_ERROR "${CMAKE_PROJECT_NAME} documentation: unable to find Doxygen")
endif()

# Creating pdf from latex requires latexmk (which depends on perl, latexpdf et. al)
find_program(LATEXMK_EXECUTABLE NAMES latexmk DOC "LateX PDF Generator")
if (NOT LATEXMK_EXECUTABLE)
  message(FATAL_ERROR "${CMAKE_PROJECT_NAME} documentation: unable to find latexmk")
endif()

if (SPHINX_EXECUTABLE AND Doxygen_FOUND AND LATEXMK_EXECUTABLE)
  set(BUILD_DOCS ON PARENT_SCOPE) # for examples' docs
  set(BUILD_DOCS ON)
  message(STATUS "${CMAKE_PROJECT_NAME} documentation: Success (build)")
endif()

if(BUILD_DOCS)
  add_subdirectory(manual)
endif()

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  COMPONENT dev)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.pdf
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  COMPONENT dev)
