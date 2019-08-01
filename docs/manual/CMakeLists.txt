#
# Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
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

# Generate ddsc API docs in XML format using Doxygen, if the ddsc target is defined.
# The XML will serve as input for sphinx' breathe plugin
if (TARGET ${CMAKE_PROJECT_NAME}::ddsc)
  # Process doxygen configuration file, for ddsc
  set(doxy_conf_project "${CMAKE_PROJECT_NAME_FULL} C API Documentation")
  set(doxy_conf_outputdir "ddsc_api")
  set(doxy_conf_input "${PROJECT_SOURCE_DIR}/src/core/ddsc/include/dds/dds.h ${PROJECT_SOURCE_DIR}/src/core/ddsc/include/dds")
  configure_file(Doxyfile.in Doxyfile @ONLY)

  add_custom_target(ddsc_docs
    ${DOXYGEN_EXECUTABLE} Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running Doxygen for API docs generation"
    VERBATIM)

  # Remove generated files when cleaning the build tree
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${doxy_conf_outputdir})

  # Add ddsc api docs to sphinx' breathe projects
  set(sph_conf_breathe_projs "\"ddsc_api\": \"${doxy_conf_outputdir}/xml\"")

  add_custom_command(TARGET ddsc_docs
    POST_BUILD
    WORKING_DIRECTORY "${doxy_conf_outputdir}"
    COMMAND ${CMAKE_COMMAND} -E tar "zcf" "${CMAKE_PROJECT_NAME}_C_HTML.tar.gz" "ddsc")
endif()

# Process sphinx configuration file
set(sph_conf_author "Eclipse Cyclone DDS project")
set(sph_conf_version "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(sph_conf_release "${PROJECT_VERSION}")
configure_file(conf.py.in conf.py @ONLY)

# Define a list of output formats (-b option for sphinx-build)
set(docs_builders "")
list(APPEND docs_builders html)
list(APPEND docs_builders latex)

# Define custom commands for running sphinx-build for different docs builders
set(docs_outputs "")
foreach(builder ${docs_builders})
  set(docs_builder_output "docs_${builder}_output")
  # Log stdout (not stderr) to a file instead of messing up build output
  set(docs_builder_log "sphinx-build-${builder}.log")

  add_custom_command(OUTPUT ${docs_builder_output}
    COMMAND ${SPHINX_EXECUTABLE}
    -b ${builder}
    -d ${CMAKE_CURRENT_BINARY_DIR}/cache
    -c ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/docs/manual
    ${CMAKE_CURRENT_BINARY_DIR}/${builder} > ${docs_builder_log}
    COMMENT "Running Sphinx for ${builder} output"
    VERBATIM)

  # FIXME: This is definitely in the wrong location
  if(builder STREQUAL html)
    add_custom_command(OUTPUT ${docs_builder_output}
      COMMAND ${CMAKE_COMMAND}
      -E tar "zcf"
      "${CMAKE_PROJECT_NAME}HTML.tar.gz"
      "html"
      APPEND
      VERBATIM)
  endif()

  # Create a pdf from the latex builder output, by appending a latexmk command
  # TODO look into rinohtype as an alternative (don't bother with rst2pdf, it's no good)
  if(builder STREQUAL latex)
    add_custom_command(OUTPUT ${docs_builder_output}
      COMMAND ${LATEXMK_EXECUTABLE}
      -interaction=nonstopmode
      -silent
      -output-directory=${builder}
      -pdf -dvi- -ps- -cd- ${builder}/${CMAKE_PROJECT_NAME}.tex
      APPEND
      VERBATIM)

    # Move the pdf, so that install rules don't need to differentiate between built and downloaded docs
    add_custom_command(OUTPUT ${docs_builder_output}
      COMMAND ${CMAKE_COMMAND}
      -E rename
      "latex/${CMAKE_PROJECT_NAME}.pdf"
      "${CMAKE_PROJECT_NAME}.pdf"
      APPEND
      VERBATIM)
  endif()

  # OUTPUT is a fake target / symbolic name, not an actual file
  set_property(SOURCE ${docs_builder_output} PROPERTY SYMBOLIC 1)
  # Remove generated files when cleaning the build tree
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    ${builder}
    ${docs_builder_log})

  # Include this builder as a dependency of the general 'docs' target
  list(APPEND docs_outputs ${docs_builder_output})
endforeach()

# Remove generated files when cleaning the build tree
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
  ${CMAKE_PROJECT_NAME}HTML.tar.gz
  ${CMAKE_PROJECT_NAME}.pdf)

add_custom_target(docs ALL DEPENDS ddsc_docs ${docs_outputs})
