#
# Copyright(c) 2006 to 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
cmake_minimum_required(VERSION 3.16)
project(FreeRTOS-Sim VERSION 10.0.2.0 LANGUAGES C)

include(GNUInstallDirs)

# Some distributions place libraries in lib64 when the architecture is x86_64,
# but since the simulator is compiled with -m32, lib is a better name.
if(UNIX AND CMAKE_INSTALL_LIBDIR STREQUAL "lib64")
  set(CMAKE_INSTALL_LIBDIR "lib")
endif()

# Conflicts may be introduced when placing the libraries or headers in the
# default system locations, i.e. /usr/lib and /usr/include on *NIX platforms.
# The install prefix must therefore be postfixed with the project name.
if(UNIX)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}")
endif()

set(ENTRYPOINT "real_main"
      CACHE STRING "Alternate name of original entrypoint")
set(FREERTOS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Sim"
      CACHE STRING "Location of FreeRTOS POSIX Port sources")

set(source_path "${FREERTOS_SOURCE_DIR}/Source")

list(APPEND sources
  "${source_path}/croutine.c"
  "${source_path}/event_groups.c"
  "${source_path}/list.c"
  "${source_path}/queue.c"
  "${source_path}/tasks.c"
  "${source_path}/timers.c"
  "${source_path}/portable/MemMang/heap_3.c"
  "${source_path}/portable/GCC/POSIX/port.c")

list(APPEND headers
  "${source_path}/include/croutine.h"
  "${source_path}/include/deprecated_definitions.h"
  "${source_path}/include/event_groups.h"
  "${source_path}/include/FreeRTOS.h"
  "${source_path}/include/list.h"
  "${source_path}/include/mpu_prototypes.h"
  "${source_path}/include/mpu_wrappers.h"
  "${source_path}/include/portable.h"
  "${source_path}/include/projdefs.h"
  "${source_path}/include/queue.h"
  "${source_path}/include/semphr.h"
  "${source_path}/include/StackMacros.h"
  "${source_path}/include/task.h"
  "${source_path}/include/timers.h"
  "${source_path}/portable/GCC/POSIX/portmacro.h")

list(APPEND headers
  "${CMAKE_CURRENT_SOURCE_DIR}/include/FreeRTOSConfig.h")

add_library(freertos-sim ${sources})
target_compile_definitions(
  freertos-sim PUBLIC __GCC_POSIX=1 MAX_NUMBER_OF_TASKS=300)
target_compile_options(
  freertos-sim
    PUBLIC
      -m32
    PRIVATE
      -W -Wall -Werror -Wmissing-braces -Wno-cast-align -Wparentheses -Wshadow
      -Wno-sign-compare -Wswitch -Wuninitialized -Wunknown-pragmas
      -Wunused-function -Wunused-label -Wunused-parameter -Wunused-value
      -Wunused-variable -Wmissing-prototypes)
target_include_directories(
  freertos-sim
    PUBLIC
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
      "$<BUILD_INTERFACE:${source_path}/include>"
      "$<BUILD_INTERFACE:${source_path}/portable/GCC/POSIX>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_link_libraries(
  freertos-sim PUBLIC -m32 -pthread)

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG" OR
   CMAKE_BUILD_TYPE STREQUAL "RELWITHDEBINFO")
  target_compile_options(freertos-sim PUBLIC -ggdb)
  target_link_libraries(freertos-sim PUBLIC -ggdb)
endif()

# The FreeRTOS POSIX Port does not require hardware to be initialized (unless
# lwIP is used), but the scheduler must be started before it is safe to execute
# application code. A "loader" is built to avoid modifications to existing
# code. The generated toolchain file will automatically redefine "main" to
# "real_main". The "real_main" function is executed once the scheduler is
# started.
#
# The loader is not part of the freertos-sim target as it has no place in the
# board support package.
add_library(freertos-sim-loader
  "${CMAKE_CURRENT_SOURCE_DIR}/src/loader.c")
set_source_files_properties(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/loader.c"
  PROPERTIES COMPILE_DEFINITIONS real_main=${ENTRYPOINT})
target_link_libraries(freertos-sim-loader freertos-sim)

install(
  FILES ${headers}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

configure_file("freertos-sim.cmake.in" "freertos-sim.cmake" @ONLY)
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/freertos-sim.cmake"
  DESTINATION "${CMAKE_INSTALL_DATADIR}")

install(
  TARGETS freertos-sim freertos-sim-loader
  EXPORT "${PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

