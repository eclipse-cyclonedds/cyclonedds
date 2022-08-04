#
# Copyright(c) 2006 to 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
project(shm_throughput LANGUAGES C)
cmake_minimum_required(VERSION 3.16)

if(NOT TARGET CycloneDDS::ddsc)
  # Find the CycloneDDS package.
  find_package(CycloneDDS REQUIRED)
endif()

idlc_generate(TARGET ShmThroughput_lib FILES "ShmThroughput.idl")

add_executable(ShmThroughputPublisher shmpublisher.c)
add_executable(ShmThroughputSubscriber shmsubscriber.c)

target_link_libraries(ShmThroughputPublisher ShmThroughput_lib CycloneDDS::ddsc)
target_link_libraries(ShmThroughputSubscriber ShmThroughput_lib CycloneDDS::ddsc)
