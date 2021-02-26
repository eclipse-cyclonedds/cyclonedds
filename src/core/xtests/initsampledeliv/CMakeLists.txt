#
# Copyright(c) 2019 ADLINK Technology Limited and others
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

idlc_generate(TARGET InitSampleDeliv_lib FILES InitSampleDelivData.idl)

add_executable(InitSampleDelivPub publisher.c)
add_executable(InitSampleDelivSub subscriber.c)

target_link_libraries(InitSampleDelivPub InitSampleDeliv_lib ddsc)
target_link_libraries(InitSampleDelivSub InitSampleDeliv_lib ddsc)
