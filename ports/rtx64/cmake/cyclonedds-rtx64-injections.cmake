#
# Copyright(c) 2026 IntervalZero, Inc
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

###############################################################################
##
## CMake configuration file that contains the injections needed to build
## Cyclone DDS for RTX64.
##
###############################################################################

##
## Make sure we don't execute this file multiple times.
##
if(DEFINED RTX64_INJECTIONS_LOADED)
    return()
endif()
set(RTX64_INJECTIONS_LOADED TRUE)

message(STATUS "RTX64 injections")

##
## Hard-code known RTX64 features instead of searching them at configuration time.
##
set(HAVE_GETOPT_H "" CACHE INTERNAL "")
set(HAVE_STDDEF_H "" CACHE INTERNAL "")
set(HAVE_STDINT_H "" CACHE INTERNAL "")
set(HAVE_SYS_TYPES_H "" CACHE INTERNAL "")
set(HAVE_CLOCK_GETTIME "" CACHE INTERNAL "")
set(HAVE_CLOCK_GETTIME_RT "" CACHE INTERNAL "")
set(HAVE_SIZEOF_SOCKADDR_IN6 1 CACHE INTERNAL "")
set(SIZEOF_SOCKADDR_IN6 1 CACHE INTERNAL "")
set(CMAKE_HAVE_PTHREAD_H "" CACHE INTERNAL "")
set(DDSRT_HAVE_GETHOSTNAME 1 CACHE INTERNAL "")
set(DDSRT_HAVE_INET_NTOP 1 CACHE INTERNAL "")
set(DDSRT_HAVE_INET_PTON 1 CACHE INTERNAL "")
set(DDSRT_HAVE_IP_ADD_SOURCE_MEMBERSHIP 1 CACHE INTERNAL "")
set(DDSRT_HAVE_MCAST_JOIN_SOURCE_GROUP 1 CACHE INTERNAL "")
set(DDSRT_HAVE_GETADDRINFO "" CACHE INTERNAL "")
set(DDSRT_HAVE_GETHOSTBYNAME_R "" CACHE INTERNAL "")
set(DDSRT_HAVE_CONDATTR_SETCLOCK "" CACHE INTERNAL "")
set(DDSRT_HAVE_RUSAGE "" CACHE INTERNAL "")

##
## Disable default features which are not supported by RTX64.
##
set(ENABLE_SECURITY 0)

##
## Make sure we use the idlc tools and libraries from the Windows build
##
set(HOST_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/build_host/install" CACHE PATH "Path to the Windows build host binaries")
set(_idlc_backend "${HOST_INSTALL_PREFIX}/lib/cycloneddsidlc.lib" CACHE FILEPATH "Host cycloneddsidlc backend" FORCE)
list(PREPEND CMAKE_PROGRAM_PATH "${HOST_INSTALL_PREFIX}/bin")
if(NOT TARGET idlc)
    add_executable(idlc IMPORTED GLOBAL)
    set_target_properties(idlc PROPERTIES
        IMPORTED_LOCATION "${HOST_INSTALL_PREFIX}/bin/idlc.exe"
    )
    add_executable(CycloneDDS::idlc ALIAS idlc)
endif()
if(NOT TARGET libidlc AND NOT TARGET CycloneDDS::libidlc)
    add_library(libidlc SHARED IMPORTED GLOBAL)
    set_target_properties(libidlc PROPERTIES
        IMPORTED_LOCATION "${HOST_INSTALL_PREFIX}/bin/cycloneddsidlc.dll"
    )
    add_library(CycloneDDS::libidlc ALIAS libidlc)
endif()

# Remove the automatically added dependency on dbghelp and dl, which are not supported on RTX64.
if(NOT TARGET dbghelp)
    add_library(dbghelp INTERFACE IMPORTED)
endif()
if(NOT TARGET dl)
    add_library(dl INTERFACE IMPORTED)
endif()

##
## Overload the find_package_name() macro to bypass the research of the Threads library.
##
if(NOT RTX64_FIND_PACKAGE_OVERRIDDEN)
    set(RTX64_FIND_PACKAGE_OVERRIDDEN TRUE)

    macro(find_package name)
        if("${name}" STREQUAL "Threads")
            message(STATUS "find_package(Threads) bypassed for RTX64 toolchain")

            set(Threads_FOUND TRUE)
            set(THREADS_FOUND TRUE)
            set(CMAKE_THREAD_LIBS_INIT "")
            set(CMAKE_HAVE_THREADS_LIBRARY 1)

            if(NOT TARGET Threads::Threads)
                add_library(Threads::Threads INTERFACE IMPORTED)
            endif()
        else()
            _find_package(${ARGV})
        endif()
    endmacro()
endif()

##
## Create a temporary script that runs the RTX64 StampTool on the target file.
##
set(STAMPTOOL_WRAPPER "${CMAKE_BINARY_DIR}/run_stamptool.bat")
file(WRITE "${STAMPTOOL_WRAPPER}" 
"@echo off
\"%RTX64Common%\\bin\\StampTool.exe\" %*
"
)

##
## Intercept the calls to "add_executable" to add the post-build command for RTX64 StampTool.
##
macro(add_executable _target_name)
    _add_executable(${ARGV})
    set(_other_args ${ARGN})
    list(FIND _other_args "IMPORTED" _is_imported)
    list(FIND _other_args "ALIAS" _is_alias)
    if(_is_imported EQUAL -1 AND _is_alias EQUAL -1)
        set(IS_RTSS_CONFIG "$<OR:$<CONFIG:RtssDebug>,$<CONFIG:RtssRelease>>")
        add_custom_command(
            TARGET ${_target_name} POST_BUILD
            COMMAND $<${IS_RTSS_CONFIG}:${CMAKE_COMMAND}> 
                    $<${IS_RTSS_CONFIG}:-E> 
                    $<${IS_RTSS_CONFIG}:echo> 
                    $<${IS_RTSS_CONFIG}:"RTX64 StampTool">
            COMMAND $<${IS_RTSS_CONFIG}:${STAMPTOOL_WRAPPER}>
                    $<${IS_RTSS_CONFIG}:$<TARGET_FILE:${_target_name}>>
            VERBATIM
        )
    endif()
endmacro()

##
## Intercept the calls to "add_library" to add the post-build command for RTX64 StampTool.
##
macro(add_library _target_name)
    _add_library(${ARGV})
    set(_other_args ${ARGN})
    list(FIND _other_args "IMPORTED" _is_imported)
    list(FIND _other_args "ALIAS" _is_alias)
    if(_is_imported EQUAL -1 AND _is_alias EQUAL -1)
        get_target_property(_lib_type ${_target_name} TYPE)
        if(_lib_type STREQUAL "SHARED_LIBRARY")
            target_compile_definitions(${_target_name} PRIVATE "DDS_EXPORT=__declspec(dllexport)")
            target_compile_definitions(${_target_name} INTERFACE "DDS_EXPORT=__declspec(dllimport)")
        endif()
        if(_lib_type STREQUAL "SHARED_LIBRARY" OR _lib_type STREQUAL "MODULE_LIBRARY")
            set(IS_RTSS_CONFIG "$<OR:$<CONFIG:RtssDebug>,$<CONFIG:RtssRelease>>")
            add_custom_command(
                TARGET ${_target_name} POST_BUILD
                COMMAND $<${IS_RTSS_CONFIG}:${CMAKE_COMMAND}> 
                        $<${IS_RTSS_CONFIG}:-E> 
                        $<${IS_RTSS_CONFIG}:echo> 
                        $<${IS_RTSS_CONFIG}:"RTX64 StampTool">
                COMMAND $<${IS_RTSS_CONFIG}:${STAMPTOOL_WRAPPER}>
                        $<${IS_RTSS_CONFIG}:$<TARGET_FILE:${_target_name}>>
                VERBATIM
            )
        endif()
    endif()
endmacro()

