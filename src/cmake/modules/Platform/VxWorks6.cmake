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
# CMake Platform file for VxWorks
#
# This file will be used as platform file if CMAKE_SYSTEM_NAME is defined
# as VxWorks in the toolchain file.
#
# Most information is resolved by analyzing the absolute location of the
# compiler on the file system, but can be overridden if required.
#
# Setting CMAKE_SYSTEM_PROCESSOR is mandatory. The variable should be set to
# e.g. ARMARCH* if the target architecture is arm.
#
# NOTES:
#  * For now support for VxWorks Diab compiler will NOT be implemented.
#  * If certain settings are not explicitly defined, this platform file will
#    try to deduce it from the installation path. It will, however, not go out
#    of it's way to validate and cross-reference settings.
#
# https://cmake.org/Wiki/CMake_Cross_Compiling
#

if((NOT "${CMAKE_GENERATOR}" MATCHES "Makefiles") AND
   (NOT "${CMAKE_GENERATOR}" MATCHES "Ninja"))
    message(FATAL_ERROR "Cross compilation for VxWorks is not supported for "
                        "${CMAKE_GENERATOR}")
endif()

set(WIND_PROCESSOR_TYPE_PATTERN ".*(cc|c\\+\\+)(arm|mips|pentium|ppc).*")
set(WIND_HOST_TYPE_PATTERN "^x86-(linux2|win32)$")
set(WIND_PLATFORM_PATTERN "^[0-9\.]+-vxworks-([0-9\.]+)$")

# Try to deduce the system architecture from either CMAKE_C_COMPILER or
# CMAKE_CXX_COMPILER (one of which must be specified).
#
# Path examples:
# <WindRiver>/gnu/4.3.3-vxworks-6.9/x86-linux2/bin
# <WindRiver>/gnu/4.1.2-vxworks-6.8/x86-win32/bin
foreach(COMPILER CMAKE_C_COMPILER CMAKE_CXX_COMPILER)
    if("${${COMPILER}}" MATCHES "${WIND_PROCESSOR_TYPE_PATTERN}")
        string(
            REGEX REPLACE
            "${WIND_PROCESSOR_TYPE_PATTERN}" "\\2"
            PROCESSOR_TYPE
            ${${COMPILER}})
        if(NOT WIND_PROCESSOR_TYPE)
            set(WIND_PROCESSOR_TYPE ${PROCESSOR_TYPE})
        endif()

        get_filename_component(COMPILER_NAME "${${COMPILER}}" NAME)
        if((NOT "${COMPILER_NAME}" STREQUAL "${${COMPILER}}") AND
           (NOT "${COMPILER_DIRECTORY}"))
            get_filename_component(
                COMPILER_PATH "${${COMPILER}}" REALPATH)
            get_filename_component(
                COMPILER_DIRECTORY "${COMPILER_PATH}" DIRECTORY)
        endif()
    else()
         message(FATAL_ERROR "${COMPILER} did not conform to the expected "
                             "executable format. i.e. it did not end with "
                             "arm, mips, pentium, or ppc.")
    endif()
endforeach()


get_filename_component(C_COMPILER_NAME "${CMAKE_C_COMPILER}" NAME)
get_filename_component(CXX_COMPILER_NAME "${CMAKE_CXX_COMPILER}" NAME)

# Ideally the location of the compiler should be resolved at this, but invoke
# find_program as a last resort.
if(NOT COMPILER_DIRECTORY)
    find_program(
        COMPILER_PATH NAMES "${C_COMPILER_NAME}" "${CXX_COMPILER_NAME}")
    if(COMPILER_PATH)
        get_filename_component(
            COMPILER_DIRECTORY "${COMPILER_PATH}" COMPILER_PATH)
    else()
        # The compiler must be successfully be detected by now.
        message(FATAL_ERROR "Could not determine location of compiler path.")
    endif()
endif()


get_filename_component(basename "${COMPILER_DIRECTORY}" NAME)
get_filename_component(basedir "${COMPILER_DIRECTORY}" DIRECTORY)
while(basename)
    if("${basename}" MATCHES "${WIND_PLATFORM_PATTERN}")
        string(
            REGEX REPLACE "${WIND_PLATFORM_PATTERN}" "\\1" version ${basename})
        if(NOT CMAKE_SYSTEM_VERSION)
          set(CMAKE_SYSTEM_VERSION ${version})
        endif()

        # The current base directory may be the WindRiver directory depending
        # on wether a "gnu" directory exists or not, but that is evaluated in
        # the next iteration.
        set(WIND_HOME "${basedir}")
        set(WIND_PLATFORM "${basename}")
    elseif(CMAKE_SYSTEM_VERSION AND WIND_HOME AND WIND_HOST_TYPE)
        # The "gnu" directory may not be part of the path. If it is, strip it.
        if("${basename}" STREQUAL "gnu")
            set(WIND_HOME "${basedir}")
        endif()
        break()
    elseif("${basename}" MATCHES "${WIND_HOST_TYPE_PATTERN}")
        set(WIND_HOST_TYPE "${basename}")
    endif()

    get_filename_component(basename ${basedir} NAME)
    get_filename_component(basedir ${basedir} DIRECTORY)
endwhile()


# VxWorks commands require the WIND_BASE environment variable, so this script
# will support it too. If the environment variable is not set, the necessary
# path information is deduced from the compiler path.
if(NOT WIND_BASE)
    set(WIND_BASE $ENV{WIND_BASE})
endif()

if(NOT WIND_BASE)
    set(WIND_BASE "${WIND_HOME}/vxworks-${CMAKE_SYSTEM_VERSION}")
endif()

# Verify the location WIND_BASE references actually exists.
if(NOT EXISTS ${WIND_BASE})
    message(FATAL_ERROR "VxWorks base directory ${WIND_BASE} does not exist, "
                        "please ensure the toolchain information is correct.")
elseif(NOT ENV{WIND_BASE})
    # WIND_BASE environment variable must be exported during generation
    # otherwise compiler tests will fail.
    set(ENV{WIND_BASE} "${WIND_BASE}")
endif()


if(NOT CMAKE_C_COMPILER_VERSION)
    execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -dumpversion
        OUTPUT_VARIABLE CMAKE_C_COMPILER_VERSION)
    string(STRIP "${CMAKE_C_COMPILER_VERSION}" CMAKE_C_COMPILER_VERSION)
    message(STATUS "VxWorks C compiler version ${CMAKE_C_COMPILER_VERSION}")
endif()

if(NOT CMAKE_CXX_COMPILER_VERSION)
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -dumpversion
        OUTPUT_VARIABLE CMAKE_CXX_COMPILER_VERSION)
    string(STRIP "${CMAKE_CXX_COMPILER_VERSION}" CMAKE_CXX_COMPILER_VERSION)
    message(STATUS "VxWorks CXX compiler version ${CMAKE_C_COMPILER_VERSION}")
endif()

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)


# CMAKE_SOURCE_DIR does not resolve to the actual source directory because
# platform files are processed to early on in the process.
set(ROOT "${CMAKE_MODULE_PATH}/../")

if(WIN32)
    set(CMAKE_C_COMPILER_LAUNCHER "${CMAKE_BINARY_DIR}/launch-c.bat")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CMAKE_BINARY_DIR}/launch-cxx.bat")
    configure_file(
        "${ROOT}/launch-c.bat.in" "${CMAKE_C_COMPILER_LAUNCHER}" @ONLY)
    configure_file(
        "${ROOT}/launch-cxx.bat.in" "${CMAKE_CXX_COMPILER_LAUNCHER}" @ONLY)
else()
    # Check if a directory like lmapi-* exists (VxWorks 6.9) and append it to
    # LD_LIBRARY_PATH.
    file(GLOB WIND_LMAPI LIST_DIRECTORIES true "${WIND_HOME}/lmapi-*")
    if(WIND_LMAPI)
        set(WIND_LMAPI "${WIND_LMAPI}/${WIND_HOST_TYPE}/lib")
    endif()

    set(CMAKE_C_COMPILER_LAUNCHER "${CMAKE_BINARY_DIR}/launch-c")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CMAKE_BINARY_DIR}/launch-cxx")
    configure_file(
        "${ROOT}/launch-c.in" "${CMAKE_C_COMPILER_LAUNCHER}" @ONLY)
    configure_file(
        "${ROOT}/launch-cxx.in" "${CMAKE_CXX_COMPILER_LAUNCHER}" @ONLY)
    execute_process(COMMAND chmod a+rx "${CMAKE_C_COMPILER_LAUNCHER}")
    execute_process(COMMAND chmod a+rx "${CMAKE_CXX_COMPILER_LAUNCHER}")
endif()


set(WIND_INCLUDE_DIRECTORY "${WIND_BASE}/target/h")

# Versions before 6.8 have a different path for common libs.
if("${CMAKE_SYSTEM_VERSION}" VERSION_GREATER "6.8")
    set(WIND_LIBRARY_DIRECTORY "${WIND_BASE}/target/lib/usr/lib/${WIND_PROCESSOR_TYPE}/${CMAKE_SYSTEM_PROCESSOR}/common")
else()
    set(WIND_LIBRARY_DIRECTORY "${WIND_BASE}/target/usr/lib/${WIND_PROCESSOR_TYPE}/${CMAKE_SYSTEM_PROCESSOR}/common")
endif()

if(NOT EXISTS "${WIND_LIBRARY_DIRECTORY}")
    message(FATAL_ERROR "${CMAKE_SYSTEM_PROCESSOR} is not part of the "
                        "${WIND_PROCESSOR_TYPE} processor family.")
endif()

include_directories(BEFORE SYSTEM "${WIND_INCLUDE_DIRECTORY}")
link_directories("${WIND_LIBRARY_DIRECTORY}")

