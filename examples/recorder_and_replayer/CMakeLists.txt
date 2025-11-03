# Copyright(c) 2023 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(recorder_replayer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED)
include(FetchContent)

# Prevent subprojects (like mcap_builder) from attempting to find lz4/zstd
set(CMAKE_DISABLE_FIND_PACKAGE_lz4 TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_zstd TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_LZ4 TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_ZSTD TRUE)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  mcap_builder
  GIT_REPOSITORY https://github.com/olympus-robotics/mcap_builder.git
  GIT_TAG main
)
FetchContent_MakeAvailable(mcap_builder)

if(NOT TARGET CycloneDDS::ddsc)
  find_package(CycloneDDS REQUIRED)
endif()

add_library(dds_topic_descriptor_serde STATIC dds_topic_descriptor_serde.c)
if(TARGET CycloneDDS::ddsc)
  # dds_topic_descriptor_serde need dds_stream_countops()
  target_include_directories(dds_topic_descriptor_serde
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/cdr/include")
endif()
target_include_directories(dds_topic_descriptor_serde PUBLIC .)

target_link_libraries(dds_topic_descriptor_serde PRIVATE CycloneDDS::ddsc)

add_executable(recorder dds_recorder.cpp)

if(TARGET CycloneDDS::ddsc)
  target_include_directories(recorder
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/cdr/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/include")
endif()
target_link_libraries(recorder CycloneDDS::ddsc dds_topic_descriptor_serde mcap)


# add_executable(replayer dds_replayer.cpp)

# if(TARGET CycloneDDS::ddsc)
#   target_include_directories(replayer
#     PRIVATE
#     "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/cdr/include"
#     "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/include")
# endif()
# target_link_libraries(replayer CycloneDDS::ddsc dds_topic_descriptor_serde mcap)

# force to ignore many compile warnings in mcap...
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  set(disable_flags 
    "-Wno-documentation"
    "-Wno-error=documentation"
    "-Wno-sign-conversion"
    "-Wno-error=sign-conversion"
    "-Wno-implicit-int-conversion"
    "-Wno-error=implicit-int-conversion"
  )
  target_compile_options(mcap PRIVATE ${disable_flags})
  target_compile_options(recorder PRIVATE ${disable_flags})
  # target_compile_options(replayer PRIVATE ${disable_flags})
endif()
