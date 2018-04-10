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
find_path(CRITERION_INC criterion/criterion.h PATH_SUFFIXES criterion)
find_library(CRITERION_LIB criterion)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Criterion DEFAULT_MSG CRITERION_LIB CRITERION_INC)

if (CRITERION_FOUND AND NOT TARGET Criterion)
  add_library(Criterion INTERFACE IMPORTED)

  set_property(TARGET Criterion PROPERTY INTERFACE_LINK_LIBRARIES "${CRITERION_LIB}")
  set_property(TARGET Criterion PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CRITERION_INC}")
endif()

