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
if(TARGET CONAN_PKG::openssl)
  add_library(OpenSSL::SSL INTERFACE IMPORTED)
  target_link_libraries(OpenSSL::SSL INTERFACE CONAN_PKG::openssl)
  set(OPENSSL_FOUND TRUE)
else()
  # Loop over a list of possible module paths (without the current directory).
  get_filename_component(DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

  foreach(MODULE_DIR ${CMAKE_MODULE_PATH} ${CMAKE_ROOT}/Modules)
    get_filename_component(MODULE_DIR "${MODULE_DIR}" ABSOLUTE)
    if(NOT MODULE_DIR STREQUAL DIR)
      if(EXISTS "${MODULE_DIR}/FindOpenSSL.cmake")
        set(FIND_PACKAGE_FILE "${MODULE_DIR}/FindOpenSSL.cmake")
        break()
      endif()
    endif()
  endforeach()

  if(FIND_PACKAGE_FILE)
    include("${FIND_PACKAGE_FILE}")
  endif()
endif()

# OpenSSL DLL on Windows: use of BIO_s_fd and BIO_s_file (directly or indirectly) requires
# the executable to incorporate OpenSSL applink.c.  CMake 18 adds support for handling
# this as part of the OpenSSL package, but we can't require such a new CMake version.
if(OPENSSL_FOUND AND EXISTS "${OPENSSL_INCLUDE_DIR}/openssl/applink.c")
  set(CYCLONEDDS_OPENSSL_APPLINK "${OPENSSL_INCLUDE_DIR}/openssl/applink.c")
else()
  set(CYCLONEDDS_OPENSSL_APPLINK "")
endif()
