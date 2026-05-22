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
## CMake configuration file that describes the toolchain for building
## RTX64 applications and libraries using the Microsoft's MSVC compiler.
##
###############################################################################

##
## Indicate where to find the platform file and the injections file.
##
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(CMAKE_PROJECT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/cmake/cyclonedds-rtx64-injections.cmake" CACHE FILEPATH "" FORCE)

##
## The target platform is RTX64.
##
set(CMAKE_SYSTEM_NAME RTX64)

##
## Declare the 2 build configurations allowed for RTX64 applications and libraries.
##
set(CMAKE_CONFIGURATION_TYPES
    "RtssRelease;RtssDebug"
    CACHE STRING "Configurations" FORCE)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

##
## Force CMake to use RtssDebug config (and not Debug config) for its compiler tests (try_compile).
##
set(CMAKE_TRY_COMPILE_CONFIGURATION "RtssDebug" CACHE STRING "Test config" FORCE)

##
## Specify the default compiler and linker options to use in RtssDebug and RtssRelease configurations.
##
set(_rtx64_base_flags "/I$(RTX64SDKDir4)/include /GS- /GR- /FC /EHsc /W3 /openmp- /Gs99999 /Gu /D_AMD64_ /DUNDER_RTSS /DWIN32 /D_WINDOWS")
set(CMAKE_C_FLAGS_INIT "${_rtx64_base_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_rtx64_base_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "/DLL")

set(CMAKE_C_COMPILE_OPTIONS_DLL "/LD")
set(CMAKE_CXX_COMPILE_OPTIONS_DLL "/LD")

set(CMAKE_C_FLAGS_RTSSRELEASE "/MT /Ox /Oi /Zi /Gy /DNDEBUG" CACHE STRING "RtssRelease C Flags" FORCE)
set(CMAKE_C_FLAGS_RTSSDEBUG "/JMC /MTd /Od /Zi /D_DEBUG" CACHE STRING "RtssDebug C Flags" FORCE)

set(CMAKE_CXX_FLAGS_RTSSRELEASE "/MT /Ox /Oi /Zi /Gy /DNDEBUG" CACHE STRING "RtssRelease C++ Flags" FORCE)
set(CMAKE_CXX_FLAGS_RTSSDEBUG "/JMC /MTd /Od /Zi /D_DEBUG" CACHE STRING "RtssDebug C++ Flags" FORCE)

set(CMAKE_EXE_LINKER_FLAGS_RTSSDEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /LIBPATH:$(RTX64SDKDir4)/lib/amd64 /INCREMENTAL:NO /MANIFEST:NO /MANIFESTUAC:NO /LTCG:OFF /Driver /NODEFAULTLIB /ENTRY:_RtapiProcessEntryCRT /STACK:131072,131072 /SUBSYSTEM:NATIVE /DEBUG StartupCrt.lib libcmtd.lib libucrtd.lib libvcruntimed.lib rtx_rtss.lib rttcpip.lib" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RTSSRELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LIBPATH:$(RTX64SDKDir4)/lib/amd64 /INCREMENTAL:NO /MANIFEST:NO /MANIFESTUAC:NO /LTCG:OFF /Driver /NODEFAULTLIB /ENTRY:_RtapiProcessEntryCRT /SUBSYSTEM:NATIVE /DEBUG /OPT:REF /OPT:ICF StartupCrt.lib libcmt.lib libucrt.lib libvcruntime.lib rtx_rtss.lib rttcpip.lib" CACHE STRING "" FORCE)

set(CMAKE_SHARED_LINKER_FLAGS_RTSSDEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /LIBPATH:$(RTX64SDKDir4)/lib/amd64 /INCREMENTAL:NO /MANIFEST:NO /MANIFESTUAC:NO /LTCG:OFF /Driver /NODEFAULTLIB /ENTRY:_RtapiDllEntryCRT /STACK:131072,131072 /SUBSYSTEM:NATIVE /DEBUG StartupDllCrt.lib libcmtd.lib libucrtd.lib libvcruntimed.lib rtx_rtss.lib rttcpip.lib" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_RTSSRELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LIBPATH:$(RTX64SDKDir4)/lib/amd64 /INCREMENTAL:NO /MANIFEST:NO /MANIFESTUAC:NO /LTCG:OFF /Driver /NODEFAULTLIB /ENTRY:_RtapiDllEntryCRT /SUBSYSTEM:NATIVE /DEBUG /OPT:REF /OPT:ICF StartupDllCrt.lib libcmt.lib libucrt.lib libvcruntime.lib rtx_rtss.lib rttcpip.lib" CACHE STRING "" FORCE)

