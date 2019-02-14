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

option(BUILD_DOCS "Build documentation." OFF)
option(DOWNLOAD_DOCS "Download documentation." OFF)

# When BUILD_DOCS is set, missing deps are treated as fatal errors
if (BUILD_DOCS)
    set(mode FATAL_ERROR)
else()
    set(mode STATUS)
endif()

find_program(SPHINX_EXECUTABLE NAMES sphinx-build DOC "Sphinx documentation builder")
if (NOT SPHINX_EXECUTABLE)
    message(${mode} "${CMAKE_PROJECT_NAME} documentation: unable to find sphinx-build executable")
endif()

find_package(Doxygen)
if (NOT Doxygen_FOUND)
    message(${mode} "${CMAKE_PROJECT_NAME} documentation: unable to find Doxygen")
endif()

# Creating pdf from latex requires latexmk (which depends on perl, latexpdf et. al)
find_program(LATEXMK_EXECUTABLE NAMES latexmk DOC "LateX PDF Generator")
if (NOT LATEXMK_EXECUTABLE)
    message(${mode} "${CMAKE_PROJECT_NAME} documentation: unable to find latexmk")
endif()

if ((NOT DOWNLOAD_DOCS) AND SPHINX_EXECUTABLE AND Doxygen_FOUND AND LATEXMK_EXECUTABLE)
    # User requested docs (USE_DOCS=1) and did not explicitely request to download docs (DOWNLOAD_DOCS=0)
    # All prerequisites are available to build docs, so force BUILD_DOCS even when the user did not enable it explicitely
    set(BUILD_DOCS ON PARENT_SCOPE) # for examples' docs
    set(BUILD_DOCS ON)
    message(STATUS "${CMAKE_PROJECT_NAME} documentation: Success (build)")
else()
    # User requested docs (USE_DOCS=1) and prefers to download instead of build (or prerequisites are not available).
    # So force DOWNLOAD_DOCS even when user did not enable it explicitely
    set(DOWNLOAD_DOCS ON PARENT_SCOPE) # for examples' docs
    set(DOWNLOAD_DOCS ON)
    message(STATUS "${CMAKE_PROJECT_NAME} documentation: Success (download)")
endif()

#set(DOCS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/..")

if (DOWNLOAD_DOCS)
    set(JENKINS_BASE_URI "http://jenkins.prismtech.com:8080/")
    set(JENKINS_DOCS_JOB_NAME "BuildChameleonLinux64bit")
    set(PROJECT_PDF_URI "${JENKINS_BASE_URI}/job/${JENKINS_DOCS_JOB_NAME}/lastSuccessfulBuild/artifact/cham/builds/docs/${CMAKE_PROJECT_NAME}.pdf")
    set(PROJECT_HTML_URI "${JENKINS_BASE_URI}/job/${JENKINS_DOCS_JOB_NAME}/lastSuccessfulBuild/artifact/cham/builds/docs/${CMAKE_PROJECT_NAME}HTML.tar.gz")

    add_custom_target(docs ALL)
    find_program(WGET_EXECUTABLE NAMES wget DOC "wget")
    if (WGET_EXECUTABLE)
        # prevent wget to create numbered downloads.
        add_custom_command(TARGET docs
            COMMAND ${CMAKE_COMMAND}
              -E remove -f "${CMAKE_PROJECT_NAME}HTML.tar.gz"
            VERBATIM)

        add_custom_command(TARGET docs
            COMMAND ${WGET_EXECUTABLE}
              -q ${PROJECT_HTML_URI} ${PROJECT_PDF_URI}
            COMMENT "Downloading documentation from target."
            VERBATIM)

        # To make downloading and packaging easier.
#        add_custom_command(TARGET docs
#            COMMAND ${CMAKE_COMMAND}
#              -E rename ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.pdf ${DOCS_OUTPUT_DIR}/${CMAKE_PROJECT_NAME}.pdf
#            VERBATIM)
    else()
        message(STATUS "Unable to find wget. Download docs now.")
        # Just try to download the docs straight away.
        file(DOWNLOAD ${PROJECT_HTML_URI} ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}HTML.tar.gz)
        file(DOWNLOAD ${PROJECT_PDF_URI} ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.pdf)
    endif()

    add_custom_command(TARGET docs
        COMMAND ${CMAKE_COMMAND}
          -E tar "zxf" "${CMAKE_PROJECT_NAME}HTML.tar.gz" .
        VERBATIM)

    # Remove downloaded files when cleaning the build tree
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
        html
        ${CMAKE_PROJECT_NAME}HTML.tar.gz
        ${CMAKE_PROJECT_NAME}.pdf)

elseif(BUILD_DOCS)
    # Generate ddsc API docs in XML format using Doxygen, if the ddsc target is defined.
    # The XML will serve as input for sphinx' breathe plugin
    if (TARGET ${CMAKE_PROJECT_NAME}::ddsc)
        # Process doxygen configuration file, for ddsc
        set(doxy_conf_project "${CMAKE_PROJECT_NAME_FULL} C API Documentation")
        set(doxy_conf_outputdir "ddsc_api")
        set(doxy_conf_input "${PROJECT_SOURCE_DIR}/core/ddsc/include/ddsc/dds.h ${PROJECT_SOURCE_DIR}/core/ddsc/include/ddsc")
        configure_file(Doxyfile.in Doxyfile @ONLY)

        add_custom_target(ddsc_docs
          ${DOXYGEN_EXECUTABLE} Doxyfile
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
          COMMENT "Running Doxygen for API docs generation"
          VERBATIM
        )

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
              ${PROJECT_SOURCE_DIR}/docs
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
endif()

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT dev)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.pdf
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT dev)
