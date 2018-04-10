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
# It will generate the Cobertura output from the gcov results.
#
# Example usage:
# $ cmake -DCOVERAGE_SETTINGS=<cham bld>/CoverageSettings.cmake -P <cham src>/cmake/scripts/CoveragePreCobertura.cmake
# $ ctest -T test
# $ ctest -T coverage
# $ ctest -DCOVERAGE_SETTINGS=<cham bld>/CoverageSettings.cmake -P <cham src>/cmake/scripts/CoveragePostCobertura.cmake
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

# Find gcovr to generate Cobertura results
find_program(GCOVR_PATH gcovr PARENT_SCOPE)
if(NOT GCOVR_PATH)
    message(FATAL_ERROR "Could not find gcovr to generate Cobertura coverage.")
endif()

# Create location to put the result file.
file(MAKE_DIRECTORY ${COVERAGE_OUTPUT_DIR})

execute_process(COMMAND ${GCOVR_PATH} -x -r ${COVERAGE_SOURCE_DIR} -e ".*/${COVERAGE_EXCLUDE_TESTS}/.*" -e ".*/${COVERAGE_EXCLUDE_EXAMPLES}/.*" -e ".*/${COVERAGE_EXCLUDE_BUILD_SUPPORT}/.*" -o ${COVERAGE_OUTPUT_DIR}/cobertura.xml
                WORKING_DIRECTORY ${COVERAGE_RUN_DIR})


message(STATUS "The Cobertura report can be found here: ${COVERAGE_OUTPUT_DIR}/cobertura.xml")

