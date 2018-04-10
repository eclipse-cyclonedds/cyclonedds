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

#
# This script assumes that all test have been run and gcov results are available.
# It will generate the HTML output from the gcov results.
#
# Example usage:
# $ cmake -DCOVERAGE_SETTINGS=<cham bld>/CoverageSettings.cmake -P <cham src>/cmake/scripts/CoveragePreHtml.cmake
# $ ctest -T test
# $ ctest -T coverage
# $ ctest -DCOVERAGE_SETTINGS=<cham bld>/CoverageSettings.cmake -P <cham src>/cmake/scripts/CoveragePostHtml.cmake
# If you start the scripts while in <cham bld> then you don't have to provide the COVERAGE_SETTINGS file.
#
cmake_minimum_required(VERSION 3.5)

# Get Coverage configuration file
if(NOT COVERAGE_SETTINGS)
    set(COVERAGE_SETTINGS ${CMAKE_CURRENT_BINARY_DIR}/CoverageSettings.cmake)
endif()
include(${COVERAGE_SETTINGS})

# Some debug
#message(STATUS "Config file:      ${COVERAGE_SETTINGS}")
#message(STATUS "Source directory: ${COVERAGE_SOURCE_DIR}")
#message(STATUS "Test directory:   ${COVERAGE_RUN_DIR}")
#message(STATUS "Output directory: ${COVERAGE_OUTPUT_DIR}")

# Find tools to generate HTML coverage results
find_program(LCOV_PATH lcov PARENT_SCOPE)
if(NOT LCOV_PATH)
    message(FATAL_ERROR "Could not find lcov to generate HTML coverage.")
endif()
find_program(GENHTML_PATH genhtml PARENT_SCOPE)
if(NOT GENHTML_PATH)
    message(FATAL_ERROR "Could not find genhtml to generate HTML coverage.")
endif()

# Create location to put the result file.
file(MAKE_DIRECTORY ${COVERAGE_OUTPUT_DIR})
set(COVERAGE_HTML_OUTPUT  "${COVERAGE_OUTPUT_DIR}/html")
file(MAKE_DIRECTORY ${COVERAGE_HTML_OUTPUT})

# Setup tmp analysis files
set(COVERAGE_INFO    "${COVERAGE_HTML_OUTPUT}/coverage_html.info")
set(COVERAGE_CLEANED "${COVERAGE_INFO}.cleaned")

# Execute lcov and genhtml commands to get HTML results
execute_process(COMMAND ${LCOV_PATH} ${COVERAGE_QUIET_FLAG} --directory . --capture --output-file ${COVERAGE_INFO}
                WORKING_DIRECTORY ${COVERAGE_RUN_DIR})
execute_process(COMMAND ${LCOV_PATH} ${COVERAGE_QUIET_FLAG} --remove ${COVERAGE_INFO} "${COVERAGE_EXCLUDE_TESTS}/*" "${COVERAGE_EXCLUDE_EXAMPLES}/*" "${COVERAGE_EXCLUDE_BUILD_SUPPORT}/*" "/usr/*" --output-file ${COVERAGE_CLEANED}
                WORKING_DIRECTORY ${COVERAGE_RUN_DIR})
execute_process(COMMAND ${GENHTML_PATH}  ${COVERAGE_QUIET_FLAG} -o ${COVERAGE_HTML_OUTPUT} ${COVERAGE_CLEANED}
                WORKING_DIRECTORY ${COVERAGE_RUN_DIR})

# Remove tmp analysis files
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${COVERAGE_INFO} ${COVERAGE_CLEANED}
                WORKING_DIRECTORY ${COVERAGE_RUN_DIR})


message(STATUS "The HTML coverage report can be found here: ${COVERAGE_HTML_OUTPUT}/index.html")

