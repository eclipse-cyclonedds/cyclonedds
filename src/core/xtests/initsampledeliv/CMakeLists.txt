#
# Copyright(c) 2019 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
idlc_generate(TARGET InitSampleDeliv_lib FILES InitSampleDelivData.idl WARNINGS no-implicit-extensibility)

add_executable(InitSampleDelivPub publisher.c)
add_executable(InitSampleDelivSub subscriber.c)

target_link_libraries(InitSampleDelivPub InitSampleDeliv_lib ddsc compat)
target_link_libraries(InitSampleDelivSub InitSampleDeliv_lib ddsc compat)
