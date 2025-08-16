#
# Copyright(c) 2025 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
set(CMAKE_CXX_STANDARD 17)
include(GenerateExportHeader)

if(ENABLE_ICEORYX2)
  message(STATUS "Building Iceoryx2 PSMX plugin")
  set(psmx_iox2_sources
    src/psmx_iox2_impl.c
    include/psmx_iox2_impl.h)

  if(BUILD_SHARED_LIBS)
    add_library(psmx_iox2 SHARED ${psmx_iox2_sources})
  else()
    add_library(psmx_iox2 OBJECT ${psmx_iox2_sources})
    set_property(GLOBAL APPEND PROPERTY cdds_plugin_list psmx_iox2)
    set_property(GLOBAL PROPERTY psmx_iox2_symbols iox2_create_psmx)
  endif()

  set_target_properties(psmx_iox2 PROPERTIES VERSION ${PROJECT_VERSION})
  generate_export_header(psmx_iox2 BASE_NAME DDS_PSMX_IOX2 EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/psmx_iox2_export.h")

  target_include_directories(psmx_iox2 PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/ddsrt/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/core/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../ddsrt/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../core/ddsc/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../core/ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

  target_link_libraries(psmx_iox2 PRIVATE iceoryx2-c::static-lib)
  if(BUILD_SHARED_LIBS)
    target_link_libraries(psmx_iox2 PRIVATE ddsc)
  endif()

  if(MSVC)
    target_compile_definitions(psmx_iox2 PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()

  install(TARGETS psmx_iox2
    EXPORT "${PROJECT_NAME}"
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

if(ENABLE_ICEORYX)
  message(STATUS "Building Iceoryx PSMX plugin")
  set(psmx_iox_sources
    src/psmx_iox_impl.cpp
    include/psmx_iox_impl.hpp
    src/scheduling.hpp
    src/scheduling.cpp)

  if(BUILD_SHARED_LIBS)
    add_library(psmx_iox SHARED ${psmx_iox_sources})
  else()
    add_library(psmx_iox OBJECT ${psmx_iox_sources})
    set_property(GLOBAL APPEND PROPERTY cdds_plugin_list psmx_iox)
    set_property(GLOBAL PROPERTY psmx_iox_symbols iox_create_psmx)
  endif()

  set_target_properties(psmx_iox PROPERTIES VERSION ${PROJECT_VERSION})
  generate_export_header(psmx_iox BASE_NAME DDS_PSMX_IOX EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/psmx_iox_export.h")

  target_include_directories(psmx_iox PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/ddsrt/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/core/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../ddsrt/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../core/ddsc/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../core/ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

  target_link_libraries(psmx_iox PRIVATE iceoryx_hoofs::iceoryx_hoofs iceoryx_posh::iceoryx_posh)
  if(BUILD_SHARED_LIBS)
    target_link_libraries(psmx_iox PRIVATE ddsc)
  endif()

  if(MSVC)
    target_compile_definitions(psmx_iox PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()

  install(TARGETS psmx_iox
    EXPORT "${PROJECT_NAME}"
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
