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

# Set a default build type if none was specified
set(default_build_type "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

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

# Conan
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
  include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  if(APPLE)
    # By default Conan strips all RPATHs (see conanbuildinfo.cmake), which
    # causes tests to fail as the executables cannot find the library target.
    # By setting KEEP_RPATHS, Conan does not set CMAKE_SKIP_RPATH and the
    # resulting binaries still have the RPATH information. This is fine because
    # CMake will strip the build RPATH information in the install step.
    #
    # NOTE:
    # Conan's default approach is to use the "imports" feature, which copies
    # all the dependencies into the bin directory. Of course, this doesn't work
    # quite that well for libraries generated in this Project (see Conan
    # documentation).
    #
    # See the links below for more information.
    # https://github.com/conan-io/conan/issues/337
    # https://docs.conan.io/en/latest/howtos/manage_shared_libraries/rpaths.html
    # https://gitlab.kitware.com/cmake/community/wikis/doc/cmake/RPATH-handling
    conan_basic_setup(KEEP_RPATHS)
  else()
    conan_basic_setup()
  endif()
endif()

# Set reasonably strict warning options for clang, gcc, msvc
# Enable coloured ouput if Ninja is used for building
if(${CMAKE_C_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_C_COMPILER_ID} STREQUAL "AppleClang")
  add_definitions(-Wall -Wextra -Wconversion -Wunused)
  if(${CMAKE_GENERATOR} STREQUAL "Ninja")
    add_definitions(-Xclang -fcolor-diagnostics)
  endif()
elseif(${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
  add_definitions(-Wall -Wextra -Wconversion)
  if(${CMAKE_GENERATOR} STREQUAL "Ninja")
    add_definitions(-fdiagnostics-color=always)
  endif()
elseif(${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
  add_definitions(/W3)
endif()

# Make it easy to enable one of Clang's/gcc's analyzers, and default to using
# the address sanitizer for ordinary debug builds; gcc is giving some grief on
# Travis, so don't enable it for gcc by default
if(NOT USE_SANITIZER)
  if(${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND
      NOT (${CMAKE_GENERATOR} STREQUAL "Xcode") AND
      (${CMAKE_C_COMPILER_ID} STREQUAL "Clang"
        OR ${CMAKE_C_COMPILER_ID} STREQUAL "AppleClang"))
    message(STATUS "Enabling address sanitizer; set USE_SANITIZER=none to prevent this")
    set(USE_SANITIZER address)
  else()
    set(USE_SANITIZER none)
  endif()
endif()
if(NOT (${USE_SANITIZER} STREQUAL "none"))
  message(STATUS "Sanitizer set to ${USE_SANITIZER}")
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=${USE_SANITIZER}")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=${USE_SANITIZER}")
  set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=${USE_SANITIZER}")
endif()

include(FileIDs)
include(GNUInstallDirs)
include(AnalyzeBuild)
# Include Coverage before CTest so that COVERAGE_COMMAND can be modified
# in the Coverage module should that ever be necessary.
include(Coverage)
set(MEMORYCHECK_COMMAND_OPTIONS "--track-origins=yes --leak-check=full --trace-children=yes --child-silent-after-fork=yes --xml=yes --xml-file=TestResultValgrind_%p.xml --tool=memcheck --show-reachable=yes --leak-resolution=high")

# By default building the testing tree is enabled by including CTest, but
# since not everybody has CUnit, and because it is not strictly required to
# build the product itself, switch to off by default.
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
