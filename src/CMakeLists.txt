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
cmake_minimum_required(VERSION 3.6)

FUNCTION(PREPEND var prefix)
   SET(listVar "")
   FOREACH(f ${ARGN})
      LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

# Update the git submodules
execute_process(COMMAND git submodule init
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
execute_process(COMMAND git submodule update
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Set module path before defining project so platform files will work.
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")
set(CMAKE_PROJECT_NAME_FULL "Cyclone DDS")
string(REPLACE " " "" PROJECT_NAME "${CMAKE_PROJECT_NAME_FULL}")
project(${PROJECT_NAME} VERSION 0.1.0)

find_package(Abstraction REQUIRED)

# Set some convenience variants of the project-name
string(REPLACE " " "-" CMAKE_PROJECT_NAME_DASHED "${CMAKE_PROJECT_NAME_FULL}")
string(TOUPPER ${CMAKE_PROJECT_NAME} CMAKE_PROJECT_NAME_CAPS)
string(TOLOWER ${CMAKE_PROJECT_NAME} CMAKE_PROJECT_NAME_SMALL)

set(CMAKE_C_STANDARD 99)
if(CMAKE_SYSTEM_NAME STREQUAL "VxWorks")
  add_definitions(-std=c99)
endif()

if(${CMAKE_C_COMPILER_ID} STREQUAL "SunPro")
  add_definitions(-m64)
  add_definitions(-xc99)
  add_definitions(-D__restrict=restrict)
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -m64")
  set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -m64")
endif()

include(FileIDs)
include(GNUInstallDirs)
include(AnalyzeBuild)
# Include Coverage before CTest so that COVERAGE_COMMAND can be modified
# in the Coverage module should that ever be necessary.
include(Coverage)
set(MEMORYCHECK_COMMAND_OPTIONS "--track-origins=yes --leak-check=full --trace-children=yes --child-silent-after-fork=yes --xml=yes --xml-file=TestResultValgrind_%p.xml --tool=memcheck --show-reachable=yes --leak-resolution=high")
set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_CURRENT_LIST_DIR}/valgrind.supp" CACHE FILEPATH "suppression file")

# By default building the testing tree is enabled by including CTest, but
# since not everybody has CUnit and/or Criterion installed, and because it is
# not strictly required to build the product itself, switch to off by default.
option(BUILD_TESTING "Build the testing tree." OFF)
include(CTest)

# Build all executables and libraries into the top-level /bin and /lib folders.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_subdirectory(idlc)
add_subdirectory(os)
add_subdirectory(etc)
add_subdirectory(util)
add_subdirectory(core)
add_subdirectory(tools)
add_subdirectory(scripts)

option(USE_DOCS "Enable documentation." OFF)
if(USE_DOCS)
    add_subdirectory(docs)
else()
    message(STATUS "${CMAKE_PROJECT_NAME} documentation: disabled (-DUSE_DOCS=1 to enable)")
endif()

add_subdirectory(examples)

# Pull-in CPack and support for generating <Package>Config.cmake and packages.
include(Packaging)
