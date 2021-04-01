#
# Copyright(c) 2006 to 2021 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
idlc_generate(ShmThroughput_lib ShmThroughput.idl)

add_executable(ShmThroughputPublisher shmpublisher.c)
add_executable(ShmThroughputSubscriber shmsubscriber.c)

target_link_libraries(ShmThroughputPublisher ShmThroughput_lib CycloneDDS::ddsc)
target_link_libraries(ShmThroughputSubscriber ShmThroughput_lib CycloneDDS::ddsc)

install(
  FILES
    "ShmThroughput.idl"
    "shmpublisher.c"
    "shmsubscriber.c"
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/throughput"
  COMPONENT dev)

install(
  FILES "CMakeLists.export"
  RENAME "CMakeLists.txt"
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/throughput"
  COMPONENT dev)
