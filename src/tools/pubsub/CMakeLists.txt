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
add_executable(pubsub pubsub.c common.c testtype.c porting.c)
target_link_libraries(pubsub ddsc compat)

if(WIN32)
  target_compile_definitions(pubsub PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

install(
  TARGETS pubsub
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT dev
)
if (MSVC)
  install(FILES $<TARGET_PDB_FILE:pubsub>
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev
    OPTIONAL
  )
endif()
