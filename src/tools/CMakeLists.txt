#
# Copyright(c) 2006 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

set(CMAKE_INSTALL_TOOLSDIR "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/tools")

add_subdirectory(_confgen)
if(BUILD_IDLC)
  add_subdirectory(idlpp)
  add_subdirectory(idlc)
endif()
add_subdirectory(ddsperf)
