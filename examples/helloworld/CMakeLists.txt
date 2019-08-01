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
cmake_minimum_required(VERSION 3.5)

if (NOT TARGET CycloneDDS::ddsc)
    # Find the CycloneDDS package. If it is not in a default location, try
    # finding it relative to the example where it most likely resides.
    find_package(CycloneDDS REQUIRED PATHS ../../)
endif()

# This is a convenience function, provided by the CycloneDDS package,
# that will supply a library target related the the given idl file.
# In short, it takes the idl file, generates the source files with
# the proper data types and compiles them into a library.
idlc_generate(HelloWorldData_lib "HelloWorldData.idl")

# Both executables have only one related source file.
add_executable(HelloworldPublisher publisher.c)
add_executable(HelloworldSubscriber subscriber.c)

# Both executables need to be linked to the idl data type library and
# the ddsc API library.
target_link_libraries(HelloworldPublisher HelloWorldData_lib CycloneDDS::ddsc)
target_link_libraries(HelloworldSubscriber HelloWorldData_lib CycloneDDS::ddsc)

#
# The helloworld example is the only example to have a different CMakeLists.txt
# for building in source and install environments. This is because it has to
# do some specific installations to support the Getting Started Guide.
# The CMakelists.export will be used for the install environments.
#

install(
  TARGETS HelloworldSubscriber HelloworldPublisher
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/helloworld/bin"
  COMPONENT dev)

install(
  FILES "CMakeLists.export"
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/helloworld"
  COMPONENT dev
  RENAME "CMakeLists.txt")

get_target_property(GENERATED_FILES HelloWorldData_lib INTERFACE_SOURCES)
install(
  FILES ${GENERATED_FILES}
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/helloworld/generated"
  COMPONENT dev)

