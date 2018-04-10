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
set(CMAKE_INSTALL_TOOLSDIR "${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/tools")
add_subdirectory(pubsub)
add_subdirectory(config)
add_subdirectory(ddsls)

# VxWorks build machines use OpenJDK 8, which lack jfxrt.jar. Do not build launcher on that platform.
#
# TODO Instead of making inclusion dependent on platform name, the condition should instead be on the
#      jdk vendor (Oracle JDK, not OpenJDK). Find a way to make CMake aware of jdk vendor.
#option(BUILD_LAUNCHER "Enable building of launcher." ON)
#if(NOT CMAKE_SYSTEM_NAME STREQUAL "VxWorks" AND BUILD_LAUNCHER)
#  add_subdirectory(launcher)
#endif()
