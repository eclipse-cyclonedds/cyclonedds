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
set(CMAKE_INSTALL_EXAMPLESDIR "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/examples")

install(
  FILES
    helloworld/HelloWorldData.idl
    helloworld/publisher.c
    helloworld/subscriber.c
    helloworld/CMakeLists.txt
    helloworld/readme.rst
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/helloworld"
  COMPONENT dev)

install(
  FILES
    roundtrip/RoundTrip.idl
    roundtrip/ping.c
    roundtrip/pong.c
    roundtrip/CMakeLists.txt
    roundtrip/readme.rst
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/roundtrip"
  COMPONENT dev)

install(
  FILES
    throughput/Throughput.idl
    throughput/publisher.c
    throughput/subscriber.c
    throughput/CMakeLists.txt
    throughput/readme.rst
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/throughput"
  COMPONENT dev)

if (BUILD_DOCS)
  find_package(Sphinx REQUIRED)
  sphinx_add_docs(
    examples_docs
    BUILDER html
    SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  install(
    DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/examples_docs/"
    DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}"
    COMPONENT dev
    PATTERN "_sources" EXCLUDE)
endif()

add_subdirectory(helloworld)
add_subdirectory(roundtrip)
add_subdirectory(throughput)
