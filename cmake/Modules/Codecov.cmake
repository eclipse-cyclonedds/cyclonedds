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

# This module is meant to be used in conjuntion with CMake-codecov
# https://github.com/RWTH-HPC/CMake-codecov
#
# A codecov target is added if ENABLE_COVERAGE is on.
# Generate a codecov.io submission file with cmake --build . --target codecov.
# Submit the file to codecov.io by putting the proper commands in .travis.yml.
# See https://codecov.io/bash for details.

if(ENABLE_COVERAGE)
  if(NOT TARGET gcov)
    add_custom_target(gcov)
  endif()
  if(NOT TARGET codecov)
    set(CODECOV_FILE codecov.dump)
    set(CODECOV_ARCHIVE codecov.tar.gz)
    add_custom_target(codecov)
    add_dependencies(codecov gcov)
    add_custom_command(
      TARGET codecov
      POST_BUILD
      BYPRODUCTS
        "${CMAKE_BINARY_DIR}/${CODECOV_FILE}"
        "${CMAKE_BINARY_DIR}/${CODECOV_ARCHIVE}"
      COMMAND ${CMAKE_COMMAND} ARGS
        -DPROJECT_ROOT="${CMAKE_SOURCE_DIR}"
        -DCODECOV_FILE="${CODECOV_FILE}"
        -P ${CMAKE_CURRENT_LIST_DIR}/Codecov/codecov.cmake
      COMMAND ${CMAKE_COMMAND} ARGS
        -E tar czf ${CODECOV_ARCHIVE} -- ${CODECOV_FILE}
        COMMENT "Generating ${CODECOV_ARCHIVE}"
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  endif()
endif()
