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
# This script will run all tests and generates various coverage reports.
#
# Example usage:
# $ cmake -DCOVERAGE_SETTINGS=<cham bld>/CoverageSettings.cmake -P <cham src>/cmake/scripts/CoverageConvenience.cmake
# If you start the scripts while in <cham bld> then you don't have to provide the COVERAGE_SETTINGS file.
#
cmake_minimum_required(VERSION 3.5)

# Get Coverage configuration file
if(NOT COVERAGE_SETTINGS)
    set(COVERAGE_SETTINGS ${CMAKE_CURRENT_BINARY_DIR}/CoverageSettings.cmake)
endif()
include(${COVERAGE_SETTINGS})

message(STATUS "Config file:      ${COVERAGE_SETTINGS}")
message(STATUS "Source directory: ${COVERAGE_SOURCE_DIR}")
message(STATUS "Test directory:   ${COVERAGE_RUN_DIR}")
message(STATUS "Output directory: ${COVERAGE_OUTPUT_DIR}")

set(COVERAGE_SCRIPTS_DIR "${COVERAGE_SOURCE_DIR}/cmake/scripts")

###############################################################################
#
# Detect generators
#
###############################################################################
set(GENERATE_COVERAGE TRUE)
if(GENERATE_COVERAGE)
    find_program(GCOV_PATH gcov PARENT_SCOPE)
    if(NOT GCOV_PATH)
        set(GENERATE_COVERAGE FALSE)
        message(STATUS "[SKIP] Coverage generators - gcov (could not find gcov)")
    endif()
endif()
if(GENERATE_COVERAGE)
    message(STATUS "[ OK ] Coverage generators - gcov")
endif()

set(GENERATE_COVERAGE_HTML TRUE)
if(GENERATE_COVERAGE_HTML)
    find_program(LCOV_PATH lcov PARENT_SCOPE)
    if(NOT LCOV_PATH)
        set(GENERATE_COVERAGE_HTML FALSE)
        message(STATUS "[SKIP] Coverage generators - HTML (could not find lcov)")
    endif()
endif()
if(GENERATE_COVERAGE_HTML)
    find_program(GENHTML_PATH genhtml PARENT_SCOPE)
    if(NOT GENHTML_PATH)
        set(GENERATE_COVERAGE_HTML FALSE)
        message(STATUS "[SKIP] Coverage generators - HTML (could not find genhtml)")
    endif()
endif()
if(GENERATE_COVERAGE_HTML)
    message(STATUS "[ OK ] Coverage generators - HTML (lcov and genhtml)")
endif()

set(GENERATE_COVERAGE_COBERTURA TRUE)
if(GENERATE_COVERAGE_COBERTURA)
    find_program(GCOVR_PATH gcovr PARENT_SCOPE)
    if(NOT GCOVR_PATH)
        set(GENERATE_COVERAGE_COBERTURA FALSE)
        message(STATUS "[SKIP] Coverage generators - Cobertura (could not find gcovr)")
    endif()
endif()
if(GENERATE_COVERAGE_COBERTURA)
    message(STATUS "[ OK ] Coverage generators - Cobertura (gcovr)")
endif()

if(NOT GENERATE_COVERAGE)
    message(FATAL_ERROR "Could not find the main coverage generator 'gcov'")
elseif(NOT GENERATE_COVERAGE_HTML AND NOT GENERATE_COVERAGE_COBERTURA)
    message(FATAL_ERROR "Could not find either of the two coverage report generators")
endif()



###############################################################################
#
# Setup environment
#
###############################################################################
message(STATUS "Setup environment")
if(GENERATE_COVERAGE_HTML)
    execute_process(COMMAND ${CMAKE_COMMAND} -DCOVERAGE_SETTINGS=${COVERAGE_SETTINGS} -P ${COVERAGE_SCRIPTS_DIR}/CoveragePreHtml.cmake
                    WORKING_DIRECTORY ${COVERAGE_RUN_DIR})
endif()
if(GENERATE_COVERAGE_COBERTURA)
    execute_process(COMMAND ${CMAKE_COMMAND} -DCOVERAGE_SETTINGS=${COVERAGE_SETTINGS} -P ${COVERAGE_SCRIPTS_DIR}/CoveragePreCobertura.cmake
                    WORKING_DIRECTORY ${COVERAGE_RUN_DIR})
endif()



###############################################################################
#
# Generate coverage results by running all the tests
#
###############################################################################
message(STATUS "Run all test to get coverage")
execute_process(COMMAND ctest ${COVERAGE_QUIET_FLAG} -T test
                WORKING_DIRECTORY ${COVERAGE_RUN_DIR})
execute_process(COMMAND ctest ${COVERAGE_QUIET_FLAG} -T coverage
                WORKING_DIRECTORY ${COVERAGE_RUN_DIR})



###############################################################################
#
# Generate coverage reports
#
###############################################################################
if(GENERATE_COVERAGE_HTML)
    execute_process(COMMAND ${CMAKE_COMMAND} -DCOVERAGE_SETTINGS=${COVERAGE_SETTINGS} -P ${COVERAGE_SCRIPTS_DIR}/CoveragePostHtml.cmake
                    WORKING_DIRECTORY ${COVERAGE_RUN_DIR})
endif()
if(GENERATE_COVERAGE_COBERTURA)
    execute_process(COMMAND ${CMAKE_COMMAND} -DCOVERAGE_SETTINGS=${COVERAGE_SETTINGS} -P ${COVERAGE_SCRIPTS_DIR}/CoveragePostCobertura.cmake
                    WORKING_DIRECTORY ${COVERAGE_RUN_DIR})
endif()

