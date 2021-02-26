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
idlc_generate(TARGET RoundTrip_lib FILES RoundTrip.idl)

add_executable(RoundtripPing ping.c)
add_executable(RoundtripPong pong.c)

target_link_libraries(RoundtripPing RoundTrip_lib CycloneDDS::ddsc)
target_link_libraries(RoundtripPong RoundTrip_lib CycloneDDS::ddsc)

install(
  FILES
    "RoundTrip.idl"
    "ping.c"
    "pong.c"
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/roundtrip"
  COMPONENT dev)

install(
  FILES "CMakeLists.export"
  RENAME "CMakeLists.txt"
  DESTINATION "${CMAKE_INSTALL_EXAMPLESDIR}/roundtrip"
  COMPONENT dev)
