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
configure_file("cmake/default.xml.in" "${CMAKE_PROJECT_NAME_SMALL}.xml" @ONLY)

set(CONFIG_FILES "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME_SMALL}.xml")

install(
  FILES ${CONFIG_FILES}
  DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/${CMAKE_PROJECT_NAME}"
  COMPONENT lib
)
