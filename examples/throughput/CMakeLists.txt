#
# Copyright(c) 2021 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
project(throughput LANGUAGES C)
cmake_minimum_required(VERSION 3.5)

if(NOT TARGET CycloneDDS::ddsc)
  # Find the CycloneDDS package.
  if(YOCTO_CROSSCOMPILING)
    find_package(CycloneDDS REQUIRED PATHS ${STAGING_DIR_NATIVE} NO_CMAKE_FIND_ROOT_PATH)
  else()
    find_package(CycloneDDS REQUIRED)
  endif()
endif()

# This is a convenience function, provided by the CycloneDDS package,
# that will supply a library target related the the given idl file.
# In short, it takes the idl file, generates the source files with
# the proper data types and compiles them into a library.
idlc_generate(TARGET Throughput_lib FILES Throughput.idl)

# Both executables have only one related source file.
add_executable(ThroughputPublisher publisher.c)
add_executable(ThroughputSubscriber subscriber.c)

# Both executables need to be linked to the idl data type library and
# the ddsc API library.
if(YOCTO_CROSSCOMPILING)
  unset(DDSC_LIB CACHE)
  find_library(DDSC_LIB NAMES ddsc REQUIRED PATHS ${STAGING_DIR_HOST}/usr/lib NO_DEFAULT_PATH)
  target_link_libraries(ThroughputPublisher Throughput_lib ${DDSC_LIB})
  target_link_libraries(ThroughputSubscriber Throughput_lib ${DDSC_LIB})
else()
  target_link_libraries(ThroughputPublisher Throughput_lib CycloneDDS::ddsc)
  target_link_libraries(ThroughputSubscriber Throughput_lib CycloneDDS::ddsc)
endif()
