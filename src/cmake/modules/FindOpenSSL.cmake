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
if(TARGET CONAN_PKG::OpenSSL)
  add_library(OpenSSL::SSL INTERFACE IMPORTED)
  target_link_libraries(OpenSSL::SSL INTERFACE CONAN_PKG::OpenSSL)
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

