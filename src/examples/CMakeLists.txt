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
#
set(CMAKE_INSTALL_EXAMPLESDIR "${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/examples")

add_subdirectory(helloworld)
add_subdirectory(roundtrip)
add_subdirectory(throughput)

if (USE_DOCS)
    # TODO Move to docs CMakeLists
    set(EXAMPLES_HTML_ARCHIVE "${CMAKE_PROJECT_NAME}ExamplesHTML.tar.gz")
    if (DOWNLOAD_DOCS)
        message(STATUS "${CMAKE_PROJECT_NAME} Examples documentation: Success (download)")
        set(EXAMPLES_HTML_URI "http://jenkins.prismtech.com:8080/job/BuildChameleonLinux64bit/lastSuccessfulBuild/artifact/cham/builds/examples/${EXAMPLES_HTML_ARCHIVE}")
        file(DOWNLOAD "${EXAMPLES_HTML_URI}" "${CMAKE_CURRENT_BINARY_DIR}/Downloaded${EXAMPLES_HTML_ARCHIVE}"
            TIMEOUT 10
            STATUS status)
        list(GET status 0 status_code)
        list(GET status 1 status_string)
        if (NOT status_code EQUAL 0)
            message(FATAL_ERROR
                "Failed to download ${EXAMPLES_HTML_URI} (Code: ${status_code}, ${status_string})")
        endif()

        add_custom_target(examples_docs ALL)
        add_custom_command(TARGET examples_docs
            COMMAND ${CMAKE_COMMAND} -E tar "zxf" "Downloaded${EXAMPLES_HTML_ARCHIVE}"
            VERBATIM)

    elseif (BUILD_DOCS)
        message(STATUS "${CMAKE_PROJECT_NAME} Examples documentation: Success (build)")
        # Process sphinx configuration file
        set(sph_conf_author "ADLINK")
        string(TIMESTAMP sph_conf_copyright "%Y, ADLINK")
        set(sph_conf_version "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
        set(sph_conf_release "${PROJECT_VERSION}")
        configure_file(sphinx-conf.py.in conf.py @ONLY)

        set(builder_output "examples_html_output")
        set(builder_log "sphinx-examples-html.log")
        add_custom_command(OUTPUT ${builder_output}
            COMMAND ${SPHINX_EXECUTABLE}
              -b html
              -d ${CMAKE_CURRENT_BINARY_DIR}/cache
              -c ${CMAKE_CURRENT_BINARY_DIR}
              ${CMAKE_CURRENT_SOURCE_DIR}
              ${CMAKE_CURRENT_BINARY_DIR} > ${builder_log}
            COMMENT "Running Sphinx for examples html output"
            VERBATIM)

        # OUTPUT is a fake target / symbolic name, not an actual file
        set_property(SOURCE ${builder_output} PROPERTY SYMBOLIC 1)
        add_custom_target(examples_docs ALL DEPENDS ${builder_output})

        # Archive the output html files, will become a jenkins artifact for use in other jobs
        # TODO this hardcoded list and archiving should be replaced by ExternalData refs
        set(html_outputs
            "_static"
            "search.html"
            "searchindex.js"
            "genindex.html"
            "examples.html"
            "helloworld/readme.html"
            "roundtrip/readme.html"
            "throughput/readme.html")

        add_custom_command(OUTPUT ${builder_output}
            COMMAND ${CMAKE_COMMAND}
              -E tar "zcf"
              "${EXAMPLES_HTML_ARCHIVE}"
              ${html_outputs}
            APPEND
            VERBATIM)

        # Remove generated files when cleaning the build tree
        set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
            "cache"
            "${builder_log}"
            "objects.inv"
            ${html_outputs}
            "_sources"
            "${EXAMPLES_HTML_ARCHIVE}")
    endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(platform_exclude "examples/helloworld/Makefile")
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(platform_exclude "examples/helloworld/vs|examples/helloworld/HelloWorld\.sln")
else()
  set(platform_exclude "this_is_a_placeholder")
endif()

# Install example source-files
install(
  DIRECTORY "${PROJECT_SOURCE_DIR}/examples/"
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}"
  COMPONENT dev
  PATTERN "CMakeLists.export" EXCLUDE
  PATTERN "examples/CMakeLists.txt" EXCLUDE
  REGEX ${platform_exclude} EXCLUDE
  REGEX "\.rst|\.py" EXCLUDE
)

# Install example html docs files
# TODO this should be replaced by install commands that use ExternalData refs (preferably in examples' CMakeLists.txt)
install(
  DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/"
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}"
  COMPONENT dev
  FILES_MATCHING
  PATTERN "CMakeFiles" EXCLUDE
  PATTERN "cache" EXCLUDE
  PATTERN "_sources" EXCLUDE
  PATTERN "*.html"
  PATTERN "*.js"
  PATTERN "_static/*"
)

