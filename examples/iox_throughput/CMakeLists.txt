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
idlc_generate(IoxThroughput_lib IoxThroughput.idl)

add_executable(IoxThroughputPublisher ioxpublisher.c)
add_executable(IoxThroughputSubscriber ioxsubscriber.c)

target_link_libraries(IoxThroughputPublisher IoxThroughput_lib CycloneDDS::ddsc)
target_link_libraries(IoxThroughputSubscriber IoxThroughput_lib CycloneDDS::ddsc)

install(
  FILES
    "IoxThroughput.idl"
    "ioxpublisher.c"
    "ioxsubscriber.c"
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/throughput"
  COMPONENT dev)

install(
  FILES "CMakeLists.export"
  RENAME "CMakeLists.txt"
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/throughput"
  COMPONENT dev)
