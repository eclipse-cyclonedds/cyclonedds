#
# Copyright(c) 2020 to 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

# Ubuntu 22.04 shared library builds with ASAN fail with:
#
# /usr/bin/ld: ../../openssl/CMakeFiles/security_openssl.dir/src/openssl_support.c.o: \
#   warning: relocation against `__asan_option_detect_stack_use_after_return' in \
#   read-only section `.text'
# /usr/bin/ld: ../../openssl/CMakeFiles/security_openssl.dir/src/openssl_support.c.o: \
#   relocation R_X86_64_PC32 against symbol `__asan_option_detect_stack_use_after_return' \
#   can not be used when making a shared object; recompile with -fPIC
#
# Nowhere does it use this for something other than a shared library, so it seems to me
# that CMake is using incorrect build flags. Unfortunately, grokking CMake is beyond my
# abilities, so an ugly workaround will have to do for now.

if(BUILD_SHARED_LIBS)
  add_library(security_openssl INTERFACE)
  target_sources(security_openssl INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/openssl_support.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/dds/security/openssl_support.h")
  target_include_directories(
    security_openssl INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
else()
  add_library(security_openssl OBJECT)
  target_sources(security_openssl PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/openssl_support.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/dds/security/openssl_support.h")
  target_include_directories(
    security_openssl PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../core/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../api/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsrt/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/ddsc/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../core/ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../../ddsrt/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../../core/include>")
  target_link_libraries(security_openssl PUBLIC OpenSSL::SSL)
  install(
    TARGETS security_openssl
    EXPORT "${PROJECT_NAME}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib)
endif()
