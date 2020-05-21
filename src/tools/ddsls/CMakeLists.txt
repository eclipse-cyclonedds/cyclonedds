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
add_executable(ddsls ddsls.c)

if(NOT WITH_FREERTOS)
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  find_package(Threads REQUIRED)
  target_link_libraries(ddsls PUBLIC ddsc Threads::Threads)
elseif()
  target_link_libraries(ddsls ddsc)
endif()

install(
  TARGETS ddsls
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT dev
)
