#
# Copyright(c) 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

set(srcs_cdr
  dds_cdrstream.c)

set(hdrs_private_cdr
  dds_cdrstream.h)

prepend(hdrs_private_cdr "${CMAKE_CURRENT_LIST_DIR}/include/dds/cdr/" ${hdrs_private_cdr})
prepend(srcs_cdr "${CMAKE_CURRENT_LIST_DIR}/src/" ${srcs_cdr})

target_sources(ddsc PRIVATE ${srcs_cdr} ${hdrs_private_cdr})
target_include_directories(ddsc PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")

install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev)
